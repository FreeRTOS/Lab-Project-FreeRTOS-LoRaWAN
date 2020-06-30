#include "FreeRTOS.h"
#include "task.h"
#include "timer.h"
#include "LoRaMac.h"
#include "board.h"
#include "utilities.h"
#include "radio.h"
#include "message_buffer.h"
#include "queue.h"

/*
 * @brief Default data rate used for uplink messages.
 */
#define LORAWAN_DEFAULT_DATARATE           DR_0

/**
 * @brief Can be used to enable/disable adaptive data rate and
 * frequency hopping.
 */
#define LORAWAN_ADR_ON                     1

/**
 * @brief Defines maximum message size for transmission.
 * Applications should take care of duty cycle restrictions and fair access policies for
 * each regions while determining the size of message to be transmitted. Larger messages can
 * lead to longer air-time for radio, and this can lead to radio using up all the duty cycle limit.
 */
#define LORAWAN_MAX_MESG_SIZE              242

/**
 * @brief LoRa MAC layer port size.
 */
#define LORAWAN_APP_PORT_SIZE              ( 1 )

/**
 * @brief LoRa MAC layer port used for communication. Downlink messages
 * should be send on this port number.
 */
#define LORAWAN_APP_PORT                   2

/**
 * @brief Flag for LoRaMAC stack to tell if its a public network such as TTN.
 */
#define LORAWAN_PUBLIC_NETWORK             1

/**
 * @brief Queue size for LoRaMAC response queue.
 */
#define LORAWAN_MAX_QUEUED_RESPONSES       ( 10 )

/**
 * @brief Maximum join attempts before giving up. LoRa tries a range of channels
 * within the band to send join requests for the first time.
 */
#define LORAWAN_MAX_JOIN_ATTEMPTS          ( 1000 )

/**
 * @brief Should send confirmed messages (with acknowledgment) or not.
 */
#define LORAWAN_SEND_CONFIRMED_MESSAGES    ( 1 )

/**
 * @brief Maximum time to wait for a downlink message. This should be greater than or equal to the
 * (RX1 + RX2) window duration for low latency downlink.
 */

#define LORAWAN_RECEIVE_TIMEOUT_MS         ( 5000 )

/**
 * @brief Defines the application data transmission duty cycle time in milliseconds.
 */
#define LORAWAN_APP_TX_DUTYCYCLE_MS        ( 5000 )

/**
 * @brief Defines a random jitter bound for application data transmission duty cycle in milliseconds.
 */
#define LORAWAN_APP_TX_DUTYCYCLE_RND_MS    ( 1000 )

/**
 * @brief EUI and keys that needs to be provisioned for device identity and security.
 */

#define DEV_EUI        { 0x32, 0x38, 0x33, 0x35, 0x60, 0x38, 0x71, 0x01 };
#define JOIN_EUI       { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x02, 0xD1, 0xD4 };
#define APP_NWK_KEY    { 0xF5, 0x42, 0x96, 0x98, 0x8B, 0xC2, 0x23, 0x86, 0x56, 0x24, 0x1D, 0x73, 0x0A, 0xFA, 0x95, 0x0B };

/**
 * @biref Defines types of join methods.
 */
typedef enum
{
    LORAWAN_JOIN_MODE_OTAA = 0,
    LORAWAN_JOIN_MODE_ABP
} LoraWanJoinMode_t;

typedef enum
{
    LORAMAC_RESPONSE_TYPE_MLME = 0,
    LORAMAC_RESPONSE_TYPE_MCPS
} LoRaMacResponseType_t;

typedef struct LoRaMacResponse
{
    LoRaMacResponseType_t type;
    union
    {
        MlmeConfirm_t mlmeConfirm;
        McpsConfirm_t mcpsConfirm;
    } resp;
} LoRaMacResponse_t;

typedef struct LoRaMacMessage
{
    uint8_t port;
    size_t length;
    uint8_t buffer[ LORAWAN_MAX_MESG_SIZE ];
} LoRaMacMessage_t;


/**
 * @brief Strings for denoting status responses from LoRaMAC layer.
 */
