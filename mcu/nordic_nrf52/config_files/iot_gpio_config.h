/*
 * FreeRTOS
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @file   iot_gpio_config.h
 * @brief  Additional settings for GPIO, such as CommonIO-to-Board pin mapping.
 */

#ifndef _AWS_COMMON_IO_GPIO_CONFIG_H_
#define _AWS_COMMON_IO_GPIO_CONFIG_H_

#include "nrf_gpio.h"
#include "board-config.h"

#define IOT_GPIO_LOGGING_ENABLED             1
#define IOT_COMMON_IO_GPIO_NUMBER_OF_PINS    14

#define IOT_COMMON_IO_GPIO_PIN_MAP                                \
        {                                                         \
            NRF_GPIO_PIN_MAP(0, 3),  /* RADIO_RESET */            \
            NRF_GPIO_PIN_MAP(1, 13), /* RADIO_MOSI */             \
            NRF_GPIO_PIN_MAP(1, 14), /* RADIO_MISO */             \
            NRF_GPIO_PIN_MAP(1, 15), /* RADIO_SCLK */             \
            NRF_GPIO_PIN_MAP(1, 8),  /* RADIO_NSS */              \
            NRF_GPIO_PIN_MAP(1, 4),  /* RADIO_BUSY */             \
            NRF_GPIO_PIN_MAP(1, 6),  /* RADIO_DIO_1 */            \
            NRF_GPIO_PIN_MAP(1, 10), /* RADIO_ANT_SWITCH_POWER */ \
            NRF_GPIO_PIN_MAP(0, 4),  /* RADIO_FREQ_SEL */         \
            NRF_GPIO_PIN_MAP(0, 29), /* RADIO_XTAL_SEL */         \
            NRF_GPIO_PIN_MAP(0, 28), /* RADIO_DEVICE_SEL */       \
            NRF_GPIO_PIN_MAP(0, 13), /* LED_APP_TOGGLE */         \
            NRF_GPIO_PIN_MAP(0, 14), /* LED_APP_DEMO */           \
            NRF_GPIO_PIN_MAP(0, 16)  /* LED_RX_TOGGLE */          \
        }

/* Set defaults which are not overridden */
#include "iot_gpio_config_defaults.h"

#endif /* ifndef _AWS_COMMON_IO_GPIO_CONFIG_H_ */
