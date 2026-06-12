/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "uwb_impl.h"

#include "aliro/aliro.h"
#include "aliro/memory.h"
#include "aliro/utils.h"
#include "mutex_guard.h"

#include "uwbproto.h"
#include "uwbdefs.h"
#include "aliro_proto.h"
#include "aliro_uwb.h"
#include "uwbsettings.h"

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uwbImplMU2BP);

namespace Aliro::Uwb {

#define UWB_PROTO_STACKSIZE         ( 4096 )
#define UWB_PROTO_PRIORITY          ( 11 )

K_THREAD_STACK_DEFINE( sUwbProtoStack, UWB_PROTO_STACKSIZE );

bool UltraWideBandImpl::sThreadStarted = false;

extern "C" {
    static void _UwbProtoTask(void *p1, void *p2, void *p3);
    static int _SessionStateCallback(uwb_session_t *session, uint8_t state, uint8_t reason);
}

static void _UwbProtoTask(void *p1, void *p2, void *p3)
{
    //UltraWideBandImpl *self = (UltraWideBandImpl *)p1;
    uint32_t delay;
    int ret;

    while (1)
    {
        delay = 100;

        ret = UWBslice(&delay);
        if (ret)
        {
            LOG_ERR("UwbSlice error!");
        }

        // wait for start semaphore TODO
        k_msleep(delay);
    }
}

static int _SessionStateCallback(uwb_session_t *session, uint8_t state, uint8_t reason)
{
    return UltraWideBandImpl::Instance().SessionStateChanged(session, state, reason);
}

int UltraWideBandImpl::SessionStateChanged(uwb_session_t *session, uint8_t state, uint8_t reason)
{
    int ret = 0;
    uint32_t session_handle;
    uint32_t outLength;
    struct uwbSessionContext *connection;

    connection = (struct uwbSessionContext *)session->ble_conn_ctx;

    LOG_INF("++++++ Session %08X State now %d cause %d", (uint32_t)(uintptr_t)connection, state, reason);

    session_handle = session->session_handle;

    /* RangingSessionState
    Uninitialized = 0x00,
    Initialized,
    Idle,
    Ranging,
    RangingSuspended,
    RangingResumed,
    Destroyed,
    */

    switch (state)
    {
    case UWB_SESSION_INITIALIZED:
        LOG_DBG("Session %08X Initialized", session_handle);
        if (connection)
        {
            connection->uwbSession = session;
            connection->session_state = SS_INIT;

            VerifyAndCall(mCallbacks.mRangingSessionStateChanged, connection->sessionHandle,
                      RangingSessionState::Initialized);
        }
        break;

    case UWB_SESSION_DEINITIALIZED:
        LOG_DBG("Session %08X de-Initialized", session_handle);
        if (connection)
        {
            connection->session_state = SS_OVER;

            VerifyAndCall(mCallbacks.mRangingSessionStateChanged, connection->sessionHandle,
                      RangingSessionState::Uninitialized);
        }
        break;

    case UWB_SESSION_ACTIVE:
        LOG_DBG("ctx %08X   session %08X started",
                (uint32_t)(uintptr_t)session, session_handle);
        if (connection)
        {
            connection->session_state = SS_ACTIVE;
        }
        VerifyAndCall(mCallbacks.mRangingSessionStateChanged, connection->sessionHandle,
                  RangingSessionState::Ranging);
        // transmit it
        ret = AliroUWBbuildState(ALIRO_OP_SRC_THIS_USER_BLE_UWB, RDR_STATUS_STARTED_UNSECURE, mMessage, sizeof(mMessage), &outLength);
        if (!ret && outLength)
        {
            LOG_HEXDUMP_INF(mMessage, outLength, "State change -------------------");
            TransmitBleMessage(connection->sessionHandle, mMessage, outLength);
        }
        break;

    case UWB_SESSION_IDLE:
        LOG_DBG("Session %08X Idle", session_handle);
        if (connection)
        {
            connection->session_state = SS_IDLE;
        }
        VerifyAndCall(mCallbacks.mRangingSessionStateChanged, connection->sessionHandle,
                  RangingSessionState::Idle);
        break;

    case UWB_SESSION_ERROR:
        LOG_INF("Session %08X Error %02X", session_handle, reason);
        if (connection)
        {
            connection->session_state = SS_OVER;
        }
        break;

    default:
        break;
    }

//exit:
    return ret;
}

AliroError UltraWideBandImpl::_Init(const Callbacks &callbacks)
{
    int ret;

    ret = UWBsettingsInit(_SessionStateCallback, false);
    if (ret)
    {
        return ALIRO_ERROR_INTERNAL;
    }

    VerifyOrReturnStatus(k_mutex_init(&mMutex) == 0, ALIRO_ERROR_INTERNAL, LOG_ERR("Failed to initialize mutex"));
    sys_slist_init(&mActiveSessionsList);

    if (!sThreadStarted)
    {
        // Start a thread to run the UCI protocol in
        //
        k_tid_t id = k_thread_create( &mThread, sUwbProtoStack, K_THREAD_STACK_SIZEOF( sUwbProtoStack ), _UwbProtoTask, this, NULL, NULL, UWB_PROTO_PRIORITY, 0, K_NO_WAIT );
        if (id == NULL)
        {
            return ALIRO_NO_MEMORY;
        }

        ret = k_sem_init(&mStartSem, 0, 1);
        if (ret)
        {
            return ALIRO_ERROR_INTERNAL;
        }

        sThreadStarted = true;
    }

    mCallbacks = callbacks;
    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_Deinit()
{
    return ALIRO_NO_ERROR;
}

void UltraWideBandImpl::
    _BleTimeSync() { /* No operation for dummy implementation; override in derived classes if needed. */ };

void UltraWideBandImpl::TransmitBleMessage(SessionContextHandle sessionHandle, uint8_t *data, size_t length)
{
    VerifyOrExit(data && length, LOG_ERR("No UWB data"));
    AliroStack::Instance().SendBleMessage(sessionHandle, data, length);
exit:
    return;
}

AliroError UltraWideBandImpl::_HandleBleMessage(const uint8_t *data, size_t length,
                        SessionContextHandle sessionHandle)
{
    AliroError err = ALIRO_INVALID_ARGUMENT;
    int ret;
    struct uwbSessionContext *connection;
    size_t outLength;
    size_t outAppConfigLength;
    size_t outVendorConfigLength;
    uint8_t vendorConfig[128];

    LOG_HEXDUMP_INF(data, length, "BLE BLOB");

    VerifyOrReturnStatus(data && length, ALIRO_INVALID_ARGUMENT, LOG_ERR("No Data or 0-length in BLE msg"));

    //LOG_INF("Handle BLE msg for handle %08X", (uint32_t)(uintptr_t)&sessionHandle);

    connection = FindSession(sessionHandle);
    VerifyOrReturnStatus(connection != NULL, ALIRO_INVALID_STATE, LOG_ERR("No active session for BLE msg"));

    switch (data[0])
    {
    case ALIRO_PROTO_TYPE_AP:
        VerifyOrReturnStatus(length > 0, ALIRO_INVALID_ARGUMENT);
        LOG_INF("BLE Msg: AP %02x", data[1]);
        break;
    case ALIRO_PROTO_TYPE_UWB:
        VerifyOrReturnStatus(length > 0, ALIRO_INVALID_ARGUMENT);
        LOG_INF("BLE Msg: UWB %02x", data[1]);
        switch (data[1])
        {
        case ALIRO_PT_UWB_SSM2:
            VerifyOrReturnStatus(length > 4, ALIRO_INVALID_ARGUMENT);
            ret = AliroUWBparseM2(&connection->configParameters, &connection->sessionParameters, data + 4, length - 4);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);
            AliroUWBConfigPrint("Post M2", &connection->sessionParameters);
            ret = AliroUWBbuildM3(&connection->sessionParameters, mMessage, sizeof(mMessage), &outLength);
            VerifyOrReturnStatus(!ret && outLength > 0, ALIRO_ERROR_INTERNAL);
            LOG_HEXDUMP_INF(mMessage, outLength, "M3 -------------------");
            TransmitBleMessage(connection->sessionHandle, mMessage, outLength);
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_UWB_SSM4:
            VerifyOrReturnStatus(length > 4, ALIRO_INVALID_ARGUMENT);
            ret = AliroUWBparseM4(&connection->configParameters, &connection->sessionParameters, data + 4, length - 4);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);
            AliroUWBConfigPrint("Post M4", &connection->sessionParameters);
            // create an app config message
            ret = AliroUWBbuildAppConfiguration(
                        &connection->sessionParameters,
                        mMessage,
                        sizeof(mMessage),
                        &outAppConfigLength);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);

            // create a vedor app config message
            ret = AliroUWBbuildVendorConfiguration(
                        &connection->sessionParameters,
                        connection->sessionIdentifier,
                        connection->ursk.data(),
                        sizeof(connection->ursk),
                        vendorConfig,
                        sizeof(vendorConfig),
                        &outVendorConfigLength);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);

