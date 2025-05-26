/* SPDX-License-Identifier: MIT */
/* 
 *  Main module for communicating between microcontroller and cloud.
 * */

#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "esp_sntp.h"
#include "app_config.h"

/* Azure Provisioning/IoT Hub library includes */
#include "azure_sample_connection.h"
#include "azure_iot_hub_client.h"
#include "azure_iot_provisioning_client.h"
#include "backoff_algorithm.h"
#include "transport_tls_socket.h"
#include "azure_sample_crypto.h"
#include "hc_sr04.h"

/* Compile time error for undefined configs. */

#if !defined( appconfigHOSTNAME ) && !defined( appconfigENABLE_DPS_SAMPLE )
    #error "Define the config appconfigHOSTNAME by following the instructions in file app_config.h."
#endif

#if !defined( appconfigENDPOINT ) && defined( appconfigENABLE_DPS_SAMPLE )
    #error "Define the config dps endpoint by following the instructions in file app_config.h."
#endif

#ifndef appconfigROOT_CA_PEM
    #error "Please define Root CA certificate of the IoT Hub(appconfigROOT_CA_PEM) in app_config.h."
#endif

#if defined( appconfigDEVICE_SYMMETRIC_KEY ) && defined( appconfigCLIENT_CERTIFICATE_PEM )
    #error "Please define only one auth appconfigDEVICE_SYMMETRIC_KEY or appconfigCLIENT_CERTIFICATE_PEM in app_config.h."
#endif

#if !defined( appconfigDEVICE_SYMMETRIC_KEY ) && !defined( appconfigCLIENT_CERTIFICATE_PEM )
    #error "Please define one auth appconfigDEVICE_SYMMETRIC_KEY or appconfigCLIENT_CERTIFICATE_PEM in app_config.h."
#endif

/*-----------------------------------------------------------*/

/**
 * @brief The maximum number of retries for network operation with server.
 */
#define RETRY_MAX_ATTEMPTS                      ( 5U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define RETRY_MAX_BACKOFF_DELAY_MS              ( 5000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define RETRY_BACKOFF_BASE_MS                   ( 500U )

/**
 * @brief Timeout for receiving CONNACK packet in milliseconds.
 */
#define CONNACK_RECV_TIMEOUT_MS                 ( 10000U )

/**
 * @brief  The content type of the Telemetry message published in this example.
 * @remark Message properties must be url-encoded.
 *         This message property is not required to send telemetry.
 */
#define MESSAGE_CONTENT_TYPE                    "application%2Fjson"

/**
 * @brief  The content encoding of the Telemetry message published in this example.
 * @remark Message properties must be url-encoded.
 *         This message property is not required to send telemetry.
 */
#define MESSAGE_CONTENT_ENCODING                "us-ascii"

/**
 * @brief Time in ticks to wait between each cycle of the main loop.
 */
#define DELAY_BETWEEN_ITERATIONS_TICKS     ( pdMS_TO_TICKS( 10 * 1000U ) )

/**
 * @brief Timeout for MQTT_ProcessLoop in milliseconds.
 */
#define PROCESS_LOOP_TIMEOUT_MS                 ( 500U )

/**
 * @brief Delay (in ticks) between consecutive cycles of MQTT publish operations in a
 * app iteration.
 *
 * Note that the process loop also has a timeout, so the total time between
 * publishes is the sum of the two delays.
 */
#define DELAY_BETWEEN_PUBLISHES_TICKS           ( pdMS_TO_TICKS( 2000U ) )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define TRANSPORT_SEND_RECV_TIMEOUT_MS          ( 2000U )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define Provisioning_Registration_TIMEOUT_MS    ( 3 * 1000U )

/**
 * @brief Wait timeout for subscribe to finish.
 */
#define SUBSCRIBE_TIMEOUT                       ( 10000U )
/*-----------------------------------------------------------*/

static uint8_t ucPropertyBuffer[ 80 ];
static uint8_t ucScratchBuffer[ 128 ];
struct NetworkContext
{
    void * pParams;
};

