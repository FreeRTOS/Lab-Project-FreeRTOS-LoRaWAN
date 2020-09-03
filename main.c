#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"
#include "LoRaMac.h"
#include "board.h"
#include "utilities.h"
#include "radio.h"
#include "queue.h"
#include "spi.h"
#include "sx126x-board.h"
#include  "board-config.h"

/*
 * @brief Default data rate used for uplink messages.
 * 
 * Data rate for LoRAWAN depends on two factors: spreading factor and bandwidth. Default value is set to DR_0 which is
 * the highest spreading factor and lowest bandwidth for a region. Higher spreading factor lead to better reception
 * and longer range from the gateway. However this also leads to longest air time for radio and lowest data rate
 * for transmission.
 *
 */
#define LORAWAN_DEFAULT_DATARATE           ( DR_0 )

/**
 * @brief Used to turn on/off adapative data rate.
 *
 * Enabling adaptive data rate allows the network to set optimized data rates for end devices
 * thereby optimizing on air time and power consumption. Its recommended to enable adaptive
 * data rate for static devices and devices with stable RF conditions.
 */
#define LORAWAN_ADR_ON                     ( 1 )

/**
 * @brief Maximum payload length defined for the demo.
 *
 * This can be used to cap the maximum packet size that can be transferred anytime by the demo.
 * LoRaWAN payload can vary upto 222 bytes. However applications should take care of duty cycle restrictions and
 * fair access policies for each region while determining the size of a message to be transmitted.
 * Larger messages leads to longer air-time and increased power consumption for the 
 * radio as well as using up all of the duty cycle for a channel.
 */
#define LORAWAN_MAX_PAYLOAD_LENGTH         ( 222 )

/**
 * @brief LoRa MAC layer port used by the application.
 * Downlink unicast messages should be send to this port number.
 */
#define LORAWAN_APP_PORT                   ( 2 )

/**
 * @brief Flag to indicate the underlying stack, if application is using a public network such 
 * as The Things Network.
 */
#define LORAWAN_PUBLIC_NETWORK             ( 1 )

/**
 * @brief Maximum join attempts before giving up.
 * 
 * Retry attempts tries to send join requests in different channels thereby finding a suitable gateway which
 * is tuned to that channel.
 */
#define LORAWAN_MAX_JOIN_ATTEMPTS          ( 1000 )

/**
 * @brief Interval between retry attempts for OTAA join.
 * It waits for a retry interval +- random jitter ( to avoid dos ) before attempting to 
 * join again with LoRaWAN network.
 */
#define LORAWAN_JOIN_RETRY_INTERVAL_MS      ( 2000 )

/**
 * @brief Should send confirmed messages (with an acknowledgment) or not.
 */
#define LORAWAN_SEND_CONFIRMED_MESSAGES    ( 0 )

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
#define LORAWAN_APPLICATION_TX_INTERVAL_SEC        ( 700U )

/**
 * @brief  Maximum time to wait for a downlink packet after sending uplink.
 *
 */
#define LORAWAN_APPLICATION_RX_WAIT_MS          ( 1000 )

/**
 * @brief Defines a random jitter bound in milliseconds for application data transmission duty cycle.
 *
 * This allows devices to space their transmissions slighltly between each other in cases like all devices reboots and tries to
 * join server at same time.
 */
#define LORAWAN_MAX_JITTER_MS    ( 500 )

/**
 * @brief Queue size used by an application task to block on downlink messages.
 * 
 * Class A application sends an uplink and then uses two receive slots for any downlink messages from the server. It does not receive
 * messages from server any other time. Queue size is set to 1 since the application task in demo sends an uplink and waits for any
 * downlink before sending next uplink. Queue size can be adjusted based on application task needs.
 * 
 */
#define LORAWAN_DOWNLINK_QUEUE_SIZE        ( 1 )


/**
 * @breif Response queue size for the demo
 *
 * For class A application at most 3 responses can be enqueued at any time (SRV_MAC_LINK_CHECK_ANS, SRV_MAC_DEVICE_TIME_ANS, CONFIRMED_ACK)
 */
#define LORAWAN_RESPONSE_QUEUE_SIZE        ( 3 ) 


/**
 * @brief Events used for inter task notifications.
 */
