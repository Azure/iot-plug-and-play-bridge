cmake_minimum_required(VERSION 2.8.11)
project(azure_iot_pnp_bridge)

if (POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

#
#making a global variable to know if we are on linux, windows, or macosx.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WINDOWS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE)
    # on Linux, enable valgrind
    # these commands (MEMORYCHECK...) need to apear BEFORE include(CTest) or they will not have any effect
    find_program(MEMORYCHECK_COMMAND valgrind)
    set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MACOSX TRUE)
    add_definitions(-DMACOSX)
endif()


if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4232")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /wd4232")
    # Make warning as error
    add_definitions(/WX)
else()
    # Make warning as error
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()


IF(WIN32)
    # windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(skip_samples ON)

add_subdirectory(src)
add_subdirectory(deps/azure-iot-sdk-c-pnp)