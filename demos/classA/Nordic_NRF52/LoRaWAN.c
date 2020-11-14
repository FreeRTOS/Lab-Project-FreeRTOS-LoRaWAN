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

#include "LoRaWAN.h"
#include "task.h"
#include "queue.h"
#include "utilities.h"
#include "board-config.h"

/**
 * @brief An event to indicate there are pending events to be processed from radio layer.
 */
#define LORAWAN_EVENT_RADIO_PENDING    ( 0x1U )

/**
 * @brief An event to indicate there are pending events to be processed from MAC layer.
 */
#define LORAWAN_EVENT_MAC_PENDING      ( 0x2U )

/**
 * @brief Max value for unsined long integer.
 */
#define ULONG_MAX                      ( 0xFFFFFFFFUL )

/**
 * @brief LoRaWAN EUIs which are 64 bits or 8 bytes in size.
 */
#define LORAWAN_EUI_SIZE               ( 8 )

/**
 * @brief LoRaWAN Keys which are 128 bits or 16 bytes in size.
 */
#define LORAWAN_KEY_SIZE               ( 16 )

/**
 * @brief Handle for LoRaMAC task.
 */
static TaskHandle_t xLoRaMacTask;

/**
 * @brief Qeue to receive incoming events from LoRa Network server.
 */
static QueueHandle_t xEventQueue;

/**
 * @brief Queue to receive responses for requests sent to LoRa Network Server.
 */
static QueueHandle_t xResponseQueue;

/**
 * @brief  Static primitives registered with LoRaMAC stack.
 */
static LoRaMacPrimitives_t xLoRaMacPrimitives = { 0 };

/**
 * @brief Static callbacks registered with LoRaMAC stack.
 */
static LoRaMacCallback_t xLoRaMacCallbacks = { 0 };

/**
 * @brief Region for LoRaWAN.
 */
LoRaMacRegion_t lorawanRegion = LORAMAC_REGION_US915;


/**
 * @brief Strings for denoting status responses from LoRaMAC layer.
 */
static const char * MacStatusStrings[] =
{
    "OK",                            /* LORAMAC_STATUS_OK */
    "Busy",                          /* LORAMAC_STATUS_BUSY */
    "Service unknown",               /* LORAMAC_STATUS_SERVICE_UNKNOWN */
    "Parameter invalid",             /* LORAMAC_STATUS_PARAMETER_INVALID */
    "Frequency invalid",             /* LORAMAC_STATUS_FREQUENCY_INVALID */
    "Datarate invalid",              /* LORAMAC_STATUS_DATARATE_INVALID */
    "Frequency or datarate invalid", /* LORAMAC_STATUS_FREQ_AND_DR_INVALID */
    "No network joined",             /* LORAMAC_STATUS_NO_NETWORK_JOINED */
    "Length error",                  /* LORAMAC_STATUS_LENGTH_ERROR */
    "Region not supported",          /* LORAMAC_STATUS_REGION_NOT_SUPPORTED */
    "Skipped APP data",              /* LORAMAC_STATUS_SKIPPED_APP_DATA */
    "Duty-cycle restricted",         /* LORAMAC_STATUS_DUTYCYCLE_RESTRICTED */
    "No channel found",              /* LORAMAC_STATUS_NO_CHANNEL_FOUND */
    "No free channel found",         /* LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND */
    "Busy beacon reserved time",     /* LORAMAC_STATUS_BUSY_BEACON_RESERVED_TIME */
    "Busy ping-slot window time",    /* LORAMAC_STATUS_BUSY_PING_SLOT_WINDOW_TIME */
    "Busy uplink collision",         /* LORAMAC_STATUS_BUSY_UPLINK_COLLISION */
    "Crypto error",                  /* LORAMAC_STATUS_CRYPTO_ERROR */
    "FCnt handler error",            /* LORAMAC_STATUS_FCNT_HANDLER_ERROR */
    "MAC command error",             /* LORAMAC_STATUS_MAC_COMMAD_ERROR */
    "ClassB error",                  /* LORAMAC_STATUS_CLASS_B_ERROR */
    "Confirm queue error",           /* LORAMAC_STATUS_CONFIRM_QUEUE_ERROR */
    "Multicast group undefined",     /* LORAMAC_STATUS_MC_GROUP_UNDEFINED */
    "Unknown error",                 /* LORAMAC_STATUS_ERROR */
};

