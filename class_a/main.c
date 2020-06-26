#include "FreeRTOS.h"
#include "task.h"
#include "timer.h"
#include "LoRaMac.h"
#include "semphr.h"

/*
 * @brief Default data rate used for uplink messages.
 */
#define LORAWAN_DEFAULT_DATARATE                    DR_0

/**
 * @brief Can be used to enable/disable adaptive data rate and
 * frequency hopping.
 */
#define LORAWAN_ADR_ON                              1

/**
 * @brief Defines maximum message size for transmission.
 * Applications should take care of duty cycle restrictions and fair access policies for
 * each regions while determining the size of message to be transmitted. Larger messages can
 * lead to longer air-time for radio, and this can lead to radio using up all the duty cycle limit.
 */
#define LORAWAN_MAX_MESG_SIZE                     242

/**
 * @brief LoRa MAC layer port used for communication. Downlink messages
 * should be send on this port number.
 */
#define LORAWAN_APP_PORT                            2

/**
 * @brief Flag for LoRaMAC stack to tell if its a public network such as TTN.
 */
#define LORAWAN_PUBLIC_NETWORK                      1

/**
 * @brief Maximum join attempts before giving up. LoRa tries a range of channels
 * within the band to send join requests for the first time.
 */
#define LORAWAN_MAX_JOIN_ATTEMPTS                    ( 1000 )

/**
 * @brief Should send confirmed messages (with acknowledgment) or not.
 */
#define LORAWAN_SEND_CONFIRMED_MESSAGES                 ( 1 )

/**
 * @brief Defines the application data transmission duty cycle time in milliseconds.
 */
#define LORAWAN_APP_TX_DUTYCYCLE_MS                        ( 5000 )

/**
 * @brief Defines a random delay for application data transmission duty cycle in milliseconds.
 */
#define LORAWAN_APP_TX_DUTYCYCLE_RND_MS                     ( 1000 )

/**
 * @brief EUI and keys that needs to be provisioned for device identity and security.
 */

#define DEV_EUI       { 0x32, 0x38, 0x33, 0x35, 0x60, 0x38, 0x71, 0x01 };
#define JOIN_EUI      { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x02, 0xD1, 0xD4 };
#define APP_NWK_KEY   { 0xF5, 0x42, 0x96, 0x98, 0x8B, 0xC2, 0x23, 0x86, 0x56, 0x24, 0x1D, 0x73, 0x0A, 0xFA, 0x95, 0x0B };
/**
 * @biref Defines types of join methods.
 */
typedef enum
{
    LORAWAN_JOIN_MODE_OTAA = 0,
    LORAWAN_JOIN_MODE_ABP
} LoraWanJoinMode_t;


/*!
 * Device states used to control the LoRaWAN task.
 */
typedef enum eDeviceState
{
    DEVICE_STATE_RESTORE,
    DEVICE_STATE_START,
    DEVICE_STATE_JOIN,
    DEVICE_STATE_SEND,
    DEVICE_STATE_CYCLE,
    DEVICE_STATE_SLEEP
}DeviceState_t;

/**
 * @brief Strings for denoting status responses from LoRaMAC layer.
 */
