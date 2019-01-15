======================================================================================
    DeviceAggregator - Bridge to expose peripherals to Azure IoT using PnP Interfaces
======================================================================================

How to build the project?

Use CMake GUI to create Visual Studio solution for azure-iot-sdk-c-pnp as follows:

1. Open CMake GUI and point the source code to azure-iot-sdk-c-pnp folder
2. In Where to build binaries, specify azure-iot-sdk-c-pnp\cmake_x64
3. Open the azure_iot_sdks.sln and build it
4. Now right click on solution and add existing project
5. Point to DeviceAggregator\DeviceAggregator.vcxproj
6. Build DeviceAggregator