/**
 * @brief Strings for denoting events from LoRaMAC layer.
 */
static const char * EventInfoStatusStrings[] =
{
    "OK",                            /* LORAMAC_EVENT_INFO_STATUS_OK */
    "Error",                         /* LORAMAC_EVENT_INFO_STATUS_ERROR */
    "Tx timeout",                    /* LORAMAC_EVENT_INFO_STATUS_TX_TIMEOUT */
    "Rx 1 timeout",                  /* LORAMAC_EVENT_INFO_STATUS_RX1_TIMEOUT */
    "Rx 2 timeout",                  /* LORAMAC_EVENT_INFO_STATUS_RX2_TIMEOUT */
    "Rx1 error",                     /* LORAMAC_EVENT_INFO_STATUS_RX1_ERROR */
    "Rx2 error",                     /* LORAMAC_EVENT_INFO_STATUS_RX2_ERROR */
    "Join failed",                   /* LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL */
    "Downlink repeated",             /* LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED */
    "Tx DR payload size error",      /* LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR */
    "Downlink too many frames loss", /* LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS */
    "Address fail",                  /* LORAMAC_EVENT_INFO_STATUS_ADDRESS_FAIL */
    "MIC fail",                      /* LORAMAC_EVENT_INFO_STATUS_MIC_FAIL */
    "Multicast fail",                /* LORAMAC_EVENT_INFO_STATUS_MULTICAST_FAIL */
    "Beacon locked",                 /* LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED */
    "Beacon lost",                   /* LORAMAC_EVENT_INFO_STATUS_BEACON_LOST */
    "Beacon not found"               /* LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND */
};



static void prvMcpsConfirm( McpsConfirm_t * mcpsConfirm )
{
    LoRaWANEventInfo_t event;

    configPRINTF( ( "MCPS CONFIRM status: %s\n", EventInfoStatusStrings[ mcpsConfirm->Status ] ) );

    event.status = mcpsConfirm->Status;

    switch( mcpsConfirm->McpsRequest )
    {
        case MCPS_CONFIRMED:
            event.type = LORAWAN_EVENT_CONFIRMED_MESSAGE_ACK;
            event.info.ackReceived = mcpsConfirm->AckReceived;
            break;

        case MCPS_UNCONFIRMED:
            event.type = LORAWAN_EVENT_UNCONFIRMED_MESSAGE_SENT;
            break;

        default:
            event.type = LORAWAN_EVENT_UNKOWN;
            break;
    }

    if( xQueueSend( xResponseQueue, &event, portMAX_DELAY ) != pdTRUE )
    {
        configPRINTF( ( "Failed to send mcps response to the queue.\r\n" ) );
    }
}

