/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_stm32f4discovery
 * @{
 *
 * @file
 * @name        Peripheral MCU configuration for the STM32F4discovery board
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 */

#ifndef PERIPH_CONF_H_
#define PERIPH_CONF_H_

#include "periph_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Clock system configuration
 * @{
 */
#define CLOCK_HSE           (8000000U)          /* external oscillator */
#define CLOCK_CORECLOCK     (168000000U)        /* desired core clock frequency */

/* the actual PLL values are automatically generated */
#define CLOCK_PLL_M         (CLOCK_HSE / 1000000)
#define CLOCK_PLL_N         ((CLOCK_CORECLOCK / 1000000) * 2)
#define CLOCK_PLL_P         (2U)
#define CLOCK_PLL_Q         (CLOCK_PLL_N / 48)
#define CLOCK_AHB_DIV       RCC_CFGR_HPRE_DIV1
#define CLOCK_APB2_DIV      RCC_CFGR_PPRE2_DIV2
#define CLOCK_APB1_DIV      RCC_CFGR_PPRE1_DIV4
#define CLOCK_FLASH_LATENCY FLASH_ACR_LATENCY_5WS

/* bus clocks for simplified peripheral initialization, UPDATE MANUALLY! */
#define CLOCK_AHB           (CLOCK_CORECLOCK / 1)
#define CLOCK_APB2          (CLOCK_CORECLOCK / 2)
#define CLOCK_APB1          (CLOCK_CORECLOCK / 4)
/** @} */

/**
 * @name Timer configuration
 * @{
 */
#define TIMER_NUMOF         (2U)
#define TIMER_0_EN          1
#define TIMER_1_EN          1
#define TIMER_IRQ_PRIO      1

/* Timer 0 configuration */
#define TIMER_0_DEV         TIM2
#define TIMER_0_CHANNELS    4
#define TIMER_0_FREQ        (CLOCK_CORECLOCK / 2)
#define TIMER_0_MAX_VALUE   (0xffffffff)
#define TIMER_0_CLKEN()     (periph_clk_en(APB1, RCC_APB1ENR_TIM2EN))
#define TIMER_0_ISR         isr_tim2
#define TIMER_0_IRQ_CHAN    TIM2_IRQn

/* Timer 1 configuration */
#define TIMER_1_DEV         TIM5
#define TIMER_1_CHANNELS    4
#define TIMER_1_FREQ        (CLOCK_CORECLOCK / 2)
#define TIMER_1_MAX_VALUE   (0xffffffff)
#define TIMER_1_CLKEN()     (periph_clk_en(APB1, RCC_APB1ENR_TIM5EN))
#define TIMER_1_ISR         isr_tim5
#define TIMER_1_IRQ_CHAN    TIM5_IRQn
/** @} */

/**
 * @brief   UART configuration
 * @{
 */
static const uart_conf_t uart_config[] = {
    {
        .dev        = USART2,
        .rcc_mask   = RCC_APB1ENR_USART2EN,
        .rx_pin     = GPIO_PIN(PORT_A, 3),
        .tx_pin     = GPIO_PIN(PORT_A, 2),
        .af         = GPIO_AF7,
        .bus        = APB1,
        .irqn       = USART2_IRQn,
        .dma_stream = 6,
        .dma_chan   = 4
    },
    {
        .dev        = USART3,
        .rcc_mask   = RCC_APB1ENR_USART3EN,
        .rx_pin     = GPIO_PIN(PORT_D, 9),
        .tx_pin     = GPIO_PIN(PORT_D, 8),
        .af         = GPIO_AF7,
        .bus        = APB1,
        .irqn       = USART3_IRQn,
        .dma_stream = 3,
        .dma_chan   = 4
    },
};

/* assign ISR vector names */
#define UART_0_ISR          isr_usart2
#define UART_0_DMA_ISR      isr_dma1_stream6
#define UART_1_ISR          isr_usart3
#define UART_1_DMA_ISR      isr_dma1_stream3

/* deduct number of defined UART interfaces */
#define UART_NUMOF          (sizeof(uart_config) / sizeof(uart_config[0]))
/** @} */

/**
 * @brief   ADC configuration
 *
 * We need to define the following fields:
 * PIN, device (ADCx), channel
 * @{
 */
#define ADC_CONFIG {              \
    {GPIO_PIN(PORT_A, 1), 0, 1},  \
    {GPIO_PIN(PORT_A, 4), 0, 4},  \
    {GPIO_PIN(PORT_C, 1), 1, 11}, \
    {GPIO_PIN(PORT_C, 2), 1, 12}  \
}

#define ADC_NUMOF           (4)
/** @} */

/**
 * @brief   DAC configuration
 *
 * We need to define the following fields:
 * PIN, DAC channel
 * @{
 */
#define DAC_CONFIG {            \
    { GPIO_PIN(PORT_A, 4), 0 }, \
    { GPIO_PIN(PORT_A, 5), 1 }, \
}

#define DAC_NUMOF           (2)
/** @} */

/**
 * @name PWM configuration
 * @{
 */
#define PWM_NUMOF           (2U)
#define PWM_0_EN            1
#define PWM_1_EN            1
#define PWM_MAX_CHANNELS    4

