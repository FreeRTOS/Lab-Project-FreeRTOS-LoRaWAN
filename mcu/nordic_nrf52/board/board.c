/*!
 * \file      board.c
 *
 * \brief     Target board general functions implementation
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
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */

#include "utilities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "se-identity.h"
#include "spi.h"
#include "sx126x-board.h"
#include "board-config.h"

#include "nrf_drv_gpiote.h"

//#include "rtc-board.h"

void BoardCriticalSectionBegin( uint32_t *mask )
{
    (void)mask;
    taskENTER_CRITICAL();
}

void BoardCriticalSectionEnd( uint32_t *mask )
{
    (void)mask;
    taskEXIT_CRITICAL();
}

// Strictly for demo purposes, just hardcode some ID to make rest of the stack happy
void BoardGetUniqueId( uint8_t *id )
{
    uint8_t eui[8] = LORAWAN_DEVICE_EUI;
    memcpy(id, eui, 8);
}

void BoardInitMcu( void )
{
    
    // Radio's DIO1 will route irq line through gpio, hence gpiote
    configASSERT(NRF_SUCCESS == nrf_drv_gpiote_init());
    SpiInit(&SX126x.Spi, SPI_1, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
    SX126xIoInit();
    //RtcInit();
    //EepromMcuInit();



}