static const char * MacStatusStrings[] =
{
    "OK",                                /* LORAMAC_STATUS_OK */
    "Busy",                              /* LORAMAC_STATUS_BUSY */
    "Service unknown",                   /* LORAMAC_STATUS_SERVICE_UNKNOWN */
    "Parameter invalid",                 /* LORAMAC_STATUS_PARAMETER_INVALID */
    "Frequency invalid",                 /* LORAMAC_STATUS_FREQUENCY_INVALID */
    "Datarate invalid",                  /* LORAMAC_STATUS_DATARATE_INVALID */
    "Frequency or datarate invalid",     /* LORAMAC_STATUS_FREQ_AND_DR_INVALID */
    "No network joined",                 /* LORAMAC_STATUS_NO_NETWORK_JOINED */
    "Length error",                      /* LORAMAC_STATUS_LENGTH_ERROR */
    "Region not supported",              /* LORAMAC_STATUS_REGION_NOT_SUPPORTED */
    "Skipped APP data",                  /* LORAMAC_STATUS_SKIPPED_APP_DATA */
    "Duty-cycle restricted",             /* LORAMAC_STATUS_DUTYCYCLE_RESTRICTED */
    "No channel found",                  /* LORAMAC_STATUS_NO_CHANNEL_FOUND */
    "No free channel found",             /* LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND */
    "Busy beacon reserved time",         /* LORAMAC_STATUS_BUSY_BEACON_RESERVED_TIME */
    "Busy ping-slot window time",        /* LORAMAC_STATUS_BUSY_PING_SLOT_WINDOW_TIME */
    "Busy uplink collision",             /* LORAMAC_STATUS_BUSY_UPLINK_COLLISION */
    "Crypto error",                      /* LORAMAC_STATUS_CRYPTO_ERROR */
    "FCnt handler error",                /* LORAMAC_STATUS_FCNT_HANDLER_ERROR */
    "MAC command error",                 /* LORAMAC_STATUS_MAC_COMMAD_ERROR */
    "ClassB error",                      /* LORAMAC_STATUS_CLASS_B_ERROR */
    "Confirm queue error",               /* LORAMAC_STATUS_CONFIRM_QUEUE_ERROR */
    "Multicast group undefined",         /* LORAMAC_STATUS_MC_GROUP_UNDEFINED */
    "Unknown error",                     /* LORAMAC_STATUS_ERROR */
};

/**
 * @brief Strings for denoting events from LoRaMAC layer.
 */
static const char * EventInfoStatusStrings[] =
{
    "OK",                                /* LORAMAC_EVENT_INFO_STATUS_OK */
    "Error",                             /* LORAMAC_EVENT_INFO_STATUS_ERROR */
    "Tx timeout",                        /* LORAMAC_EVENT_INFO_STATUS_TX_TIMEOUT */
    "Rx 1 timeout",                      /* LORAMAC_EVENT_INFO_STATUS_RX1_TIMEOUT */
    "Rx 2 timeout",                      /* LORAMAC_EVENT_INFO_STATUS_RX2_TIMEOUT */
    "Rx1 error",                         /* LORAMAC_EVENT_INFO_STATUS_RX1_ERROR */
    "Rx2 error",                         /* LORAMAC_EVENT_INFO_STATUS_RX2_ERROR */
    "Join failed",                       /* LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL */
    "Downlink repeated",                 /* LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED */
    "Tx DR payload size error",          /* LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR */
    "Downlink too many frames loss",     /* LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS */
    "Address fail",                      /* LORAMAC_EVENT_INFO_STATUS_ADDRESS_FAIL */
    "MIC fail",                          /* LORAMAC_EVENT_INFO_STATUS_MIC_FAIL */
    "Multicast fail",                    /* LORAMAC_EVENT_INFO_STATUS_MULTICAST_FAIL */
    "Beacon locked",                     /* LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED */
    "Beacon lost",                       /* LORAMAC_EVENT_INFO_STATUS_BEACON_LOST */
    "Beacon not found"                   /* LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND */
};

/**
 * @brief The callbacks registered with LoRaMAC stack layer. \
 */
static LoRaMacPrimitives_t LoRaMacPrimitives;

static LoRaMacCallback_t LoRaMacCallbacks;

