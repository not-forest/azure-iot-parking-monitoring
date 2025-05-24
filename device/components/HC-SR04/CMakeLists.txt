# SPDX-License-Identifier: MIT

set(COMPONENT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/hc_sr04.c
)

set(COMPONENT_INCLUDE_DIRS
    .
    ${PROJECT_DIR}/config
)

idf_component_register(
    SRCS ${COMPONENT_SOURCES}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    REQUIRES esp_driver_gpio
)
