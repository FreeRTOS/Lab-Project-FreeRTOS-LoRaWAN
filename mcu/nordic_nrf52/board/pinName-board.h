#ifndef __PIN_NAME_BOARD_H__
#define __PIN_NAME_BOARD_H__

#include "nrf_gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * NRF52840 Pins. This is not an exhaustive list, just a list of the pins we need for the lora stack
 */
#define MCU_PINS \
    P_0_3  = NRF_GPIO_PIN_MAP(0, 3),  \
    P_0_4  = NRF_GPIO_PIN_MAP(0, 4),  \
    P_0_13 = NRF_GPIO_PIN_MAP(0, 13), \
    P_0_14 = NRF_GPIO_PIN_MAP(0, 14), \
    P_0_16 = NRF_GPIO_PIN_MAP(0, 16), \
    P_0_28 = NRF_GPIO_PIN_MAP(0, 28), \
    P_0_29 = NRF_GPIO_PIN_MAP(0, 29), \
    P_1_4  = NRF_GPIO_PIN_MAP(1, 4),  \
    P_1_6  = NRF_GPIO_PIN_MAP(1, 6),  \
    P_1_8  = NRF_GPIO_PIN_MAP(1, 8),  \
    P_1_10 = NRF_GPIO_PIN_MAP(1, 10), \
    P_1_13 = NRF_GPIO_PIN_MAP(1, 13), \
    P_1_14 = NRF_GPIO_PIN_MAP(1, 14), \
    P_1_15 = NRF_GPIO_PIN_MAP(1, 15)  

#ifdef __cplusplus
}
#endif

#endif // __PIN_NAME_BOARD_H__
