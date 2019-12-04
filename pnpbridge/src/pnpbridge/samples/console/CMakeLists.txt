#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge_bin)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()

# Core PnpBridge C Files
set(pnp_bridge_c_core_files
    ./main.c
)

# Core PnpBridge headers
set(pnp_bridge_h_core_files
)

# set(install_staticlibs
    # ${PROJECT_NAME}
# )

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_definitions(-DPNP_LOGGING_ENABLED)


# set(pnp_bridge_h_install_files
    # ${pnp_bridge_h_files}
# )

set(pnp_bridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(inc)
include_directories(../../../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../../../deps/azure-iot-sdk-c-pnp/digitaltwin_client/inc)
include_directories(../../../../deps/azure-iot-sdk-c-pnp/iothub_client/inc)
include_directories(../../../../deps/azure-iot-sdk-c-pnp/provisioning_client/inc)
include_directories(${pnp_bridge_INC_FOLDER})

if(WIN32)
include_directories(../../../../deps/azure-iot-sdk-c-pnp/c-utility/pal/windows)
endif()

set(pnp_bridge_common_libs
    aziotsharedutil
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
    pnpbridge
    pnpbridge_adapters
    digitaltwin_client
    pnpbridge_serial
    pnpbridge_modbus
)

if(WIN32)
set(pnp_bridge_common_libs
    ${pnp_bridge_common_libs}
    pnpbridge_camera
    pnpbridge_coredevicehealth
)
endif()

add_executable(${PROJECT_NAME}
    ${pnp_bridge_c_core_files}
    ${pnp_bridge_h_core_files}
)

if(WIN32)
# Ole32 (COM) is used by camera health monitoring adapter
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs} cfgmgr32 mfplat mfsensorgroup Ole32)
else()
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs})
endif()

configure_file(./config.json config.json COPYONLY)
