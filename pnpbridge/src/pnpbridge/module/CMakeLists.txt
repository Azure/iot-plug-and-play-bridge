# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)


set(PROJECT_NAME pnpbridge_module)

macro(compileAsC99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
    endif()
  else()
    set (CMAKE_C_STANDARD 99)
    set (CMAKE_CXX_STANDARD 11)
  endif()
endmacro(compileAsC99)

compileAsC99()

# Core PnpBridge C Files
set(pnp_bridge_c_core_files
    ./main.c
    ./../src/configuration_parser.c
    ./../src/iothub_comms.c
    ./../src/pnpadapter_manager.c
    ./../src/pnpbridge.c
    ./../src/utility.c
    ./../src/pnpadapter_api.c
)

# Core PnpBridge headers
set(pnp_bridge_h_core_files
    ./../inc/configuration_parser.h
    ./../inc/iothub_comms.h
    ./../inc/pnpadapter_api.h
    ./../inc/pnpadapter_manager.h
    ./../inc/pnpbridge.h
    ./../inc/pnpbridge_common.h
)

# Pnp Common Helper C Files
set(pnp_helper_c_core_files
    ./../common/pnp_device_client.c
    ./../common/pnp_dps.c
    ./../common/pnp_protocol.c
)

# Pnp Common Helper headers
set(pnp_helper_h_core_files
    ./../common/pnp_device_client.h
    ./../common/pnp_dps.h
    ./../common/pnp_protocol.h
    ./../common/pnp_bridge_client.h
)

# set(install_staticlibs
    # ${PROJECT_NAME}
# )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_EDGE_MODULES")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_EDGE_MODULES")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MODULE_CLIENT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MODULE_CLIENT")

add_definitions(-DPNP_LOGGING_ENABLED)

# set(pnp_bridge_h_install_files
    # ${pnp_bridge_h_files}
# )

include_directories(inc)
include_directories(common)
include_directories(../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/iothub_client/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/provisioning_client/inc)

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
    uhttp
    pthread
    curl
    ssl
    crypto
    m
    pnpbridge_adapters
    pnpbridge_modbus
    pnpbridge_mqtt
    pnpbridge_serial
    pnpbridge_environmentalsensor
)


add_executable(${PROJECT_NAME}
    ${pnp_bridge_c_core_files}
    ${pnp_bridge_h_core_files}
    ${pnp_helper_c_core_files}
    ${pnp_helper_h_core_files}
)

target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs})