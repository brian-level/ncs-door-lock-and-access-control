
#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/types.h>
#include <zephyr/drivers/spi.h>

// This implements a UCI I/O interface for a SPI connection on
// a Nordic nRF-Connect/Zephyr system to transfer UCI messages
//

// The protocol is s request->reply model. Both a request and
// reply are broken into a header and data phase.  The header
// is a 4 byte header the last byte of which is a count of
// following data bytes.
//
// The flow is controlled by two gpio lines "irq" and "sync"
//
// When the peripheral has data to send up, it interrupts the
// controller with the irq gpio.  When the controller takes
// the irq and is ready to read data it signals the peripheral
// with the sync gpio and waits for the peripheral to then
// signal ready by deactivating the irq line.
//

int NRFSPIwrite(
    const uint8_t *inData,
    const int inCount);
int NRFSPIread(
    uint8_t *outData,
    int inCoun);

int NRFSPIenableChip(bool enable);
int NRFSPIstartSync(void);
int NRFSPIstopSync(void);
int NRFSPIpoll(bool *outReadable);

void NRFSPIdeinit(void);
int  NRFSPIinit(void);