static const char* MacStatusStrings[] =
{
        "OK",                            // LORAMAC_STATUS_OK
        "Busy",                          // LORAMAC_STATUS_BUSY
        "Service unknown",               // LORAMAC_STATUS_SERVICE_UNKNOWN
        "Parameter invalid",             // LORAMAC_STATUS_PARAMETER_INVALID
        "Frequency invalid",             // LORAMAC_STATUS_FREQUENCY_INVALID
        "Datarate invalid",              // LORAMAC_STATUS_DATARATE_INVALID
        "Frequency or datarate invalid", // LORAMAC_STATUS_FREQ_AND_DR_INVALID
        "No network joined",             // LORAMAC_STATUS_NO_NETWORK_JOINED
        "Length error",                  // LORAMAC_STATUS_LENGTH_ERROR
        "Region not supported",          // LORAMAC_STATUS_REGION_NOT_SUPPORTED
        "Skipped APP data",              // LORAMAC_STATUS_SKIPPED_APP_DATA
        "Duty-cycle restricted",         // LORAMAC_STATUS_DUTYCYCLE_RESTRICTED
        "No channel found",              // LORAMAC_STATUS_NO_CHANNEL_FOUND
        "No free channel found",         // LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND
        "Busy beacon reserved time",     // LORAMAC_STATUS_BUSY_BEACON_RESERVED_TIME
        "Busy ping-slot window time",    // LORAMAC_STATUS_BUSY_PING_SLOT_WINDOW_TIME
        "Busy uplink collision",         // LORAMAC_STATUS_BUSY_UPLINK_COLLISION
        "Crypto error",                  // LORAMAC_STATUS_CRYPTO_ERROR
        "FCnt handler error",            // LORAMAC_STATUS_FCNT_HANDLER_ERROR
        "MAC command error",             // LORAMAC_STATUS_MAC_COMMAD_ERROR
        "ClassB error",                  // LORAMAC_STATUS_CLASS_B_ERROR
        "Confirm queue error",           // LORAMAC_STATUS_CONFIRM_QUEUE_ERROR
        "Multicast group undefined",     // LORAMAC_STATUS_MC_GROUP_UNDEFINED
        "Unknown error",                 // LORAMAC_STATUS_ERROR
};

/**
 * @brief Strings for denoting events from LoRaMAC layer.
 */
static const char* EventInfoStatusStrings[] =
{ 
        "OK",                            // LORAMAC_EVENT_INFO_STATUS_OK
        "Error",                         // LORAMAC_EVENT_INFO_STATUS_ERROR
        "Tx timeout",                    // LORAMAC_EVENT_INFO_STATUS_TX_TIMEOUT
        "Rx 1 timeout",                  // LORAMAC_EVENT_INFO_STATUS_RX1_TIMEOUT
        "Rx 2 timeout",                  // LORAMAC_EVENT_INFO_STATUS_RX2_TIMEOUT
        "Rx1 error",                     // LORAMAC_EVENT_INFO_STATUS_RX1_ERROR
        "Rx2 error",                     // LORAMAC_EVENT_INFO_STATUS_RX2_ERROR
        "Join failed",                   // LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL
        "Downlink repeated",             // LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED
        "Tx DR payload size error",      // LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR
        "Downlink too many frames loss", // LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS
        "Address fail",                  // LORAMAC_EVENT_INFO_STATUS_ADDRESS_FAIL
        "MIC fail",                      // LORAMAC_EVENT_INFO_STATUS_MIC_FAIL
        "Multicast fail",                // LORAMAC_EVENT_INFO_STATUS_MULTICAST_FAIL
        "Beacon locked",                 // LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED
        "Beacon lost",                   // LORAMAC_EVENT_INFO_STATUS_BEACON_LOST
        "Beacon not found"               // LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND
};

/**
 * @brief Device state to LoRaWAN operations.
 */
static DeviceState_t DeviceState;


static LoRaMacPrimitives_t LoRaMacPrimitives;
static LoRaMacCallback_t LoRaMacCallbacks;

/**
 * @brief Main loRaWAN task handle.
 */
static TaskHandle_t loRaWANTask;

static uint8_t message[LORAWAN_MAX_MESG_SIZE];

/**
 * @brief Semaphore used to wake up lorawan task of interrupt wakeupLoRaTasks
 * or status/events from LoRaMAC stack.
 */
static SemaphoreHandle_t wakeupLoRaTask;

/**
 * @brief Packet timer used for controlled transmission of packets, thereby
 * confirming to duty cycle restrictions.
 */
static TimerEvent_t packetTimer;


/**
 * @brief Initiates a join network.
 */
static LoRaMacStatus_t prxJoinNetwork( void );

static LoRaMacStatus_t prxJoinNetwork( void )
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq;
    mlmeReq.Type = MLME_JOIN;
    mlmeReq.Req.Join.Datarate = LORAWAN_DEFAULT_DATARATE;

    // Starts the join procedure
    status = LoRaMacMlmeRequest( &mlmeReq );

    if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
    {
        configPRINTF(( "Next Tx in  : ~%lu second(s)\n", ( mlmeReq.ReqReturn.DutyCycleWaitTime / 1000 ) ));
    }

    return status;
}

/*
 * @brief Timer callback for backoff to obey duty cycle restrictions.
 */
