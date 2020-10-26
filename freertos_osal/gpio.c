/*!
 * \file      gpio.c
 *
 * \brief     GPIO driver implementation
 *
 * \remark: Relies on the specific board GPIO implementation as well as on
 *          IO expander driver implementation if one is available on the target
 *          board.
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2013. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of the Semtech corporation nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "FreeRTOS.h"
#include "gpio.h"
#include "iot_gpio.h"

static Gpio_t * GpioIrq[ 16 ];

/* Callbback installed for all CommonIO GPIO, that maps args to form call to LoraMac GPIO callback */
static void prvMappingCallback( uint8_t ucPinState,
                                void * pvUserContext )
{
    /* User Context is installed to the the LoraMac Gpio_t */
    Gpio_t * xGpio_LM = ( Gpio_t * ) pvUserContext;

    xGpio_LM->IrqHandler( NULL );
}

void GpioInit( Gpio_t * obj,
               PinNames pin,
               PinModes mode,
               PinConfigs config,
               PinTypes type,
               uint32_t value )
{
    configASSERT( obj );
    obj->pinIndex = pin;

    /* LoraMac Gpio_t context will be used to track CommonIO Handle, to avoid having to change LM Gpio_t */
    obj->Context = iot_gpio_open( pin );
    configASSERT( obj->Context != NULL );

    IotGpioHandle_t xGpio = ( IotGpioHandle_t ) obj->Context;

    int32_t xReturnCode;

    if( mode == PIN_INPUT )
    {
        IotGpioDirection_t xDirection = eGpioDirectionInput;
        IotGpioPull_t xPull = eGpioPullNone;

        if( type == PIN_NO_PULL )
        {
            xPull = eGpioPullNone;
        }
        else if( type == PIN_PULL_UP )
        {
            xPull = eGpioPullUp;
        }
        else if( type == PIN_PULL_DOWN )
        {
            xPull = eGpioPullDown;
        }
        else
        {
            configASSERT( 0 ); /* Invalid pin pull/push configuration */
        }

        xReturnCode = iot_gpio_ioctl( xGpio, eSetGpioDirection, &xDirection );
        configASSERT( xReturnCode == IOT_GPIO_SUCCESS );
        xReturnCode = iot_gpio_ioctl( xGpio, eSetGpioPull, &xPull );
        configASSERT( xReturnCode == IOT_GPIO_SUCCESS );
    }
    else if( mode == PIN_OUTPUT )
    {
        IotGpioDirection_t xDirection = eGpioDirectionOutput;
        xReturnCode = iot_gpio_ioctl( xGpio, eSetGpioDirection, &xDirection );
        configASSERT( xReturnCode == IOT_GPIO_SUCCESS );
        iot_gpio_write_sync( xGpio, value );
    }
    else
    {
        configASSERT( 0 );
    }
}

void GpioSetContext( Gpio_t * obj,
                     void * context )
{
    /*TODO: Implement this. */
}

void GpioSetInterrupt( Gpio_t * obj,
                       IrqModes irqMode,
                       IrqPriorities irqPriority,
                       GpioIrqHandler * irqHandler )
{
    /* For the nrf52 port, only DIO1 will be used to route IRQ from radio with rising edge trigger */
    configASSERT( irqHandler && obj && obj->Context && obj->pinIndex == RADIO_DIO_1 && irqMode == IRQ_RISING_EDGE );
    IotGpioHandle_t xGpio = ( IotGpioHandle_t ) obj->Context;

    /* CommonIO GPIO and lora mac GPIO callbacks have different args and get mapped via*/
    obj->IrqHandler = irqHandler;
    iot_gpio_set_callback( xGpio, prvMappingCallback, obj );

    IotGpioPull_t xPull = eGpioPullDown;
    int32_t xReturnCode = iot_gpio_ioctl( xGpio, eSetGpioPull, &xPull );
    configASSERT( xReturnCode == IOT_GPIO_SUCCESS );

    IotGpioInterrupt_t xInterruptType = eGpioInterruptRising;
    xReturnCode = iot_gpio_ioctl( xGpio, eSetGpioInterrupt, &xInterruptType );
    configASSERT( xReturnCode == IOT_GPIO_SUCCESS );

    /* Cater to this stack's metadata */
    GpioIrq[ ( obj->pin ) & 0x0F ] = obj;
}

void GpioRemoveInterrupt( Gpio_t * obj )
{
    /*TODO: Implement this. */
}

void GpioWrite( Gpio_t * obj,
                uint32_t value )
{
    configASSERT( obj && obj->Context );
    IotGpioHandle_t xGpio = ( IotGpioHandle_t ) obj->Context;

    int32_t xReturnCode = iot_gpio_write_sync( xGpio, value );
    configASSERT( xReturnCode == IOT_GPIO_SUCCESS );
}

void GpioToggle( Gpio_t * obj )
{
    /*TODO: Implement this. */
}

uint32_t GpioRead( Gpio_t * obj )
{
    configASSERT( obj && obj->Context );
    IotGpioHandle_t xGpio = ( IotGpioHandle_t ) obj->Context;
    uint8_t ucPinValue = 0xFF;

    int32_t xReturnCode = iot_gpio_read_sync( xGpio, &ucPinValue );
    configASSERT( xReturnCode == IOT_GPIO_SUCCESS );

    return ucPinValue;
}