static void prvMcpsIndication( McpsIndication_t * mcpsIndication )
{
    LoRaWANEventInfo_t event = { 0 };

    configPRINTF( ( "MCPS INDICATION status: %s\n", EventInfoStatusStrings[ mcpsIndication->Status ] ) );

    if( ( mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_OK ) &&
        ( mcpsIndication->RxData == true ) )
    {
        event.type = LORAWAN_EVENT_DOWNLINK_DATA;
        event.status = LORAMAC_EVENT_INFO_STATUS_OK;
        configASSERT( mcpsIndication->BufferSize <= lorawanConfigMAX_MESSAGE_SIZE );
        event.info.downlinkData.port = mcpsIndication->Port;
        event.info.downlinkData.length = mcpsIndication->BufferSize;
        event.info.downlinkData.dataRate = mcpsIndication->RxDatarate;
        memcpy( event.info.downlinkData.data, mcpsIndication->Buffer, mcpsIndication->BufferSize );

        if( xQueueSend( xEventQueue, &event, 1 ) != pdTRUE )
        {
            configPRINTF( ( "Failed to send downlink data event to the queue.\r\n" ) );
        }
    }

    /* Check Multicast */
    /* Check Port */
    /* Check Datarate */
    if( mcpsIndication->FramePending == true )
    {
        /**
         * There are some pending commands to be sent uplink. Set an uplink event for the control task to
         * schedule an uplink whenever possible.
         */
        event.type = LORAWAN_EVENT_DOWNLINK_PENDING;
        event.status = LORAMAC_EVENT_INFO_STATUS_OK;

        if( xQueueSend( xEventQueue, &event, 1 ) != pdTRUE )
        {
            configPRINTF( ( "Failed to send pending downlink event to the queue.\r\n" ) );
        }
    }

    if( mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS )
    {
        event.type = LORAWAN_EVENT_TOO_MANY_FRAME_LOSS;
        event.status = LORAMAC_EVENT_INFO_STATUS_OK;

        if( xQueueSend( xEventQueue, &event, 1 ) != pdTRUE )
        {
            configPRINTF( ( "Failed to send to too many frame loss event to the queue.\r\n" ) );
        }
    }
}

static void prvMlmeIndication( MlmeIndication_t * MlmeIndication )
{
    LoRaWANEventInfo_t event = { 0 };

    configPRINTF( ( "MLME Indication status: %s\n", EventInfoStatusStrings[ MlmeIndication->Status ] ) );

    event.status = MlmeIndication->Status;

    if( event.status == LORAMAC_EVENT_INFO_STATUS_OK )
    {
        if( MlmeIndication->MlmeIndication == MLME_SCHEDULE_UPLINK )
        {
            event.type = LORAWAN_EVENT_DOWNLINK_PENDING;

            if( xQueueSend( xEventQueue, &event, 1 ) != pdTRUE )
            {
                configPRINTF( ( "Failed to send pending downlink event to the queue.\r\n" ) );
            }
        }
    }
}

static void prvMlmeConfirm( MlmeConfirm_t * mlmeConfirm )
{
    LoRaWANEventInfo_t event = { 0 };

    configPRINTF( ( "MLME CONFIRM  status: %s\n", EventInfoStatusStrings[ mlmeConfirm->Status ] ) );

    event.status = mlmeConfirm->Status;

    switch( mlmeConfirm->MlmeRequest )
    {
        case MLME_JOIN:
            event.type = LORAWAN_EVENT_JOIN_RESPONSE;

            if( xQueueSend( xResponseQueue, &event, 1 ) != pdTRUE )
            {
                configPRINTF( ( "Failed to send JOIN response to the queue.\r\n" ) );
            }

            break;

        case MLME_DEVICE_TIME:
            event.type = LORAWAN_EVENT_DEVICE_TIME_UPDATED;

            if( xQueueSend( xEventQueue, &event, 1 ) != pdTRUE )
            {
                configPRINTF( ( "Failed to send device time updated event to the queue.\r\n" ) );
            }

            break;

        case MLME_LINK_CHECK:
            event.type = LORAWAN_EVENT_LINK_CHECK_REPLY;
            event.info.linkCheck.DemodMargin = mlmeConfirm->DemodMargin;
            event.info.linkCheck.NbGateways = mlmeConfirm->NbGateways;

            if( xQueueSend( xEventQueue, &event, 1 ) != pdTRUE )
            {
                configPRINTF( ( "Failed to send link check reply event to the queue.\r\n" ) );
            }

            break;

        default:
            break;
    }
}