static void OnPacketTimerEvent( void* context )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    TimerStop( &packetTimer );

    mibReq.Type = MIB_NETWORK_ACTIVATION;
    status = LoRaMacMibGetRequestConfirm( &mibReq );

    if( status == LORAMAC_STATUS_OK )
    {
        if( mibReq.Param.NetworkActivation == ACTIVATION_TYPE_NONE )
        {
            /**
             * Network not joined yet. Try to join again.
             */
            DeviceState = DEVICE_STATE_JOIN;
        }
        else
        {
            /**
             * Device already joined LoRaWAN network. Start
             * sending data.
             */
            DeviceState = DEVICE_STATE_SEND;
        }
    }

    xSemaphoreGive( wakeupLoRaTask );
}

static void McpsConfirm( McpsConfirm_t *mcpsConfirm )
{
    configPRINTF(( "\n MCPS CONFIRM status: %s\n", EventInfoStatusStrings[mcpsConfirm->Status] ));
    if( mcpsConfirm->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
        DeviceState = DEVICE_STATE_SLEEP;
    }
    else
    {
        switch( mcpsConfirm->McpsRequest )
        {
        case MCPS_UNCONFIRMED:
        {
            // Check Datarate
            // Check TxPower
            configPRINTF(("Unconfirmed message has been sent out.\n"));
            DeviceState = DEVICE_STATE_CYCLE;
            break;
        }
        case MCPS_CONFIRMED:
        {
            // Check Datarate
            // Check TxPower
            // Check AckReceived
            // Check NbTrials
            if( mcpsConfirm->AckReceived == true )
            {
                configPRINTF(("ACK received for confirmed message.\n"));
                DeviceState = DEVICE_STATE_CYCLE;
            }
            else
            {
                configPRINTF(("NACK received for confirmed message.\n"));
                DeviceState = DEVICE_STATE_SLEEP;
            }
            break;
        }
        case MCPS_PROPRIETARY:
        {
            break;
        }
        default:
            break;
        }

    }
}

/*!
 * Prints the provided buffer in HEX
 * 
 * \param buffer Buffer to be printed
 * \param size   Buffer size to be printed
 */
static void PrintHexBuffer( uint8_t *buffer, uint8_t size )
{
    uint8_t newline = 0;

    for( uint8_t i = 0; i < size; i++ )
    {
        if( newline != 0 )
        {
            configPRINTF(( "\n" ));
            newline = 0;
        }

        configPRINTF(( "%02X ", buffer[i] ));

        if( ( ( i + 1 ) % 16 ) == 0 )
        {
            newline = 1;
        }
    }
    configPRINTF(( "\n" ));
}


static void McpsIndication( McpsIndication_t *mcpsIndication )
{
    configPRINTF(( "\nMCPS INDICATION status: %s\n", EventInfoStatusStrings[mcpsIndication->Status] ));
    if( mcpsIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
        return;
    }

    switch( mcpsIndication->McpsIndication )
    {
    case MCPS_UNCONFIRMED:
    {
        break;
    }
    case MCPS_CONFIRMED:
    {
        break;
    }
    case MCPS_PROPRIETARY:
    {
        break;
    }
    case MCPS_MULTICAST:
    {
        break;
    }
    default:
        break;
    }

    // Check Multicast
    // Check Port
    // Check Datarate
    // Check FramePending
    if( mcpsIndication->FramePending == true )
    {
        // The server signals that it has pending data to be sent.
        // We schedule an uplink as soon as possible to flush the server.
        OnPacketTimerEvent( NULL );
    }
    // Check Buffer
    // Check BufferSize
    // Check Rssi
    // Check Snr
    // Check RxSlot
    if( mcpsIndication->RxData == true )
    {
        configPRINTF(( "Downlink data received: port: %d, slot: %d, data_rate:%d, rssi: %d, snr:%d data:\n",
                mcpsIndication->Port,
                mcpsIndication->RxSlot,
                mcpsIndication->RxDatarate,
                mcpsIndication->Rssi,
                mcpsIndication->Snr );
        PrintHexBuffer( mcpsIndication->Buffer, mcpsIndication->BufferSize ));
    }
}