/* PWM 0 device configuration */
#define PWM_0_DEV           TIM1
#define PWM_0_CHANNELS      4
#define PWM_0_CLK           (168000000U)
#define PWM_0_CLKEN()       (periph_clk_en(APB2, RCC_APB2ENR_TIM1EN))
#define PWM_0_CLKDIS()      (periph_clk_dis(APB2, RCC_APB2ENR_TIM1EN))
/* PWM 0 pin configuration */
#define PWM_0_PORT          GPIOE
#define PWM_0_PORT_CLKEN()  (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOEEN))
#define PWM_0_PIN_CH0       9
#define PWM_0_PIN_CH1       11
#define PWM_0_PIN_CH2       13
#define PWM_0_PIN_CH3       14
#define PWM_0_PIN_AF        1

/* PWM 1 device configuration */
#define PWM_1_DEV           TIM3
#define PWM_1_CHANNELS      3
#define PWM_1_CLK           (84000000U)
#define PWM_1_CLKEN()       (periph_clk_en(APB1, RCC_APB1ENR_TIM3EN))
#define PWM_1_CLKDIS()      (periph_clk_dis(APB1, RCC_APB1ENR_TIM3EN))
/* PWM 1 pin configuration */
#define PWM_1_PORT          GPIOB
#define PWM_1_PORT_CLKEN()  (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOBEN))
#define PWM_1_PIN_CH0       4
#define PWM_1_PIN_CH1       5
#define PWM_1_PIN_CH2       0
#define PWM_1_PIN_CH3       1
#define PWM_1_PIN_AF        2
/** @} */

/**
 * @name SPI configuration
 * @{
 */
#define SPI_NUMOF           (2U)
#define SPI_0_EN            1
#define SPI_1_EN            1
#define SPI_IRQ_PRIO        1

/* SPI 0 device config */
#define SPI_0_DEV               SPI1
#define SPI_0_CLKEN()           (periph_clk_en(APB2, RCC_APB2ENR_SPI1EN))
#define SPI_0_CLKDIS()          (periph_clk_dis(APB2, RCC_APB2ENR_SPI1EN))
#define SPI_0_BUS_DIV           1   /* 1 -> SPI runs with half CPU clock, 0 -> quarter CPU clock */
#define SPI_0_IRQ               SPI1_IRQn
#define SPI_0_IRQ_HANDLER       isr_spi1
/* SPI 0 pin configuration */
#define SPI_0_SCK_PORT          GPIOA
#define SPI_0_SCK_PIN           5
#define SPI_0_SCK_AF            5
#define SPI_0_SCK_PORT_CLKEN()  (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOAEN))
#define SPI_0_MISO_PORT         GPIOA
#define SPI_0_MISO_PIN          6
#define SPI_0_MISO_AF           5
#define SPI_0_MISO_PORT_CLKEN() (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOAEN))
#define SPI_0_MOSI_PORT         GPIOA
#define SPI_0_MOSI_PIN          7
#define SPI_0_MOSI_AF           5
#define SPI_0_MOSI_PORT_CLKEN() (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOAEN))

/* SPI 1 device config */
#define SPI_1_DEV               SPI2
#define SPI_1_CLKEN()           (periph_clk_en(APB1, RCC_APB1ENR_SPI2EN))
#define SPI_1_CLKDIS()          (periph_clk_dis(APB1, RCC_APB1ENR_SPI2EN))
#define SPI_1_BUS_DIV           0   /* 1 -> SPI runs with half CPU clock, 0 -> quarter CPU clock */
#define SPI_1_IRQ               SPI2_IRQn
#define SPI_1_IRQ_HANDLER       isr_spi2
/* SPI 1 pin configuration */
#define SPI_1_SCK_PORT          GPIOB
#define SPI_1_SCK_PIN           13
#define SPI_1_SCK_AF            5
#define SPI_1_SCK_PORT_CLKEN()  (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOBEN))
#define SPI_1_MISO_PORT         GPIOB
#define SPI_1_MISO_PIN          14
#define SPI_1_MISO_AF           5
#define SPI_1_MISO_PORT_CLKEN() (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOBEN))
#define SPI_1_MOSI_PORT         GPIOB
#define SPI_1_MOSI_PIN          15
#define SPI_1_MOSI_AF           5
#define SPI_1_MOSI_PORT_CLKEN() (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOBEN))
/** @} */

/**
 * @name I2C configuration
 * @{
 */
#define I2C_NUMOF           (1U)
#define I2C_0_EN            1
#define I2C_IRQ_PRIO        1
#define I2C_APBCLK          (42000000U)

/* I2C 0 device configuration */
#define I2C_0_DEV           I2C1
#define I2C_0_CLKEN()       (periph_clk_en(APB1, RCC_APB1ENR_I2C1EN))
#define I2C_0_CLKDIS()      (periph_clk_dis(APB1, RCC_APB1ENR_I2C1EN))
#define I2C_0_EVT_IRQ       I2C1_EV_IRQn
#define I2C_0_EVT_ISR       isr_i2c1_ev
#define I2C_0_ERR_IRQ       I2C1_ER_IRQn
#define I2C_0_ERR_ISR       isr_i2c1_er
/* I2C 0 pin configuration */
#define I2C_0_SCL_PORT      GPIOB
#define I2C_0_SCL_PIN       6
#define I2C_0_SCL_AF        4
#define I2C_0_SCL_CLKEN()   (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOBEN))
#define I2C_0_SDA_PORT      GPIOB
#define I2C_0_SDA_PIN       7
#define I2C_0_SDA_AF        4
#define I2C_0_SDA_CLKEN()   (periph_clk_en(AHB1, RCC_AHB1ENR_GPIOBEN))
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_CONF_H_ */
/** @} */
