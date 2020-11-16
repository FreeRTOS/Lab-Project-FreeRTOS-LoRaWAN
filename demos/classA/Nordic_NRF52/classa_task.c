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


#include "LoRaWAN.h"
#include "utilities.h"


/**
 * @brief Default region is set to US915. Application can choose to configure a different region
 * by setting the appropirate compiler flag for the region and setting this config to the corresponding
 * region.
 */
#define LORAWAN_REGION    LORAMAC_REGION_US915


/**
 * @brief LoRa MAC layer port used by the application.
 * Downlink unicast messages should be send to this port number.
 */
#define LORAWAN_APP_PORT                       ( 2 )

/**
 * @brief Should send confirmed messages (with an acknowledgment) or not.
 */
#define LORAWAN_CONFIRMED_SEND                 ( 0 )

/**
 * @brief Defines the application data transmission duty cycle time in milliseconds.
 *
 * Although there is a duty cycle restriction which allows to transmit for only a portion of time on a channel,
 * there are also additional policies enforced by networks to prevent interference and quality degradation for transmission.
 *
 * For example TTN enforces uplink of 36sec airtime/day and 10 downlink messages/day. Demo sends 1 byte of unconfirmed uplink,
 * using a default data rate of DR_0 (SF = 10, BW=125Khz) for US915, so airtime for each uplink ~289ms, hence setting the interval
 * to 700 seconds to achieve maximum air time of 36sec per day.
 *
 */
#define LORAWAN_APPLICATION_TX_INTERVAL_SEC    ( 700U )

/**
 * @brief Defines a random jitter bound in milliseconds for application data transmission duty cycle.
 *
 * This allows devices to space their transmissions slighltly between each other in cases like all devices reboots and tries to
 * join server at same time.
 */
#define LORAWAN_APPLICATION_JITTER_MS          ( 500 )


/**
 * @brief Maximum time to wait to receive a downlink packet or event after sending an uplink packet.
 *
 * As per LoRaWAN spec, class A end device uses two receive windows slots after sending an uplink packet. For US915the max window duration is
 * 3000 ms and the second RX window max delay is 2 seconds. So setting the receive timeout to higher than the receive window slots.
 */
#define CLASSA_RECEIVE_WINDOW_DURATION_MS    ( 6000 )


/*!
 * Prints the provided buffer in HEX
 *
 * \param buffer Buffer to be printed
 * \param size   Buffer size to be printed
 */
static void prvPrintHexBuffer( uint8_t * buffer,
                               uint8_t size )
{
    uint8_t newline = 0;

    for( uint8_t i = 0; i < size; i++ )
    {
        if( newline != 0 )
        {
            configPRINTF( ( "\n" ) );
            newline = 0;
        }

        configPRINTF( ( "%02X\n", buffer[ i ] ) );

        if( ( ( i + 1 ) % 16 ) == 0 )
        {
            newline = 1;
        }
    }

    configPRINTF( ( "\n" ) );
}

static LoRaMacStatus_t prvFetchDownlinkPacket( void )
{
    LoRaMacStatus_t status;
    LoRaWANMessage_t uplink = { 0 };
    LoRaWANMessage_t downlink = { 0 };

    /* Send an empty uplink message in confirmed mode. */
    uplink.length = 0;
    uplink.port = LORAWAN_APP_PORT;

    status = LoRaWAN_Send( &uplink, true );

    if( status == LORAMAC_STATUS_OK )
    {
        configPRINTF( ( "Successfully sent an uplink packet, confirmed = true.\r\n" ) );

        if( LoRaWAN_Receive( &downlink, CLASSA_RECEIVE_WINDOW_DURATION_MS ) == pdTRUE )
        {
            configPRINTF( ( "Received downlink data on port %d:\r\n", downlink.port ) );
            prvPrintHexBuffer( downlink.data, downlink.length );
        }
    }

    return status;
}


