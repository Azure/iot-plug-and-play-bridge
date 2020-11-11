
# Building, Deploying, and Extending the IoT Plug and Play bridge

To get started with a simple example, view the [Reference Documentation](https://www.aka.ms/iot-pnp-bridge-docs).

These instructions and samples assume basic familiarity with Azure Digital Twin and IoT Plug and Play concepts.  To learn more background information, see [here](https://aka.ms/iotpnpdocs). This section describes the following:

* [IoT Plug and Play bridge Components](#IoT-Plug-and-Play-bridge-Components)
* [Extending IoT Plug and Play bridge: Authoring new PnP Bridge Adapters](#Extending-IoT-Plug-and-Play-bridge-Authoring-new-PnP-bridge-Adapters)
* [Building and Running the IoT Plug and Play bridge on an IoT device or gateway](#building-and-running-the-iot-plug-and-play-bridge-on-an-iot-device-or-gateway)
* [Building and Running the IoT Plug and Play bridge as an edge module on IoT Edge Runtime](#building-and-running-the-iot-plug-and-play-bridge-as-an-edge-module-on-iot-edge-runtime)
* [Folder Structure](#Folder-Structure)
* [Support](#Support)

## IoT Plug and Play bridge Components

![A diagram that outlines the structure of the IoT Plug and Play bridge. It consists of an adapter manager, and pnp bridge adapters that bind devices/peripherals to Azure Digital Twin Interfaces.](/pnpbridge/docs/Pictures/AzurePnPBridgeComponents.png)

### PnP Bridge Adapters

The IoT Plug and Play bridge supports a set of PnP Bridge Adapters for various types of devices. These adapters are statically manifested using an adapter manifest and used by the IoT Plug and Play bridge’s PnP Bridge Adapter manager to elicit appropriate adapter functions. The adapter manager only initiates adapter creation for adapters that are required by the interface components defined in the configuration file. A corresponding PnP Bridge Adapter is elicited once for each interface component’s creation. The PnP Bridge Adapter sets up interface resources to create and acquire a digital twin interface handle and bind all the required interface functionality of the device with the digital twin. With information from the configuration file, the PnP Bridge Adapter establishes a communication channel or creates a device watcher to wait for such a channel to become available which when successfully completes enables the full device to digital twin communication through the IoT Plug and Play bridge.

### Configuration JSON

The IoT Plug and Play bridge is supplied with a JSON based configuration that generally specifies the following pieces of information:

* How to make a connection with Azure IoT, i.e. via connection strings, authentication parameters or device provisioning information if the connection is made using DPS
* The location of the device capability model that will be used by the Pnp Bridge (static and immutable for now) which defines capabilities of an IoT plug and play device
* A list of interface components (aka devices) and the following information regarding each component:
* The interface id and component name
* The PnP Bridge Adapter required to interact with this component
* Device information that the PnP Bridge Adapter would require to establish communication with the device such as hardware ID or adapter/interface/protocol specific info
* An optional PnP Bridge Adapter “sub-type” or an “interface configuration” if the adapter supports communication with similar devices in multiple ways
For example, a bluetooth sensor interface component could be configured like this:

```JSON
  {
        "_comment": "Component BLE sensor",
        "pnp_bridge_component_name": "blesensor1",
        "pnp_bridge_adapter_id": "bluetooth-sensor-pnp-adapter",
        "pnp_bridge_adapter_config": {
            "bluetooth_address": "267541100483311",
            "blesensor_identity" : "Blesensor1"
        }
  }
  ```

* An optional list of global PnP Bridge Adapter parameters with information that is used to create the adapters
For example the Bluetooth Sensor PnP Bridge Adapter has a dictionary (name : interface configuration) of supported configurations it supports, and an interface component which requires the bluetooth sensor adapter ("pnp_bridge_adapter_id": "bluetooth-sensor-pnp-adapter") can pick one of these as its desired "blesensor_identity":

```JSON
{
  "pnp_bridge_adapter_global_configs": {
      "bluetooth-sensor-pnp-adapter": {
          "Blesensor1" : {
              "company_id": "0x499",
              "endianness": "big",
              "telemetry_descriptor": [
                {
                  "telemetry_name": "humidity",
                  "data_parse_type": "uint8",
                  "data_offset": 1,
                  "conversion_bias": 0,
                  "conversion_coefficient": 0.5
                },
                {
                  "telemetry_name": "temperature",
                  "data_parse_type": "int8",
                  "data_offset": 2,
                  "conversion_bias": 0,
                  "conversion_coefficient": 1.0
                },
                {
                  "telemetry_name": "pressure",
                  "data_parse_type": "int16",
                  "data_offset": 4,
                  "conversion_bias": 0,
                  "conversion_coefficient": 1.0
                },
                {
                  "telemetry_name": "acceleration_x",
                  "data_parse_type": "int16",
                  "data_offset": 6,
                  "conversion_bias": 0,
                  "conversion_coefficient": 0.00980665
                },
                {
                  "telemetry_name": "acceleration_y",
                  "data_parse_type": "int16",
                  "data_offset": 8,
                  "conversion_bias": 0,
                  "conversion_coefficient": 0.00980665
                },
                {
                  "telemetry_name": "acceleration_z",
                  "data_parse_type": "int16",
                  "data_offset": 10,
                  "conversion_bias": 0,
                  "conversion_coefficient": 0.00980665
                }
              ]
          }
      }
  }
}
```

There are currently 6 PnP Bridge Adapters supported by the Pnp Bridge:

* [Core Device Health Adapter](./docs/coredevicehealth_adapter.md): Connects devices with a specific hardware ID based on a list of adapter-supported device interface classes
* [Camera Adapter](./src/adapters/src/Camera/readme.md): Connects cameras on Windows
* [SerialPnp Adapter](./../serialpnp/Readme.md): Connects devices that can communicate over the SerialPnp Protocol.
* [Modbus Adapter](./docs/modbus_adapters.md): Connects sensors connected via Modbus
* [BluetoothSensor Adapter](./docs/bluetooth_sensor_adapter.md): Connects detected BLE Bluetooth sensors 
* [MQTT Adapter](./docs/mqtt_adapter.md): Connects detected MQTT messages from a broker

Here's an example output of an interface component that uses the core device health adapter discovered via a Windows device watcher:

```JSON
{
      "_comment": "USB device",
      "pnp_bridge_component_name": "USBDevice",
      "pnp_bridge_adapter_id": "core-device-health",
      "pnp_bridge_adapter_config": {
          "hardware_id": "USB\\VID_06CB&PID_00B3"
      }
}
```

Since this interface component has specified its hardware ID, the core device health adapter will look through all interfaces with interface classes it supports and connect to the matching hardware ID if or when it finds it. The global adapter parameters for the core device health adapter looks like this:

```JSON
{
  "core-device-health": {
          "device_interface_classes": [
              "A5DCBF10-6530-11D2-901F-00C04FB951ED"
            ]
      }
}
```

### Configuring Plug and Play Bridge
Depending on whether the IoT Plug and Play bridge is being run natively on an IoT device or gateway (Linus or Windows) or as edge module on an IoT edge runtime (Linux) configuration happens in one of two ways:

- If the Plug and Play bridge is running natively on an IoT device or gateway, use the [configuration file](#configuration-file) to configure the bridge.
- If the Plug and Play bridge is running as an edge module on an IoT edge runtime, use the [desired property](#edge-module-configuration-using-desired-property) of the module twin to configure the bridge.

### Configuration file

When Plug and Play bridge is running natively on an IoT device or gateway,it uses a configuration file to get the IoT Central / IoT Hub connection settings and to configure devices for which pnp interfaces will be published. The path to this configuration file can either be supplied to the bridge executable as a command line parameter or the binplaced config.json in the `pnpbridge\cmake\pnpbridge_x86\src\pnpbridge\samples\console` directory can be modified directly. In the future the configuration could come from cloud.

The schema for the configuration file is located under [**src\pnpbridge\src\pnpbridge_config_schema.json**](./src/pnpbridge/src/pnpbridge_config_schema.json).

Use this with VS code while authoring a PnpBridge configuration file to get schema validation.

### Edge Module Configuration Using Desired Property

When Plug and Play bridge is running as an edge module on an IoT edge runtime, the JSON payload for the configuration, which tells the IoT Plug and Play bridge which sensors and peripherals should be exposed up to Azure, must be sent from the cloud to the module in the form of a desired property update for `PnpBridgeConfig`. The Plug and Play bridge will wait for this property update from the module twin to begin adapter and component configuration.

## Extending IoT Plug and Play bridge: Authoring new PnP Bridge Adapters

The IoT Plug and Play bridge establishes cloud connections and facilitates data flow and device management of IoT devices using PnP Bridge Adapters. There are two main functions of the PnP Bridge Adapter:

1. The creation of [Digital Twin Interfaces](https://github.com/Azure/azure-iot-sdk-c/blob/public-preview/digitaltwin_client/doc/interfaces.md) and binding device side functionality with the cloud-based capabilities such as the flow of telemetry or property and command updates.
2. The establishment of control and data communication with the device hardware or firmware.

Each PnP Bridge Adapter is designed to specifically interact with certain types of devices based on how connections are made or how device side communication is established. However, even if the communication happens over some handshaking protocol, a PnP Bridge Adapters may have multiple ways of interpreting the data from a supported device. In such cases, an interface component in the configuration file that specifies this adapter to be its associated PnP Bridge Adapters, should also specify which “interface configuration” its adapter must use in order to parse the data coming from the device.

The PnP Bridge Adapter interacts with the device using whichever communication protocol it supports and APIs provided by the underlying operating system (Windows, Linux or uses a platform abstraction layer) or vendor provided APIs for interaction with custom hardware/firmware. On the cloud side, the PnP Bridge Adapter uses APIs provided by the Azure IoT Device C SDK to create digital twin interfaces and bind callback functions to report updates to telemetry, properties or commands.

### Create a PnP Bridge Adapter

  The bridge provides the PnP Bridge Adapter with a list of adapter APIs that are defined in pnpadapter_api.h and expects each adapter to implement the following interfaces:

  ```C
    typedef struct _PNP_ADAPTER {
        // Identity of the pnp adapter that is retrieved from the config
        const char* identity;

        PNPBRIDGE_ADAPTER_CREATE createAdapter;
        PNPBRIDGE_COMPONENT_CREATE createPnpComponent;
        PNPBRIDGE_COMPONENT_START startPnpComponent;
        PNPBRIDGE_COMPONENT_STOP stopPnpComponent;
        PNPBRIDGE_COMPONENT_DESTROY destroyPnpComponent;
        PNPBRIDGE_ADAPTER_DESTOY destroyAdapter;
    } PNP_ADAPTER, * PPNP_ADAPTER;

  ```

  The API's above are described in [pnpadapter_api.h](src/pnpbridge/inc/pnpadapter_api.h)

### Brief description of the PnP Bridge Adapter interface

1. `PNPBRIDGE_ADAPTER_CREATE` creates the adapter and sets up resources for generic interface management. Adapter may also rely on global adapter parameters for adapter creation. This is called once for a single adapter.
2. `PNPBRIDGE_COMPONENT_CREATE` creates digital twin client interfaces and binds callback functions. The adapter starts initiating the communication channel (an active connection) to the device. The adapter may set up resources to start the flow telemetry but does not start reporting telemetry until `PNPBRIDGE_COMPONENT_START` is called. This call is made once for each interface component in the configuration file. 
3. `PNPBRIDGE_COMPONENT_START` is called to allow the PnP Bridge Adapter to start reporting telemetry from the device to the digital twin client. This call is made once for each interface component in the configuration file.
4. `PNPBRIDGE_COMPONENT_STOP` stops the flow of telemetry. 
5. `PNPBRIDGE_COMPONENT_DESTROY` destroys the digital twin client and associated interface resources. This call is made once for each interface component in the configuration file when the bridge tears down or when a fatal error occurs.
6. `PNPBRIDGE_ADAPTER_DESTROY` cleans up adapter resources.

### Bridge Core's Interaction with PnP Bridge Adapters

When the bridge starts, the PnP Bridge Adapter manager looks through each interface component that is in the configuration file and calls `PNPBRIDGE_ADAPTER_CREATE` on the appropriate adapter. The adapter may utilize optional global adapter configuration parameters to set up resources to support the various “interface configurations”. For every device in the configuration file, the manager initiates interface creation by calling the corresponding PnP Bridge Adapter’s `PNPBRIDGE_COMPONENT_CREATE`. The adapter receives optional adapter configuration corresponding to the interface component and may use this information to set up connections with the device after create digital twin client interfaces and bind callback functions for property updates and commands. The establishment of device connections should not block the return of this callback after digital twin interface creation succeeds. This is such that the active device connection is made independent of the active interface client which the bridge expects to create. If a connection fails, this will imply the device is inactive and the adapter could choose to retry making this connection later. Once all the interface components specified in the configuration file are created the PnP Bridge Adapter manager curates and compiles the complete list of interfaces and registers them once with Azure IoT. Registration is an asynchronous and blocking call which on completion trigger a registration callback which is handled by the PnP Bridge Adapter after which it also starts handling property update and command callbacks from the cloud. The adapter manager then calls `PNPBRIDGE_INTERFACE_ START` on each component and the corresponding adapter start reporting telemetry to the digital twin client on the IoT Central / IoT Hub.

### General Guidelines to Design a New PnP Bridge Adapter:

1. Determine what device capabilities are supported and what the interface definition of components using this adapter looks like.
2. Determine what information the configuration file must provide to the adapter in terms of global or interface adapter parameters to set up adapter resources.
3. Determine what low-level device communication is required to support the properties and commands of the component. How will the adapter parse the raw data buffers from this type of device and convert it to the telemetry message that the interface definition specifies?
4. Implement the PnP Bridge Adapter interface described above.
5. Add the new adapter to the adapter manifest and build the bridge

### Enabling new PnP Bridge Adapters in the IoT Plug and Play bridge

Enable the adapters in Pnp Bridge by adding a reference to these adapters in adapters/src/shared/adapter_manifest.c:

  ```C
    extern PNP_ADAPTER MyPnpAdapter;
    PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
      .
      .
      &MyPnpAdapter
    }
  ```

>Note: PnP Bridge Adapter callbacks are invoked in a sequential fashion. An adapter shouldn't block a callback since this will prevent the Bridge Core from making forward progress.

### Sample Camera Adapter

The following [readme](./src/adapters/src/Camera/readme.md) provides details on a sample camera adapter that can be enabled with this preview.

## Building and Running the IoT Plug and Play bridge on an IoT device or gateway

| Platform | Supported |
| :-----------: | :-----------: |
| Windows |  :heavy_check_mark: |
| Linux | :heavy_check_mark: |

### Prerequisites

To complete this section, you need to install the following software on your local machine:

* A development environment that supports compiling C++ such as: [Visual Studio (Community, Professional, or Enterprise)](https://visualstudio.microsoft.com/downloads/)- make sure that you include the NuGet package manager component and the Desktop Development with C++ workload when you install Visual Studio.
* [CMake](https://cmake.org/download/) - when you install CMake, select the option `Add CMake to the system PATH`.
* If you are building on Windows, you will also need to download Windows 17763 SDK: [https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)

### Step 1: Get the required dependencies

After cloning the IoT Plug and Play bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo:

```cmd
%REPO_DIR%\> cd pnpbridge

%REPO_DIR%\pnpbridge\> git submodule update --init --recursive
```

>Note: If you run into issues with the git clone sub module update failing, this is a known issue with Windows file paths and git see: [https://github.com/msysgit/git/pull/110](https://github.com/msysgit/git/pull/110) . You can try the following command to resolve the issue: `git config --system core.longpaths true`

### Step 2: Build the Azure IoT Plug and Play bridge (on Windows)

```cmd
%REPO_DIR%\pnpbridge\> cd scripts\windows

%REPO_DIR%\pnpbridge\scripts\windows> build.cmd
```

To use visual studio, open the generated solution file:

```cmd
%REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86

%REPO_DIR%\pnpbridge\cmake\pnpbridge_x86> azure_iot_pnp_bridge.sln
```
Once you open the solution in Visual Studio, build the application. On the menu bar, choose "Build > Build Solution". This project used CMAKE for generating project files. Any modifications made to the project in visual studio might be lost if the appropriate CMAKE files are not updated.

### Step 2: Build the Azure IoT Plug and Play bridge (on Linux)

```bash
 /%REPO_DIR%/pnpbridge/ $ cd scripts/linux

 /%REPO_DIR%/pnpbridge/scripts/linux $ ./setup.sh

 /%REPO_DIR%/pnpbridge/scripts/linux $ ./build.sh
```

### Step 3: Setting up the Configuration JSON for Generic Sensors

Modify the folowing parameters under **pnp_bridge_connection_parameters** node in    the config file  ([%REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\pnpbridge\samples\console](./src/pnpbridge/src/pnpbridge_config_schema.json)):

  Using Connection string (Note: the symmetric_key must match the SAS key in the connection string):

  ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[To fill in]",
        "root_interface_model_id": "[To fill in]",
        "auth_parameters": {
            "auth_type": "symmetric_key",
            "symmetric_key": "[To fill in]"
        }
      }
    }
  ```

  Or using DPS:

  ```JSON
  {
      "connection_parameters": {
        "connection_type" : "dps",
        "root_interface_model_id": "[To fill in]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        },
        "dps_parameters" : {
          "global_prov_uri" : "[GLOBAL PROVISIONING URI] - typically it is global.azure-devices-provisioning.net",
          "id_scope": "[IoT Central / IoT Hub ID SCOPE]",
          "device_id": "[DEVICE ID]"
        }
      }
  }
  ```

In this example we further modified the configuration file.

### Step 4: Start the IoT Plug and Play bridge for Generic Sensors

Start IoT Plug and Play bridge by running it in a command prompt.

  ```cmd
  %REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86\src\pnpbridge\samples\console

  %REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\pnpbridge\samples\console>    Debug\pnpbridge_bin.exe
  ```

  > Tip: The path to the configuration file can also be passed to the bridge executable as a command line parameter.
  
  > Tip: If you have either a built-in camera or a USB camera connected to your PC running the IoT Plug and Play bridge, you can start an application that uses camera, such as the built-in "Camera" app.  Once you started running the Camera app, IoT Plug and Play bridge's console output window will show the monitoring stats and the frame rate of the camera will be reported through Azure IoT Plug and Play (Digital Twin) interface to Azure.

## Building and Running the IoT Plug and Play bridge as an edge module on IoT Edge Runtime

| Platform | Supported |
| :-----------: | :-----------: |
| Windows |  :x: |
| Linux | :heavy_check_mark: |

### Prerequisites

To complete this section, you need to install the following software on your local machine:

* A development environment that supports compiling C++ such as: [Visual Studio (Community, Professional, or Enterprise)](https://visualstudio.microsoft.com/downloads/)- make sure that you include the NuGet package manager component and the Desktop Development with C++ workload when you install Visual Studio.
* [Azure IoT Edge for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.azure-iot-edge), [Azure IoT Edge Tools for Visual Studio 2017](https://marketplace.visualstudio.com/items?itemName=vsc-iot.vsiotedgetools) or [Azure IoT Edge Tools for Visual Studio 2019](https://marketplace.visualstudio.com/items?itemName=vsc-iot.vs16iotedgetools) 
* An Azure IoT Edge device on Linux
* A container engine such as [Docker](https://docs.docker.com/docker-for-windows/install/). IoT Edge modules are packaged as containers, so you need a container engine on your development machine to build and manage them.
* A free or standard-tier [IoT hub](https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-create-through-portal) in Azure.

### Step 1: Get the required dependencies

After cloning the IoT Plug and Play bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo:

```cmd
%REPO_DIR%\> cd pnpbridge

%REPO_DIR%\pnpbridge\> git submodule update --init --recursive
```

>Note: If you run into issues with the git clone sub module update failing, this is a known issue with Windows file paths and git see: [https://github.com/msysgit/git/pull/110](https://github.com/msysgit/git/pull/110) . You can try the following command to resolve the issue: `git config --system core.longpaths true`

### Step 2: Setup Visual Studio tools
To make the building and creation of the module image easy, setup IoT extensions for Visual Studio using instructions [here](https://docs.microsoft.com/en-us/azure/iot-edge/tutorial-develop-for-linux#set-up-vs-code-and-tools).

### Step 3: Create a container registry
You will need the container registry to push the IoT Plug and Play bridge module image onto. Create one using instructions [here](https://docs.microsoft.com/en-us/azure/iot-edge/tutorial-develop-for-linux#create-a-container-registry).

### Step 4: Install Edge Runtime & Create IoT Edge device
 - Create an IoT edge device using IoT Hub
 - Install the edge runtime on your linux machine using instructions from [here](https://docs.microsoft.com/en-us/azure/iot-edge/how-to-install-iot-edge?tabs=linux)
 - Provision the IoT edge device on the edge runtime using the symmetric key. More information can be found [here](https://docs.microsoft.com/en-us/azure/iot-edge/how-to-manual-provision-symmetric-key?tabs=azure-portal%2Clinux#provision-an-iot-edge-device)
 - Restart IoT Edge runtime

### Step 5: Sign into Docker
Provide your container registry credentials to Docker so that it can push your container image to be stored in the registry.

### Step 6: Update Dockerfile
Open pnpbridge\Dockerfile.amd64 (or the dockerfile corresponding to the architecture you want to use) and add/update the following environment variables:

```bash
  ENV IOTHUB_DEVICE_CONNECTION_STRING="<ENTER CONNECTION STRING OF EDGE MODULE DEVICE>"
  ENV PNP_BRIDGE_ROOT_MODEL_ID "dtmi:com:example:RootPnpBridgeSampleDevice;1"
  ENV PNP_BRIDGE_HUB_TRACING_ENABLED "false"
```

### Step 7: Build IoT Plug and Play Bridge module image
 - Open pnpbridge\module.json and update the container registry/image information :
```JSON
{
  "$schema-version": "0.0.1",
  "description": "",
  "image": {
    "repository": "<ENTER CONTAINER REGISTRY URL>/<NAME OF IMAGE>",
    "tag": {
      "version": "<ENTER VERSION TAG>",
      "platforms": {
        "amd64": "./Dockerfile.amd64",
        "amd64.debug": "./Dockerfile.amd64.debug",
        "arm32v7": "./Dockerfile.arm32v7",
        "arm64v8": "./Dockerfile.arm64v8"
      }
    },
    "buildOptions": []
  },
  "language": "c"
}

```
 - Right click module.json and select "Build and Push IoT Edge Module image"

### Step 8: Update the deployment manifest

  After the image has been successfully built and pushed to your container registry open pnpbridge\deployment.template.json and update the following:

 - Add the container registry credentials:
```JSON
{
  "registryCredentials": {
     "pnpbridge": {
        "username": "$CONTAINER_REGISTRY_USERNAME_pnpbridge",
        "password": "$CONTAINER_REGISTRY_PASSWORD_pnpbridge",
        "address": "<ADD CONTAINER REGISTRY ADDRESS HERE>"
    }
  }
}

```
- Add the image name of the module image you created:
```JSON
{
  "modules": {
          "ModulePnpBridge": {
            "version": "1.0",
            "type": "docker",
            "status": "running",
            "restartPolicy": "always",
            "settings": {
              "image": "<CONTAINER REGISTRY URL>/<NAME OF IMAGE>:<TAG>",
              "createOptions": "{}"
            }
          }
        }
}

```

- Add the Pnp Bridge device & adapter configuration with desired property `PnpBridgeConfig`, for example, the following configues a Modbus TCP carbon monoxide sensor:
```JSON
{
  "ModulePnpBridge": {
      "properties.desired": {
        "PnpBridgeConfig": {
          "pnp_bridge_interface_components": [
            {
                "_comment": "Component 1 - Modbus Device",
                "pnp_bridge_component_name": "Co2Detector1",
                "pnp_bridge_adapter_id": "modbus-pnp-interface",
                "pnp_bridge_adapter_config": {
                    "unit_id": 1,
                    "tcp": {
                        "host": "10.159.29.2",
                        "port": 502
                    },
                    "modbus_identity": "DL679"
                }
            }
        ],
        "pnp_bridge_adapter_global_configs": {
            "modbus-pnp-interface": {
                "DL679": {
                    "telemetry": {
                        "co2": {
                            "startAddress": "40001",
                            "length": 1,
                            "dataType": "integer",
                            "defaultFrequency": 5000,
                            "conversionCoefficient": 1
                        },
                        "temperature": {
                            "startAddress": "40003",
                            "length": 1,
                            "dataType": "decimal",
                            "defaultFrequency": 5000,
                            "conversionCoefficient": 0.01
                        }
                    },
                    "properties": {
                        "firmwareVersion": {
                            "startAddress": "40482",
                            "length": 1,
                            "dataType": "hexstring",
                            "defaultFrequency": 60000,
                            "conversionCoefficient": 1,
                            "access": 1
                        },
                        "modelName": {
                            "startAddress": "40483",
                            "length": 2,
                            "dataType": "string",
                            "defaultFrequency": 60000,
                            "conversionCoefficient": 1,
                            "access": 1
                        },
                        "alarmStatus_co2": {
                            "startAddress": "00305",
                            "length": 1,
                            "dataType": "boolean",
                            "defaultFrequency": 1000,
                            "conversionCoefficient": 1,
                            "access": 1
                        },
                        "alarmThreshold_co2": {
                            "startAddress": "40225",
                            "length": 1,
                            "dataType": "integer",
                            "defaultFrequency": 30000,
                            "conversionCoefficient": 1,
                            "access": 2
                        }
                    },
                    "commands": {
                        "clearAlarm_co2": {
                            "startAddress": "00305",
                            "length": 1,
                            "dataType": "flag",
                            "conversionCoefficient": 1
                        }
                    }
                }
            }
          }
        }
      }
    }
}

```

- Build the deployment manifest: Right Click, Build IoT Edge Solution

- Deploy the newly created manifest to your device pnpbridge\config\deployment.amd64.json: Right Click, select Create Deployment for Single device

## Folder Structure

### /deps/azure-iot-sdk-c-pnp

Git submodules that contains Azure IoT Device C SDK code

### /scripts

Build scripts

### /src/pnpbridge

Source code for IoT Plug and Play bridge core

### /src/adapters

Source code for various PnP Bridge Adapters

## Support

For any questions raise an issue or contact - [pnpbridge@microsoft.com](mailto:pnpbridge@microsoft.com)
