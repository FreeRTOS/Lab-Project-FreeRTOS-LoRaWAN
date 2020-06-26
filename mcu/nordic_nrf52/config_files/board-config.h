/*!
 * \file      board-config.h
 *
 * \brief     Board configuration
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 *               ___ _____ _   ___ _  _____ ___  ___  ___ ___
 *              / __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
 *              \__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
 *              |___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
 *              embedded.connectivity.solutions===============
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Daniel Jaeckle ( STACKFORCE )
 *
 * \author    Johannes Bruder ( STACKFORCE )
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "nrf_gpio.h"
#include "nrf_drv_spi.h"

#ifdef __cplusplus
extern "C"
{
#endif



#define BOARD_TCXO_WAKEUP_TIME                      0

/*!
 * Board MCU pins definitions
 */

 /* Interfaced pins */
#define RADIO_RESET                                 P_0_3  // Out: Active LOW shield reset
#define RADIO_MOSI                                  P_1_13 // Out: SPI Slave input
#define RADIO_MISO                                  P_1_14 // In : SPI Slave output
#define RADIO_SCLK                                  P_1_15 // Out: SPI Slave clock
#define RADIO_NSS                                   P_1_8  // Out: SPI Slave select
#define RADIO_BUSY                                  P_1_4  // In :
#define RADIO_DIO_1                                 P_1_6  // In : Setup to be the IRQ line. Needs GPIOTE

/* Probe pins for different stages of chip setup. See PCB Circuit. Read ONLY */
#define RADIO_ANT_SWITCH_POWER                      P_1_10  // In
#define RADIO_FREQ_SEL                              P_0_4   // In
#define RADIO_XTAL_SEL                              P_0_29  // In
#define RADIO_DEVICE_SEL                            P_0_28  // In

/* Does not influence radio chip */
#define LED_APP_TOGGLE                              P_0_13 // Out: nrf52 onboard LED2. Used for status/sanity check. Does not affect chip
#define LED_APP_DEMO                                P_0_14
#define LED_RX_TOGGLE                               P_0_16


#define LORA_MAC_SPI_FREQUENCY                     NRF_DRV_SPI_FREQ_4M
#ifndef LORA_MAC_SPI_FREQUENCY
    #define LORA_MAC_SPI_FREQUENCY                 10000000
#endif

#ifdef __cplusplus
}
#endif

#endif // __BOARD_CONFIG_H__
