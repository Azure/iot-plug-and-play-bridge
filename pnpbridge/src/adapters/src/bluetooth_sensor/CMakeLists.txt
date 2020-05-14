#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(PROJECT_NAME pnpbridge_bluetoothsensor)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

set(pnpbridge_adapters_c_files
    BluetoothSensorPnPBridgeInterface.cpp
    BluetoothSensorDeviceAdapterBase.cpp
    InterfaceDescriptor.cpp
)

IF(WIN32)
set(pnpbridge_adapters_c_files
    ${pnpbridge_adapters_c_files}
    BluetoothSensorDeviceAdapterWin.cpp
)
ENDIF(WIN32)

set(pnpbridge_adapters_h_files
    BluetoothSensorPnPBridgeInterface.h
    BluetoothSensorDeviceAdapter.h
    BluetoothSensorDeviceAdapterBase.h
)

IF(WIN32)
set(pnpbridge_adapters_h_files
    ${pnpbridge_adapters_h_files}
    BluetoothSensorDeviceAdapterWin.h
)
ENDIF(WIN32)

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
