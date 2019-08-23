# Building, Deploying and Extending the Azure IoT Plug and Play bridge

## Compile the Bridge

| Platform | Supported |
| :-----------: | :-----------: |
| Windows |  :heavy_check_mark: |
| Linux | :heavy_check_mark: |

### Development Pre-Requisites (for Windows)
* Ensure CMake (https://cmake.org/download/) and Visual Studio 2017 are installed. **CMake should be in the search PATH.**
* Download Windows 17763 SDK: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk

### Build the Azure IoT Plug and Play bridge

#### Step 1: Get the required dependencies for the Azure IoT Plug and Play bridge
After cloning the Azure PnP Bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo:
```
%REPO_DIR%\> cd pnpbridge

%REPO_DIR%\pnpbridge\> git submodule update --init --recursive
```

At this step, if you confirmed you have access to the Azure repo but are not able to successfully clone the required repos, you might be hitting authentication issues. To resolve this, install the [Git Credential Manager for Windows](https://github.com/Microsoft/Git-Credential-Manager-for-Windows/releases)

#### Step 2: Build the Azure IoT Plug and Play bridge

```
%REPO_DIR%\pnpbridge\> cd scripts\windows

%REPO_DIR%\pnpbridge\scripts\windows> build.cmd
```
To use visual studio, open the generated solution file:


```
%REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86

%REPO_DIR%\pnpbridge\cmake\pnpbridge_x86> azure_iot_pnp_bridge.sln
```

This project used CMAKE for generating project files. Any modifications made to the project in visual studio might be lost if the appropriate CMAKE files are not updated.

## Quickstart

To try out Azure IoT Plug and Play bridge, follow the steps bellow:

* Create an Azure IoT device using Microsoft Azure documentation online and obtain the connections string. If using DPS, the bridge supports symmetric (e.g. Shared Access Signature or SAS) key for securely connecting to the device.
* Modify the folowing parameters under **pnp_bridge_parameters** node in the config file (config.json):

  Using Connection string:

  ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[CONNECTION STRING]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        }
      }
    }
  ```
  Or using DPS:

  ```JSON
  {
      "connection_parameters": {
        "connection_type" : "dps",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        },
        "dps_parameters" : {
          "global_prov_uri" : "[GLOABL PROVISIONING URI]",
          "id_scope": "[IOT HUB ID SCOPE]",
          "device_id": "[DEVICE ID]",
          "device_capability_model_uri": "[DEVICE CAPABILITY MODEL URI]",
          "model_repository_uri": "[MODEL REPOSITORY URI]"
        }
      }
    }
  ```
  > Note: If using Azure IoT Central, the primary connection fields you will need to change in the default config file are id_scope, device_id, symmetric_key, and device_capability_model_uri. Refer to the [Azure IoT Central documentation on device connectivity](https://docs.microsoft.com/en-us/azure/iot-central/concepts-connectivity-pnp?toc=/azure/iot-central-pnp/toc.json&bc=/azure/iot-central-pnp/breadcrumb/toc.json) for how to generate the id_scope, device_id, and symmetric_key for your device. The device_capability_model_uri is the "Id" that is listed for your device's Device Capability Model in Azure IoT Central.

* Start PnpBridge by running it in a command prompt.

  ```
  %REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86\src\pnpbridge\samples\console

  %REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\pnpbridge\samples\console>    Debug\pnpbridge_bin.exe
  ```

  > Note: If you have either a built-in camera or a USB camera connected to your   PC running the PnpBridge, you can start an application that uses camera, such as the built-in "Camera" app.  Once you started running the Camera app, PnpBridge console output window will show the monitoring stats and the framerate of the camera will be reported through Azure IoT PnP interface to Azure.     

## Azure IoT Plug and Play bridge Components

### Discovery Adapter

A discovery adapter is a device watcher that is capable of reporting devices back to the Azure IoT Plug and Play Bridge (PnP Bridge) in the form of a json message. These messages are used by the bridge to match a device from the Azure IoT Plug and Play bridge configuration and call into a PnP adapter to publish a Azure IoT PnP interface.
The message format should conform to below template:

```JSON
{
  "identity": "<DiscoveryAdapterIdentity>",
  "match_parameters": {
    "<UserDefinedKey1>" : "<UserDefinedValue1>",
    "<UserDefinedKey2>" : "<UserDefinedValue2>"
  }
}
```

For devices that self describe Azure IoT PnP interface(s), the message above should contain "InterfaceId" in the root document object.

There are currently 3 discovery adapters in PnP Bridge:

* WindowPnPDiscovery: Reports a PnP device based on a device class provided as part of PnP Bridge configuration
* CameraDisovery: Reports a camera on Windows
* SerialPnp: Reports a [serialpnp device](./../serialpnp/Readme.md). This allows devices like MCUs to self describe Azure IoT PnP interface.

Here's an example output of json message published by WindowPnPDiscovery adapter:

```JSON
{
  "identity": "windows-pnp-discovery",
  "match_parameters": {
    "hardware_id" : "USB\\VID_045E&PID_07CD"
  }
}
```

If there are any initial parameters that needs to be passed as part of the Discovery Adapter's StartDiscovery method, these parameters can be specified in PnP Bridge Configuration. For example, we want the WindowPnPDiscovery to report device that register USB device interface class.

```JSON
"discovery_adapters": {
    "parameters": [
      {
        "identity": "core-device-discovery",
        "device_interface_classes": [
          "A5DCBF10-6530-11D2-901F-00C04FB951ED"
        ]
      }
    ]
}
```

### PnP Adapter

A PnP Adapter implements the bindings of Azure IoT PnP interface. There are currently 3 PnP adapters in the Azure IoT Plug and Play bridge:

* CoreDeviceHealth: Implements basic device health interface
* CameraPnP: Implements camera specific health interface
* SerialPnp: Implements interfaces associated with MCUs or other devices that are [SerialPnP protocol compliant](https://github.com/Azure/AzurePnPBridgePreview/blob/master/serialpnp/Readme.md)

### Configuration file

PnpBridge uses a configuration file to get the IoT Hub connection settings and to configure devices for which PnP interfaces will be published. In the future this configuration could come from cloud.

The schema for the conifguration file is located under:
**src\pnpbridge\src\pnpbridge_config_schema.json**.

Use this with VS code while authoring a PnpBridge configuration file to get schema validation.

## Authoring new PnP Bridge Adapters

To extend Azure IoT Plug and Play bridge in order to support new device discovery and implement new Azure IoT PnP interfaces, follow the steps below. All the API declarations are part of "PnpBridge.h". 

* Create a Discovery Adapter

  If an existing discovery adapter's message is sufficient to identify a device then you can skip implementing Discovery adapter. To implement a new Discovery adapter, implement the methods in the structure below, populate a static instance of the structure:

  ```C
    typedef struct _DISCOVERY_ADAPTER {
        // Identity of the Discovery Adapter
        const char* identity;

        // Discovery Adapter wide initialization callback
        DISCOVERYADAPTER_START_DISCOVERY startDiscovery;
        
        // Discovery Adapter wide shutdown callback
        DISCOVERYADAPTER_STOP_DISCOVERY stopDiscovery;
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

  The API's above are described in pnpadapter_api.h

  When a `Pnp Adapter` `createPnpInterface` callback is invoked, it should create a `PNP_INTERFACE_CLIENT_HANDLE` using the PnP SDK and call the PnpBridge's `PnpAdapterInterface_Create` API. An adapter can create multiple interfaces within a `createPnpInterface` callback. 

* Enable the adapters in PnP Bridge by adding a reference to these adapters in adapters/src/shared/adapter_manifest.c:

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
  "devices": [
      {
        "_comment": "MyDevice",
        "match_filters": {
          "match_type": "Exact",
          "match_parameters": {
            "my_custom_identity": "my-sample-device"
          }
        },
        "interface_id": "http://contoso.com/mypnpinterface/1.0.0",
        "pnp_parameters": {
          "identity": "my-pnp-adapter"
        }
      }
    ]
  ```

Note: PnpBridge adapter callbacks are invoked in a sequential fashion. An adapter shouldn't block a callback since this will prevent PnpBridge from making forward progress.

## Sample Camera Adapter
The following [readme](src/adapters/src/camera/readme.md) provides details on a sample camera adapter that can be enabled with this preview.

## Folder Structure

### /deps/azure-iot-sdk-c-pnp

Git submodules that contains Azure IoT C PNP SDK code

### /scripts

Build scripts

### /src/pnpbridge

Source code for PnpBridge core

### /src/adapters

Source code for various PnpBridge adapters

### Support

For any questions raise an issue or contact - [Azure IoT PnP Bridge](mailto:pnpbridge@microsoft.com)
