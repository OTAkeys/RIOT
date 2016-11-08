/*
 * Copyright (C) 2015 PHYTEC Messtechnik GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     drivers_kw2xrf
 * @{
 * @file
 * @brief       Implementation of SPI-functions for the kw2xrf driver
 *
 * @author      Johann Fischer <j.fischer@phytec.de>
 * @author      Jonas Remmert <j.remmert@phytec.de>
 * @}
 */
#include "kw2xrf.h"
#include "kw2xrf_reg.h"
#include "kw2xrf_spi.h"
#include "periph/spi.h"
#include "periph/gpio.h"
#include "cpu_conf.h"
#include "irq.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define SPI_MODE                (SPI_MODE_0)
#define KW2XRF_IBUF_LENGTH      (9)

static uint8_t ibuf[KW2XRF_IBUF_LENGTH];

/** Set up in kw2xrf_spi_init during initialization */
static spi_t kw2xrf_spi;
static spi_clk_t kw2xrf_clk;
static spi_cs_t kw2xrf_cs_pin;

static inline void kw2xrf_spi_transfer_head(void)
{
    spi_acquire(kw2xrf_spi, kw2xrf_cs_pin, SPI_MODE, kw2xrf_clk);
}

static inline void kw2xrf_spi_transfer_tail(void)
{
    spi_release(kw2xrf_spi);
}

int kw2xrf_spi_init(spi_t spi, spi_clk_t spi_clk, spi_cs_t cs_pin)
{
    int res;
    kw2xrf_spi = spi;
    kw2xrf_clk = spi_clk;
    kw2xrf_cs_pin = cs_pin;     /**< for later reference */

    res = spi_init_cs(kw2xrf_spi, kw2xrf_cs_pin);
    if (res != SPI_OK) {
        DEBUG("kw2xrf_spi_init: error initializing SPI_DEV(%i) (code %i)\n",
              kw2xrf_spi, res);
        return -1;
    }

    return 0;
}

void kw2xrf_write_dreg(uint8_t addr, uint8_t value)
{
    kw2xrf_spi_transfer_head();
    spi_transfer_reg(kw2xrf_spi, kw2xrf_cs_pin, addr, value);
    kw2xrf_spi_transfer_tail();
    return;
}

uint8_t kw2xrf_read_dreg(uint8_t addr)
{
    uint8_t value;
    kw2xrf_spi_transfer_head();
    value = spi_transfer_reg(kw2xrf_spi, kw2xrf_cs_pin,
                             (addr | MKW2XDRF_REG_READ), 0x0);
    kw2xrf_spi_transfer_tail();
    return value;
}

void kw2xrf_write_iregs(uint8_t addr, uint8_t *buf, uint8_t length)
{
    if (length > (KW2XRF_IBUF_LENGTH - 1)) {
        length = KW2XRF_IBUF_LENGTH - 1;
    }

    ibuf[0] = addr;

    for (uint8_t i = 0; i < length; i++) {
        ibuf[i + 1] = buf[i];
    }

    kw2xrf_spi_transfer_head();
    spi_transfer_regs(kw2xrf_spi, kw2xrf_cs_pin, MKW2XDM_IAR_INDEX,
                      ibuf, NULL, length + 1);
    kw2xrf_spi_transfer_tail();

    return;
}

void kw2xrf_read_iregs(uint8_t addr, uint8_t *buf, uint8_t length)
{
    if (length > (KW2XRF_IBUF_LENGTH - 1)) {
        length = KW2XRF_IBUF_LENGTH - 1;
    }

    ibuf[0] = addr;

    kw2xrf_spi_transfer_head();
    spi_transfer_regs(kw2xrf_spi, kw2xrf_cs_pin,
                      MKW2XDM_IAR_INDEX | MKW2XDRF_REG_READ,
                      ibuf, ibuf, length + 1);
    kw2xrf_spi_transfer_tail();

    for (uint8_t i = 0; i < length; i++) {
        buf[i] = ibuf[i + 1];
    }

    return;
}

void kw2xrf_write_fifo(uint8_t *data, uint8_t length)
{
    kw2xrf_spi_transfer_head();
    spi_transfer_regs(kw2xrf_spi, kw2xrf_cs_pin, MKW2XDRF_BUF_WRITE,
                      data, NULL, length);
    kw2xrf_spi_transfer_tail();
}

void kw2xrf_read_fifo(uint8_t *data, uint8_t length)
{
    kw2xrf_spi_transfer_head();
    spi_transfer_regs(kw2xrf_spi, kw2xrf_cs_pin, MKW2XDRF_BUF_READ,
                      NULL, data, length);
    kw2xrf_spi_transfer_tail();
}
