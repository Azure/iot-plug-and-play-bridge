#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for version
cmake_minimum_required(VERSION 2.8.11)

compileAsC11()
set(theseTestsName pnpbridge_configuration_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)


set(${theseTestsName}_c_files
../../src/configuration_parser.c
../../../../deps/azure-iot-sdk-c-pnp/deps/parson/parson.c
)

set(${theseTestsName}_h_files
../../../../deps/azure-iot-sdk-c-pnp/deps/parson/parson.h
)

configure_file(./config_valid.json ./config_valid.json COPYONLY)

build_c_test_artifacts(${theseTestsName} ON "tests/pnpbridge_tests")

