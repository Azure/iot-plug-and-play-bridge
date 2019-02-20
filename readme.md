# This pre-release is confidential
Access to this pre-release software is confidential and should not be disclosed or shared with other parties per your NDA with Microsoft.


# Azure PnP Bridge

## Introduction
Azure PnP Bridge is the open source effort from Microsoft that connects the PC sensor and peripheral ecosystem to Azure. It builds on top of [Azure IoT PnP](https://github.com/Azure/Azure-IoT-PnP-Preview/) so that with minimal to no code, developers can easily connect peripherals to Azure, remotely access their data, monitor them, and manage them. This allows developers to perform analytics and gain valuable insights on their IoT devices from either the edge or the cloud. The Azure PnP Bridge accomplishes this without requiring peripherals to have direct Azure connectivity, enabling them to use a Windows host as a gateway to Azure instead. Because of this, it is particularly well suited for connecting cameras and environmental sensors to Azure.

Azure PnP Bridge can be deployed as a standalone executable on any IoT device, PC, industrial PC, server, or gateway running Windows 10. It can also be compiled into your application code. A simple configuration file tells the Azure PnP Bridge which sensors and peripherals should be exposed up to Azure. Once configured, the Azure PnP Bridge uses the Azure IoT PnP SDK to dynamically publish Azure IoT PnP interfaces for the specified sensors and peripherals up to Azure. Developers can then use existing Azure services and solution accelerators to receive data from and send commands to their sensors and peripherals. 


## Pre-Requisites
- Windows 10 OS:
  - For Camera health monitoring functionality, current Insider Preview OS version is required.
    - All other functionality is available on all Windows 10 builds. 
  - All Windows SKUs are supported. For example:
    - Windows IoT Enterprise
    - Windows Server
    - Windows Desktop
    - Windows IoT Core

- Hardware:
  - Any hardware platform capable of supporting the above OS SKUs and versions.
  - Serial, USB, and Camera peripherals are supported natively. The Azure PnP Bridge can be extended to support any custom peripheral ([see peripherals section below](#peripherals-supported-by-default)) 

- Private Preview of Azure IoT PnP: https://github.com/Azure/Azure-IoT-PnP-Preview/. The included build scripts will automatically clone the required Azure IoT PnP C SDK for you.

- Private Preview of Azure IoT Central at https://aka.ms/iotc-demo (optional, a fully-managed IoT SaaS solution with UI that can be used to monitor and manage your device from Azure)

- Azure IoT Edge (optional, can be used to help deploy, run, and manage software on the device)

## Azure PnP Bridge Architecture
![Architecture](./PnpBridge/docs/Pictures/AzurePnPBridge.png)

## Peripherals supported by default

Azure PnP Bridge supports the following types of peripherals by default. Developers can extend the Azure PnP Bridge to support additional peripherals via the instructions in the [PnP Bridge documentation](./PnpBridge/ReadMe.md).
- Peripherals controlled by MCUs that are accessed over serial ports:
    - Refer to the [SerialPnP documentation](./SerialPnP/Readme.md) for information on how such devices should self-describe their interfaces to the PnP Bridge
- USB peripherals that can be discovered by Windows
- All cameras supported by Windows OS (Webcam, MIPI cameras and IP of RGB/IR/Depth cameras).

## Get Started

Follow [PnPBridge\Readme.md](./PnpBridge/ReadMe.md) to get started on building, deploying and extending the PnP Bridge.


## Contributing
This project welcomes contributions and suggestions. Most contributions require you to agree to a Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us the rights to use your contribution. For details, visit https://cla.microsoft.com. When you submit a pull request, a CLA-bot will automatically determine whether you need to provide a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions provided by the bot. You will only need to do this once across all repos using our CLA. This project has adopted the Microsoft Open Source Code of Conduct. For more information see the Code of Conduct FAQ or contact opencode@microsoft.com with any additional questions or comments.
