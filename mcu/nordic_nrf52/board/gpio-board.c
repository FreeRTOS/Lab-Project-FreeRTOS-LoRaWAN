/*!
 * \file      gpio-board.c
 *
 * \brief     Target board GPIO driver implementation
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
#include "board-config.h"
#include "rtc-board.h"
#include "gpio-board.h"
#include "FreeRTOS.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"



static Gpio_t *GpioIrq[16];

/* This API has been reduced to only support radio interface pins on nrf52 
   and does NOT serve as generalized API for all onboard pins */
void CheckNrfPin(uint32_t pin)
{
    configASSERT(pin == RADIO_RESET ||
           pin == RADIO_MOSI || 
           pin == RADIO_MISO || 
           pin == RADIO_SCLK ||
           pin == RADIO_NSS ||
           pin == RADIO_BUSY ||
           pin == RADIO_DIO_1 ||
           pin == RADIO_ANT_SWITCH_POWER ||
           pin == RADIO_FREQ_SEL ||
           pin == RADIO_XTAL_SEL || 
           pin == RADIO_DEVICE_SEL);
}
      

void GpioMcuInit( Gpio_t *obj, PinNames pin, PinModes mode, PinConfigs config, PinTypes type, uint32_t value )
{
    /* nrf52 port only uses this for lora radio driver */
    configASSERT(obj);
    CheckNrfPin(pin);
    obj->pinIndex = pin;

    if( mode == PIN_INPUT )
    {
        if ( type == PIN_NO_PULL ) 
        {
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
        }
        else if ( type == PIN_PULL_UP )
        {
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
        } 
        else if ( type == PIN_PULL_DOWN ) 
        {
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
        }
        else 
        {
            configASSERT(0); // Invalid pin pull/push configuration
        }
    } 
    else if( mode == PIN_OUTPUT )
    {   
        nrf_gpio_cfg_output(pin); 
        GpioMcuWrite( obj, value );
    } 
    #ifdef USE_ANALOGIC
    else if( mode == PIN_ANALOGIC )
    {
        /* The reset on the chip is active low. Higher level APIs pull it down, but there after we must escape the reset state by holding
           the chip's reset HIGH. They had a quirky way of assigning a pull-up resistor....which for consistency is replicated for nrf52 */
        if (pin == RADIO_RESET) 
        {
            //nrf_gpio_cfg_input(pinIndex, NRF_GPIO_PIN_PULLUP);
            GpioMcuInit(obj, pin, PIN_OUTPUT, config, type, 1);
        }
        else
        {
            configASSERT(0);
        }
    }
    #endif
    else
    {
        configASSERT(0);
    }
}

/*
void GpioMcuSetContext( Gpio_t *obj, void* context )
{
    obj->Context = context;
}
*/

// Ex: .//src/boards/NucleoL152/sx1262mbxcas-board.c:    GpioSetInterrupt( &SX126x.DIO1, IRQ_RISING_EDGE, IRQ_HIGH_PRIORITY, dioIrq );
void GpioMcuSetInterrupt( Gpio_t *obj, IrqModes irqMode, IrqPriorities irqPriority, GpioIrqHandler *irqHandler )
{
    // For the nrf52 port, only DIO1 will be used to route IRQ from radio with rising edge trigger
    configASSERT(irqHandler && obj && obj->pinIndex == RADIO_DIO_1 && irqMode == IRQ_RISING_EDGE);
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    in_config.pull = NRF_GPIO_PIN_PULLDOWN;
    configASSERT(NRF_SUCCESS == nrf_drv_gpiote_in_init(obj->pinIndex, &in_config, irqHandler));
    
    // Enable event generation from the pin 
    nrf_drv_gpiote_in_event_enable(obj->pinIndex, true);

    // Cater to this stack's metadata
    obj->IrqHandler = irqHandler;
    GpioIrq[( obj->pin ) & 0x0F] = obj;
}

/*

void GpioMcuRemoveInterrupt( Gpio_t *obj )
{
    if( obj->pin < IOE_0 )
    {
        // Clear callback before changing pin mode
        GpioIrq[( obj->pin ) & 0x0F] = NULL;

        GPIO_InitTypeDef   GPIO_InitStructure;

        GPIO_InitStructure.Pin =  obj->pinIndex ;
        GPIO_InitStructure.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init( obj->port, &GPIO_InitStructure );
    }
    else
    {
#if defined( BOARD_IOE_EXT )
        // IOExt Pin
        GpioIoeRemoveInterrupt( obj );
#endif
    }
}
*/

void GpioMcuWrite( Gpio_t *obj, uint32_t value )
{
    configASSERT(obj);
    CheckNrfPin(obj->pinIndex);

    nrf_gpio_pin_write(obj->pinIndex, value);
}

/*
void GpioMcuToggle( Gpio_t *obj )
{
    if( obj->pin < IOE_0 )
    {
        if( obj == NULL )
        {
            assert_param( FAIL );
        }

        // Check if pin is not connected
        if( obj->pin == NC )
        {
            return;
        }
        HAL_GPIO_TogglePin( obj->port, obj->pinIndex );
    }
    else
    {
#if defined( BOARD_IOE_EXT )
        // IOExt Pin
        GpioIoeToggle( obj );
#endif
    }
}
*/

uint32_t GpioMcuRead( Gpio_t *obj )
{
    configASSERT(obj);
    CheckNrfPin(obj->pinIndex);
    
    return nrf_gpio_pin_read(obj->pinIndex);
}

/*
void EXTI0_1_IRQHandler( void )
{
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_0 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_1 );
}

void EXTI2_3_IRQHandler( void )
{
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_2 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_3 );
}

void EXTI4_15_IRQHandler( void )
{

    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_4 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_5 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_6 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_7 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_8 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_9 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_10 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_11 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_12 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_13 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_14 );
    HAL_GPIO_EXTI_IRQHandler( GPIO_PIN_15 );
}

void HAL_GPIO_EXTI_Callback( uint16_t gpioPin )
{
    uint8_t callbackIndex = 0;

    if( gpioPin > 0 )
    {
        while( gpioPin != 0x01 )
        {
            gpioPin = gpioPin >> 1;
            callbackIndex++;
        }
    }

    if( ( GpioIrq[callbackIndex] != NULL ) && ( GpioIrq[callbackIndex]->IrqHandler != NULL ) )
    {
        GpioIrq[callbackIndex]->IrqHandler( GpioIrq[callbackIndex]->Context );
    }
}
*/