# Azure IoT PnP Bridge

Azure IoT PnP Bridge is an open source effort from Microsoft to expose peripherals on a Device (Gateway) to Microsoft Azure IoT services using Azure IoT PnP Interfaces. The vision is to enable IoT developers to light up peripherals and expose it to Azure IoT services at scale with mininimum amount of device side code. The bridge would also provide peripheral monitoring and remote managing.

## Compile the Bridge

| Platform | Supported |
| :-----------: | :-----------: |
| Windows |  :heavy_check_mark: |
| Linux | :heavy_multiplication_x: |

### Development Pre-Requisite
* In order to build Private Preview Azure PnP Bridge, you need to join Microsoft Azure team: https://github.com/Azure
* Ensure CMake and Visual Studio 2017 are installed. **CMake should be in the search PATH.**
* Download Windows 17763 SDK: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk

### Build Azure IoT PnP Bridge

After cloning Azure IoT PnP Bridge repo, run following commands in "Developer Command Prompt for VS 2017":
  * cd src\PnpBridge
  * git submodule update --init --recursive 
  * cd Scripts
  * build.prereq.cmd
  * Open PnpBridge\src\PnpBridge\PnpBridge.vcxproj with "Visual Studio 2017"
  * Ensure build target is set to Release|Debug and x86|x64 matches to the build.prereq.cmd's build and build the solution
  * Pnpbridge.exe is the Azure PnP Bridge binary

## Quickstart

To try out Azure IoT PnP Bridge, follow the steps bellow:

* Create an Azure IoT device using Microsoft Azure documentation online. Obtain the connections string.
* Modify the folowing config parameters:
  * Update the connection string in "ConnectionString" key under "PnpBridgeParameters".
* Start PnpBridge by running it in a command prompt.

## PnP Bridge Architecture

## Folder Structure

### /deps/azure-iot-sdk-c-pnp

Git submodules that contains Azure IoT C PNP SDK code

### /scripts

Build scripts

### /src/PnpBridge

Source code for Azure IoT PnP Bridge

### Support

For any questions contact - [Azure IoT PnP Bridge] (bf1dde55.microsoft.com@amer.teams.ms)





