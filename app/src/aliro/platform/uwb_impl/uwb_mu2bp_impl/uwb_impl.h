/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "aliro/errors.h"
#include "aliro/utils.h"
#include "uwb.h"
#include "uwbproto.h"
#include "aliro_uwb.h"

#include <cstddef>

#include <zephyr/kernel.h>
#include <zephyr/types.h>


namespace Aliro::Uwb
{

#define NI_MAX_CONNECTIONS  (UWB_MAX_SESSIONS)
#define NI_MAX_MESSAGE      (256)

/**
 * @class UltraWideBandImpl
 * @brief Dummy implementation of the UltraWideBand interface.
 *
 * This class provides the dummy implementation of the UltraWideBand interface, handling UWB operations
 * such as initialization, ranging session management, and BLE message handling.
 */
class UltraWideBandImpl : public UltraWideBand<UltraWideBandImpl>
{
public:
    /**
     * @brief Gets the instance of the UltraWideBand implementation.
     *
     * @return The instance of the UltraWideBand implementation.
     */
    static UltraWideBandImpl &Instance()
    {
        static UltraWideBandImpl sInstance;
        return sInstance;
    }

    AliroError _Init(const Callbacks &callbacks);
    AliroError _Deinit();
    void _BleTimeSync();
    AliroError _HandleBleMessage(const uint8_t *data, size_t length, SessionContextHandle sessionContextData);
    AliroError _ConfigureRangingSession(SessionIdentifier sessionId, const CryptoTypes::Ursk &ursk,
                                        ProtocolVersion protocolVersion, SessionContextHandle sessionContextHandle);
    AliroError _InitiateRangingSession(SessionContextHandle sessionContextData);
    AliroError _TerminateRangingSession(SessionContextHandle sessionContextData);
    AliroError _SuspendRangingSession(SessionContextHandle sessionContextData, bool force);
    AliroError _ResumeRangingSession(SessionContextHandle sessionContextData);

    // Delete copy and move constructors and assignment operators.
    UltraWideBandImpl(const UltraWideBandImpl &) = delete;
    UltraWideBandImpl &operator=(const UltraWideBandImpl &) = delete;
    UltraWideBandImpl(UltraWideBandImpl &&) = delete;
    UltraWideBandImpl &operator=(UltraWideBandImpl &&) = delete;

    int SessionStateChanged(uwb_session_t *session, uint8_t state, uint8_t reason);

private:
    UltraWideBandImpl() = default;
    ~UltraWideBandImpl() = default;

    struct uwbSessionContext
    {
        uwbSessionContext(SessionContextHandle sessionContextHandle)
            : sessionHandle(sessionContextHandle), uwbSession(NULL)
        {
            UWBinitSessionParameters(&sessionParameters);
        }

        sys_snode_t mSessionContextNode{};

        SessionIdentifier sessionIdentifier;
        SessionContextHandle sessionHandle;
        CryptoTypes::Ursk ursk;
        ProtocolVersion protocolVersion;
        uwb_session_params_t sessionParameters;

        uwb_session_t *uwbSession;

        bool        in_use;
        uint32_t    session_id;
        uint8_t     device_role;
        uint8_t     device_type;
        uint8_t     profile_id;
        uint8_t     our_mac_addr[2];
        uint16_t    our_uwb_ver[2];

        e_session_state_t session_state;
    };

    void TransmitBleMessage(SessionContextHandle sessionHandle, uint8_t *data, size_t length);

    struct uwbSessionContext *FindSession(const uwb_session_t *uwbSession);
    struct uwbSessionContext *FindSession(const SessionContextHandle sessionHandle);
    AliroError AddSession(SessionContextHandle sessionHandle);
    void RemoveSession(struct uwbSessionContext *sessionCtx);
    void RemoveAllSessions();

    sys_slist_t     mActiveSessionsList{};
    uint8_t         mMessage[NI_MAX_MESSAGE];

    Callbacks       mCallbacks{};

    struct k_sem    mStartSem;
    struct k_thread mThread;
    k_mutex         mMutex{};

    static bool     sThreadStarted;
};

} // namespace Aliro::Uwb
