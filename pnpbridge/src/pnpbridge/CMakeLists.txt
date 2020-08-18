#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()

# Core PnpBridge C Files
set(pnp_bridge_c_core_files
    ./src/configuration_parser.c
    ./src/iothub_comms.c
    ./src/pnpadapter_manager.c
    ./src/pnpbridge.c
    ./src/utility.c
    ./src/pnpadapter_api.c
)

# Core PnpBridge headers
set(pnp_bridge_h_core_files
    ./inc/configuration_parser.h
    ./inc/iothub_comms.h
    ./inc/pnpadapter_api.h
    ./inc/pnpadapter_manager.h
    ./inc/pnpbridge.h
    ./inc/pnpbridge_common.h
)

# Pnp Common Helper C Files
set(pnp_helper_c_core_files
    ./common/pnp_device_client.c
    ./common/pnp_dps.c
    ./common/pnp_protocol.c
)

# Pnp Common Helper headers
set(pnp_helper_h_core_files
    ./common/pnp_device_client.h
    ./common/pnp_dps.h
    ./common/pnp_protocol.h
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
set(pnp_bridge_common_pnp_inc ${CMAKE_CURRENT_LIST_DIR}/../pnpbridge/common CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/iothub_client/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/provisioning_client/inc)
include_directories(${pnp_bridge_INC_FOLDER})
include_directories(${pnp_bridge_common_pnp_inc})

if(WIN32)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/pal/windows)
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
)

if(WIN32)
set(pnp_bridge_common_libs
    ${pnp_bridge_common_libs}
)
endif()

add_library(${PROJECT_NAME} STATIC 
    ${pnp_bridge_c_core_files}
    ${pnp_bridge_h_core_files}
    ${pnp_helper_c_core_files}
    ${pnp_helper_h_core_files}
)

#add_executable(${PROJECT_NAME}
#    ${pnp_bridge_c_core_files}
#    ${pnp_bridge_h_core_files}
#    ${pnp_helper_c_core_files}
#    ${pnp_helper_h_core_files}
#)

add_subdirectory(tests)
add_subdirectory(samples)

if(WIN32)
# Ole32 (COM) is used by camera health monitoring adapter
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs} cfgmgr32 mfplat mfsensorgroup Ole32 runtimeobject)
else()
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs})
endif()
