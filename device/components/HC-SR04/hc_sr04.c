/* SPDX-License-Identifier: MIT */
/* 
 *  HC-SR04 Ultrasonic Sensor Module driver.
 * */

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "hc_sr04.h"

const char* TAG = "HC-SR04";

volatile int64_t pstart = 0;
volatile uint32_t d_raw = 0;

// Interrupt is being called on any edge of Echo pin. Based on the pin state we calculate the time passed.
static void IRAM_ATTR hc_sr04_isr_handler(void*_) {
    int64_t now = esp_timer_get_time();

    // Measuring echo impulse length with 1uS accuracy.
    if (gpio_get_level(sensorconfigDISTANCE_SENSOR_MAPPING_ECHO)) {
        pstart = now;
    } else {
        d_raw = now - pstart;
    }
}

/* 
 *  @brief Initializes the HV-SR04 sensor based on the provided mapping.
 * */
void vInitHCSR04( void ) {
    ESP_LOGI(TAG, "Configuring HC-SR04 driver based on the following mapping:\nTrig -> D%u\nEcho -> D%u", 
             sensorconfigDISTANCE_SENSOR_MAPPING_TRIG,
             sensorconfigDISTANCE_SENSOR_MAPPING_ECHO);

    gpio_config(&echo_pin_config);
    gpio_config(&trig_pin_config);
    gpio_isr_handler_add(sensorconfigDISTANCE_SENSOR_MAPPING_ECHO, hc_sr04_isr_handler, NULL);
}

/*
 *  @brief Forces a pulse burst on "Trig" pin to obtain the latest distance measurement.
 * */
void vMeasureDistance( void ) {
    gpio_set_level(sensorconfigDISTANCE_SENSOR_MAPPING_TRIG, 1);
    esp_rom_delay_us(10);   // Minimal trigger pulse value according to datasheet.
    gpio_set_level(sensorconfigDISTANCE_SENSOR_MAPPING_TRIG, 0);
}

/* 
 *  @brief Returns a distance in cm from the sensor to the first physical obstacle.
 *
 *  This function is non-blocking.
 * */
uint32_t ulGetDistanceCm( void ) {
    uint32_t dist = d_raw / 58; 
    ESP_LOGI(TAG, "Last measured sensor distance: %lu", dist);
    return dist;  // Approximate value according to sensor datasheet.
}
