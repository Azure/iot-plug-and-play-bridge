#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge_mqtt)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

set(pnpbridge_adapters_c_files
    ./mqtt_pnp.cpp
    ./json_rpc.cpp
    ./mqtt_manager.cpp
    ./json_rpc_protocol_handler.cpp
)

set(pnpbridge_adapters_h_files
    ./mqtt_pnp.hpp
    ./json_rpc.hpp
    ./mqtt_manager.hpp
    ./mqtt_protocol_handler.hpp
    ./json_rpc_protocol_handler.hpp
)

add_definitions("-D_UNICODE") 

set(pnpbridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/../../pnpbridge/inc CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(../../../../deps/azure-iot-sdk-c-pnp/umqtt/inc)

include_directories(${pnpbridge_INC_FOLDER})

IF(WIN32)
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_definitions(-DPNP_LOGGING_ENABLED)

add_library(${PROJECT_NAME} STATIC 
    ${pnpbridge_adapters_c_files}
    ${pnpbridge_adapters_h_files}
)
