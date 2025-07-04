/* Copyright (c) Microsoft Corporation. All rights reserved. */
/* SPDX-License-Identifier: MIT */
/* 
 *  This is only a main entry point that ensures a stable connection for communicating with Azure Cloud.
 * */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "azure_sample_connection.h"
#include "azure_iot_config.h"
#include "app_config.h"
#include "hc_sr04.h"

#include "projdefs.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
/*-----------------------------------------------------------*/

#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR     1

#if CONFIG_SAMPLE_IOT_WIFI_SCAN_METHOD_FAST
    #define SAMPLE_IOT_WIFI_SCAN_METHOD    WIFI_FAST_SCAN
#elif CONFIG_SAMPLE_IOT_WIFI_SCAN_METHOD_ALL_CHANNEL
    #define SAMPLE_IOT_WIFI_SCAN_METHOD    WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_SAMPLE_IOT_WIFI_CONNECT_AP_BY_SIGNAL
    #define SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD    WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_SAMPLE_IOT_WIFI_CONNECT_AP_BY_SECURITY
    #define SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD    WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_SAMPLE_IOT_WIFI_AUTH_OPEN
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_OPEN
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WEP
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WEP
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA2_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA2_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA_WPA2_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA2_ENTERPRISE
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA3_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA3_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA2_WPA3_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WAPI_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WAPI_PSK
#endif /* if CONFIG_SAMPLE_IOT_WIFI_AUTH_OPEN */

#define SNTP_SERVER_FQDN                                "pool.ntp.org"
/*-----------------------------------------------------------*/

static const char * TAG = CONFIG_AZURE_IOT_DEVICE_ID;

static bool g_timeInitialized = false;

static xSemaphoreHandle s_semph_get_ip_addrs;
static esp_ip4_addr_t s_ip_addr;

static bool s_is_connected_to_internet = false;

extern tAppState T_APPS;

/*-----------------------------------------------------------*/

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created withing common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool is_our_netif( 
    const char * prefix,
    esp_netif_t * netif 
) {
    return strncmp( prefix, esp_netif_get_desc( netif ), strlen( prefix ) - 1 ) == 0;
}
/*-----------------------------------------------------------*/

static void on_got_ip( 
    void * arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void * event_data 
) {
    ip_event_got_ip_t * event = ( ip_event_got_ip_t * ) event_data;

    if( !is_our_netif( TAG, event->esp_netif ) ) {
        ESP_LOGW( TAG, "Got IPv4 from another interface \"%s\": ignored",
                  esp_netif_get_desc( event->esp_netif ) );
        return;
    }

    ESP_LOGI( TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR,
              esp_netif_get_desc( event->esp_netif ), IP2STR( &event->ip_info.ip ) );
    memcpy( &s_ip_addr, &event->ip_info.ip, sizeof( s_ip_addr ) );
    s_is_connected_to_internet = true;
    xSemaphoreGive( s_semph_get_ip_addrs );
}
/*-----------------------------------------------------------*/

static void on_wifi_disconnect( 
    void * arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void * event_data 
) {
    ESP_LOGI( TAG, "Wi-Fi disconnected, trying to reconnect..." );
    s_is_connected_to_internet = false;
    esp_err_t err = esp_wifi_connect();

    if( err == ESP_ERR_WIFI_NOT_STARTED ) {
        return;
    }

    ESP_ERROR_CHECK( err );
}
/*-----------------------------------------------------------*/

static esp_netif_t * get_example_netif_from_desc( const char * desc ) {
    esp_netif_t * netif = NULL;
    char * expected_desc;

    asprintf( &expected_desc, "%s: %s", TAG, desc );

    while( ( netif = esp_netif_next_unsafe( netif ) ) != NULL ) {
        if( strcmp( esp_netif_get_desc( netif ), expected_desc ) == 0 ) {
            free( expected_desc );
            return netif;
        }
    }

    free( expected_desc );
    return netif;
}
/*-----------------------------------------------------------*/

static esp_netif_t * wifi_start( void ) {
    char * desc;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init( &cfg ) );

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    /* Prefix the interface description with the module TAG */
    /* Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask) */
    asprintf( &desc, "%s: %s", TAG, esp_netif_config.if_desc );
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    esp_netif_t * netif = esp_netif_create_wifi( WIFI_IF_STA, &esp_netif_config );
    free( desc );
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT,
                                                 WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL ) );
    ESP_ERROR_CHECK( esp_event_handler_register( IP_EVENT,
                                                 IP_EVENT_STA_GOT_IP, &on_got_ip, NULL ) );
    #ifdef CONFIG_EXAMPLE_CONNECT_IPV6
        ESP_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT,
                                                     WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, netif ) );
        ESP_ERROR_CHECK( esp_event_handler_register( IP_EVENT,
                                                     IP_EVENT_GOT_IP6, &on_got_ipv6, NULL ) );
    #endif

    ESP_ERROR_CHECK( esp_wifi_set_storage( WIFI_STORAGE_RAM ) );
    wifi_config_t wifi_config = {
        .sta                    =
        {
            .ssid               = CONFIG_SAMPLE_IOT_WIFI_SSID,
            .password           = CONFIG_SAMPLE_IOT_WIFI_PASSWORD,
            .scan_method        = SAMPLE_IOT_WIFI_SCAN_METHOD,
            .sort_method        = SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi     = CONFIG_SAMPLE_IOT_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    ESP_LOGI( TAG, "Connecting to %s...", wifi_config.sta.ssid );
    ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
    ESP_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_STA, &wifi_config ) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_wifi_connect();
    return netif;
}
/*-----------------------------------------------------------*/