static void MlmeIndication ( MlmeIndication_t* MlmeIndication )
{
    // Implementation for MLME indication
}


static void MlmeConfirm( MlmeConfirm_t *mlmeConfirm )
{
    LoRaMacEventInfoStatus_t status = mlmeConfirm->Status;
    MibRequestConfirm_t mibGet;
    static int nJoinAttempts = LORAWAN_MAX_JOIN_ATTEMPTS;

    configPRINTF(("MLME CONFIRM  status: %s\n", EventInfoStatusStrings[status]));

    switch( mlmeConfirm->MlmeRequest )
    {
    case MLME_JOIN:
    {

        if( status == LORAMAC_EVENT_INFO_STATUS_OK )
        {
            configPRINTF(("OTAA JOIN Successful\n\n"));

            mibGet.Type = MIB_DEV_ADDR;
            LoRaMacMibGetRequestConfirm( &mibGet );
            configPRINTF(( "DevAddr : %08lX\n", mibGet.Param.DevAddr ));

            mibGet.Type = MIB_CHANNELS_DATARATE;
            LoRaMacMibGetRequestConfirm( &mibGet );
            configPRINTF(( "DATA RATE   : DR_%d\n", mibGet.Param.ChannelsDatarate ));

            DeviceState = DEVICE_STATE_SEND;
            xSemaphoreGive( wakeupLoRaTask );
        }
        else
        {
            if( nJoinAttempts > 0 )
            {
                configPRINTF(("Join Failed, max join attempts left: %d\n", nJoinAttempts ));
                nJoinAttempts--;
                DeviceState = DEVICE_STATE_CYCLE;
                xSemaphoreGive( wakeupLoRaTask );
            }
            else
            {
                configPRINTF(( "Join Failed, no attempts left..\n" ));
                DeviceState = DEVICE_STATE_SLEEP;
            }
        }
        break;
    }

    default:
        break;
    }
}

static void OnMacProcessNotify( void )
{
    xSemaphoreGiveFromISR(wakeupLoRaTask, NULL);
}
uint8_t  getBatteryLevel( void )
{
    return 0;
}

static LoRaMacStatus_t configure( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;
    uint8_t devEUI[8] = DEV_EUI;
    uint8_t joinEUI[8] = JOIN_EUI;
    uint8_t appKey[16] = APP_NWK_KEY;

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
        mibReq.Param.SystemMaxRxError = 20;
        status = LoRaMacMibSetRequestConfirm( &mibReq );
    }

    return status;
}


/*!
 * \brief   Prepares the payload of the frame
 *
 * \retval  [1: frame could be send, 0: error]
 */
static LoRaMacStatus_t sendFrame( bool confirmed )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    uint32_t appDataSize;
    LoRaMacStatus_t status;

    message[0] = 0xFF;
    appDataSize = 1;

    if( LoRaMacQueryTxPossible( appDataSize, &txInfo ) != LORAMAC_STATUS_OK )
    {
        // Send empty frame in order to flush MAC commands
        configPRINTF(("TX not possible for data size.\n"));
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
    }
    else
    {
        if( confirmed == false )
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = LORAWAN_APP_PORT;
            mcpsReq.Req.Unconfirmed.fBuffer = message;
            mcpsReq.Req.Unconfirmed.fBufferSize = appDataSize;
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = LORAWAN_APP_PORT;
            mcpsReq.Req.Confirmed.fBuffer = message;
            mcpsReq.Req.Confirmed.fBufferSize = appDataSize;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
    }

    status = LoRaMacMcpsRequest( &mcpsReq );

    configPRINTF(( "MCPS-Request status : %s\n", MacStatusStrings[status] ));

    if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
    {
        configPRINTF(( "TX: Duty cycle limit reached.\n"));
    }

    return status;
}

