#include "nrfspi.h"
#include "uwb_cli.h"
#include "assertmacros.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/types.h>
#include <zephyr/drivers/spi.h>

#include "autoconf.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nrfspi);

// This code implements the controller (SPIM) side of SPI bus
//
#define SPI_DEV_NODE DT_NODELABEL(spi1)
#define UCI_NODE DT_NODELABEL(uci_spi)

#define SPI_MAX_DATA        (256)
#define SPI_PACKET_SIZE     (4 + SPI_MAX_DATA)

typedef struct
{
    bool                    initialized;
    bool                    enabled;
    int                     max_packet;
    const struct device     *dev;
    uint32_t                rxRequested;
    struct gpio_callback    irqCallback;
    struct spi_config       spi_cfg;
    const struct gpio_dt_spec *irq_gpio;
    const struct gpio_dt_spec *sync_gpio;
    const struct gpio_dt_spec *ce_gpio;
}
nrfspi_t;

static nrfspi_t mSPI;

static int _nrfspi_trx(
    nrfspi_t *nrfspi,
    const uint8_t *txdata,
    const int txcount,
    uint8_t *rxdata,
    const int rxsize)
{
    int ret;

    struct spi_buf rx_buf[1];
    struct spi_buf_set rx;

    struct spi_buf tx_buf[1];
    struct spi_buf_set tx;

    rx_buf[0].buf = (uint8_t*)rxdata;
    rx_buf[0].len = rxsize;

    rx.buffers = rx_buf;
    rx.count = rxdata ? 1 : 0;

    tx_buf[0].buf = (uint8_t*)txdata;
    tx_buf[0].len = txcount;

    tx.buffers = tx_buf;
    tx.count = txdata ? 1 : 0;

    ret = spi_transceive(nrfspi->dev, &nrfspi->spi_cfg, &tx, &rx);
    return ret;
}

static void _host_irq_callback(const struct device *dev,
                               struct gpio_callback *cb, uint32_t pins)
{

    nrfspi_t *nrfspi = CONTAINER_OF(cb, nrfspi_t, irqCallback);

    // just count the request, well do the xfer in poll if
    // needed. the host should never interrupt unless it has
    // the whole packet to send already loaded into the sp
    // tx buffer on its side
    //
    nrfspi->rxRequested++;

    // disable host int for a bit
    gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_DISABLE);

    // signal any waiter to wake up
    TimeSignalApplicationEvent();

}

static const struct spi_cs_control mspi_cs =
{
    .gpio = GPIO_DT_SPEC_GET_BY_IDX(SPI_DEV_NODE, cs_gpios, DT_NODE_CHILD_IDX(UCI_NODE)),
    .delay = 0
};

static const struct gpio_dt_spec mspi_irq  = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_spi_irq), gpios);
static const struct gpio_dt_spec mspi_sync = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_spi_sync), gpios);
static const struct gpio_dt_spec mspi_ce   = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_spi_ce), gpios);

/*
The time required for the module to go into DPD state is < 100 탎 controlled by the firmware.
The required time for the module to enter HPD state is less then 100 탎 starting for the instance
that CE is de-asserted, in both modes VDD_1V8_DIG is turned off. The Wakeup timing from DPD
state is around 370 탎, the wakeup from HPD state is triggered once CE is asserted and takes
around 380 탎.
*/

/*
 * From NXP code

    masterConfig.baudRate_Bps = UWB_SPI_BAUDRATE;           // 8MHx
    masterConfig.dataWidth    = kSPI_Data8Bits;             // 8 bit
    masterConfig.polarity     = kSPI_ClockPolarityActiveHigh;// CPOL=0
    masterConfig.phase        = kSPI_ClockPhaseFirstEdge;   // CPHA=0
    masterConfig.direction    = kSPI_MsbFirst;              //

    masterConfig.delayConfig.preDelay      = 15U;
    masterConfig.delayConfig.postDelay     = 15U;
    masterConfig.delayConfig.frameDelay    = 15U;
    masterConfig.delayConfig.transferDelay = 15U;

    masterConfig.sselPol = kSPI_SpolActiveAllLow;           //
    masterConfig.sselNum = UWB_SPI_SSEL;
*/

