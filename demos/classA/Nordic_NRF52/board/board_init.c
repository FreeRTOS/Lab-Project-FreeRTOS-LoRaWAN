
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* AWS library includes. */
#include "iot_logging_task.h"

/* Nordic BSP includes */
#include "bsp.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"
#include "sensorsim.h"
#include "timers.h"
#include "app_timer.h"
#include "ble_conn_state.h"
#include "nrf_drv_clock.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble_conn_params.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "bsp_btn_ble.h"
#include "app_uart.h"
#include "queue.h"
#include "nrf_drv_gpiote.h"

#include "SEGGER_RTT.h"

#if defined( UART_PRESENT )
    #include "nrf_uart.h"
#endif
#if defined( UARTE_PRESENT )
    #include "nrf_uarte.h"
#endif

/*-----------------------------------------------------------*/

/* Logging Task Defines. */
#define mainLOGGING_MESSAGE_QUEUE_LENGTH    ( 15 )
#define mainLOGGING_TASK_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 4 )

/* BLE Lib defines. */
#define mainBLE_SERVER_UUID                 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

/* UART Buffer sizes. */
#define UART_TX_BUF_SIZE                    ( 256 )                                 /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                    ( 256 )                                 /**< UART RX buffer size. */

/* LED-associated defines */
#define LED_ONE                             BSP_LED_0_MASK
#define LED_TWO                             BSP_LED_1_MASK
#define LED_THREE                           BSP_LED_2_MASK
#define LED_FOUR                            BSP_LED_3_MASK
#define ALL_APP_LED                     \
    ( BSP_LED_0_MASK | BSP_LED_1_MASK | \
      BSP_LED_2_MASK | BSP_LED_3_MASK )                                        /**< Define used for simultaneous operation of all application LEDs. */
#define LED_BLINK_INTERVAL_MS               ( 300 )                            /**< LED blinking interval. */

/*-----------------------------------------------------------*/

SemaphoreHandle_t xUARTTxComplete;
QueueHandle_t UARTqueue = NULL;

/*-----------------------------------------------------------*/
typedef struct{
	uint8_t * pcData;
    size_t xDataSize;
}INPUTMessage_t;


/**@brief   Function for handling uart events.
 *
 * /**@snippet [Handling the data received over UART] */

void prvUartEventHandler( app_uart_evt_t * pxEvent )
{
    /* Declared as static so it can be pushed into the queue from the ISR. */
    static volatile uint8_t ucRxByte = 0;
    INPUTMessage_t xInputMessage;
   BaseType_t xHigherPriorityTaskWoken;
    switch( pxEvent->evt_type )
    {
        case APP_UART_DATA_READY:
            app_uart_get( (uint8_t *)&ucRxByte );
            app_uart_put( ucRxByte );

            xInputMessage.pcData = (uint8_t *)&ucRxByte;
            xInputMessage.xDataSize = 1;

            xQueueSendFromISR(UARTqueue, (void * )&xInputMessage, &xHigherPriorityTaskWoken);
            /* Now the buffer is empty we can switch context if necessary. */
            //portYIELD_FROM_ISR (xHigherPriorityTaskWoken);

            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER( pxEvent->data.error_communication );
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER( pxEvent->data.error_code );
            break;

        case APP_UART_TX_EMPTY:
             xSemaphoreGiveFromISR(xUARTTxComplete, &xHigherPriorityTaskWoken);
            break;

        default:
            break;
    }
}

/**@brief  Function for initializing the UART module.
 */
static void prvUartInit( void )
{
    uint32_t xErrCode;

    app_uart_comm_params_t const xConnParams =
    {
        .rx_pin_no        = RX_PIN_NUMBER,
        .tx_pin_no        = TX_PIN_NUMBER,
        .rts_pin_no       = RTS_PIN_NUMBER,
        .cts_pin_no       = CTS_PIN_NUMBER,
        .flow_control     = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity       = false,
        #if defined( UART_PRESENT )
            .baud_rate    = NRF_UART_BAUDRATE_115200
        #else
               .baud_rate = NRF_UARTE_BAUDRATE_115200
                            #endif
    };

    APP_UART_FIFO_INIT( &xConnParams,
                        UART_RX_BUF_SIZE,
                        UART_TX_BUF_SIZE,
                        prvUartEventHandler,
                        _PRIO_APP_HIGH,
                        xErrCode );
    APP_ERROR_CHECK( xErrCode );
}


/*-----------------------------------------------------------*/