void vLorawanClassATask( void * params )
{
    LoRaMacStatus_t status;
    uint32_t ulDutyCycleWaitTimeMs;
    uint32_t ulTxIntervalMs;
    LoRaWANMessage_t uplink;
    LoRaWANMessage_t downlink;
    LoRaWANEventInfo_t event;


    configPRINTF( ( "###### ===== Class A LoRaWAN application ==== ######\n\n" ) );

    status = LoRaWAN_Init( LORAWAN_REGION );

    if( status != LORAMAC_STATUS_OK )
    {
        configPRINTF( ( "Failed to initialize lorawan error = %d\r\n", status ) );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        configPRINTF( ( "Initiating OTAA join procedure.\r\n" ) );

        status = LoRaWAN_Join();
    }

    if( status != LORAMAC_STATUS_OK )
    {
        configPRINTF( ( "Failed to join to a lorawan network, error = %d\r\n", status ) );
    }
    else
    {
        /*
         * Adaptive data rate is set to ON by default but this can be changed runtime if needed
         * for mobiled devices with no fixed locations.
         */

        LoRaWAN_SetAdaptiveDataRate( true );

        /**
         * Successfully joined a LoRaWAN network. Now the  task runs in an infinite loop,
         * sends periodic uplink message of 1 byte by obeying fair access policy for the LoRaWAN network.
         * If the MAC has indicated to schedule an uplink message as soon as possible, then it sends
         * an uplink message immediately after the duty cycle wait time. After each uplink it also waits
         * on downlink queue for any messages from the network server.
         */

        configPRINTF( ( "Successfully joined a LoRaWAN network. Sending data in loop.\r\n" ) );

        uplink.port = LORAWAN_APP_PORT;
        uplink.length = 1;
        uplink.data[ 0 ] = 0xFF;
        uplink.dataRate = 0;

        for( ; ; )
        {
            status = LoRaWAN_Send( &uplink, LORAWAN_CONFIRMED_SEND );

            if( status == LORAMAC_STATUS_OK )
            {
                configPRINTF( ( "Successfully sent an uplink packet, confirmed = %d \r\n", LORAWAN_CONFIRMED_SEND ) );


                configPRINTF( ( "Waiting for downlink data.\r\n" ) );

                if( LoRaWAN_Receive( &downlink, CLASSA_RECEIVE_WINDOW_DURATION_MS ) == pdTRUE )
                {
                    configPRINTF( ( "Received downlink data on port %d:\r\n", downlink.port ) );
                    prvPrintHexBuffer( downlink.data, downlink.length );
                }
                else
                {
                    configPRINTF( ( "No downlink data.\r\n" ) );
                }

                /**
                 * Poll for events from LoRa network server.
                 */
                for( ; ; )
                {
                    if( LoRaWAN_PollEvent( &event, 0 ) == pdTRUE )
                    {
                        switch( event.type )
                        {
                            case LORAWAN_EVENT_DOWNLINK_PENDING:

                                /**
                                 * MAC layer indicated there are pending acknowledgments to be sent
                                 * uplink as soon as possible. Wait for duty cycle time and send an uplink.
                                 */
                                configPRINTF( ( "Received a downlink pending event. Send an empty uplink to fetch downlink packets.\r\n" ) );
                                status = prvFetchDownlinkPacket();
                                break;

                            case LORAWAN_EVENT_TOO_MANY_FRAME_LOSS:

                                /**
                                 *  If LoRaMAC stack reports a too many frame loss event, it indicates that gateway and device frame counter
                                 *  values are not in sync. The only way to recover from this is to initiate a rejoin procedure to reset
                                 *  the frame counter at both sides.
                                 */
                                configPRINTF( ( "Too many frame loss detected. Rejoining to LoRaWAN network.\r\n" ) );
                                status = LoRaWAN_Join();

                                if( status != LORAMAC_STATUS_OK )
                                {
                                    configPRINTF( ( "Cannot rejoin to the LoRAWAN network.\r\n" ) );
                                }

                                break;

                            case LORAWAN_EVENT_DEVICE_TIME_UPDATED:
                                configPRINTF( ( "Device time synchronized. \r\n" ) );
                                break;


                            default:
                                configPRINTF( ( "Unhandled event type %d received.\r\n", event.type ) );
                                break;
                        }
                    }
                    else
                    {
                        configPRINTF( ( "No more downlink events.\r\n" ) );
                        break;
                    }
                }

                if( status == LORAMAC_STATUS_OK )
                {
                    /**
                     * Frame was sent successfully. Wait for next TX schedule to send uplink thereby obeying fair
                     * access policy.
                     */

                    ulTxIntervalMs = ( LORAWAN_APPLICATION_TX_INTERVAL_SEC * 1000 ) + randr( -LORAWAN_APPLICATION_JITTER_MS, LORAWAN_APPLICATION_JITTER_MS );

                    configPRINTF( ( "TX-RX cycle complete. Waiting for %u seconds, before starting next cycle.\r\n", ( ulTxIntervalMs / 1000 ) ) );

                    vTaskDelay( pdMS_TO_TICKS( ulTxIntervalMs ) );
                }
                else
                {
                    configPRINTF( ( "Failed to recover from an error. Exiting the demo.\r\n" ) );
                    break;
                }
            }
            else
            {
                configPRINTF( ( "Failed to send an uplink packet with error = %d\r\n", status ) );
                configPRINTF( ( "Waiting for %u seconds, before sending next uplink.\r\n", LORAWAN_APPLICATION_TX_INTERVAL_SEC ) );
                vTaskDelay( pdMS_TO_TICKS( LORAWAN_APPLICATION_TX_INTERVAL_SEC * 1000 ) );
            }
        }
    }

    LoRaWAN_Cleanup();

    vTaskDelete( NULL );
}
