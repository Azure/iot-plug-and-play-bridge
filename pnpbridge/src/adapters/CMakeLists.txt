#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge_adapters)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()


set(pnp_bridge_adapters_c_shared_files
    ./src/shared/adapter_manifest.c
)

set(pnp_bridge_adapters_h_shared_files
)

if(MSVC)
set(pnp_bridge_adapters_c_files
    ${pnp_bridge_adapters_c_shared_files}
)
else()
set(pnp_bridge_adapters_c_files
    ${pnp_bridge_adapters_c_shared_files}
)
endif()


if(MSVC)
set(pnp_bridge_adapters_h_files
    ${pnp_bridge_adapters_h_shared_files}
)
else()
set(pnp_bridge_adapters_h_files
    ${pnp_bridge_adapters_h_shared_files}
)
endif()

add_definitions("-D_UNICODE") 


set(pnp_bridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/../pnpbridge/inc CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/digitaltwin_client/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/iothub_client/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/iothub_client/samples/pnp/common)

include_directories(${pnp_bridge_INC_FOLDER})

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_definitions(-DPNP_LOGGING_ENABLED)

add_library(${PROJECT_NAME} STATIC 
    ${pnp_bridge_adapters_c_files}
    ${pnp_bridge_adapters_h_files}
)

if(MSVC)
foreach(source IN LISTS pnp_bridge_adapters_c_files pnp_bridge_adapters_h_files)
    get_filename_component(source_path "${source}" PATH)
	string(REPLACE "./src/" "" source_path_msvc "${source_path}")
    string(REPLACE "/" "\\" source_path_msvc "${source_path_msvc}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()
endif()

#if(NOT ${skip_samples})
    add_subdirectory(samples)
#endif()

add_subdirectory(src)