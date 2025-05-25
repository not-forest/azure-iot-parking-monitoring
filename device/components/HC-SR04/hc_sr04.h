/* SPDX-License-Identifier: MIT */
/* 
 *  HC-SR04 Ultrasonic Sensor Module driver.
 * */

#ifndef H_HC_SR04
#define H_HC_SR04
#include <stdint.h>

#include "driver/gpio.h"

/*
 * @brief Pin mapping for "Trig" input of the HC_SR04 sensor.
 */
#define sensorconfigDISTANCE_SENSOR_MAPPING_TRIG CONFIG_TRIG_MAPPING

/*
 * @brief Pin mapping for "Echo" output of the HC_SR04 sensor.
 */
#define sensorconfigDISTANCE_SENSOR_MAPPING_ECHO CONFIG_ECHO_MAPPING

static gpio_config_t trig_pin_config = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pin_bit_mask = (1ULL << sensorconfigDISTANCE_SENSOR_MAPPING_TRIG),
};

static gpio_config_t echo_pin_config = {
    .intr_type = GPIO_INTR_ANYEDGE,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pin_bit_mask = (1ULL << sensorconfigDISTANCE_SENSOR_MAPPING_ECHO),
};

/*
 *  @brief Periodic sensor handler which mutates the state of the parking lot according to 
 *  obtained distance value. Adaptive thresholding is used in order to be compatible with
 *  parking yards of different height.
 * */
void vSensorHandlingTask( void*_ );

/* 
 *  @brief Initializes the HV-SR04 sensor based on the provided mapping.
 * */
void vInitHCSR04( void );

/*
 *  @brief Starts a pulse burst on "Trig" pin to obtain the latest distance measurement.
 * */
void vMeasureDistance( void );

/* 
 *  @brief Returns a distance in cm from the sensor to the first physical obstacle.
 *
 *  This function is non-blocking.
 * */
uint32_t ulGetDistanceCm( void );

#endif // !H_HC_SR04