typedef enum LoRaWanEvent {
    
    /**
     * These events are used by the Radio and MAC layer to indicate pending data
     * that needs to be processed.
     */
    
    LORAWAN_EVENT_RADIO_PENDING = 0x1,
    LORAWAN_EVENT_MAC_PENDING = 0x2,
    
    /**
     * These are control events generated from LoRaMAC stack. Events are used
     * by the LoRaWAN task to perform control activities for class A applications.
     */
    LORAWAN_EVENT_LORAMAC_STARTED = 0x1,
    LORAWAN_EVENT_LORAMAC_FAILED = 0x2,
    LORAWAN_EVENT_FRAME_LOSS = 0x4,
    LORAWAN_EVENT_SCHEDULE_UPLINK = 0x8,

} LoRaWanEvent_t ;


/**
 * @brief Identifies the type of response sent from the LoRaMAC stack.
 */
typedef enum
{
    LORAWAN_RESPONSE_TYPE_MLME = 0,
    LORAWAN_RESPONSE_TYPE_MCPS
} LoRaWanResponseType_t;

/**
 * @brief Response structure which used to hold a response message from LoRaMAC stack.
 */
typedef struct LoRaWanResponse
{
    LoRaWanResponseType_t type;
    union
    {
        MlmeConfirm_t mlmeConfirm;
        McpsConfirm_t mcpsConfirm;
    } resp;
} LoRaWanResponse_t;

/**
 * @brief Structure used to hold an uplink message.
 * Application task can queue an uplink message any time, however actual transmission occurs 
 * by obeying transmission policies for that region.
 */
typedef struct LoRaWanUplinkMessage
{
    uint8_t isConfirmed;
    uint16_t port;
    uint16_t length;
    uint8_t data[ LORAWAN_MAX_PAYLOAD_LENGTH ];
} LoRaWanUplinkMessage_t;

/**
 * @brief Structure used to hold a downlink message.
 */
typedef struct LoRaWanDowlinkMessage
{
    
    uint16_t port;
    uint16_t length;
    uint8_t data[LORAWAN_MAX_PAYLOAD_LENGTH];
} LoRaWanDowlinkMessage_t;


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


/**
 * @brief This task initializes and runs the underlying LoRaMAC stack.
 
 * The task receives events from LoRa radio interrupts and calls LoRaMAC stack APIs for processing the events.
 * It also receives notifications from LoRaMAC stack which needs processing. Task registers callback
 * handlers with the LoRaMAC stack and uses it to dispatch events and messages to other tasks.
 */
static TaskHandle_t xLoRaMacTask;


/**
 * @brief Task for LoRaWAN class A application.
 *
 * Task sends periodic uplink messages, and also performs control activities like frame loss detection and rejoin procedure,
 * send empty uplink messages if needed to respond back to network server commands etc.
 * 
 * LoRaWAN  task interacts with LoRaMAC task using task notifications and response queue.
 */
static TaskHandle_t xLoRaWanTask;

/**
 * @brief Response Queue for LoRaWAN.
 *
 * The response queue is used to receive responses for queued requests in the same order.
 */
static QueueHandle_t xResponseQueue;

/**
 * @brief Downlink Queue for LoRaWAN.
 *
 * Used by application task to block on messages received downlink.
 */
static QueueHandle_t xDownlinkQueue;


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

static void prvMcpsConfirm( McpsConfirm_t * mcpsConfirm )
{
    LoRaWanResponse_t response;

    configPRINTF( ( "MCPS CONFIRM status: %s\n", EventInfoStatusStrings[ mcpsConfirm->Status ] ) );
    
    response.type = LORAWAN_RESPONSE_TYPE_MCPS;
    memcpy( &response.resp.mcpsConfirm, mcpsConfirm, sizeof( McpsConfirm_t ) );
    
    xQueueSend( xResponseQueue, &response, portMAX_DELAY );
}