/**
 * @brief Main LoRaWAN Class A application task handle.
 */
static TaskHandle_t xLoRaWANTask;

/**
 * @brief This task runs the LoRaMAC stack processing, based on
 * events received from radio interrupts or other timer expired
 * events.
 */
static TaskHandle_t xLoRaMacTask;

/**
 * @brief Queue used to wait for responses from LoRaMAC stack
 */
static QueueHandle_t xLoRaMacResponseQueue;

/**
 * @brief Buffer used to store the downlink messages received from the
 * LoRaMAC stack. User applications can do a blocking receiveFrom operation for a timeout
 * value, usually after a successful uplink or whenever downlink is expected.
 *
 */
static MessageBufferHandle_t xLoRaMacMessageBuffer;

static void McpsConfirm( McpsConfirm_t * mcpsConfirm )
{
    LoRaMacResponse_t response = { 0 };

    configPRINTF( ( "MCPS CONFIRM status: %s\n", EventInfoStatusStrings[ mcpsConfirm->Status ] ) );

    response.type = LORAMAC_RESPONSE_TYPE_MCPS;
    memcpy( &response.resp.mcpsConfirm, mcpsConfirm, sizeof( MlmeConfirm_t ) );

    xQueueSend( xLoRaMacResponseQueue, &response, portMAX_DELAY );
}

/*!
 * Prints the provided buffer in HEX
 *
 * \param buffer Buffer to be printed
 * \param size   Buffer size to be printed
 */
