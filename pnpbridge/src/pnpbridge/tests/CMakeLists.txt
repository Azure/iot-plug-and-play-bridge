#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.12)

compileAsC11()

if(${LINUX})
   add_definitions(-DAZIOT_LINUX)
endif()

usePermissiveRulesForSdkSamplesAndTests()

add_unittest_directory(pnpbridge_configuration_ut)
add_unittest_directory(pnpbridge_discovery_manager_ut)