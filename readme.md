
# Azure PnP Bridge

## Introduction
Azure PnP Bridge is the open source effort from Microsoft to bridge the PC sensor and peripheral ecosystem with Azure, so that IoT developers can easily connect, remotely monitor, and remotely manage their IoT sensors and peripherals on a large scale. By bridging IoT sensors and peripherals with Azure, the Azure PnP Bridge also enables developers to perform data analytics from either the edge or the cloud to gain insights on their IoT devices.

Azure PnP Bridge can be deployed as a standalone executable that can be dropped onto any IoT device PC, or gateway running Windows 10. It can also be compiled into your application code. A simple configuration file tells the Azure PnP Bridge which sensors and peripherals should be exposed up to Azure. Once configured, the Azure PnP Bridge uses the Azure IoT PnP SDK to dynamically publish Azure IoT PnP interfaces for the specified sensors and peripherals up to Azure. Developers can then use existing Azure services and solution accelerators to receive data from and send commands to their sensors and peripherals. 


## <a id="pre-requisites"></a>Pre-Requisites
- Windows 10 OS:
  - For Camera health monitoring functionality, current Insider Preview OS version is required.
    - All other functionality is available on all Windows 10 builds. 
  - All Windows SKUs are supported. For example:
    - Windows IoT Enterprise
    - Windows Server
    - Windows Desktop
    - Windows IoT Core

- Hardware:
  - Any hardware capable supporting above OS SKUs and versions.
  - PnP Bridge is extensible and you can write simple adapters to support additional peripherals. 
    [See peripherals section below](#peripherals-supported-by-default)

- Private Preview of Azure IoT Central at https://aka.ms/iotc-demo.

- IoT Edge (optional, can be used to help deploy, run, and manage software on the device)

## Azure PnP Bridge Architecture
![Architecture](./PnpBridge/docs/Pictures/AzurePnPBridge.png)

## Using the Azure PnP Bridge

### Required development setup
- In order to build Private Preview Azure PnP Bridge, you need to join Microsoft Azure team: https://github.com/Azure

- Ensure cmake and Visual Studio 2017 are installed

- Download Windows 17763 SDK: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk

### Peripherals supported by default

Azure PnP Bridge supports the following types of peripherals by default. Developers can extend the Azure PnP Bridge to support additional peripherals. See [readme under PnPBridge](PnpBridge\readme.md)
- Peripherals such as MCUs that are typically accessed over serial ports:
    - Refer to the [SerialPnP\Readme](SerialPnP\readme.md) for information on how such devices can self-describe their interfaces.
- USB or other peripherals that can be discovered by Windows
- All cameras supported by Windows OS (Webcam, MIPI cameras and IP of RGB/IR/Depth cameras).

See [PnPBridge\Readme.md](PnpBridge\readme.md) for information on how to write simple adapters to extend to other peripherals.

### Get Started

Follow [PnPBridge\Readme.md](PnpBridge\readme.md) to get started on building, deploying and extending the PnP Bridge.


## Contributing
This project welcomes contributions and suggestions. Most contributions require you to agree to a Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us the rights to use your contribution. For details, visit https://cla.microsoft.com. When you submit a pull request, a CLA-bot will automatically determine whether you need to provide a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions provided by the bot. You will only need to do this once across all repos using our CLA. This project has adopted the Microsoft Open Source Code of Conduct. For more information see the Code of Conduct FAQ or contact opencode@microsoft.com with any additional questions or comments.
