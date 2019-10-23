#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge_camera)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

set(pnpbridge_adapters_c_files
    CameraIotPnpAPIs.cpp
    CameraCaptureEngineCallbacks.cpp
    CameraCaptureEngine.cpp
    CameraMediaCapture.cpp
    CameraPnpDiscovery.cpp
    CameraIotPnpDevice.cpp
    JsonWrapper.cpp
    CameraStatConsumer.cpp
    NetworkCameraIotPnpDevice.cpp
)

set(pnpbridge_adapters_h_files
    CameraCaptureEngine.h
    CameraCaptureEngineCallbacks.h
    CameraMediaCapture.h
    CameraIotPnpAPIs.h
    CameraIotPnpDevice.h
    CameraPnpDiscovery.h
    CameraStatConsumer.h
    JsonWrapper.h
    pch.h
    NetworkCameraIotPnpDevice.h
)

add_definitions("-D_UNICODE") 

set(pnpbridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/../../pnpbridge/inc CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(../inc)
include_directories(../../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/azure-macro-utils-c/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/deps/umock-c/inc)
include_directories(../../../deps/azure-iot-sdk-c-pnp/digitaltwin_client/inc)

include_directories(${pnpbridge_INC_FOLDER})

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
