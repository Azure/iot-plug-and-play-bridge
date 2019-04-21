#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME PnpBridge)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()

set(pnp_bridge_c_shared_files
    ConfigurationParser.c
    DiscoveryManager.c
    IotHubComms.c
    PnpAdapterManger.c
    PnpBridge.c
    utility.c
    core/PnpAdapterInterface.c
    core/PnpBridgeMemory.c
    Adapters/AdapterManifest.c
    Adapters/SerialPnp/SerialPnp.c
)

if(MSVC)
set(pnp_bridge_c_files
    ${pnp_bridge_c_shared_files}
    Adapters/Camera/CameraIotPnpAPIs.cpp
    Adapters/Camera/CameraCaptureEngineCallbacks.cpp
    Adapters/Camera/CameraCaptureEngine.cpp
    Adapters/Camera/CameraPnpDiscovery.cpp
    Adapters/Camera/CameraIotPnpDevice.cpp
    Adapters/Camera/JsonWrapper.cpp
    Adapters/Camera/CameraStatConsumer.cpp
    Adapters/CoreDeviceHealth/CoreDeviceHealth.c
    Adapters/CoreDeviceHealth/WindowsPnPDeviceDiscovery.c
    Adapters/ModbusPnp/ModbusCapability.c
    Adapters/ModbusPnp/ModbusPnp.c
    Adapters/ModbusPnp/ModbusConnection/ModbusConnection.c
    Adapters/ModbusPnp/ModbusConnection/ModbusConnectionHelper.c
    Adapters/ModbusPnp/ModbusConnection/ModbusRtuConnection.c
    Adapters/ModbusPnp/ModbusConnection/ModbusTCPConnection.c
)
else()
set(pnp_bridge_c_files
    ${pnp_bridge_c_shared_files}
)
endif()

set(pnp_bridge_h_shared_files
    common.h
    ConfigurationParser.h
    DiscoveryAdapterInterface.h
    DiscoveryManager.h
    IotHubComms.h
    PnpAdapterInterface.h
    PnpAdapterManager.h
    PnpBridge.h
    PnpBridgeCommon.h
    PnpBridgeh.h
    core/PnpBridgeMemory.h
    Adapters/PnpAdapterManifest.h
    Adapters/DiscoveryAdapterManifest.h
    Adapters/CmdAndPropHandler.h
    Adapters/SerialPnp/SerialPnp.h
)

if(MSVC)
set(pnp_bridge_h_files
    ${pnp_bridge_h_shared_files}
    Adapters/Camera/CameraCaptureEngine.h
    Adapters/Camera/CameraCaptureEngineCallbacks.h
    Adapters/Camera/CameraIotPnpAPIs.h
    Adapters/Camera/CameraIotPnpDevice.h
    Adapters/Camera/CameraPnpDiscovery.h
    Adapters/Camera/CameraStatConsumer.h
    Adapters/Camera/JsonWrapper.h
    Adapters/Camera/pch.h
    Adapters/CoreDeviceHealth/CoreDeviceHealth.h
    Adapters/CoreDeviceHealth/WindowsPnpDeviceDiscovery.h
    Adapters/ModbusPnp/CmdPropHandlers.h
    Adapters/ModbusPnp/ModbusCapability.h
    Adapters/ModbusPnp/ModbusEnum.h
    Adapters/ModbusPnp/ModbusPnp.h
    Adapters/ModbusPnp/ModbusConnection/ModbusConnection.h
    Adapters/ModbusPnp/ModbusConnection/ModbusConnectionHelper.h
    Adapters/ModbusPnp/ModbusConnection/ModbusRtuConnection.h
    Adapters/ModbusPnp/ModbusConnection/ModbusTCPConnection.h
)
else()
set(pnp_bridge_h_files
    ${pnp_bridge_h_shared_files}
)
endif()

add_definitions("-D_UNICODE") 

set(install_staticlibs
    ${PROJECT_NAME}
)

set(pnp_bridge_h_install_files
    ${pnp_bridge_h_files}
)

set(pnp_bridge_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "this is what needs to be included if using pnp_bridge lib" FORCE)

include_directories(../../deps/azure-iot-sdk-c-pnp/deps/parson)
include_directories(../../deps/azure-iot-sdk-c-pnp/c-utility/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/pnp_client/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/iothub_client/inc)
include_directories(../../deps/azure-iot-sdk-c-pnp/provisioning_client/inc)
include_directories(${pnp_bridge_INC_FOLDER})

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_definitions(-DPNP_LOGGING_ENABLED)

add_executable(${PROJECT_NAME}
    ${pnp_bridge_c_files}
    ${pnp_bridge_h_files}
)

foreach(source IN LISTS pnp_bridge_c_files pnp_bridge_h_files)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${source_path}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

set(pnp_bridge_common_libs
    iothub_client 
    parson
    pnp_client
    iothub_client_http_transport
    iothub_client_amqp_transport
    iothub_client_amqp_ws_transport
    iothub_client_mqtt_transport
    iothub_client_mqtt_ws_transport
    umqtt
    aziotsharedutil 
    msr_riot
    prov_mqtt_transport
    hsm_security_client
    prov_device_ll_client
    prov_auth_client
    utpm
)

if(MSVC)
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs} cfgmgr32 mfplat mfsensorgroup)
else()
target_link_libraries(${PROJECT_NAME} ${pnp_bridge_common_libs})
endif()
