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

#ifndef LORAWAN_CONFIG_H
#define LORAWAN_CONFIG_H

/**
 * @brief Flag to indicate if application is using a public network such 
 * as The Things Network.
 */
#define lorawanConfigPUBLIC_NETWORK             ( 1 )

 
/**
 * @brief Maximum join attempts before giving up.
 * 
 * Retry attempts tries to send join requests in different channels thereby finding a suitable gateway which
 * is tuned to that channel.
 */
#define lorawanConfigMAX_JOIN_ATTEMPTS          ( 1000 )


/**
 * @brief Interval between retry attempts for OTAA join.
 * It waits for a retry interval +- random jitter ( to avoid dos ) before attempting to 
 * join again with LoRaWAN network.
 */
#define lorawanConfigJOIN_RETRY_INTERVAL_MS      ( 2000 )


/**
 * @brief Defines a random jitter bound in milliseconds for application data transmission duty cycle.
 *
 * This allows devices to space their transmissions slighltly between each other in cases like all devices reboots and tries to
 * join server at same time.
 */
#define lorawanConfigMAX_JITTER_MS              ( 500 )



/**
 * @brief Default config to enable or disable adaptive data rate.
 *
 * Enabling adaptive data rate allows the network to set optimized data rates for end devices
 * thereby optimizing on air time and power consumption. Its recommended to enable adaptive
 * data rate for static devices and devices with stable RF conditions.
 * Adaptive data rate can be toggled runtime using API.
 * 
 */
#define lorawanConfigADR_ON                     ( 1 )


/**
 * @brief Default config to set the number of retries of a failed send attempt.
 * 
 */
#define lorawanConfigMAX_SEND_RETRIES           ( 8 )


/**
 * @brief Overall timing error threshold for the system.
 */
#define lorawanConfigRX_MAX_TIMING_ERROR        ( 50 )


/**
 * @brief Maximum payload length defined by LoRaWAN spec
 *
 * This can be used to cap the maximum packet size that can be transferred anytime by the application.
 * LoRaWAN payload can vary upto 222 bytes. However applications should take care of duty cycle restrictions and
 * fair access policies for each region while determining the size of a message to be transmitted.
 * Larger messages leads to longer air-time and increased power consumption for the 
 * radio as well as using up all of the duty cycle for a channel.
 */
#define lorawanConfigMAX_MESSAGE_SIZE         ( 222 )


/**
 * @brief Size of response queue used to receive responses to requests.
 * Queue is used to separate out events from responses so application can do a synchronous call to 
 * join to a network or send a confirmed message. Since there is atmost 1 LoRaWAN operation at a time, queue size
 * is set to 1.
 */
#define lorawanConfigRESPONSE_QUEUE_SIZE        ( 1 )


/**
 * @breif Queue size for downlink events.
 *
 * Class A application sends an uplink and then uses two receive slots for any downlink messages from the server. It does not receive
 * messages from server any other time.
 * For class A application at most 4 events can be received downlink per uplink at any time (SRV_MAC_LINK_CHECK_ANS, SRV_MAC_DEVICE_TIME_ANS, FRAME LOSS, DOWNLINK DATA)
 * Queue size can be adjusted based on application needs.
 */
#define lorawanConfigEVENT_QUEUE_SIZE        ( 4 )

#endif /* LORAWAN_CONFIG_H */