static AzureIoTHubClient_t xAzureIoTHubClient;

/**
 * @brief Static buffer used to hold MQTT messages being sent and received.
 */
static uint8_t ucMQTTMessageBuffer[ appconfigNETWORK_BUFFER_SIZE ];

/*-----------------------------------------------------------*/

/**
 * @brief Unix time.
 *
 * @return Time in milliseconds.
 */
uint64_t ullGetUnixTime( void );
uint64_t ullGetUnixTime( void ) {
    time_t now = time( NULL );

    if( now == ( time_t ) ( -1 ) ) {
        LogInfo( ( "Failed obtaining current time.\r\n") );
    }

    return now;
}

/* 
 *  @brief Contains the current state of this IoT device.
 * */
tAppState T_APPS = { UINT32_MAX, .bParkingLotIsFree = true };

/*-----------------------------------------------------------*/


/**
 * @brief Cloud message callback handler
 */
static void prvHandleCloudMessage( 
    AzureIoTHubClientCloudToDeviceMessageRequest_t * pxMessage,
    void * pvContext 
) {
    ( void ) pvContext;

    LogInfo( ( "Cloud message payload : %.*s \r\n",
               ( int ) pxMessage->ulPayloadLength,
               ( const char * ) pxMessage->pvMessagePayload ) );
}
/*-----------------------------------------------------------*/

/**
 * @brief Command message callback handler
 */
static void prvHandleCommand( 
    AzureIoTHubClientCommandRequest_t * pxMessage,
    void * pvContext 
) {
    LogInfo( ( "Command payload : %.*s \r\n",
               ( int ) pxMessage->ulPayloadLength,
               ( const char * ) pxMessage->pvMessagePayload ) );

    AzureIoTHubClient_t * xHandle = ( AzureIoTHubClient_t * ) pvContext;

    if( AzureIoTHubClient_SendCommandResponse( xHandle, pxMessage, 200,
                                               NULL, 0 ) != eAzureIoTSuccess )
    {
        LogInfo( ( "Error sending command response\r\n" ) );
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief Property mesage callback handler
 */
static void prvHandlePropertiesMessage( AzureIoTHubClientPropertiesResponse_t * pxMessage,
                                        void * pvContext )
{
    ( void ) pvContext;

    switch( pxMessage->xMessageType )
    {
        case eAzureIoTHubPropertiesRequestedMessage:
            LogInfo( ( "Device property document GET received" ) );
            break;

        case eAzureIoTHubPropertiesReportedResponseMessage:
            LogInfo( ( "Device property reported property response received" ) );
            break;

        case eAzureIoTHubPropertiesWritablePropertyMessage:
            LogInfo( ( "Device property desired property received" ) );
            break;

        default:
            LogError( ( "Unknown property message" ) );
    }

    LogInfo( ( "Property document payload : %.*s \r\n",
               ( int ) pxMessage->ulPayloadLength,
               ( const char * ) pxMessage->pvMessagePayload ) );
}
/*-----------------------------------------------------------*/

/**
 * @brief Setup transport credentials.
 */
static uint32_t prvSetupNetworkCredentials( NetworkCredentials_t * pxNetworkCredentials )
{
    pxNetworkCredentials->xDisableSni = pdFALSE;
    /* Set the credentials for establishing a TLS connection. */
    pxNetworkCredentials->pucRootCa = ( const unsigned char * ) appconfigROOT_CA_PEM;
    pxNetworkCredentials->xRootCaSize = sizeof( appconfigROOT_CA_PEM );
    #ifdef CLIENT_CERTIFICATE_PEM
        pxNetworkCredentials->pucClientCert = ( const unsigned char * ) appconfigCLIENT_CERTIFICATE_PEM;
        pxNetworkCredentials->xClientCertSize = sizeof( appconfigCLIENT_CERTIFICATE_PEM );
        pxNetworkCredentials->pucPrivateKey = ( const unsigned char * ) appconfigCLIENT_PRIVATE_KEY_PEM;
        pxNetworkCredentials->xPrivateKeySize = sizeof( appconfigCLIENT_PRIVATE_KEY_PEM );
    #endif

    return 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief Connect to server with backoff retries.
 */
static uint32_t prvConnectToServerWithBackoffRetries( const char * pcHostName,
                                                      uint32_t port,
                                                      NetworkCredentials_t * pxNetworkCredentials,
                                                      NetworkContext_t * pxNetworkContext )
{
    TlsTransportStatus_t xNetworkStatus;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xReconnectParams;
    uint16_t usNextRetryBackOff = 0U;

    /* Initialize reconnect attempts and interval. */
    BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                       RETRY_BACKOFF_BASE_MS,
                                       RETRY_MAX_BACKOFF_DELAY_MS,
                                       RETRY_MAX_ATTEMPTS );

    /* Attempt to connect to IoT Hub. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase till maximum
     * attempts are reached.
     */
    do
    {
        LogInfo( ( "Creating a TLS connection to %s:%lu.\r\n", pcHostName, port ) );
        /* Attempt to create a mutually authenticated TLS connection. */
        xNetworkStatus = TLS_Socket_Connect( pxNetworkContext,
                                             pcHostName, port,
                                             pxNetworkCredentials,
                                             TRANSPORT_SEND_RECV_TIMEOUT_MS,
                                             TRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( xNetworkStatus != eTLSTransportSuccess )
        {
            /* Generate a random number and calculate backoff value (in milliseconds) for
             * the next connection retry.
             * Note: It is recommended to seed the random number generator with a device-specific
             * entropy source so that possibility of multiple devices retrying failed network operations
             * at similar intervals can be avoided. */
            xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xReconnectParams, configRAND32(), &usNextRetryBackOff );

            if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                LogError( ( "Connection to the IoT Hub failed, all attempts exhausted." ) );
            }
            else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogWarn( ( "Connection to the IoT Hub failed [%d]. "
                           "Retrying connection with backoff and jitter [%d]ms.",
                           xNetworkStatus, usNextRetryBackOff ) );
                vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );
            }
        }
    } while( ( xNetworkStatus != eTLSTransportSuccess ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );

    return xNetworkStatus == eTLSTransportSuccess ? 0 : 1;
}