static LoRaMacStatus_t prvConfigure( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_PUBLIC_NETWORK;
        mibReq.Param.EnablePublicNetwork = lorawanConfigPUBLIC_NETWORK;
        LoRaMacMibSetRequestConfirm( &mibReq );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_ADR;
        mibReq.Param.AdrEnable = lorawanConfigADR_ON;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    #if defined( REGION_EU868 ) || defined( REGION_RU864 ) || defined( REGION_CN779 ) || defined( REGION_EU433 )
        if( status == LORAMAC_STATUS_OK )
        {
            status = LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );
        }
    #endif

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_SYSTEM_MAX_RX_ERROR;
        mibReq.Param.SystemMaxRxError = lorawanConfigRX_MAX_TIMING_ERROR;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    return status;
}


static LoRaMacStatus_t prvSetOTAACredentials( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;
    uint8_t devEUI[ LORAWAN_EUI_SIZE ];
    uint8_t joinEUI[ LORAWAN_EUI_SIZE ];
    uint8_t appKey[ LORAWAN_KEY_SIZE ];


    #ifdef lorawanConfigGET_DEV_EUI
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_DEV_EUI( devEUI );
            mibReq.Type = MIB_DEV_EUI;
            mibReq.Param.DevEui = devEUI;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif

    #ifdef lorawanConfigGET_JOIN_EUI
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_JOIN_EUI( joinEUI );
            mibReq.Type = MIB_JOIN_EUI;
            mibReq.Param.JoinEui = joinEUI;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif


    #ifdef lorawanConfigGET_APP_KEY
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_APP_KEY( appKey );
            mibReq.Type = MIB_APP_KEY;
            mibReq.Param.AppKey = appKey;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }

        if( status == LORAMAC_STATUS_OK )
        {
            mibReq.Type = MIB_NWK_KEY;
            mibReq.Param.AppKey = appKey;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif /* ifdef lorawanConfigGET_APP_KEY */

    return status;
}

