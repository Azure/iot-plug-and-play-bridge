# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge_environmentalsensor)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()

set(pnpbridge_adaptersample_c_files
    ./environmental_sensor.c
    ./environmental_sensor_pnpbridge.c
    ./adapter_manifest_pnpbridge.c
)

set(pnpbridge_adaptersample_h_files
    ./environmental_sensor.h
    ./environmental_sensor_pnpbridge.h
)

add_definitions("-D_UNICODE") 


set(pnp_bridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/../pnpbridge/inc CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)
set(pnp_bridge_common_pnp_inc ${CMAKE_CURRENT_LIST_DIR}/../pnpbridge/common CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)
set(AZUREIOT_INC_FOLDER "/usr/include/azureiot" "/usr/include/azureiot/inc")

include_directories(${AZUREIOT_INC_FOLDER})
include_directories(../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/iothub_client/inc)

include_directories(${pnp_bridge_INC_FOLDER})
include_directories(${pnp_bridge_common_pnp_inc})

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_definitions(-DPNP_LOGGING_ENABLED)

add_library(${PROJECT_NAME} STATIC 
    ${pnpbridge_adaptersample_c_files}
    ${pnpbridge_adaptersample_h_files}
)

set(pnp_bridge_common_libs
    aziotsharedutil
    pnpbridge
    iothub_client 
    iothub_client_http_transport
    iothub_client_amqp_transport
    iothub_client_amqp_ws_transport
    iothub_client_mqtt_transport
    iothub_client_mqtt_ws_transport
    parson
    umqtt
    msr_riot
    hsm_security_client
    prov_auth_client
    prov_device_client
    prov_mqtt_transport
    utpm
)

# add_executable(${PROJECT_NAME}
    # ${pnpbridge_adaptersample_c_files}
    # ${pnpbridge_adaptersample_h_files}
# )
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs})

configure_file(./config.json config.json COPYONLY)
