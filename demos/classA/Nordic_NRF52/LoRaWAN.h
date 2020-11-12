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

#ifndef LORAWAN_H
#define LORAWAN_H

#include "FreeRTOS.h"

#include "LoRaWANConfig.h"
#include "LoRaMac.h"

/**
 * @brief Values indicating different join types.
 */
typedef enum LoRaWANJoinType
{
    LORAWAN_JOIN_TYPE_OTAA = 0, /**< @brief Over The Air Activation join type. */
    LORAWAN_JOIN_TYPE_ABP       /**< @brief Activation by personalization join type. */
} LoRaWANJoinType_t;

/**
 * @brief Structure which holds the LoRaWAN payload information.
 * The same structure is used for both payload send and received.
 */
typedef struct LoRaWANMessage
{
    uint16_t port;                                 /**< @brief Application port for the payload. */
    uint8_t data[ lorawanConfigMAX_MESSAGE_SIZE ]; /**< @brief The buffer of fixed maximum size used to hold the payload. */
    size_t length;                                 /**< @brief Length of the payload. */
    uint8_t dataRate;                              /**< @brief the data rate used to transfer the payload. */
} LoRaWANMessage_t;

/**
 * @brief Information sent as part of link check reply event.
 */
typedef struct LinkCheckInfo
{
    uint8_t DemodMargin; /**< @brief Demodulation margin. Contains the link margin [dB] of the last successfully received LinkCheckReq. */
    uint8_t NbGateways;  /**< @brief Number of gateways which received the last LinkCheckReq. */
} LinkCheckInfo_t;

/**
 * @brief Event types received from LoRaWAN network.
 */
typedef enum LoRaWANEventType
{
    LORAWAN_EVENT_UNKOWN = 0,               /**< @brief Type to denote an unexpected event type. */
    LORAWAN_EVENT_JOIN_RESPONSE,            /**< @brief Successful or failed join response. */
    LORAWAN_EVENT_UNCONFIRMED_MESSAGE_SENT, /**< @brief Indicates an unconfirmed  payload is sent out of radio. */
    LORAWAN_EVENT_CONFIRMED_MESSAGE_ACK,    /**< @brief Indicates an acknowledgment for an confirmed uplink payload. */
    LORAWAN_EVENT_DOWNLINK_DATA,            /**< @brief Indicates a downlink payload from network server. */
    LORAWAN_EVENT_DOWNLINK_PENDING,         /**< @brief Indicates that server has to send more downlink data or waiting for a mac command uplink. */
    LORAWAN_EVENT_TOO_MANY_FRAME_LOSS,      /**< @brief Indicates too many frames are missed between end device and LoRa network server. */
    LORAWAN_EVENT_DEVICE_TIME_UPDATED,      /**< @brief Indicates the device time has been synchronized with LoRa network server. */
    LORAWAN_EVENT_LINK_CHECK_REPLY          /**< @brief Reply for a link check request from end device. */
} LoRaWANEventType_t;

/**
 * @brief Structure to hold event information.
 */
typedef struct LoRaWANEventInfo
{
    LoRaWANEventType_t type;         /**< @brief Type of event. */
    LoRaMacEventInfoStatus_t status; /**< @breif Status associated with the event. */

    union
    {
        LinkCheckInfo_t linkCheck;     /**< @brief Link check information associated with LORAWAN_EVENT_LINK_CHECK_REPLY. */
        bool ackReceived;              /**< @brief Acknoweldgement flag for a confirmed uplink. */
        LoRaWANMessage_t downlinkData; /**< @brief Downlink data associated with event LORAWAN_EVENT_DOWNLINK_DATA. */
    } info;
} LoRaWANEventInfo_t;

/**
 * @brief Initializes LoRaWAN stack for the specified region.
 * Configures and starts underlying LoRaMAC stack. Creates a task to process LoRaMAC packets.
 *
 * @param[in] region The region for the LoRaWAN network.
 * @return LORAMAC_STATUS_OK if the initialization was successful. Appropirate error code otherwise.
 */
LoRaMacStatus_t LoRaWAN_Init( LoRaMacRegion_t region );

/**
 * @brief Performs a join operation with the LoRa Network Server.
 * For OTAA, it performs a blocking call untill handshake is complete. It performs JOIN retries at
 * random intervals for a configured number of tries.
 *
 * @param[in] joinType Type of JOIN operation, wether OTAA or ABP.
 * @param[in] dataRate Data rate used for the JOIN operation.
 * @return LORAMAC_STATUS_OK if the join was successful. Appropirate error code otherwise.
 */
LoRaMacStatus_t LoRaWAN_Join( LoRaWANJoinType_t joinType,
                              int8_t dataRate );


/**
 * @brief Enables or disables adaptive data rate.
 * Adaptive data rate mechanism is used by LoRa Network Server to find the right data rate by observing the
 * uplink traffic from end-device. Its useful and recommended to be turned on for fixed devices.
 *
 * @param[in] enable Enable or disable flag
 * @return LORAMAC_STATUS_OK if the operation was successful. Appropirate error code otherwise.
 */
LoRaMacStatus_t LoRaWAN_SetAdaptiveDataRate( bool enable );

/**
 * @brief Request for device time synchronization with LoRa Network Server.
 * Piggy backs a MAC command along with next payload to request for current time from LoRa network server. Uses the response from
 * LoRa network server to correct the clock drift for the device. Response will be send via an event.
 *
 * @return LORAMAC_STATUS_OK if the request operation was successful. Appropirate error code otherwise.
 */
LoRaMacStatus_t LoRaWAN_RequestDeviceTimeSync( void );

/**
 * @brief Request for link check with LoRa Network Server.
 * Piggy backs a MAC command along with next payload to do link check with LoRa network server. Gets back the response from LoRa Network
 * Server as an event.
 *
 * @return LORAMAC_STATUS_OK if the request operation was successful. Appropirate error code otherwise.
 */
LoRaMacStatus_t LoRaWAN_RequestLinkCheck( void );

/**
 * @brief Sends payload to LoRa Network server.
 * This is a blocking call untill the payload is send out for an unconfirmed payload, or an acknoweledgement is received or retries
 * are exhausted for a confirmed payload. Number of retries for a confirmed payload is configurable. The retries tries different
 * frequencies so that can be received by gateway.
 *
 * @param[in] pMessage Pointer to the payload along with other information.
 * @param[in] confirmed Should send a confirmed payload or not.
 * @return LORAMAC_STATUS_OK if the request operation was successful. Appropirate error code otherwise.
 */
LoRaMacStatus_t LoRaWAN_Send( LoRaWANMessage_t * pMessage,
                              bool confirmed );

/**
 * @brief Receives a downlink event from LoRa Network server.
 * Blocks for the specified timeout provided. Downlink events can be a payload or other control events.
 *
 * @param[out] pEventInfo Pointer to structure containing event type and other information.
 * @param[in] timeoutMS Timeout in milliseconds to block for an event.
 * @return pdFALSE if there are no more events. pdTRUE if there are more events to be processed.
 */
BaseType_t LoRaWAN_Receive( LoRaWANEventInfo_t * pEventInfo,
                            uint32_t timeoutMS );

/**
 * @brief Cleans up LoRaWAN stack.
 * Stops and deinits the LoRaMAC stack. Deletes the LoRaMAC stack task.
 */
void LoRaWAN_Cleanup( void );

#endif LORAWAN_H /* LORAWAN_H */