static int _nrfspi_init(nrfspi_t *nrfspi)
{
    int ret = -ENODEV;

    memcpy(&nrfspi->spi_cfg.cs, &mspi_cs, sizeof(mspi_cs));
    nrfspi->spi_cfg.frequency = DT_PROP(UCI_NODE, spi_max_frequency);
    nrfspi->spi_cfg.slave = DT_REG_ADDR(UCI_NODE);
    nrfspi->irq_gpio = &mspi_irq;
    nrfspi->sync_gpio = &mspi_sync;
    nrfspi->ce_gpio = &mspi_ce;

    nrfspi->spi_cfg.cs.delay = 15;

    nrfspi->spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    nrfspi->spi_cfg.operation |= SPI_OP_MODE_MASTER;

    // de-assert sync
    ret = gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_INACTIVE);

    // disable chip
    ret = gpio_pin_configure_dt(nrfspi->ce_gpio, GPIO_OUTPUT_INACTIVE);

    // setup an interrupt on gpio for peripheral initiated transfers
    ret = gpio_pin_configure_dt(nrfspi->irq_gpio, GPIO_INPUT | GPIO_PULL_UP);
    require_noerr(ret, exit);

    // delay a bit to reset chip
    k_sleep(K_USEC(400));

#if 0
    // enable chip
    ret = gpio_pin_configure_dt(nrfspi->ce_gpio, GPIO_OUTPUT_ACTIVE);

    // let chip boot
    k_sleep(K_USEC(400));

    // If irq pin is stuck active, disable it and assume no spi attachement
    //
    volatile int state = gpio_pin_get_dt(nrfspi->irq_gpio);
    volatile int xstate = gpio_pin_get_raw(nrfspi->irq_gpio->port, nrfspi->irq_gpio->pin);
    int attempt;

    LOG_INF("SPI irq pin %d is %d.%d on open", nrfspi->irq_gpio->pin, state, xstate);

    // note, the slave could be just coming out of reset, so wait a second to let
    // it come up before we decide its there or not, and read it to clesr its irq
    //
    for (attempt = 0; state && (attempt < 20); attempt++)
    {
        state = gpio_pin_get_dt(nrfspi->irq_gpio);

        if (state)
        {
            uint8_t junk[32];

            nrfspi->rxRequested = 1;
            NRFSPIread(junk, sizeof(junk));
            k_sleep(K_MSEC(1));
        }
    }

    if (state)
    {
        // irq high for over 200ms, assume board is not populated
        //
        LOG_WRN("SPI pin %d irq stuck on, assuming no device attached", nrfspi->irq_gpio->pin);
        ret = -EIO;
        goto exit;
    }

#endif
    gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&nrfspi->irqCallback, _host_irq_callback, BIT(nrfspi->irq_gpio->pin));
    ret = gpio_add_callback(nrfspi->irq_gpio->port, &nrfspi->irqCallback);
    require_noerr(ret, exit);

    NRFSPIenableChip(0);

    nrfspi->rxRequested = 0;
    ret = 0;
exit:
    return ret;
}

int NRFSPIread(
    uint8_t *outRxData,
    int inRxSize)
{
    nrfspi_t *nrfspi = &mSPI;
    int ret = -EINVAL;

    require(outRxData, exit);
    require(inRxSize, exit);

    ret = -ENODEV;
    require(nrfspi->initialized, exit);

    // read the data
    ret = _nrfspi_trx(nrfspi, NULL, 0, outRxData, inRxSize);

exit:
    nrfspi->rxRequested = 0;
    gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    return ret;
}