static void prvLorawanClassATask( void *params )
{
    LoRaMacStatus_t status;
    uint32_t dutyCycleTime = 0;
    MibRequestConfirm_t mibReq;

    LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
    LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
    LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
    LoRaMacPrimitives.MacMlmeIndication = MlmeIndication;
    LoRaMacCallbacks.GetBatteryLevel = getBatteryLevel;
    LoRaMacCallbacks.MacProcessNotify = OnMacProcessNotify;

    configPRINTF(( "###### ===== Class A demo application v1.0.0 ==== ######\n\n" ));

    status = LoRaMacInitialization( &LoRaMacPrimitives, &LoRaMacCallbacks, LORAMAC_REGION_US915);
    if( status == LORAMAC_STATUS_OK )
    {
        configPRINTF(("Lora MAC initialization successful.\r\n"));
    }
    else
    {
        configPRINTF(("Lora MAC initialization failed, status = %d.\r\n", status));
    }

    if( status == LORAMAC_STATUS_OK )
    {
        status = configure();
    }

    if( status == LORAMAC_STATUS_OK )
    {
        configPRINTF(( "LoRaMAC configuration succeeded.\n" ));
    }
    else
    {
        configPRINTF(( "LoraMAC configuration failed, status = %d.\n", status ));
    }

    if( status == LORAMAC_STATUS_OK )
    {
        status =  LoRaMacStart( );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        configPRINTF(( "LoraMAC start successful. \r\n" ));
    }
    else
    {
        configPRINTF(( "LoRaMAC start failed, status = %d.\n", status ));
    }

    if( status == LORAMAC_STATUS_OK )
    {
        TimerInit( &packetTimer, OnPacketTimerEvent );

        mibReq.Type = MIB_NETWORK_ACTIVATION;
        status = LoRaMacMibGetRequestConfirm( &mibReq );
        if( status == LORAMAC_STATUS_OK )
        {
            if( mibReq.Param.NetworkActivation == ACTIVATION_TYPE_NONE )
            {
                configPRINTF(("Initiating join. \r\n"));
                DeviceState = DEVICE_STATE_JOIN;
            }
            else
            {
                configPRINTF(("Already joined.Sending data \r\n"));
                DeviceState = DEVICE_STATE_SEND;
            }
        }
        else
        {
            configPRINTF(("Error while fetching activation, status = %d.", status ));
        }
    }


    if( status == LORAMAC_STATUS_OK )
    {

        for( ;; )
        {
            xSemaphoreTake(wakeupLoRaTask, portMAX_DELAY);
            // Process Radio IRQ.
            if( Radio.IrqProcess != NULL )
            {
                Radio.IrqProcess( );
            }

            //Process Lora mac events.
            LoRaMacProcess( );

            switch( DeviceState )
            {

            case DEVICE_STATE_JOIN:
                status = prxJoinNetwork();
                if( status == LORAMAC_STATUS_OK )
                {
                    configPRINTF(("Join Initiated.\n"));
                    DeviceState = DEVICE_STATE_SLEEP;
                }
                else
                {
                    if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
                    {
                        DeviceState = DEVICE_STATE_CYCLE;
                    }
                }

                break;
            case DEVICE_STATE_SEND:
                status = sendFrame( LORAWAN_SEND_CONFIRMED_MESSAGES );
                if( status == LORAMAC_STATUS_OK )
                {
                    DeviceState = DEVICE_STATE_SLEEP;
                }
                else
                {
                    if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
                    {
                        DeviceState = DEVICE_STATE_CYCLE;
                    }
                }

                break;
            case DEVICE_STATE_CYCLE:
                // Schedule next packet transmission
                DeviceState = DEVICE_STATE_SLEEP;
                dutyCycleTime = LORAWAN_APP_TX_DUTYCYCLE_MS + randr( -LORAWAN_APP_TX_DUTYCYCLE_RND_MS, LORAWAN_APP_TX_DUTYCYCLE_RND_MS );
                // Schedule next packet transmission
                TimerSetValue( &packetTimer, dutyCycleTime );
                TimerStart( &packetTimer );
                break;
            default:
                break;
            }

        }
    }

    vTaskDelete(NULL);
}


/*******************************************************************************************
 * main
 * *****************************************************************************************/ 
void main( void )
{
    wakeupLoRaTask = xSemaphoreCreateBinary();
    if( wakeupLoRaTask != NULL )
    {
        xSemaphoreGive( wakeupLoRaTask );
        xTaskCreate( prvLorawanClassATask, "LoRa", configMINIMAL_STACK_SIZE * 40 , NULL, tskIDLE_PRIORITY + 1, &loRaWANTask );
    }
}
