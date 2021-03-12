include(CMakeForceCompiler)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

set(CMAKE_SYSROOT /home/kwenner/impinj/etk/r700/7.5.0_Octane_Embedded_Development_Tools/arm-toolchain/arm-buildroot-linux-gnueabihf/sysroot)

set(tools /home/kwenner/impinj/etk/r700/7.5.0_Octane_Embedded_Development_Tools/arm-toolchain)
set(CMAKE_C_COMPILER ${tools}/bin/arm-none-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/arm-none-linux-gnueabihf-c++)

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${tools}/lib)

include_directories(${tools}/include)