            // startup uwb radio
            //
            ret = UWBstart(
                        UWB_DeviceType_Controller,
                        connection->sessionIdentifier,
                        false,
                        (void*)connection,
                        mMessage,
                        outAppConfigLength,
                        vendorConfig,
                        outVendorConfigLength);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_UWB_SUSPEND_REQ:
        case ALIRO_PT_UWB_SUSPEND_RSP:
        case ALIRO_PT_UWB_RESUME_REQ:
        case ALIRO_PT_UWB_RESUME_RSP:
        case ALIRO_PT_UWB_SSM1:
        case ALIRO_PT_UWB_SSM3:
            LOG_ERR("Unexpected UWB Msg");
            err = ALIRO_NO_ERROR;
            break;
        }
        break;
    case ALIRO_PROTO_TYPE_NOTIFICATION:
        VerifyOrReturnStatus(length > 0, ALIRO_INVALID_ARGUMENT);
        LOG_INF("BLE Msg: NTF %02x", data[1]);
        switch (data[1])
        {
        case ALIRO_PT_NOTIFICATION_EVENT:
            VerifyOrReturnStatus(length > 1, ALIRO_INVALID_ARGUMENT);
            LOG_INF("Event Notification %02x", data[2]);
            switch (data[2])
            {
            case ALIRO_ATTR_NFT_EVENT_BUSY:
                err = ALIRO_NO_ERROR;
                break;
            case ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR:
                LOG_ERR("General Error: 0x%02x", data[3]);
                err = ALIRO_NO_ERROR;
                break;
            case ALIRO_ATTR_NTF_EVENT_RDR_DESC:
                err = ALIRO_NO_ERROR;
                break;
            }
            break;
        case ALIRO_PT_NOTIFICATION_RANGING:
            LOG_INF("Ranging Notification");

            // Build an UWB M1 message and send back
            //
            AliroUWBConfigPrint("Pre M1", &connection->sessionParameters);
            ret = AliroUWBbuildM1((uint32_t)connection->sessionIdentifier, &connection->configParameters, mMessage, sizeof(mMessage), &outLength);
            VerifyOrReturnStatus(ret == 0 && outLength > 0, ALIRO_ERROR_INTERNAL, LOG_ERR("Can't build M1"));
            LOG_HEXDUMP_INF(mMessage, outLength, "M1 -------------------");
            TransmitBleMessage(connection->sessionHandle, mMessage, outLength);
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_NOTIFICATION_RDR_STATUS_CHANGE:
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_NOTIFICATION_RDR_ACCESS_PROT_DONE:
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_NOTIFICATION_RDR_RKE_REQ:
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_NOTIFICATION_IAP:
            err = ALIRO_NO_ERROR;
            break;
        case ALIRO_PT_NOTIFICATION_IAP_RKE:
            err = ALIRO_NO_ERROR;
            break;
        default:
            return ALIRO_INVALID_ARGUMENT;
        }
        break;
    case ALIRO_PROTO_TYPE_SUPPLEMENTARY:
        LOG_INF("BLE Msg: Supplemental %02x", data[1]);
        err = ALIRO_NO_ERROR;
        break;
    case ALIRO_PROTO_TYPE_THIRD_PARTY:
        LOG_INF("BLE Msg: 3rdParty %02x", data[1]);
        err = ALIRO_NO_ERROR;
        break;
    default:
        LOG_ERR("Unknown Msg protocol %02x", data[0]);
        return ALIRO_INVALID_ARGUMENT;
    }

    if (err.ToInt())
    {
        LOG_ERR("RETURN %d %s", err.ToInt(), err.ToString());
    }
    return err;
}

