#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME PnpBridge)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

# compileAsC99()

set(pnp_bridge_c_files
    ./ConfigurationParser.c
    ./DiscoveryManager.c
    ./IotHubComms.c
    ./PnpAdapterManger.c
    ./PnpBridge.c
    ./PnpBridgeService.c
    ./utility.c
    ./Core/PnpAdapterInterface.c
    ./Core/PnpBridgeMemory.c
    ./Adapters/AdapterManifest.c
    ./Adapters/Camera/CameraIotPnpAPIs.cpp
    ./Adapters/Camera/CameraCaptureEngineCallbacks.cpp
    ./Adapters/Camera/CameraCaptureEngine.cpp
    ./Adapters/Camera/CameraPnpDiscovery.cpp
    ./Adapters/Camera/CameraIotPnpDevice.cpp
    ./Adapters/Camera/JsonWrapper.cpp
    ./Adapters/Camera/CameraStatConsumer.cpp
    ./Adapters/CoreDeviceHealth/CoreDeviceHealth.c
    ./Adapters/CoreDeviceHealth/WindowsPnPDeviceDiscovery.c
    ./Adapters/SerialPnp/SerialPnp.c
)

set(pnp_bridge_h_files
    ./common.h
    ./ConfigurationParser.h
    ./DiscoveryAdapterInterface.h
    ./DiscoveryManager.h
    ./IotHubComms.h
    ./PnpAdapterInterface.h
    ./PnpAdapterManager.h
    ./PnpBridge.h
    ./PnpBridgeCommon.h
    ./PnpBridgeh.h
    ./Core/PnpBridgeMemory.h
    ./Adapters/PnpAdapterManifest.h
    ./Adapters/DiscoveryAdapterManifest.h
    ./Adapters/CmdAndPropHandler.h
    ./Adapters/Camera/CameraCaptureEngine.h
    ./Adapters/Camera/CameraCaptureEngineCallbacks.h
    ./Adapters/Camera/CameraIotPnpAPIs.h
    ./Adapters/Camera/CameraIotPnpDevice.h
    ./Adapters/Camera/CameraPnpDiscovery.h
    ./Adapters/Camera/CameraStatConsumer.h
    ./Adapters/Camera/JsonWrapper.h
    ./Adapters/Camera/pch.h
    ./Adapters/CoreDeviceHealth/CoreDeviceHealth.h
    ./Adapters/CoreDeviceHealth/WindowsPnpDeviceDiscovery.h
    ./Adapters/SerialPnp/SerialPnp.h
)

set(pnp_bridge_libs)

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

target_link_libraries(${PROJECT_NAME} ${pnp_bridge_libs})
target_link_libraries(${PROJECT_NAME} iothub_client parson pnp_client iothub_client_http_transport iothub_client_amqp_transport iothub_client_amqp_ws_transport iothub_client_mqtt_transport iothub_client_mqtt_ws_transport umqtt aziotsharedutil)

set(pnp_bridge_libs
    ${PROJECT_NAME}
    ${pnp_bridge_libs}
)

# install(FILES ${pnp_bridge_h_install_files}
    # DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/azureiot/pnp)
# install(TARGETS ${install_staticlibs}
    # ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    # LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})