static LoRaMacStatus_t prvSetABPCredentials( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;
    uint8_t devEUI[ LORAWAN_EUI_SIZE ];
    uint8_t joinEUI[ LORAWAN_EUI_SIZE ];
    uint8_t appsKey[ LORAWAN_KEY_SIZE ];
    uint8_t nwsKey[ LORAWAN_KEY_SIZE ];

    #ifdef lorawanConfigGET_DEV_EUI
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_DEV_EUI( devEUI );
            mibReq.Type = MIB_DEV_EUI;
            mibReq.Param.DevEui = devEUI;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif

    #ifdef lorawanConfigGET_JOIN_EUI
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_JOIN_EUI( joinEUI );
            mibReq.Type = MIB_JOIN_EUI;
            mibReq.Param.JoinEui = joinEUI;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif

    /* Tell the MAC layer which network server version are we connecting too. */
    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_ABP_LORAWAN_VERSION;
        mibReq.Param.AbpLrWanVersion.Value = lorawanConfigABP_LORAWAN_VERSION;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_NET_ID;
        mibReq.Param.NetID = lorawanConfigNETWORK_ID;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    #ifdef lorawanConfigGET_DEV_ADDR
        if( status == LORAMAC_STATUS_OK )
        {
            mibReq.Type = MIB_DEV_ADDR;
            mibReq.Param.DevAddr = lorawanConfigGET_DEV_ADDR();
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif

    #ifdef lorawanConfigGET_APP_SESSION_KEY
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_APP_SESSION_KEY( appsKey );
            mibReq.Type = MIB_APP_S_KEY;
            mibReq.Param.AppSKey = appsKey;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif

    #ifdef lorawanConfigGET_NETWORK_SESSION_KEY
        if( status == LORAMAC_STATUS_OK )
        {
            lorawanConfigGET_NETWORK_SESSION_KEY( nwsKey );
            mibReq.Type = MIB_NWK_S_ENC_KEY;
            mibReq.Param.NwkSEncKey = nwsKey;
            status = LoRaMacMibSetRequestConfirm( &mibReq );
        }
    #endif

    return status;
}


static uint8_t prvGetBatteryLevel( void )
{
    return 0;
}

static void prvOnMacNotify( void )
{
    xTaskNotifyAndQuery( xLoRaMacTask, LORAWAN_EVENT_MAC_PENDING, eSetBits, NULL );
}

static void prvOnRadioNotify()
{
    BaseType_t xHigherPriorityTaskWoken;

    xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyAndQueryFromISR( xLoRaMacTask, LORAWAN_EVENT_RADIO_PENDING, eSetBits, NULL, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void prvLoRaMACTask( void * pvParameters )
{
    uint32_t ulNotifiedValue;

    ( void ) pvParameters;

    for( ; ; )
    {
        xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY );

        if( ulNotifiedValue & LORAWAN_EVENT_RADIO_PENDING )
        {
            /* Process Radio IRQ. */
            if( Radio.IrqProcess != NULL )
            {
                Radio.IrqProcess();
            }

            /*Process Lora mac events based on Radio events. */
            LoRaMacProcess();
        }
        else if( ulNotifiedValue & LORAWAN_EVENT_MAC_PENDING )
        {
            /*Process events generated from LoRaMAC. */
            LoRaMacProcess();
        }
    }

    vTaskDelete( NULL );
}

LoRaMacStatus_t LoRaWAN_Init( LoRaMacRegion_t region )
{
    uint32_t ulNotifiedValue;
    LoRaMacStatus_t status;

    memset( &xLoRaMacPrimitives, 0x00, sizeof( LoRaMacPrimitives_t ) );
    memset( &xLoRaMacCallbacks, 0x00, sizeof( LoRaMacCallback_t ) );

    xLoRaMacPrimitives.MacMcpsConfirm = prvMcpsConfirm;
    xLoRaMacPrimitives.MacMcpsIndication = prvMcpsIndication;
    xLoRaMacPrimitives.MacMlmeConfirm = prvMlmeConfirm;
    xLoRaMacPrimitives.MacMlmeIndication = prvMlmeIndication;


    xLoRaMacCallbacks.GetBatteryLevel = prvGetBatteryLevel;
    xLoRaMacCallbacks.MacProcessNotify = prvOnMacNotify;

    status = LoRaMacInitialization( &xLoRaMacPrimitives, &xLoRaMacCallbacks, region );

    if( status == LORAMAC_STATUS_OK )
    {
        status = prvConfigure();

        if( status != LORAMAC_STATUS_OK )
        {
            configPRINTF( ( "LoRa MAC default configuration failed, status = %d.\n", status ) );
        }
    }

    if( status == LORAMAC_STATUS_OK )
    {
        xEventQueue = xQueueCreate( lorawanConfigEVENT_QUEUE_SIZE, sizeof( LoRaWANEventInfo_t ) );
        xResponseQueue = xQueueCreate( lorawanConfigRESPONSE_QUEUE_SIZE, sizeof( LoRaWANEventInfo_t ) );

        if( ( xEventQueue == NULL ) || ( xResponseQueue == NULL ) )
        {
            status = LORAMAC_STATUS_ERROR;
        }
    }

    if( status == LORAMAC_STATUS_OK )
    {
        if( xTaskCreate( prvLoRaMACTask, "LoRaMac", lorawanConfigLORAMAC_TASK_STACK_SIZE, NULL, lorawanConfigLORAMAC_TASK_PRIORITY, &xLoRaMacTask ) == pdTRUE )
        {
            Radio.SetEventNotify( &prvOnRadioNotify );
        }
        else
        {
            configPRINTF( ( "LoRaMAC loop task creation failed.\r\n" ) );
            status = LORAMAC_STATUS_ERROR;
        }
    }

    if( status == LORAMAC_STATUS_OK )
    {
        status = LoRaMacStart();

        if( status != LORAMAC_STATUS_OK )
        {
            configPRINTF( ( "LoRa MAC start failed, status = %d.\n", status ) );
        }
    }

    return status;
}


LoRaMacStatus_t LoRaWAN_ActivateByPersonalization( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;

    status = prvSetABPCredentials();

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_NETWORK_ACTIVATION;
        mibReq.Param.NetworkActivation = ACTIVATION_TYPE_ABP;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    return status;
}

/**
 * @brief Join to a LORAWAN network using OTAA join mechanism..
 * Blocks until the configured number of tries are reached or join is successful.
 */

LoRaMacStatus_t LoRaWAN_Join( void )
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq = { 0 };
    MibRequestConfirm_t mibReq = { 0 };
    uint32_t ulDutyCycleTimeMS = 0U;
    LoRaWANEventInfo_t event = { 0 };
    size_t xNumTries;
    GetPhyParams_t getDefaulDR = { 0 };
    PhyParam_t defaultDr = { 0 };

    mlmeReq.Type = MLME_JOIN;


    /* Set default data rate for join. */
    getDefaulDR.Attribute = PHY_DEF_TX_DR;
    defaultDr = RegionGetPhyParam( lorawanRegion, &getDefaulDR );
    mlmeReq.Req.Join.Datarate = defaultDr.Value;


    /* Configure the credentials before each join operation. */
    status = prvSetOTAACredentials();

    if( status == LORAMAC_STATUS_OK )
    {
        for( xNumTries = 0; xNumTries < lorawanConfigMAX_JOIN_ATTEMPTS; xNumTries++ )
        {
            /**
             * Initiates the join procedure. If the stack returns a duty cycle restricted error,
             * then retry after the duty cycle period. Duty cycle errors are not counted
             * as a failed try.
             */
            do
            {
                status = LoRaMacMlmeRequest( &mlmeReq );

                if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
                {
                    ulDutyCycleTimeMS = mlmeReq.ReqReturn.DutyCycleWaitTime;
                    configPRINTF( ( "Duty cycle restriction. Next Join in : ~%lu second(s)\n", ( ulDutyCycleTimeMS / 1000 ) ) );
                    vTaskDelay( pdMS_TO_TICKS( ulDutyCycleTimeMS ) );
                }
            } while( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED );

            if( status == LORAMAC_STATUS_OK )
            {
                xQueueReceive( xResponseQueue, &event, portMAX_DELAY );

                if( ( event.type == LORAWAN_EVENT_JOIN_RESPONSE ) && ( event.status == LORAMAC_EVENT_INFO_STATUS_OK ) )
                {
                    configPRINTF( ( "Successfully joined a LoRaWAN network.\n" ) );

                    mibReq.Type = MIB_DEV_ADDR;
                    LoRaMacMibGetRequestConfirm( &mibReq );
                    configPRINTF( ( "Device address : %08lX\n", mibReq.Param.DevAddr ) );

                    mibReq.Type = MIB_CHANNELS_DATARATE;
                    LoRaMacMibGetRequestConfirm( &mibReq );
                    configPRINTF( ( "Data rate : DR_%d\n", mibReq.Param.ChannelsDatarate ) );

                    break;
                }
                else
                {
                    configPRINTF( ( "Failed to join loRaWAN network with status %d.\n", event.status ) );
                    status = LORAMAC_STATUS_ERROR;
                }
            }
            else
            {
                configPRINTF( ( "Failed to initiate a LoRaWAN JOIN request with status %d.\n", status ) );
                break;
            }

            if( xNumTries < ( lorawanConfigMAX_JOIN_ATTEMPTS - 1 ) )
            {
                ulDutyCycleTimeMS = ( lorawanConfigJOIN_RETRY_INTERVAL_MS ) +
                                    randr( -lorawanConfigMAX_JITTER_MS, lorawanConfigMAX_JITTER_MS );
                configPRINTF( ( "Retrying join attempt after %lu seconds.\n", ( ulDutyCycleTimeMS / 1000 ) ) );
                vTaskDelay( pdMS_TO_TICKS( ulDutyCycleTimeMS ) );
            }
        }
    }

    return status;
}

LoRaMacStatus_t LoRaWAN_SetAdaptiveDataRate( bool enable )
{
    MibRequestConfirm_t mibReq = { 0 };

    mibReq.Type = MIB_ADR;
    mibReq.Param.AdrEnable = enable;
    return LoRaMacMibSetRequestConfirm( &mibReq );
}


LoRaMacStatus_t LoRaWAN_RequestDeviceTimeSync( void )
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq = { 0 };

    mlmeReq.Type = MLME_DEVICE_TIME;
    return LoRaMacMlmeRequest( &mlmeReq );
}