AliroError UltraWideBandImpl::AddSession(SessionContextHandle sessionHandle)
{
    auto newCtx = Aliro::new_nothrow<struct uwbSessionContext>(sessionHandle);
    VerifyOrReturnStatus(newCtx, ALIRO_NO_MEMORY, LOG_ERR("Memory allocation failed for session context."));

    newCtx->session_state = SS_INACTIVE;

    /// TODO - generate random mac address?
#if 0 // be the controller
    newCtx->sessionParameters.device_type = UWB_DeviceType_Controller;
    newCtx->sessionParameters.device_role = UWB_DeviceRole_Initiator;
    newCtx->sessionParameters.our_mac_addr[0] = 0x8c;
    newCtx->sessionParameters.our_mac_addr[1] = 0x00;
#else //  (reader app is controllee for Aliro)
    newCtx->sessionParameters.device_type = /*UWB_DeviceType_CCC_Controllee;*/ UWB_DeviceType_Controlee;
    newCtx->sessionParameters.device_role = UWB_DeviceRole_Responder;
    newCtx->sessionParameters.our_mac_addr[0] = 0x00;
    newCtx->sessionParameters.our_mac_addr[1] = 0x8d;
#endif
    LOG_DBG("New uwbCtx %08X for BLE Conn %08X\n",
            (uint32_t)(uintptr_t)newCtx,
            (uint32_t)(uintptr_t)sessionHandle.GetRaw());

    MutexGuard lock{ mMutex };

    sys_slist_append(&mActiveSessionsList, &newCtx->mSessionContextNode);

    return ALIRO_NO_ERROR;
}