void vUartWrite( uint8_t * pucData )
{
    uint32_t xErrCode;

    SEGGER_RTT_WriteString(0, pucData);
    for( uint32_t i = 0; i < configLOGGING_MAX_MESSAGE_LENGTH; i++ )
    {
        if(pucData[ i ] == 0)
        {
            break;
        }

        do
        {
            xErrCode = app_uart_put(pucData[ i ]);
            if(xErrCode == NRF_ERROR_NO_MEM)
            {
            xErrCode = 0;
            }
        } while( xErrCode == NRF_ERROR_BUSY );
        xSemaphoreTake(xUARTTxComplete, portMAX_DELAY );

    }
}

/**@brief Function for initializing the clock.
 */
static void prvClockInit( void )
{
    ret_code_t xErrCode = nrf_drv_clock_init();

    APP_ERROR_CHECK( xErrCode );
}

/*-----------------------------------------------------------*/

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void prvTimersInit( void )
{
    /* Initialize timer module. */
    ret_code_t xErrCode = app_timer_init();

    APP_ERROR_CHECK( xErrCode );
}


void board_init( void )
{
    /* Initialize modules.*/
    xUARTTxComplete = xSemaphoreCreateBinary();
    prvUartInit();
    prvClockInit();

    // Radio's DIO1 will route irq line through gpio, hence gpiote
    configASSERT(NRF_SUCCESS == nrf_drv_gpiote_init());

    /* Activate deep sleep mode. */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    prvTimersInit();
    UARTqueue = xQueueCreate( 1, sizeof( INPUTMessage_t ) );

    xLoggingTaskInitialize( mainLOGGING_TASK_STACK_SIZE,
                            tskIDLE_PRIORITY,
                            mainLOGGING_MESSAGE_QUEUE_LENGTH );
}



/**
 * @brief Warn user if pvPortMalloc fails.
 *
 * Called if a call to pvPortMalloc() fails because there is insufficient
 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
 * internally by FreeRTOS API functions that create tasks, queues, software
 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h.
 *
 */
void vApplicationMallocFailedHook()
{
    configPRINT_STRING( ( "ERROR: Malloc failed to allocate memory\r\n" ) );
    taskDISABLE_INTERRUPTS();

    /* Loop forever */
    for( ; ; )
    {
    }
}


/**
 * @brief Loop forever if stack overflow is detected.
 *
 * If configCHECK_FOR_STACK_OVERFLOW is set to 1,
 * this hook provides a location for applications to
 * define a response to a stack overflow.
 *
 * Use this hook to help identify that a stack overflow
 * has occurred.
 *
 */
void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    char * pcTaskName )
{
    configPRINT_STRING( ( "ERROR: stack overflow\r\n" ) );
    portDISABLE_INTERRUPTS();

    /* Unused Parameters */
    ( void ) xTask;
    ( void ) pcTaskName;

    /* Loop forever */
    for( ; ; )
    {
    }
}


/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize )
{
    /* If the buffers to be provided to the Idle task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
 * @brief This is to provide the memory that is used by the RTOS daemon/time task.
 *
 * If configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetTimerTaskMemory() to provide the memory that is
 * used by the RTOS daemon/time task.
 */
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize )
{
    /* If the buffers to be provided to the Timer task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

/*
 * Extend the port so we can detect, from code, if inside an intterupt. 
 * nrf52840-dk hosts an arm cortex m4, so reusing some official freertos portable code
 */

BaseType_t xPortIsInsideInterrupt( void )
{
uint32_t ulCurrentInterrupt;
BaseType_t xReturn;

	/* Obtain the number of the currently executing interrupt. */
	__asm volatile( "mrs %0, ipsr" : "=r"( ulCurrentInterrupt ) :: "memory" );

	if( ulCurrentInterrupt == 0 )
	{
		xReturn = pdFALSE;
	}
	else
	{
		xReturn = pdTRUE;
	}

	return xReturn;
}


/**
 * @brief User defined Idle task function.
 *
 * @note Do not make any blocking operations in this function.
 */
void vApplicationIdleHook( void )
{
    /* FIX ME. If necessary, update to application idle periodic actions. */

    static TickType_t xLastPrint = 0;
    TickType_t xTimeNow;
    const TickType_t xPrintFrequency = pdMS_TO_TICKS( 5000 );

    xTimeNow = xTaskGetTickCount();

    if( ( xTimeNow - xLastPrint ) > xPrintFrequency )
    {
        configPRINT( "." );
        xLastPrint = xTimeNow;
    }
}

void vApplicationDaemonTaskStartupHook() 
{
}
