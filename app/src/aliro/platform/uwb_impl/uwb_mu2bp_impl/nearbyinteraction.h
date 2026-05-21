
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// if we handle a console (NUS) characteristic
// and report ranging data on that
//
#ifndef CONFIG_NI_BLE_CONSOLE
#define  CONFIG_NI_BLE_CONSOLE  1
#endif

#define NI_MAX_MESSAGE  256

const char *NIgetBLEname(void);
int NIbleConnectHandler(const void * const inConnectionHandle, const uint16_t inMTU, const bool isConnected);
int NIrxMessage(void *ble_conn_ctx, const uint8_t *inData, const uint32_t inCount);
void NIsendConsoleData(const char *inData, const int inLength);
int NIslice(uint32_t *delay);
int TimeWaitApplicationEvent(uint32_t inDelay);
void TimeSignalApplicationEvent(void);
int NIinit(const bool inHaveDisplay);

#ifdef __cplusplus
}
#endif