static void PrintHexBuffer( uint8_t * buffer,
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


static void McpsIndication( McpsIndication_t * mcpsIndication )
{
    uint8_t buffer[ LORAWAN_MAX_MESG_SIZE + LORAWAN_APP_PORT_SIZE ];

    configPRINTF( ( "MCPS INDICATION status: %s\n", EventInfoStatusStrings[ mcpsIndication->Status ] ) );

    if( ( mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_OK ) &&
        ( mcpsIndication->RxData == true ) )
    {
        buffer[ 0 ] = mcpsIndication->Port;
        memcpy( ( buffer + LORAWAN_APP_PORT_SIZE ), mcpsIndication->Buffer, mcpsIndication->BufferSize );

        xMessageBufferSend( xLoRaMacMessageBuffer, buffer, ( mcpsIndication->BufferSize + LORAWAN_APP_PORT_SIZE ), portMAX_DELAY );
    }

    /* Check Multicast */
    /* Check Port */
    /* Check Datarate */
    if( mcpsIndication->FramePending == true )
    {
        /**
         * There are some pending commands to be sent uplink. Trigger an empty uplink
         * or send a routine uplink message to piggyback these requests along with it.
         */
    }
}


static void MlmeIndication( MlmeIndication_t * MlmeIndication )
{
    /* Implementation for MLME indication */
}


static void MlmeConfirm( MlmeConfirm_t * mlmeConfirm )
{
    LoRaMacEventInfoStatus_t status = mlmeConfirm->Status;
    LoRaMacResponse_t response;

    response.type = LORAMAC_RESPONSE_TYPE_MLME;
    memcpy( &response.resp.mlmeConfirm, mlmeConfirm, sizeof( MlmeConfirm_t ) );

    configPRINTF( ( "MLME CONFIRM  status: %s\n", EventInfoStatusStrings[ status ] ) );

    xQueueSend( xLoRaMacResponseQueue, &response, portMAX_DELAY );
}

static void onRadioNotify( bool fromISR )
{
    BaseType_t xHigherPriorityTaskWoken;

    if( fromISR )
    {
        xHigherPriorityTaskWoken = pdFALSE;

        vTaskNotifyGiveFromISR( xLoRaMacTask, &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
    else
    {
        xTaskNotifyGive( xLoRaMacTask );
    }
}

static void onMacNotify( void )
{
    xTaskNotifyGive( xLoRaMacTask );
}

static uint8_t getBatteryLevel( void )
{
    return 0;
}

/**
 * @brief Join to a LORAWAN network using OTAA join mechanism..
 * Blocks until the configured number of tries are reached or join is successful.
 */

static LoRaMacStatus_t prvOTAAJoinNetwork( void )
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq = { 0 };
    MibRequestConfirm_t mibReq = { 0 };
    uint32_t ulDutyCycleTimeMS = 0U;
    LoRaMacResponse_t response;
    LoRaMacEventInfoStatus_t responseStatus;
    size_t xNumTries;

    mlmeReq.Type = MLME_JOIN;
    mlmeReq.Req.Join.Datarate = LORAWAN_DEFAULT_DATARATE;

    for( xNumTries = 0; xNumTries < LORAWAN_MAX_JOIN_ATTEMPTS; xNumTries++ )
    {
        /**
         * Initiates the join procedure. If the stack returns a duty cycle restricted error,
         * then retry after the duty cycle period of time. Duty cycle errors are not counted
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
            xQueueReceive( xLoRaMacResponseQueue, &response, portMAX_DELAY );

            if( ( response.type == LORAMAC_RESPONSE_TYPE_MLME ) && ( response.resp.mlmeConfirm.MlmeRequest == MLME_JOIN ) )
            {
                if( response.resp.mlmeConfirm.Status == LORAMAC_EVENT_INFO_STATUS_OK )
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
                    configPRINTF( ( "Failed to join loRaWAN network with status %d.\n", response.resp.mlmeConfirm.Status ) );
                    status = LORAMAC_STATUS_ERROR;
                }
            }
        }
        else
        {
            configPRINTF( ( "Failed to initiate a LoRaWAN JOIN request with status %d.\n", status ) );
            break;
        }

        if( xNumTries < ( LORAWAN_MAX_JOIN_ATTEMPTS - 1 ) )
        {
            configPRINTF( ( "Retrying join attempt after %lu seconds.\n", ( LORAWAN_APP_TX_DUTYCYCLE_MS / 1000 ) ) );
        }

        vTaskDelay( pdMS_TO_TICKS( LORAWAN_APP_TX_DUTYCYCLE_MS ) );
    }

    return status;
}


static LoRaMacStatus_t prvConfigure( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;
    uint8_t devEUI[ 8 ] = DEV_EUI;
    uint8_t joinEUI[ 8 ] = JOIN_EUI;
    uint8_t appKey[ 16 ] = APP_NWK_KEY;

    mibReq.Type = MIB_DEV_EUI;
    mibReq.Param.DevEui = devEUI;
    status = LoRaMacMibSetRequestConfirm( &mibReq );

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_JOIN_EUI;
        mibReq.Param.JoinEui = joinEUI;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    if( status == LORAMAC_STATUS_OK )
    {
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

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_PUBLIC_NETWORK;
        mibReq.Param.EnablePublicNetwork = LORAWAN_PUBLIC_NETWORK;
        LoRaMacMibSetRequestConfirm( &mibReq );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_ADR;
        mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
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
        mibReq.Param.SystemMaxRxError = 50;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    return status;
}


static size_t prvSend( uint8_t port,
                       uint8_t * message,
                       size_t messageSize,
                       bool confirmed )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    LoRaMacStatus_t status;
    size_t bytesSent = 0;
    uint32_t ulDutyCycleTimeMS = 0;
    LoRaMacResponse_t response = { 0 };

    status = LoRaMacQueryTxPossible( messageSize, &txInfo );

    if( status == LORAMAC_STATUS_OK )
    {
        if( confirmed == false )
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = port;
            mcpsReq.Req.Unconfirmed.fBuffer = message;
            mcpsReq.Req.Unconfirmed.fBufferSize = messageSize;
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = port;
            mcpsReq.Req.Confirmed.fBuffer = message;
            mcpsReq.Req.Confirmed.fBufferSize = messageSize;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }

        do
        {
            status = LoRaMacMcpsRequest( &mcpsReq );

            if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
            {
                ulDutyCycleTimeMS = mcpsReq.ReqReturn.DutyCycleWaitTime;
                configPRINTF( ( "Duty cycle restriction. Wait ~%lu second(s) before sending uplink.\n", ( ulDutyCycleTimeMS / 1000 ) ) );
                vTaskDelay( pdMS_TO_TICKS( ulDutyCycleTimeMS ) );
            }
        } while( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED );

        if( status == LORAMAC_STATUS_OK )
        {
            xQueueReceive( xLoRaMacResponseQueue, &response, portMAX_DELAY );

            if( ( response.type == LORAMAC_RESPONSE_TYPE_MCPS ) && ( response.resp.mcpsConfirm.Status == LORAMAC_EVENT_INFO_STATUS_OK ) )
            {
                if( ( response.resp.mcpsConfirm.McpsRequest == MCPS_UNCONFIRMED ) ||
                    ( ( response.resp.mcpsConfirm.McpsRequest == MCPS_CONFIRMED ) && ( response.resp.mcpsConfirm.AckReceived ) ) )
                {
                    bytesSent = messageSize;
                }
            }
        }
    }
    else
    {
        /* Send empty frame in order to flush MAC commands */
        configPRINTF( ( "TX not possible for data size %d.\n", messageSize ) );

        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;

        ( void ) LoRaMacMcpsRequest( &mcpsReq );
    }

    return bytesSent;
}

static size_t prvReceiveFrom( uint8_t * port,
                              uint8_t * buffer,
                              size_t bufferSize,
                              size_t timeoutMS )
{
    LoRaMacMessage_t message;
    TickType_t ticksToWait = pdMS_TO_TICKS( timeoutMS );
    size_t bytesReceived = 0, received = 0;
    uint8_t recvBuffer[ LORAWAN_MAX_MESG_SIZE + LORAWAN_APP_PORT_SIZE ];

    received = xMessageBufferReceive( xLoRaMacMessageBuffer, recvBuffer, ( bufferSize + LORAWAN_APP_PORT_SIZE ), ticksToWait );

    if( received > 0 )
    {
        *port = recvBuffer[ 0 ];
        bytesReceived = ( received - LORAWAN_APP_PORT_SIZE );
        memcpy( buffer, ( recvBuffer + LORAWAN_APP_PORT_SIZE ), bytesReceived );
    }

    return bytesReceived;
}

static void prvLoRaMacTask( void * params )
{
    uint32_t ulNotifiedValue;

    for( ; ; )
    {
        ulNotifiedValue = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

        while( ulNotifiedValue > 0 )
        {
            /* Process Radio IRQ. */
            if( Radio.IrqProcess != NULL )
            {
                Radio.IrqProcess();
            }

            /*Process Lora mac events based on Radio events. */
            LoRaMacProcess();

            ulNotifiedValue--;
        }
    }

    vTaskDelete( NULL );
}

static void prvLorawanClassATask( void * params )
{
    LoRaMacStatus_t status;
    MibRequestConfirm_t mibReq;
    uint8_t uplink[ LORAWAN_MAX_MESG_SIZE ] = { 0 };
    uint8_t downlink[ LORAWAN_MAX_MESG_SIZE ] = { 0 };
    size_t uplinkSize = 0, downlinkSize = LORAWAN_MAX_MESG_SIZE;
    size_t bytesTransferred;
    uint8_t port;
    uint32_t dutyCycleInterval;

    LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
    LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
    LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
    LoRaMacPrimitives.MacMlmeIndication = MlmeIndication;
    LoRaMacCallbacks.GetBatteryLevel = getBatteryLevel;
    LoRaMacCallbacks.MacProcessNotify = onMacNotify;

    configPRINTF( ( "###### ===== Class A demo application v1.0.0 ==== ######\n\n" ) );

    status = LoRaMacInitialization( &LoRaMacPrimitives, &LoRaMacCallbacks, LORAMAC_REGION_US915 );

    if( status == LORAMAC_STATUS_OK )
    {
        Radio.SetEventNotify( &onRadioNotify );
        configPRINTF( ( "LoRa MAC stack initialized.\r\n" ) );
    }
    else
    {
        configPRINTF( ( "LoRa MAC initialization failed, status = %d.\r\n", status ) );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        status = prvConfigure();

        if( status != LORAMAC_STATUS_OK )
        {
            configPRINTF( ( "LoRa MAC configuration failed, status = %d.\n", status ) );
        }
    }

    if( status == LORAMAC_STATUS_OK )
    {
        status = LoRaMacStart();

        if( status == LORAMAC_STATUS_OK )
        {
            configPRINTF( ( "Lora MAC started. \r\n" ) );
        }
        else
        {
            configPRINTF( ( "LoRa MAC start failed, status = %d.\n", status ) );
        }
    }

    if( status == LORAMAC_STATUS_OK )
    {
        mibReq.Type = MIB_NETWORK_ACTIVATION;
        status = LoRaMacMibGetRequestConfirm( &mibReq );

        if( status == LORAMAC_STATUS_OK )
        {
            if( mibReq.Param.NetworkActivation == ACTIVATION_TYPE_NONE )
            {
                status = prvOTAAJoinNetwork();

                if( status != LORAMAC_STATUS_OK )
                {
                    configPRINTF( ( "Failed to join LoRaWAN network.\n" ) );
                }
            }
            else
            {
                configPRINTF( ( "Device already joined to LoRaWAN network.\n" ) );
            }
        }
        else
        {
            configPRINTF( ( "Error while fetching activation status, error = %d\n.", status ) );
        }
    }

    if( status == LORAMAC_STATUS_OK )
    {
        /**
         * Task loops forever and sends periodic uplink byte 0xFF. It then blocks for a period of at least
         * receive window time for any packets downlink and display data received onto the terminal.
         */

        configPRINTF( ( "Sending and receiving messages over LoRaWAN.\n" ) );

        uplink[ 0 ] = 0xFF;
        uplinkSize = 1;

        for( ; ; )
        {
            bytesTransferred = prvSend( LORAWAN_APP_PORT, uplink, uplinkSize, LORAWAN_SEND_CONFIRMED_MESSAGES );

            if( bytesTransferred == uplinkSize )
            {
                configPRINTF( ( "Sent uplink message of size %d bytes.\n", bytesTransferred ) );
            }
            else
            {
                /**
                 * Handle partial bytes sent due to data rate restrictions.
                 */
                configPRINTF( ( "Failed to send all the bytes uplink, sent %d bytes.\n", bytesTransferred ) );
            }

            configPRINTF( ( "Wait for %d seconds to receive any downlink data.\n", ( LORAWAN_RECEIVE_TIMEOUT_MS / 1000 ) ) );

            bytesTransferred = prvReceiveFrom( &port, downlink, downlinkSize, LORAWAN_RECEIVE_TIMEOUT_MS );

            if( bytesTransferred > 0 )
            {
                configPRINTF( ( "Received downlink message, port = %d, size = %d bytes:\n", port, bytesTransferred ) );
                PrintHexBuffer( downlink, bytesTransferred );
            }

            /**
             * Wait for atleast dutycyle time +/- a random jitter value before sending the next uplink message.
             */
            dutyCycleInterval = LORAWAN_APP_TX_DUTYCYCLE_MS + randr( -LORAWAN_APP_TX_DUTYCYCLE_RND_MS, LORAWAN_APP_TX_DUTYCYCLE_RND_MS );

            configPRINTF( ( "Wait for duty cycle time (%d seconds) before sending next uplink data.\n", ( dutyCycleInterval / 1000 ) ) );

            vTaskDelay( pdMS_TO_TICKS( dutyCycleInterval ) );
        }
    }

    vTaskDelete( NULL );
}


/*******************************************************************************************
* main
* *****************************************************************************************/
void main( void )
{
    BoardInitMcu();
    xLoRaMacResponseQueue = xQueueCreate( LORAWAN_MAX_QUEUED_RESPONSES, sizeof( LoRaMacResponse_t ) );
    xLoRaMacMessageBuffer = xMessageBufferCreate( ( LORAWAN_MAX_MESG_SIZE + LORAWAN_APP_PORT_SIZE + sizeof( size_t ) ) * 10 );

    if( ( xLoRaMacResponseQueue != NULL ) && ( xLoRaMacMessageBuffer != NULL ) )
    {
        xTaskCreate( prvLoRaMacTask, "LoRaMac", configMINIMAL_STACK_SIZE * 20, NULL, tskIDLE_PRIORITY + 5, &xLoRaMacTask );
        xTaskCreate( prvLorawanClassATask, "LoRaWAN", configMINIMAL_STACK_SIZE * 40, NULL, tskIDLE_PRIORITY + 1, &xLoRaWANTask );
    }
}
