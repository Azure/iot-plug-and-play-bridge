# Building, Deplying and Extending the Azure IoT PnP Bridge

## Compile the Bridge

| Platform | Supported |
| :-----------: | :-----------: |
| Windows |  :heavy_check_mark: |
| Linux | :heavy_multiplication_x: |

### Development Pre-Requisites
* In order to build Private Preview Azure PnP Bridge, you need to join Microsoft Azure team: https://github.com/Azure
* Ensure CMake and Visual Studio 2017 are installed. **CMake should be in the search PATH.**
* Download Windows 17763 SDK: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk

### Build the Azure IoT PnP Bridge

#### Step 1: Get the required dependencies for the Azure PnP Bridge
After cloning the Azure PnP Bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo:
```
%REPO_DIR%\> cd PnpBridge

%REPO_DIR%\PnpBridge\> git submodule update --init --recursive
```

At this step, if you confirmed you have access to the Azure repo but are not able to successfully clone the required repos, you might be hitting authentication issues. To resolve this, install the [Git Credential Manager for Windows](https://github.com/Microsoft/Git-Credential-Manager-for-Windows/releases)

#### Step 2: Build the prerequisite components for the Azure PnP Bridge
```
%REPO_DIR%\PnpBridge\> cd Scripts

%REPO_DIR%\PnpBridge\Scripts\> build.prereq.cmd
```

#### Step 3: Build the Azure PnP Bridge

1. Open `\PnpBridge\src\PnpBridge\PnpBridge.vcxproj` with "Visual Studio 2017"
2. Ensure build target matches the build.prereq.cmd's build (e.g. Release|Debug and x86|x64)
3. Build the solution
4. The built `Pnpbridge.exe` is the Azure PnP Bridge binary

## Quickstart

To try out Azure IoT PnP Bridge, follow the steps bellow:

* Create an Azure IoT device using Microsoft Azure documentation online and obtain the connections string. If using DPS, the bridge supports symmetric (e.g. Shared Access Signature or SAS) key for securely connecting to the device.
* Modify the folowing parameters under PnpBridgeParameters node in the config file (config.json):

  Using DPS:

  ```JSON
    {
      "ConnectionType": "dps",
      "DpsSettings": {
        "GlobalProvUri": "[DPS URI]",
        "IdScope": "[ID Scope]",
        "DeviceId": "[Device ID",
        "DeviceKey": "[Device primary key]=",
        "DeviceCapabilityModelUri": "[Device capability model URI]",
        "ModelRepositoryUri": "[Model repository URI]"
      }
    }
  ```
  > Note: If using Azure IoT Central, the primary connection fields you will need to change in the default config file are IdScope, DeviceId, DeviceKey, and DeviceCapabilityModelUri. Refer to the [Azure IoT Central documentation on device connectivity](https://docs.microsoft.com/en-us/azure/iot-central/concepts-connectivity) for how to generate the IdScope, DeviceId, and DeviceKey for your device. The DeviceCapabilityModelUri is the "Id" that is listed for your device's Device Capability Model in Azure IoT Central.

  Using your own connection string:

  ```JSON
  {
    "ConnectionType": "ConnectionString",
    "ConnectionString": "[Connection String]"
  }
  ```

* Start PnpBridge by running it in a command prompt.
  + Note: If you have either a built-in camera or a USB camera connected to your PC running the PnpBridge, you can start an application that uses camera, such as the built-in "Camera" app.  Once you started running the Camera app, PnpBridge console output window will show the monitoring stats and the framerate of the camera will be reported through Azure IoT PnP interface to Azure.     

## PnP Bridge Components

### Discovery Adapter

A discovery adapter is a device watcher that is capable of reporting devices back to PnP Bridge in the form of a json message. These messages are used by PnpBridge to match a device from the PnP Bridge configuration and call into a PnP adapter to publish a Azure IoT PnP interface.
The message format should conform to below template:

```JSON
{
  "Identity": "<DiscoveryAdapterIdentity>",
  "MatchParameters": {
    "<UserDefinedKey1>" : "<UserDefinedValue1>",
    "<UserDefinedKey2>" : "<UserDefinedValue2>"
  }
}
```

For devices that self describe Azure IoT PnP interface(s), the message above should contain "InterfaceId" in the root document object.

There are currently 3 discovery adapters in PnP Bridge:

* WindowPnPDiscovery: Reports a PnP device based on a device class provided as part of PnP Bridge configuration
* CameraDisovery: Reports a camera on Windows
* SerialPnp: Reports a [SerialPnP device](./../SerialPnP/Readme.md). This allows devices like MCUs to self describe Azure IoT PnP interface.

Here's an example output of json message published by WindowPnPDiscovery adapter:

```JSON
{
  "Identity": "windows-pnp-discovery",
  "MatchParameters": {
    "HardwareId" : "USB\\VID_045E&PID_07CD"
  }
}
```

If there are any initial parameters that needs to be passed as part of the Discovery Adapter's StartDiscovery method, these parameters can be specified in PnP Bridge Configuration. For example, we want the WindowPnPDiscovery to report device that register USB device interface class.

```JSON
"DiscoveryAdapters": {
    "Parameters": [
      {
        "Identity": "windows-pnp-discovery",
        "DeviceInterfaceClasses": [
          "A5DCBF10-6530-11D2-901F-00C04FB951ED"
        ]
      }
    ]
}
```

### PnP Adapter

A PnP Adapter implements the bindings of Azure IoT PnP interface. There are currently 3 PnP adapters in the PnP Bridge:

* CoreDeviceHealth: Implements basic device health interface
* CameraPnP: Implements camera specific health interface
* SerialPnp: Implements interfaces associated with MCUs or other devices that are [SerialPnP protocol compliant](./../SerialPnP/Readme.md)

### Configuration file

PnpBridge uses a configuration file to get the IoT hub connection settings and to configure devices for which PnP interfaces will be published. In the future this configuration could come from cloud.

Following are the valid configuration fields:

```YAML
  PnpBridgeParameters: Json object containing various PnpBridge connectivity and global parameters [REQUIRED]
    ConnectionType: Connection type to use for IoT Hub. The two options are using a `ConnectionString` or `dps` [REQUIRED 
    DpsSettings: Json object containg DPS settings [REQUIRED]
      GlobalProvUri: DPS endpoint URI
      IdScope: ID scope
      DeviceId: Unique device id
      DeviceKey: Primary device key
      DeviceCapabilityModelUri: Device capability model URI
      ModelRepositoryUri: Model repository URI
    ConnectionString: Connection String
    traceOn: Enable tracing [OPTIONAL] [Default is true]
  
  Devices: Json array containing devices for which PnP interface needs to be published. The
           parameters are used to describe a device and bind a PnP Interface [REQUIRED] [JSON_ARRAY]
      MatchFilters: Set of identifiers to match the device reported by discovery adapter [REQUIRED]
        MatchType: Exact or Contains. Performs a comparision between the MatchParameters
                   elements and the MatchParameters provided by DiscoveryAdapter [REQUIRED]
        MatchParameters: Json array containing user defined parameters [REQUIRED]
      InterfaceId: Interface Id to be published for this device. This can be skipped if a
                   device is self describing [REQUIRED/ Except when SelfDescribing = true]
      SelfDescribing: This device will report the PnP interface schema and the InterfaceId.
                      When using this option, DiscoveryParameters must be specified [OPTIONAL] [true/false]
      PnpParameters: PnP adapter parameters is MUST for every device. This is used to bind 
                     the right PnpAdapter for this device [REQUIRED]
        Identity: PnpAdapter Identity
      DiscoveryParameters: Discovery parameters are used in case of self describing devices [OPTIONAL]
        Identity: Discovery adapter Identity is used to map this device to discovery adapter [REQUIRED]
  DiscoveryAdapters: Json object with Discovery Adapter parameters
    Parameters: Json Array containing user defined Initialization arguments for the discovery adapter
      Identity: Discovery adapter Identity used to map the discovery adapter [REQUIRED]
```

## Authoring new PnP Bridge Adapters

To extend PnP Bridge in order to support new device discovery and implement new Azure IoT PnP interfaces, follow the steps below. All the API declarations are part of "PnpBridge.h". 

* Create a Discovery Adapter

  If an existing discovery adapter's message is sufficient to identify a device then you can skip implementing Discovery adapter. To implement a new Discovery adapter, implement the methods in the structure below, populate a static instance of the structure:

  ```C
    typedef struct _DISCOVERY_ADAPTER {
        // Identity of the Discovery Adapter
        const char* identity;

        // Discovery Adapter wide initialization callback
        DISCOVERYAPAPTER_START_DISCOVER startDiscovery;
        
        // Discovery Adapter wide shutdown callback
        DISCOVERYAPAPTER_STOP_DISCOVERY stopDiscovery;
    } DISCOVERY_ADAPTER, *PDISCOVERY_ADAPTER;
  ```

  The API's above are described in DiscoveryAdapterInterface.h

* Create a PnP Adapter

  This adapter implements the Azure IoT PnP interface for a device. Following callbacks needs to be implemented:

  ```C
    typedef struct _PNP_ADAPTER {
        // Identity of the Pnp Adapter
        const char* Identity;

        // Discovery Adapter wide initialization callback 
        PNPADAPTER_PNP_INTERFACE_INITIALIZE initialize;

        // PnpBridge calls this when a matching device is found
        PNPADAPTER_BIND_PNP_INTERFACE createPnpInterface;

        // PnP Adapter wide shutdown callback
        PNPADAPTER_PNP_INTERFACE_SHUTDOWN shutdown;
    } PNP_ADAPTER, *PPNP_ADAPTER;
  ```

  The API's above are described in PnpAdapterInterface.h

  When a `Pnp Adapter` gets a `createPnpInterface` callback, it should create a `PNP_INTERFACE_CLIENT_HANDLE` using the PnP SDK and call the PnpBridge's `PnpAdapterInterface_Create` API. An adapter can create multiple interfaces within a `createPnpInterface` callback.  

* Enable the adapters in PnP Bridge by adding a reference to these adapters in Adapters/AdapterManifest.c:

  ```C
    extern DISCOVERY_ADAPTER MyDiscoveryAdapter;
    PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[] = {
      .
      .
      &MyDiscoveryAdapter
    }

    extern PNP_ADAPTER MyPnpAdapter;
    PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
      .
      .
      &MyPnpAdapter
    }
  ```

* The discovery adapter will be started automatically. To publish an Azure PnP interface add a configuration entry in config.json. In the below example, the PnpAdapter identity used is "my-pnp-adapter".

  ```JSON
  "Devices": [
      {
        "_comment": "MyDevice",
        "MatchFilters": {
          "MatchType": "Exact",
          "MatchParameters": {
            "MyCustomIdentity": "MySampleDevice"
          }
        },
        "InterfaceId": "http://contoso.com/mypnpinterface/1.0.0",
        "PnpParameters": {
          "Identity": "my-pnp-adapter"
        }
      }
    ]
  ```

Note: Adapter callbacks are invoked in a sequential fashion. An adapter shouldn't block a callback since this will prevent PnpBridge from making forward progress.

## Folder Structure

### /deps/azure-iot-sdk-c-pnp

Git submodules that contains Azure IoT C PNP SDK code

### /scripts

Build scripts

### /src/PnpBridge

Source code for Azure IoT PnP Bridge

### Support

For any questions raise an issue or contact - [Azure IoT PnP Bridge](mailto:bf1dde55.microsoft.com@amer.teams.ms)