static void prvMcpsIndication( McpsIndication_t * mcpsIndication )
{
    
    LoRaWanDowlinkMessage_t downlink;
    
    configPRINTF( ( "MCPS INDICATION status: %s\n", EventInfoStatusStrings[ mcpsIndication->Status ] ) );

    if( ( mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_OK ) &&
        ( mcpsIndication->RxData == true ) )
    {
        configASSERT( mcpsIndication->BufferSize <= LORAWAN_MAX_PAYLOAD_LENGTH );
        downlink.port = mcpsIndication->Port;
        downlink.length = mcpsIndication->BufferSize;
        memcpy( downlink.data, mcpsIndication->Buffer, mcpsIndication->BufferSize );
        
        if( xQueueSend( xDownlinkQueue, &downlink, 1 ) != pdTRUE )
        {
            configPRINTF(( "Failed to send downlink data to the queue.\r\n" ));
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
        xTaskNotifyAndQuery( xLoRaWanTask, LORAWAN_EVENT_SCHEDULE_UPLINK, eSetBits, NULL );
    }
     
    if( mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS )
    {
        xTaskNotifyAndQuery( xLoRaWanTask, LORAWAN_EVENT_FRAME_LOSS, eSetBits, NULL );
    }
}

static void prvMlmeIndication( MlmeIndication_t * MlmeIndication )
{
    LoRaMacEventInfoStatus_t status = MlmeIndication->Status;
    
    configPRINTF( ( "MLME Indication status: %s\n", EventInfoStatusStrings[ status ] ) );
    
    if( status == LORAMAC_EVENT_INFO_STATUS_OK )
    {
        if( MlmeIndication->MlmeIndication == MLME_SCHEDULE_UPLINK )
        {
            xTaskNotifyAndQuery( xLoRaWanTask, LORAWAN_EVENT_SCHEDULE_UPLINK, eSetBits, NULL );
        }
    }        
}

static void prvMlmeConfirm( MlmeConfirm_t * mlmeConfirm )
{
    LoRaMacEventInfoStatus_t status = mlmeConfirm->Status;
    LoRaWanResponse_t response;

    configPRINTF( ( "MLME CONFIRM  status: %s\n", EventInfoStatusStrings[ status ] ) );

    response.type = LORAWAN_RESPONSE_TYPE_MLME;
    memcpy( &response.resp.mlmeConfirm, mlmeConfirm, sizeof( MlmeConfirm_t ) );
    xQueueSend( xResponseQueue, &response, portMAX_DELAY );
}

static void onRadioNotify()
{
    BaseType_t xHigherPriorityTaskWoken;
    
    xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyAndQueryFromISR( xLoRaMacTask, LORAWAN_EVENT_RADIO_PENDING, eSetBits, NULL, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void prvOnMacNotify( void )
{
    xTaskNotifyAndQuery( xLoRaMacTask, LORAWAN_EVENT_MAC_PENDING, eSetBits, NULL );
}

static uint8_t prvGetBatteryLevel( void )
{
    return 0;
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
    LoRaWanResponse_t response;
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
            xQueueReceive( xResponseQueue, &response, portMAX_DELAY );

            if( ( response.type == LORAWAN_RESPONSE_TYPE_MLME ) 
                && ( response.resp.mlmeConfirm.MlmeRequest == MLME_JOIN ) )
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
            ulDutyCycleTimeMS = ( LORAWAN_JOIN_RETRY_INTERVAL_MS ) + 
                                 randr ( -LORAWAN_MAX_JITTER_MS, LORAWAN_MAX_JITTER_MS );
            configPRINTF( ( "Retrying join attempt after %lu seconds.\n", ( ulDutyCycleTimeMS / 1000 ) ) );
            vTaskDelay( pdMS_TO_TICKS( ulDutyCycleTimeMS ) );
        }
    }

    return status;
}

static LoRaMacStatus_t prvSendUplink( LoRaWanUplinkMessage_t * pUplink, uint32_t *pDutyCyleWaitTimeMS )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    LoRaMacStatus_t status;
    uint32_t ulDutyCycleTimeMS = 0;
    LoRaWanResponse_t response;

    status = LoRaMacQueryTxPossible( pUplink->length, &txInfo );

    if( status == LORAMAC_STATUS_OK )
    {
        if( pUplink->isConfirmed == false )
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = pUplink->port;
            mcpsReq.Req.Unconfirmed.fBuffer = pUplink->data;
            mcpsReq.Req.Unconfirmed.fBufferSize = pUplink->length;
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = pUplink->port;
            mcpsReq.Req.Confirmed.fBuffer = pUplink->data;
            mcpsReq.Req.Confirmed.fBufferSize = pUplink->length;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
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
         xQueueReceive( xResponseQueue, &response, portMAX_DELAY );
         if( ( response.type == LORAWAN_RESPONSE_TYPE_MCPS ) && ( response.resp.mcpsConfirm.Status == LORAMAC_EVENT_INFO_STATUS_OK ) )
         {
             if( ( response.resp.mcpsConfirm.McpsRequest == MCPS_UNCONFIRMED ) ||
                    ( ( response.resp.mcpsConfirm.McpsRequest == MCPS_CONFIRMED ) && ( response.resp.mcpsConfirm.AckReceived ) ) )
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
    
    *pDutyCyleWaitTimeMS = ulDutyCycleTimeMS;


    return status;
}

static void prvLoRaMacTask( void * params )
{
    uint32_t ulNotifiedValue;
    LoRaMacStatus_t xStatus;
    LoRaMacPrimitives_t xLoRaMacPrimitives = { 0 };
    LoRaMacCallback_t xLoRaMacCallbacks = { 0 };
   
    xLoRaMacPrimitives.MacMcpsConfirm = prvMcpsConfirm;
    xLoRaMacPrimitives.MacMcpsIndication = prvMcpsIndication;
    xLoRaMacPrimitives.MacMlmeConfirm = prvMlmeConfirm;
    xLoRaMacPrimitives.MacMlmeIndication = prvMlmeIndication;
    xLoRaMacCallbacks.GetBatteryLevel = prvGetBatteryLevel;
    xLoRaMacCallbacks.MacProcessNotify = prvOnMacNotify;


    xStatus = LoRaMacInitialization( &xLoRaMacPrimitives, &xLoRaMacCallbacks, LORAMAC_REGION_US915 );

    if( xStatus == LORAMAC_STATUS_OK )
    {
        Radio.SetEventNotify( &onRadioNotify );
        configPRINTF(( "LoRa MAC stack initialized.\r\n" ));
    }
    else
    {
        configPRINTF(( "LoRa MAC initialization failed, status = %d.\r\n", xStatus ));
    }

    if( xStatus == LORAMAC_STATUS_OK )
    {
        xStatus = prvConfigure();

        if( xStatus != LORAMAC_STATUS_OK )
        {
            configPRINTF( ( "LoRa MAC configuration failed, status = %d.\n", xStatus ) );
        }
    }

    if( xStatus == LORAMAC_STATUS_OK )
    {
        xStatus = LoRaMacStart();

        if( xStatus != LORAMAC_STATUS_OK )
        {
            configPRINTF(( "LoRa MAC start failed, status = %d.\n", xStatus ));
        }
    }
    
    if( xStatus == LORAMAC_STATUS_OK )
    {
        
        xTaskNotifyAndQuery( xLoRaWanTask, LORAWAN_EVENT_LORAMAC_STARTED, eSetBits, NULL );

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
            }
            
            if( ulNotifiedValue & LORAWAN_EVENT_MAC_PENDING )
            {
                /*Process Lora mac events based on Radio events. */
                LoRaMacProcess();
            }
        }
    }
    else
    {
        xTaskNotifyAndQuery( xLoRaWanTask, LORAWAN_EVENT_LORAMAC_FAILED, eSetBits, NULL );
    }

    vTaskDelete( NULL );
}

