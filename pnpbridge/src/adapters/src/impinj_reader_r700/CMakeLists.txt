# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge_impinj_reader_r700)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()

set(pnpbridge_adapters_c_files
    ./impinj_reader_r700.c
    ./curl_wrapper/curl_wrapper.c
    ./helpers/string_manipulation.c
)

set(pnpbridge_adapters_h_files
    ./impinj_reader_r700.h
    ./curl_wrapper/curl_wrapper.h
    ./helpers/string_manipulation.h
)

add_definitions("-D_UNICODE") 

set(pnpbridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/../../pnpbridge/inc CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)
set(pnp_bridge_common_pnp_inc ${CMAKE_CURRENT_LIST_DIR}/../pnpbridge/common CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(../inc)
include_directories(../../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../../deps/azure-iot-sdk-c-pnp/pnp_client/inc)

include_directories(${pnpbridge_INC_FOLDER})
include_directories(${pnp_bridge_common_pnp_inc})

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_definitions(-DPNP_LOGGING_ENABLED)

add_library(${PROJECT_NAME} STATIC 
    ${pnpbridge_adapters_c_files}
    ${pnpbridge_adapters_h_files}
)

set(pnp_bridge_common_libs
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

target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs})

configure_file(./config.json config.json COPYONLY)