void UltraWideBandImpl::RemoveSession(struct uwbSessionContext *sessionCtx)
{
    {
        MutexGuard lock{ mMutex };
        VerifyOrReturn(sys_slist_find_and_remove(&mActiveSessionsList, &sessionCtx->mSessionContextNode),
                   LOG_WRN("Session doesn't exist"));
    }

    if (sessionCtx && sessionCtx->uwbSession)
    {
        UWBstopSession(sessionCtx->uwbSession, true);
        sessionCtx->uwbSession = NULL;
    }

    delete sessionCtx;
}

void UltraWideBandImpl::RemoveAllSessions()
{
    while (true) {
        struct uwbSessionContext *sessionCtx = nullptr;

        {
            MutexGuard lock{ mMutex };
            sys_snode_t *node = sys_slist_get(&mActiveSessionsList);
            VerifyOrReturn(node);
            sessionCtx = CONTAINER_OF(node, struct uwbSessionContext, mSessionContextNode);
        }

        if (sessionCtx && sessionCtx->uwbSession)
        {
            UWBstopSession(sessionCtx->uwbSession, true);
            sessionCtx->uwbSession = NULL;
        }

        delete sessionCtx;
    }
}

struct UltraWideBandImpl::uwbSessionContext *UltraWideBandImpl::FindSession(SessionContextHandle sessionHandle)
{
    struct uwbSessionContext *sessionCtx{};

    MutexGuard lock{ mMutex };
    SYS_SLIST_FOR_EACH_CONTAINER (&mActiveSessionsList, sessionCtx, mSessionContextNode)
    {
        LOG_DBG("Find, Have %08lX look for %08lX", (uintptr_t)sessionCtx->sessionHandle.GetRaw(), (uintptr_t)sessionHandle.GetRaw());
        if (sessionCtx->sessionHandle == sessionHandle)
        {
            LOG_DBG("Found %08lX from %08lX", (uintptr_t)sessionCtx, (uintptr_t)sessionHandle.GetRaw());
            return sessionCtx;
        }
    }

    return nullptr;
}

AliroError UltraWideBandImpl::_ConfigureRangingSession(SessionIdentifier sessionIdentifier,
                               const CryptoTypes::Ursk &ursk,
                               ProtocolVersion protocolVersion,
                               SessionContextHandle sessionHandle)
{
    struct uwbSessionContext *connection;
    AliroError err;

    LOG_INF("%s SessionHandler: 0x%08X  Id:%08X", __FUNCTION__, (uint32_t)&sessionHandle, sessionIdentifier);

    connection = FindSession(sessionHandle);
    VerifyOrReturnStatus(connection == NULL, ALIRO_INVALID_STATE, LOG_ERR("Session already exists!"));

    err = AddSession(sessionHandle);
    VerifyOrReturnStatus(err == ALIRO_NO_ERROR, err, LOG_ERR("Can't add session"));

    connection = FindSession(sessionHandle);
    VerifyOrReturnValue(connection != NULL, ALIRO_NO_MEMORY);

    connection->sessionIdentifier = sessionIdentifier;
    connection->ursk = ursk;
    connection->protocolVersion = protocolVersion;

    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_InitiateRangingSession([[maybe_unused]] SessionContextHandle)
{
    LOG_INF("%s +++++++++++++++++++", __FUNCTION__);
    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_TerminateRangingSession(SessionContextHandle sessionHandle)
{
    struct uwbSessionContext *connection;
    AliroError err;

    connection = FindSession(sessionHandle);
    VerifyOrReturnStatus(connection != NULL, ALIRO_INVALID_STATE, LOG_ERR("No Session to terminate!"));

    if (connection->uwbSession)
    {
        UWBstopSession(connection->uwbSession, true);
        connection->uwbSession = NULL;
    }

    RemoveSession(connection);
    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_SuspendRangingSession(SessionContextHandle sessionHandle, bool force)
{
    struct uwbSessionContext *connection;
    AliroError err;

    connection = FindSession(sessionHandle);
    VerifyOrReturnStatus(connection != NULL, ALIRO_INVALID_STATE, LOG_ERR("No Session to suspend"));

    if (connection->uwbSession)
    {
        UWBstopSession(connection->uwbSession, false);
        connection->uwbSession = NULL;
    }

    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_ResumeRangingSession([[maybe_unused]] SessionContextHandle)
{
    LOG_INF("%s", __FUNCTION__);
    return ALIRO_ERROR_NOT_IMPLEMENTED;
}

} // namespace Aliro::Uwb