static void prvLorawanClassATask( void * params )
{
    LoRaMacStatus_t status;
    MibRequestConfirm_t mibReq;
    uint32_t ulDutyCycleWaitTimeMs;
    uint32_t ulNotifiedValue;
    uint32_t ulTxIntervalMs;
    LoRaWanUplinkMessage_t uplink;
    LoRaWanDowlinkMessage_t downlink;
    LoRaWanResponse_t response;
    
    configASSERT( xTaskCreate( prvLoRaMacTask, "LoRaMac", configMINIMAL_STACK_SIZE * 40, NULL, tskIDLE_PRIORITY + 1, &xLoRaMacTask ) == pdTRUE );
    
    /**
     * Wait for LoRaMAC stack to be runnnig.
     */
     
    xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY );
    
    if( ulNotifiedValue & LORAWAN_EVENT_LORAMAC_STARTED )
    {
        
        /**
         * LoRaMAC stack started. Initiate the join procedure if not already joined.
         */
         
        configPRINTF(( "Initiating OTAA join procedure.\r\n" ));
         
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

        if( status == LORAMAC_STATUS_OK )
        {
            
            /**
             * Successfully joined a LoRaWAN network. Now the  task runs in an infinite loop,
             * sends periodic uplink message of 1 byte by obeying fair access policy for the LoRaWAN network.
             * If the MAC has indicated to schedule an uplink message as soon as possible, then it sends
             * an uplink message immediately after the duty cycle wait time. After each uplink it also waits
             * on downlink queue for any messages from the network server.
             */
             
            uplink.port = LORAWAN_APP_PORT;
            uplink.isConfirmed = LORAWAN_SEND_CONFIRMED_MESSAGES;
            uplink.length = 1;
            uplink.data[0] = 0xFF;
        
            for( ;; )
            {
                
                status = prvSendUplink( &uplink, &ulDutyCycleWaitTimeMs );
                    
                if( status == LORAMAC_STATUS_OK )
                {
                    configPRINTF(( "Sent an uplink message successfully. Waiting for any downlink packets for %d ms.\r\n", LORAWAN_APPLICATION_RX_WAIT_MS ));
                    
                    /* Check for any downlink messages on the queue. */
                    if( xQueueReceive( xDownlinkQueue, &downlink, pdMS_TO_TICKS( LORAWAN_APPLICATION_RX_WAIT_MS ) ) == pdTRUE )
                    {
                        configPRINTF(( "Received downlink message on port %d:\r\n", downlink.port ));
                        prvPrintHexBuffer ( downlink.data, downlink.length );
                    }
                    else
                    {
                        configPRINTF(( "No messages downlink.\r\n" ));
                    }
                    
                    /**
                     * Poll for any events generated from MAC layer withoug blocking. 
                     */
                    xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue, 1 );
                  
                    if( ulNotifiedValue & LORAWAN_EVENT_SCHEDULE_UPLINK )
                    {
                        /**
                         * MAC layer indicated there are pending acknowledgments to be sent
                         * uplink as soon as possible. Wait for duty cycle time and send an uplink.
                         */
                        configPRINTF(( "Sending an empty uplink to flush responses to MAC commands from server.\r\n" ));
                        
                        uplink.isConfirmed = false;
                        uplink.length = 0;

                        if( ulDutyCycleWaitTimeMs > 0 )
                        {
                            vTaskDelay( pdMS_TO_TICKS( ulDutyCycleWaitTimeMs ) );
                        }
                    }
                    else if( ulNotifiedValue & LORAWAN_EVENT_FRAME_LOSS )
                    {
                        /**
                         *  If LoRaMAC stack reports a too many frame loss event, it indicates that gateway and device frame counter
                         *  values are not in sync. The only way to recover from this is to initiate a rejoin procedure to reset 
                         *  the frame counter at both sides.
                         */
                        configPRINTF(( "Too many frame loss detected. Rejoining to reset the session counters" ));
                        status = prvOTAAJoinNetwork();
                        if( status != LORAMAC_STATUS_OK )
                        {   
                            configPRINTF( ( "Failed to join LoRaWAN network.\r\n" ));
                            break;
                        }
                        else
                        {
                            configPRINTF(( "Rejoined LoRaWAN network.\r\n" ));
                        }
                    }
                    else
                    {
                        /**
                         * Frame was sent successfully. Wait for next TX schedule to send uplink thereby obeying fair 
                         * access policy.
                         */
                         
                        uplink.isConfirmed = LORAWAN_SEND_CONFIRMED_MESSAGES;
                        uplink.length = 1;
                        uplink.data[0] = 0xFF;
                        
                        ulTxIntervalMs = ( LORAWAN_APPLICATION_TX_INTERVAL_SEC * 1000 ) + randr( -LORAWAN_MAX_JITTER_MS, LORAWAN_MAX_JITTER_MS );
                        
                        configPRINTF(( "TX-RX cycle complete. Next TX in %u seconds\r\n", ( ulTxIntervalMs / 1000 ) )); 
                        
                        vTaskDelay( pdMS_TO_TICKS( ulTxIntervalMs ) );
                         
                    }
                   
                }
               
            }
        }
    }
    else
    {
        configPRINTF(( "Failed to start LoRaMAC stack\r\n" ));
    }

    vTaskDelete( NULL );
}  



/*******************************************************************************************
* main
* *****************************************************************************************/
void main( void )
{
    SpiInit(&SX126x.Spi, SPI_1, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
    SX126xIoInit();
    //RtcInit();
    //EepromMcuInit();

    configPRINTF( ( "###### ===== Class A LoRaWAN application ==== ######\n\n" ) );
    
    xResponseQueue = xQueueCreate( LORAWAN_RESPONSE_QUEUE_SIZE, sizeof( LoRaWanResponse_t ) );
    xDownlinkQueue = xQueueCreate( LORAWAN_DOWNLINK_QUEUE_SIZE, sizeof( LoRaWanDowlinkMessage_t ) );
    
    configASSERT( xResponseQueue != NULL );    
    configASSERT( xDownlinkQueue != NULL );
    
    xTaskCreate( prvLorawanClassATask, "LoRaWanClassA", configMINIMAL_STACK_SIZE * 20, NULL, tskIDLE_PRIORITY, &xLoRaWanTask );
    
}
