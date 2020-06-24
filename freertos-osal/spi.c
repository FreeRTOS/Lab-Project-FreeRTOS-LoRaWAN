/*
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


#include "FreeRTOS.h"
#include "task.h"
#include "spi.h"
#include "iot_spi.h"

static IotSPIHandle_t SpiHandle[2];

void SpiInit( Spi_t *obj, SpiId_t spiId, PinNames mosi, PinNames miso, PinNames sclk, PinNames nss )
{
    taskENTER_CRITICAL();

    obj->SpiId = spiId;

    SpiHandle[ spiId ] = iot_spi_open( spiId );
    configASSERT( SpiHandle[ spiId ] != NULL )

    if( nss == NC )
    {
        SpiFormat( obj, 8, 0, 0, 0 );
    }
    else
    {
        SpiFormat( obj, 8, 0, 0, 1 );
    }
    SpiFrequency( obj, 10000000 );

    taskEXIT_CRITICAL();
}

void SpiDeInit( Spi_t *obj )
{

    taskENTER_CRITICAL();
    iot_spi_close( SpiHandle[obj->SpiId] );
    SpiHandle[obj->SpiId] = NULL;
    taskEXIT_CRITICAL();
}

void SpiFormat( Spi_t *obj, int8_t bits, int8_t cpol, int8_t cpha, int8_t slave )
{
    IotSPIMasterConfig_t spiConfig;
    int32_t ret;

    /**
     * No support in common IO to configure these parameters.
     */
    ( void ) bits;
    ( void ) slave;

    ret = iot_spi_ioctl( SpiHandle[obj->SpiId], eSPIGetMasterConfig, &spiConfig );

    if( ret == IOT_SPI_SUCCESS )
    {
        if( ( cpol ==  0 ) && ( cpha == 0 ) )
        {
            spiConfig.eMode = eSPIMode0;
        }
        else if( ( cpol ==  0 ) && ( cpha == 1 ) )
        {
            spiConfig.eMode = eSPIMode1;

        }
        else if( ( cpol ==  1 ) && ( cpha == 0 ) )
        {
            spiConfig.eMode = eSPIMode2;
        }
        else
        {
            spiConfig.eMode = eSPIMode3;
        }

        spiConfig.eSetBitOrder = eSPIMSBFirst;

        iot_spi_ioctl( SpiHandle[obj->SpiId], eSPISetMasterConfig, &spiConfig );
    }
}

void SpiFrequency( Spi_t *obj, uint32_t hz )
{
    IotSPIMasterConfig_t spiConfig;
    int32_t ret;

    ret = iot_spi_ioctl( SpiHandle[obj->SpiId], eSPIGetMasterConfig, &spiConfig );
    if( ret == IOT_SPI_SUCCESS )
    {
        spiConfig.ulFreq = hz;
        iot_spi_ioctl( SpiHandle[obj->SpiId], eSPISetMasterConfig, &spiConfig );
    }
}

uint16_t SpiInOut( Spi_t *obj, uint16_t outData )
{
    uint8_t rxData = 0;
    BaseType_t interruptStatus = 0;

    configASSERT( ( obj != NULL ) && ( SpiHandle[obj->SpiId ] != NULL ) );

    /**
     * TODO: Remove mutual exclusion code and handle it in caller if needed.
     */

    if( xPortIsInsideInterrupt() == pdTRUE )
    {
        interruptStatus = taskENTER_CRITICAL_FROM_ISR();
    }
    else
    {
        taskENTER_CRITICAL( );
    }

    while( iot_spi_transfer_sync( SpiHandle[ obj->SpiId ], ( uint8_t * ) &outData, &rxData, 1 ) == IOT_SPI_BUS_BUSY );

    if( xPortIsInsideInterrupt() == pdTRUE )
    {
        taskEXIT_CRITICAL_FROM_ISR( interruptStatus );
    }
    else
    {
        taskEXIT_CRITICAL( );
    }

    return( rxData );
}

