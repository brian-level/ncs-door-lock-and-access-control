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
#include "uwb_cli.h"

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
    static int _SessionStateCallback(void *session, uint8_t state, uint8_t reason);
}

static void _UwbProtoTask(void *p1, void *p2, void *p3)
{
    UltraWideBandImpl *self = (UltraWideBandImpl *)p1;
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
#if 0
    int ret = 0;
    struct ble_ni_connection *connection;
    uint32_t session_handle;

    require(session, exit);
    connection = _NIfindConnection(session->ble_conn_ctx);

    // note, the session's connection can be taken away
    // if the BLE client closes the connection first
    //
    if (connection)
    {
        connection->session_state = state;
    }

    session_handle = session->session_handle;

    switch (state)
    {
    case UWB_SESSION_INITIALIZED:
        LOG_DBG("Session %08X Initialized", session_handle);

        if (connection)
        {
            connection->session_state = SS_INIT;
        }

        break;

    case UWB_SESSION_DEINITIALIZED:
        LOG_DBG("Session %08X de-Initialized", session_handle);

        if (connection)
        {
            connection->session_state = SS_OVER;
        }

        break;

    case UWB_SESSION_ACTIVE:
        LOG_DBG("ctx %08X   session %08X started",
                (uint32_t)(uintptr_t)connection->conn_ctx, session_handle);

        if (connection)
        {
            connection->session_state = SS_ACTIVE;
        }

        break;

    case UWB_SESSION_IDLE:
        LOG_DBG("Session %08X Idle", session_handle);

        if (connection)
        {
            connection->session_state = SS_IDLE;
        }

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
#endif
    return 0;
}

AliroError UltraWideBandImpl::_Init(const Callbacks &callbacks)
{
    int ret;

    ret = UWBcliInit(_SessionStateCallback, false);
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
    int ret;
    struct uwbSessionContext *connection;
    size_t outLength;

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
            ret = AliroUWBparseM2(&connection->sessionParameters, data + 4, length - 4);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);
            ret = AliroUWBbuildM3(&connection->sessionParameters, mMessage, sizeof(mMessage), &outLength);
            VerifyOrReturnStatus(!ret && outLength > 0, ALIRO_ERROR_INTERNAL);
            LOG_HEXDUMP_INF(mMessage, outLength, "M3 -------------------");
            TransmitBleMessage(connection->sessionHandle, mMessage, outLength);
            break;
        case ALIRO_PT_UWB_SSM4:
            VerifyOrReturnStatus(length > 4, ALIRO_INVALID_ARGUMENT);
            ret = AliroUWBparseM4(&connection->sessionParameters, data + 4, length - 4);
            VerifyOrReturnStatus(!ret, ALIRO_ERROR_INTERNAL);
            break;
        case ALIRO_PT_UWB_SUSPEND_REQ:
        case ALIRO_PT_UWB_SUSPEND_RSP:
        case ALIRO_PT_UWB_RESUME_REQ:
        case ALIRO_PT_UWB_RESUME_RSP:
        case ALIRO_PT_UWB_SSM1:
        case ALIRO_PT_UWB_SSM3:
            LOG_ERR("Unexpected UWB Msg");
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
                break;
            case ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR:
                LOG_ERR("General Error: 0x%02x", data[3]);
                break;
            case ALIRO_ATTR_NTF_EVENT_RDR_DESC:
                break;
            }
            break;
        case ALIRO_PT_NOTIFICATION_RANGING:
            LOG_INF("Ranging Notification");

            // Build an UWB M1 message and send back
            //
            ret = AliroUWBbuildM1((uint32_t)connection->sessionIdentifier, &connection->sessionParameters, mMessage, sizeof(mMessage), &outLength);
            VerifyOrReturnStatus(ret == 0 && outLength > 0, ALIRO_ERROR_INTERNAL, LOG_ERR("Can't build M1"));
            LOG_HEXDUMP_INF(mMessage, outLength, "M1 -------------------");
            TransmitBleMessage(connection->sessionHandle, mMessage, outLength);
            break;
        case ALIRO_PT_NOTIFICATION_RDR_STATUS_CHANGE:
            break;
        case ALIRO_PT_NOTIFICATION_RDR_ACCESS_PROT_DONE:
            break;
        case ALIRO_PT_NOTIFICATION_RDR_RKE_REQ:
            break;
        case ALIRO_PT_NOTIFICATION_IAP:
            break;
        case ALIRO_PT_NOTIFICATION_IAP_RKE:
            break;
        default:
            return ALIRO_INVALID_ARGUMENT;
        }
        break;
    case ALIRO_PROTO_TYPE_SUPPLEMENTARY:
        LOG_INF("BLE Msg: Supplemental %02x", data[1]);
        break;
    case ALIRO_PROTO_TYPE_THIRD_PARTY:
        LOG_INF("BLE Msg: 3rdParty %02x", data[1]);
        break;
    default:
        return ALIRO_INVALID_ARGUMENT;
    }

    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::AddSession(SessionContextHandle sessionHandle)
{
    auto newCtx = Aliro::new_nothrow<struct uwbSessionContext>(sessionHandle);
    VerifyOrReturnStatus(newCtx, ALIRO_NO_MEMORY, LOG_ERR("Memory allocation failed for session context."));

    newCtx->in_use = true;
    newCtx->session_state = SS_INACTIVE;

    /// TODO - generate random mac address?
#if 1 // be the controller (reader app is controllee for Aliro)
    newCtx->device_type = UWB_DeviceType_Controller;
    newCtx->device_role = UWB_DeviceRole_Initiator;
    newCtx->our_mac_addr[0] = 0x11;
    newCtx->our_mac_addr[1] = 0x11;
#else
    newCtx->device_type = UWB_DeviceType_Controlee;
    newCtx->device_role = UWB_DeviceRole_Responder;
    newCtx->our_mac_addr[0] = 0x22;
    newCtx->our_mac_addr[1] = 0x22;
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

    // TOOD - clean up uwb side
    //DestroySession(sessionCtx);
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

        // TODO
        //DestroySession(sessionCtx);
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

    LOG_INF("%s", __FUNCTION__);
    LOG_INF("SessionHandler: 0x%08X  Id:%08X", (uint32_t)&sessionHandle, sessionIdentifier);

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

    RemoveSession(connection);
    return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_SuspendRangingSession([[maybe_unused]] SessionContextHandle, [[maybe_unused]] bool)
{
    LOG_INF("%s", __FUNCTION__);
    return ALIRO_ERROR_NOT_IMPLEMENTED;
}

AliroError UltraWideBandImpl::_ResumeRangingSession([[maybe_unused]] SessionContextHandle)
{
    LOG_INF("%s", __FUNCTION__);
    return ALIRO_ERROR_NOT_IMPLEMENTED;
}

} // namespace Aliro::Uwb
