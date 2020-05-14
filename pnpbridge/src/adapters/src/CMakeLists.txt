#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for pnp_bridge

set(PROJECT_NAME pnpbridge)

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

compileAsC99()

add_subdirectory(modbus_pnp)
add_subdirectory(mqtt_pnp)
add_subdirectory(serial_pnp)
add_subdirectory(mqtt_pnp)
add_subdirectory(modbus_pnp)

IF(WIN32)
add_subdirectory(bluetooth_sensor)
add_subdirectory(camera)
add_subdirectory(core_device_health)
ENDIF(WIN32)
