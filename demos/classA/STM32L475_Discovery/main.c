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
 * 1 tab == 4 spaces!
 */


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "board_init.h"

/**
 * @brief Stack size for LoRaWAN Class A task.
 */
#define LORAWAN_CLASSA_TASK_STACK_SIZE    ( 2048 )


/**
 * @brief Prirority for LoRaWAN Class A task.
 * Priority is set to lowest task priority which is above the idle task priority.
 */
#define LORAWAN_CLASSA_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )

void vLorawanClassATask( void * params );



/*******************************************************************************************
* Main
* *****************************************************************************************/
int main( int argc,
          char ** argv )
{
    /* Perform any hardware initialization that can or must be done before RTOS is running */
    board_init();

    /* Add user tasks */
    xTaskCreate( vLorawanClassATask, "LoRaWanClassA", LORAWAN_CLASSA_TASK_STACK_SIZE, NULL, LORAWAN_CLASSA_TASK_PRIORITY, NULL );

    vTaskStartScheduler();

    return 0;
}