/*-----------------------------------------------------------*/

/**
 * @brief Azure IoT task that utilizes middleware API to connect to Azure IoT Hub.
 * The newest information is sent each minute.
 */
void prvAzureMainLoopTask( void * pvParameters ) {
    uint32_t ulScratchBufferLength = 0U;
    NetworkCredentials_t xNetworkCredentials = { 0 };
    AzureIoTTransportInterface_t xTransport;
    NetworkContext_t xNetworkContext = { 0 };
    TlsTransportParams_t xTlsTransportParams = { 0 };
    AzureIoTResult_t xResult;
    uint32_t ulStatus;
    AzureIoTHubClientOptions_t xHubOptions = { 0 };
    AzureIoTMessageProperties_t xPropertyBag;
    bool xSessionPresent;

    uint8_t * pucIotHubHostname = ( uint8_t * ) appconfigHOSTNAME;
    uint8_t * pucIotHubDeviceId = ( uint8_t * ) appconfigDEVICE_ID;
    uint32_t pulIothubHostnameLength = sizeof( appconfigHOSTNAME ) - 1;
    uint32_t pulIothubDeviceIdLength = sizeof( appconfigDEVICE_ID ) - 1;

    ( void ) pvParameters;

    /* Initialize Azure IoT Middleware.  */
    configASSERT( AzureIoT_Init() == eAzureIoTSuccess );

    ulStatus = prvSetupNetworkCredentials( &xNetworkCredentials );
    configASSERT( ulStatus == 0 );
    xNetworkContext.pParams = &xTlsTransportParams;

    for( ; ; )
    {
        // The lot is considered as free as long as the distance will be close to the calibrated value.
        T_APPS.bParkingLotIsFree = 
            ulGetDistanceCm() < T_APPS.ulCalibrationDistance - appconfigCALIBRATION_ERROR_MARGIN ? false : true;

        if( xAzureSample_IsConnectedToInternet() )
        {
            /* Attempt to establish TLS session with IoT Hub. If connection fails,
             * retry after a timeout. Timeout value will be exponentially increased
             * until  the maximum number of attempts are reached or the maximum timeout
             * value is reached. The function returns a failure status if the TCP
             * connection cannot be established to the IoT Hub after the configured
             * number of attempts. */
            if (prvConnectToServerWithBackoffRetries( 
                ( const char * ) pucIotHubHostname,
                appconfigIOTHUB_PORT,
                &xNetworkCredentials, 
                &xNetworkContext )) continue;

            /* Fill in Transport Interface send and receive function pointers. */
            xTransport.pxNetworkContext = &xNetworkContext;
            xTransport.xSend = TLS_Socket_Send;
            xTransport.xRecv = TLS_Socket_Recv;

            /* Init IoT Hub option */
            xResult = AzureIoTHubClient_OptionsInit( &xHubOptions );
            if(xResult) continue;

            xHubOptions.pucModuleID = ( const uint8_t * ) appconfigMODULE_ID;
            xHubOptions.ulModuleIDLength = sizeof( appconfigMODULE_ID ) - 1;

            xResult = AzureIoTHubClient_Init( &xAzureIoTHubClient,
                                              pucIotHubHostname, pulIothubHostnameLength,
                                              pucIotHubDeviceId, pulIothubDeviceIdLength,
                                              &xHubOptions,
                                              ucMQTTMessageBuffer, sizeof( ucMQTTMessageBuffer ),
                                              ullGetUnixTime,
                                              &xTransport );
            if(xResult) continue;

            #ifdef appconfigDEVICE_SYMMETRIC_KEY
                xResult = AzureIoTHubClient_SetSymmetricKey( &xAzureIoTHubClient,
                                                             ( const uint8_t * ) appconfigDEVICE_SYMMETRIC_KEY,
                                                             sizeof( appconfigDEVICE_SYMMETRIC_KEY ) - 1,
                                                             Crypto_HMAC );
            if(xResult) continue;
            #endif /* appconfigDEVICE_SYMMETRIC_KEY */

            /* Sends an MQTT Connect packet over the already established TLS connection,
             * and waits for connection acknowledgment (CONNACK) packet. */
            LogInfo( ( "Creating an MQTT connection to %s.\r\n", pucIotHubHostname ) );

            xResult = AzureIoTHubClient_Connect( &xAzureIoTHubClient,
                                                 false, &xSessionPresent,
                                                 CONNACK_RECV_TIMEOUT_MS );
            if(xResult) continue;

            xResult = AzureIoTHubClient_SubscribeCloudToDeviceMessage( &xAzureIoTHubClient, prvHandleCloudMessage,
                                                                       &xAzureIoTHubClient, SUBSCRIBE_TIMEOUT );
            if(xResult) goto __disconnect;

            xResult = AzureIoTHubClient_SubscribeCommand( &xAzureIoTHubClient, prvHandleCommand,
                                                          &xAzureIoTHubClient, SUBSCRIBE_TIMEOUT );
            if(xResult) goto __unsub_ctdm;

            xResult = AzureIoTHubClient_SubscribeProperties( &xAzureIoTHubClient, prvHandlePropertiesMessage,
                                                             &xAzureIoTHubClient, SUBSCRIBE_TIMEOUT );
            if(xResult) goto __unsub_cmd;

            /* Get property document after initial connection */
            xResult = AzureIoTHubClient_RequestPropertiesAsync( &xAzureIoTHubClient );
            if(xResult) goto __unsub_prop;

            /* Create a bag of properties for the telemetry */
            xResult = AzureIoTMessage_PropertiesInit( &xPropertyBag, ucPropertyBuffer, 0, sizeof( ucPropertyBuffer ) );
            if(xResult) goto __unsub_prop;

            /* Sending a default property (Content-Type). */
            xResult = AzureIoTMessage_PropertiesAppend( &xPropertyBag,
                                                        ( uint8_t * ) AZ_IOT_MESSAGE_PROPERTIES_CONTENT_TYPE, sizeof( AZ_IOT_MESSAGE_PROPERTIES_CONTENT_TYPE ) - 1,
                                                        ( uint8_t * ) MESSAGE_CONTENT_TYPE, sizeof( MESSAGE_CONTENT_TYPE ) - 1 );
            if(xResult) goto __unsub_prop;

            /* Sending a default property (Content-Encoding). */
            xResult = AzureIoTMessage_PropertiesAppend( &xPropertyBag,
                                                        ( uint8_t * ) AZ_IOT_MESSAGE_PROPERTIES_CONTENT_ENCODING, sizeof( AZ_IOT_MESSAGE_PROPERTIES_CONTENT_ENCODING ) - 1,
                                                        ( uint8_t * ) MESSAGE_CONTENT_ENCODING, sizeof( MESSAGE_CONTENT_ENCODING ) - 1 );
            if(xResult) goto __unsub_prop;
            xResult = AzureIoTMessage_PropertiesAppend( 
                &xPropertyBag, ( uint8_t * ) "status", sizeof( "status" ) - 1,
                ( uint8_t * ) "online", sizeof( "online" ) - 1 );
            if(xResult) goto __unsub_prop;

            /* MQTT Publishing  */
            if (T_APPS.bParkingLotIsFree) {
                ulScratchBufferLength = snprintf(
                    (char *)ucScratchBuffer, sizeof(ucScratchBuffer),
                    "{ \"is_free\": true }"
                );
            } else {
                ulScratchBufferLength = snprintf(
                    (char *)ucScratchBuffer, sizeof(ucScratchBuffer),
                    "{ \"is_free\": false, \"vehicle_size\": \"%ld\" }", 
                    T_APPS.ulCalibrationDistance - ulGetDistanceCm()
                );
            }

            xResult = AzureIoTHubClient_SendTelemetry(&xAzureIoTHubClient,
                                         ucScratchBuffer, ulScratchBufferLength,
                                         &xPropertyBag, eAzureIoTHubMessageQoS1, NULL);
            if(xResult) goto __unsub_prop;

            LogInfo( ( "Attempt to receive publish message from IoT Hub.\r\n" ) );
            xResult = AzureIoTHubClient_ProcessLoop( &xAzureIoTHubClient,
                                                         PROCESS_LOOP_TIMEOUT_MS );
            if(xResult) goto __unsub_prop;
            /* End MQTT Publishing.  */

            if( xAzureSample_IsConnectedToInternet() ) {
            __unsub_prop:
                xResult = AzureIoTHubClient_UnsubscribeProperties( &xAzureIoTHubClient );
                configASSERT( xResult == eAzureIoTSuccess );
            __unsub_cmd:
                xResult = AzureIoTHubClient_UnsubscribeCommand( &xAzureIoTHubClient );
                configASSERT( xResult == eAzureIoTSuccess );
            __unsub_ctdm:
                xResult = AzureIoTHubClient_UnsubscribeCloudToDeviceMessage( &xAzureIoTHubClient );
                configASSERT( xResult == eAzureIoTSuccess );
            __disconnect:
                /* Send an MQTT Disconnect packet over the already connected TLS over
                 * TCP connection. There is no corresponding response for the disconnect
                 * packet. After sending disconnect, client must close the network
                 * connection. */
                xResult = AzureIoTHubClient_Disconnect( &xAzureIoTHubClient );
                configASSERT( xResult == eAzureIoTSuccess );
            }

            /* Close the network connection.  */
            TLS_Socket_Disconnect( &xNetworkContext );
        }

        LogInfo( ( "Cloud communication iteration complete. \r\n\r\n") );
        vTaskDelay( DELAY_BETWEEN_ITERATIONS_TICKS );
    }
}
