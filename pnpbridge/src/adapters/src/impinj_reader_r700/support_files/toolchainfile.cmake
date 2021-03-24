# working directory changes between = /iot-plug-and-play-bridge/pnpbridge/
#                                &  = /iot-plug-and-play-bridge/pnpbridge/cmake/pnpbridge_linux/CMakeFiles/CMakeTmp/
# so need to detect CWD and change relative path to ETKpath.txt file appropriately (toolchain file is read multiple times)

get_filename_component(CWD_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)  # strip full path from CWD, store name of dir in CWD_NAME

if(${CWD_NAME} STREQUAL "pnpbridge")
    file(STRINGS "src/adapters/src/impinj_reader_r700/support_files/ETKpath.txt" ETK_PATH) # relative path to ETKpath.txt from /iot-plug-and-play-bridge/pnpbridge/
else()
    file(STRINGS "../../../../src/adapters/src/impinj_reader_r700/support_files/ETKpath.txt" ETK_PATH) # relative path to ETKpath.txt from /iot-plug-and-play-bridge/pnpbridge/cmake/pnpbridge_linux/CMakeFiles/CMakeTmp/
endif()

message(STATUS "ETK Path (if this is wrong, update \"pnpbridge/src/adapters/src/impinj_reader_r700/support_files/ETKpath.txt\" with absolute path to R700 ETK on your system):")
message(STATUS ${ETK_PATH})

include(CMakeForceCompiler)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

# May need to update relative paths/names for different versions of Impinj ETK, as gcc/g++ binaries are updated.
set(CMAKE_SYSROOT ${ETK_PATH}/arm-toolchain/arm-buildroot-linux-gnueabihf/sysroot)

set(tools ${ETK_PATH}/arm-toolchain)
set(CMAKE_C_COMPILER ${tools}/bin/arm-none-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/arm-none-linux-gnueabihf-c++)

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${tools}/lib)

include_directories(${tools}/include)