LoRaMacStatus_t LoRaWAN_RequestLinkCheck( void )
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq = { 0 };

    mlmeReq.Type = MLME_LINK_CHECK;
    return LoRaMacMlmeRequest( &mlmeReq );
}

LoRaMacStatus_t LoRaWAN_Send( LoRaWANMessage_t * pMessage,
                              bool confirmed )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    LoRaMacStatus_t status;
    uint32_t ulDutyCycleTimeMS = 0;
    LoRaWANEventInfo_t event;

    status = LoRaMacQueryTxPossible( pMessage->length, &txInfo );

    if( status == LORAMAC_STATUS_OK )
    {
        if( confirmed == false )
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = pMessage->port;
            mcpsReq.Req.Unconfirmed.fBuffer = pMessage->data;
            mcpsReq.Req.Unconfirmed.fBufferSize = pMessage->length;
            mcpsReq.Req.Unconfirmed.Datarate = pMessage->dataRate;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = pMessage->port;
            mcpsReq.Req.Confirmed.fBuffer = pMessage->data;
            mcpsReq.Req.Confirmed.fBufferSize = pMessage->length;
            mcpsReq.Req.Confirmed.NbTrials = lorawanConfigMAX_SEND_RETRIES;
            mcpsReq.Req.Confirmed.Datarate = pMessage->dataRate;
        }

        do
        {
            status = LoRaMacMcpsRequest( &mcpsReq );
            ulDutyCycleTimeMS = mcpsReq.ReqReturn.DutyCycleWaitTime;

            if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
            {
                configPRINTF( ( "Duty cycle restriction. Wait ~%lu second(s) before sending uplink.\n", ( ulDutyCycleTimeMS / 1000 ) ) );
                vTaskDelay( pdMS_TO_TICKS( ulDutyCycleTimeMS ) );
            }
        } while( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        xQueueReceive( xResponseQueue, &event, portMAX_DELAY );

        if( event.status == LORAMAC_EVENT_INFO_STATUS_OK )
        {
            if( ( ( confirmed == true ) && ( event.type == LORAWAN_EVENT_CONFIRMED_MESSAGE_ACK ) && ( event.info.ackReceived == true ) ) ||
                ( ( confirmed == false ) && ( event.type == LORAWAN_EVENT_UNCONFIRMED_MESSAGE_SENT ) ) )
            {
                status = LORAMAC_STATUS_OK;
            }
            else
            {
                status = LORAMAC_STATUS_ERROR;
            }
        }
        else
        {
            status = LORAMAC_STATUS_ERROR;
        }
    }

    return status;
}

BaseType_t LoRaWAN_Receive( LoRaWANEventInfo_t * pEventInfo,
                            uint32_t timeoutMS )
{
    return xQueueReceive( xEventQueue, pEventInfo, pdMS_TO_TICKS( timeoutMS ) );
}

void LoRaWAN_Cleanup( void )
{
    LoRaMacStop();
    ( void ) LoRaMacDeInitialization();
    vTaskDelete( xLoRaMacTask );
    vQueueDelete( xEventQueue );
    vQueueDelete( xResponseQueue );
}

/* Unique ID for the board used by LoRaMAC APIs. */
#ifdef lorawanConfigGET_DEV_EUI
    void BoardGetUniqueId( uint8_t * id )
    {
        lorawanConfigGET_DEV_EUI( id );
    }
#endif
