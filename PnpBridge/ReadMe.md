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
  * Open src\PnpBridge\PnpBridge.vcxproj with "Visual Studio 2017"
  * Ensure build target is set to Release|Debug and x86|x64 matches to the build.prereq.cmd's build and build the solution
  * Pnpbridge.exe is the Azure PnP Bridge binary

## Quickstart

To try out Azure IoT PnP Bridge, follow the steps bellow:

* Create an Azure IoT device using Microsoft Azure documentation online. Obtain the connections string.
* Modify the folowing config parameters:
  * Update the connection string in "ConnectionString" key under "PnpBridgeParameters".
* Start PnpBridge by running it in a command prompt.

## PnP Bridge Components

PnP Bridge has two types of adapters:

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

For devices that self-describe an Azure IoT PnP interface, the template should contain "InterfaceId" in the root document object.

There are currently 3 discovery adapters in PnP Bridge:

* WindowPnPDiscovery: Reports a PnP device based on a device class provided as part of PnP Bridge configuration
* CameraDisovery: Reports a camera on windows
* SerialPnp: Reports a serial PnP device. This uses self describing Azure IoT PnP interface.

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

A PnP Adapter implements the bindings of Azure IoT PnP interface. 

There are currently 3 discovery adapters in PnP Bridge:

* CoreDeviceHealth: Implements basic device health interface
* CameraPnP: Implements camera specific health interface
* SerialPnp: Binds to the PnP interface reported by the MCU

## Authoring new PnP Bridge Adapters

To extend PnP Bridge in order to support new device discovery and implement new Azure IoT PnP interfaces, follow the steps below. All the API declarations are part of "PnpBridge.h". 

* Create a Discovery Adapter

  If an existing discovery adapter's message is sufficient to identify a device then you can skip implementing Discovery adapter. To implement a new Discovery adapter, implement the methods in the structure below, populate a static instance of the structure:

  ```C
    typedef struct _DISCOVERY_ADAPTER {
        // Identity of the Discovery Adapter
        const char* Identity;

        // Discovery Adapter wide initialization callback
        DISCOVERYAPAPTER_START_DISCOVER StartDiscovery;
        
        // Discovery Adapter wide shutdown callback
        DISCOVERYAPAPTER_STOP_DISCOVERY StopDiscovery;
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
        PNPADAPTER_PNP_INTERFACE_INITIALIZE Initialize;

        // PnpBridge calls this when a matching device is found
        PNPADAPTER_BIND_PNP_INTERFACE CreatePnpInterface;

        // PnpBridge calls this during shutdown for each interface
        PNPADAPTER_RELEASE_PNP_INTERFACE ReleaseInterface;

        // PnP Adapter wide shutdown callback
        PNPADAPTER_PNP_INTERFACE_SHUTDOWN Shutdown;
    } PNP_ADAPTER, *PPNP_ADAPTER;
  ```

  The API's above are described in PnpAdapterInterface.h

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

* The discovery adapter will be started automatically. To publish a Azure PnP interface add a configuration entry. In the below example, the PnpAdapter identity uses is "my-pnp-adapter".

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
        "InterfaceId": "http://contoso.com/mypnpinterface/v1.0.0",
        "PnpParameters": {
          "Identity": "my-pnp-adapter"
        }
      }
    ]
  ```



## Folder Structure

### /deps/azure-iot-sdk-c-pnp

Git submodules that contains Azure IoT C PNP SDK code

### /scripts

Build scripts

### /src/PnpBridge

Source code for Azure IoT PnP Bridge

### Support

For any questions contact - [Azure IoT PnP Bridge](mailto:bf1dde55.microsoft.com@amer.teams.ms)

