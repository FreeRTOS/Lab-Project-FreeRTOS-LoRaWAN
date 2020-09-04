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
#define MCU_PINS            \
    RADIO_RESET,            \
    RADIO_MOSI,             \
    RADIO_MISO,             \
    RADIO_SCLK,             \
    RADIO_NSS,              \
    RADIO_BUSY,             \
    RADIO_DIO_1,            \
    RADIO_ANT_SWITCH_POWER, \
    RADIO_FREQ_SEL,         \
    RADIO_XTAL_SEL,         \
    RADIO_DEVICE_SEL,       \
    LED_APP_TOGGLE,         \
    LED_APP_DEMO,           \
    LED_RX_TOGGLE

#ifdef __cplusplus
}
#endif

#endif // __PIN_NAME_BOARD_H__
