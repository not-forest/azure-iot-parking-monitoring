# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Main component only establishes the communication between the microcontroller and Azure Cloud.

set(COMPONENT_INCLUDE_DIRS
    .
    ../config
)

set(COMPONENT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/azure_iot_main.c
)

idf_component_register(
    SRCS ${COMPONENT_SOURCES}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    REQUIRES esp_event esp_wifi freertos nvs_flash coreMQTT azure-sdk-for-c azure-iot-middleware-freertos sample-azure-iot
)