int NRFSPIwrite(
    const uint8_t *inTxData,
    const int inTxCount)
{
    nrfspi_t *nrfspi = &mSPI;
    int ret = -EINVAL;

    require(inTxData, exit);
    require(inTxCount, exit);

    ret = -ENODEV;
    require(nrfspi->initialized, exit);

    // write what we want to tx
    ret = _nrfspi_trx(nrfspi, inTxData, inTxCount, NULL, 0);
exit:
    return ret;
}

int NRFSPIenableChip(bool enable)
{
    int ret;
    nrfspi_t *nrfspi = &mSPI;

    ret = gpio_pin_configure_dt(nrfspi->ce_gpio, enable ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);

    mSPI.enabled = enable;

    if (enable)
    {
        gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    }
    else
    {
        gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_DISABLE);
    }

    return ret;
}

int NRFSPIstartSync(void)
{
    int ret;
    nrfspi_t *nrfspi = &mSPI;
    volatile int irq_state;
    int timeout;

    // raise sync to allow reading
    ret = gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_ACTIVE);
    require_noerr(ret, exit);

    // wait for uwbs to drop irq line so we know its preparing rx data
    //
    timeout = 0;

    do
    {
        irq_state = gpio_pin_get_dt(nrfspi->irq_gpio);

        if (irq_state)
        {
            k_sleep(K_USEC(20));
            timeout += 20;
        }
    }
    while (irq_state && timeout < 1000);

    if (irq_state)
    {
        LOG_ERR("UWBS not read-ready");
        ret = -ETIMEDOUT;
        goto exit;
    }

    // now wait for uwbs to raise irq line so we know its ready to xfer
    //
    timeout = 0;

    do
    {
        irq_state = gpio_pin_get_dt(nrfspi->irq_gpio);

        if (!irq_state)
        {
            k_sleep(K_USEC(20));
            timeout += 20;
        }
    }
    while (!irq_state && timeout < 1000);

    if (!irq_state)
    {
        LOG_ERR("UWBS not read-ready (2)");
        ret = -ETIMEDOUT;
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

int NRFSPIstopSync(void)
{
    int ret;
    nrfspi_t *nrfspi = &mSPI;

    // lower sync
    ret = gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_INACTIVE);
    return ret;
}

int NRFSPIpoll(bool *outReadable)
{
    int ret = -EINVAL;
    nrfspi_t *nrfspi;

    nrfspi = &mSPI;

    require(outReadable, exit);

    if (nrfspi->enabled)
    {
        if (nrfspi->rxRequested == 0)
        {
            // its possible another interrupt happened while we had it
            // disabled during reading, so poll the irq line here
            //
            int irq_state = gpio_pin_get_dt(nrfspi->irq_gpio);

            if (irq_state)
            {
                nrfspi->rxRequested = 1;
                TimeSignalApplicationEvent();
            }
        }

        *outReadable = nrfspi->rxRequested > 0;
    }
    else
    {
        *outReadable = false;
    }

    ret = 0;
exit:
    return ret;
}

void NRFSPIdeinit(void)
{
    nrfspi_t *nrfspi;

    nrfspi = &mSPI;

    if (nrfspi->initialized)
    {
        // abort any active spi transactions
        nrfspi->initialized = false;

        // turn off host interrupts
        gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_DISABLE);
    }
}

int NRFSPIinit(void)
{
    nrfspi_t *nrfspi;
    int ret = -ENODEV;

    nrfspi = &mSPI;

    if (!nrfspi->initialized)
    {
        nrfspi->dev = DEVICE_DT_GET(SPI_DEV_NODE);
        require(nrfspi->dev, exit);

        require(device_is_ready(nrfspi->dev), exit);

        nrfspi->initialized = true;
        nrfspi->max_packet = 255;
    }

    ret = _nrfspi_init(nrfspi);
    require_noerr(ret, exit);
exit:
    return ret;
}