static void wifi_stop( void ) {
    s_is_connected_to_internet = false;

    esp_netif_t * wifi_netif = get_example_netif_from_desc( "sta" );

    ESP_ERROR_CHECK( esp_event_handler_unregister( WIFI_EVENT,
                                                   WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect ) );
    ESP_ERROR_CHECK( esp_event_handler_unregister( IP_EVENT,
                                                   IP_EVENT_STA_GOT_IP, &on_got_ip ) );
    #ifdef CONFIG_EXAMPLE_CONNECT_IPV6
        ESP_ERROR_CHECK( esp_event_handler_unregister( IP_EVENT,
                                                       IP_EVENT_GOT_IP6, &on_got_ipv6 ) );
        ESP_ERROR_CHECK( esp_event_handler_unregister( WIFI_EVENT,
                                                       WIFI_EVENT_STA_CONNECTED, &on_wifi_connect ) );
    #endif
    esp_err_t err = esp_wifi_stop();

    if( err == ESP_ERR_WIFI_NOT_INIT ) {
        return;
    }

    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    ESP_ERROR_CHECK( esp_wifi_clear_default_wifi_driver_and_handlers( wifi_netif ) );
    esp_netif_destroy( wifi_netif );
}
/*-----------------------------------------------------------*/

static void stop( void ) {
    wifi_stop();
}
/*-----------------------------------------------------------*/

static esp_err_t example_connect( void ) {
    if( s_semph_get_ip_addrs != NULL ) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_t * esp_netif = wifi_start();
    ( void ) esp_netif;

    /* create semaphore if at least one interface is active */
    s_semph_get_ip_addrs = xSemaphoreCreateCounting( NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0 );

    ESP_ERROR_CHECK( esp_register_shutdown_handler( &stop ) );
    ESP_LOGI( TAG, "Waiting for IP(s)" );

    for( int i = 0; i < NR_OF_IP_ADDRESSES_TO_WAIT_FOR; ++i ) {
        xSemaphoreTake( s_semph_get_ip_addrs, portMAX_DELAY );
    }

    /* iterate over active interfaces, and print out IPs of "our" netifs */
    esp_netif_t * netif = NULL;
    esp_netif_ip_info_t ip;

    for( int i = 0; i < esp_netif_get_nr_of_ifs(); ++i ) {
        netif = esp_netif_next_unsafe( netif );

        if( is_our_netif( TAG, netif ) ) {
            ESP_LOGI( TAG, "Connected to %s", esp_netif_get_desc( netif ) );
            ESP_ERROR_CHECK( esp_netif_get_ip_info( netif, &ip ) );

            ESP_LOGI( TAG, "- IPv4 address: " IPSTR, IP2STR( &ip.ip ) );
        }
    }

    return ESP_OK;
}
/*-----------------------------------------------------------*/
bool xAzureSample_IsConnectedToInternet() {
    return s_is_connected_to_internet;
}

/*-----------------------------------------------------------*/

static void time_sync_notification_cb( struct timeval * tv ) {
    ESP_LOGI( TAG, "Notification of a time synchronization event" );
    g_timeInitialized = true;
}
/*-----------------------------------------------------------*/

/* 
 *  @brief Initializes time constraint for application timings. 
 * */
static void initialize_time( void ) {
    esp_sntp_setoperatingmode( SNTP_OPMODE_POLL );
    esp_sntp_setservername( 0, SNTP_SERVER_FQDN );
    sntp_set_time_sync_notification_cb( time_sync_notification_cb );
    esp_sntp_init();

    ESP_LOGI( TAG, "Waiting for time synchronization with SNTP server" );

    while( !g_timeInitialized ) {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}
/*-----------------------------------------------------------*/

/* 
 *  brief Initializes and calibrates the ultra sonic sensor during application startup. 
 * */
static void sensor_calibrate( void ) {
    vInitHCSR04();
    uint32_t acc = 0;

    ESP_LOGI(TAG, "Starting calibration...");
    for (uint16_t i = 0; i < appconfigSENSOR_CALIBRATION_PROBES; ++i) {
        uint32_t sample; 
        vMeasureDistance();
        vTaskDelay( pdMS_TO_TICKS(100) );

        sample = ulGetDistanceCm();
        ESP_LOGI(TAG, "Registered sample: #%d: %lu", i, sample);
        acc += sample;
    }

    T_APPS.ulCalibrationDistance = acc / appconfigSENSOR_CALIBRATION_PROBES; 
    ESP_LOGI(TAG, "Device is properly calibrated with default distance of %lu cm.", 
        T_APPS.ulCalibrationDistance);
}

void app_main( void ) {
    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    /*Allow other core to finish initialization */
    vTaskDelay( pdMS_TO_TICKS( 100 ) );

    sensor_calibrate();
    ( void ) example_connect();
    initialize_time();

   
    /* Main sensor handling function. */
    xTaskCreate(
        vSensorHandlingTask,
        "SensorHandlerLoop",
        2048,
        NULL,
        1,
        NULL
    );

    /* Spawning main Azure loop task. */
    xTaskCreate( 
        prvAzureMainLoopTask, 
        "AzureMainLoop", 
        appconfigSTACKSIZE, 
        NULL, 
        tskIDLE_PRIORITY, 
        NULL 
    );
}
