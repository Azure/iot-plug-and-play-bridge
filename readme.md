
# Azure PnP Bridge

## Introduction
Azure PnP Bridge is the open source effort from Microsoft to bridge PC Device PnP with Azure IoT PnP in order to make it easy for device developers to easily connect many PC peripherals to Azure in order to easily enable remote managing and monitoring peripherals and PC gateways at scale.  Azure PnP Bridge also make it easy to perform analytics on edge as well as moving data to Azure in order to perform analytics in the cloud.  
Azure PnP Bridge is a standalone executable that developers can drop onto their PC/IoT Gateway devices, with simple configuration, this service can bridge the device PnP with Azure PnP and send telemetry and event data to Azure Cloud.  Developers continue leveraging existing Device Provisioning and Azure Cloud services.  


## Pre-Requisite
- Windows 10 OS:
  - For Serial, HIDUSB devices, OS version 1809 or newer is required
  - For Camera monitoring functionality, current Insider Preview OS version is required.
  - One of the following OS flavors:
    - Windows IoT Enterprise
    - Windows Server
    - Windows Desktop SKU
    - Windows IoT Core (if no camera monitoring is needed)

- Hardware:
  - Any hardware capable supporting above OS SKUs and versions.
  - Supported peripherals (refer to 

- IoT Edge (this shouldn�t be required)

## Azure PnP Bridge Architecture

## Dev Machine Setup

### Development Pre-Requisite
- In order to build Private Preview Azure PnP Bridge, you need to join Microsoft Azure team: https://github.com/Azure

- Ensure cmake and Visual Studio 2017 are installed

- Download Windows 17763 SDK: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk

### Source Code

- Clone Azure PnP Bridge Repo: 
  git clone -b staging-pnpbridge --recursive https://xgrow18.visualstudio.com/_git/busesiot


### Build Azure PnP Bridge
  After clone Azure PnP Bridge repro, run following commands in "Developer Command Prompt for VS 2017":
  - cd PnpBridge
  - cd Scripts
  - build.prereq.cmd
  - Open PnpBridge\src\PnpBridge\PnpBridge.vcxproj with "Visual Studio 2017"
  - Ensure build target is set to Release|Debug and x86|x64 matches to the build.prereq.cmd's build and build the solution
  - Pnpbridge.exe is the Azure PnP Bridge binary

### Supported Peripherals

Azure PnP Bridge comes with set of supported peripherals:
- Peripherals connected over Serial COM ports or Serial over USB
    - Please refer Arduino/MCU programming guide on how to enable Azure PNP Interfaces
    - Peripherals connected HID over USB
    - Cameras connected over USB
    - IP cameras (? Need clear description on how to setup IP camera)
    - Built-in MIPI cameras (should we bother to list it here)

For serial/USB peripheral devices that are not included in current source code, developers can update the Azure PnP Bridge code to enable their custom protocols.

### Deploy Azure PnP Bridge

Instruction on deploy it
- How to specify Configuration for Azure PnP Bridge
- How to configure for auto start PnP Bridge
- How to provision the device
- How to set up an IoT Central Dashboard
  - https://docs.microsoft.com/en-us/azure/iot-central/tutorial-define-device-type 
- How to get the Connection String to connect the device to IoT Central
  - https://docs.microsoft.com/en-us/azure/iot-central/tutorial-add-device
