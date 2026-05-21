/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "uwb_impl.h"

#include "NearbyInteraction.h"
#include "uwbproto.h"

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

		// wait for start semaphore TOTO
		k_msleep(delay);
	}
}

AliroError UltraWideBandImpl::_Init([[maybe_unused]] const Callbacks &)
{
	int ret;

	ret = NIinit(false);
	if (ret)
	{
		return ALIRO_ERROR_INTERNAL;
	}

	if (!sThreadStarted)
	{
		// Start a thread to run the UCI protocol in
		//
		k_tid_t id = k_thread_create( &mThread, sUwbProtoStack, K_THREAD_STACK_SIZEOF( sUwbProtoStack ), _UwbProtoTask, this, NULL, NULL, UWB_PROTO_PRIORITY, 0, K_NO_WAIT );
		if (id == NULL)
		{
			return ALIRO_NO_MEMORY;
		}

		sThreadStarted = true;
	}

	return ALIRO_NO_ERROR;
}

AliroError UltraWideBandImpl::_Deinit()
{
	return ALIRO_NO_ERROR;
}

void UltraWideBandImpl::
	_BleTimeSync() { /* No operation for dummy implementation; override in derived classes if needed. */ };

AliroError UltraWideBandImpl::_HandleBleMessage([[maybe_unused]] const uint8_t *, [[maybe_unused]] size_t,
						[[maybe_unused]] SessionContextHandle)
{
	return ALIRO_ERROR_NOT_IMPLEMENTED;
}

AliroError UltraWideBandImpl::_ConfigureRangingSession([[maybe_unused]] SessionIdentifier,
						       [[maybe_unused]] const CryptoTypes::Ursk &,
						       [[maybe_unused]] ProtocolVersion,
						       [[maybe_unused]] SessionContextHandle)
{
	LOG_INF("%s", __FUNCTION__);
	return ALIRO_ERROR_NOT_IMPLEMENTED;
}

AliroError UltraWideBandImpl::_InitiateRangingSession([[maybe_unused]] SessionContextHandle)
{
	LOG_INF("%s", __FUNCTION__);
	return ALIRO_ERROR_NOT_IMPLEMENTED;
}

AliroError UltraWideBandImpl::_TerminateRangingSession([[maybe_unused]] SessionContextHandle)
{
	LOG_INF("%s", __FUNCTION__);
	return ALIRO_ERROR_NOT_IMPLEMENTED;
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
