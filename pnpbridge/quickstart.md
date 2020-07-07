# Quickstart
This quick start guides you through: 
* Configure the IoT Plug and Play bridge (on Windows) to connect a simulated [Environmental Sensor](https://github.com/Azure/IoTPlugandPlay/blob/master/samples/EnvironmentalSensor.interface.json) to Azure
* Configure the IoT Plug and Play bridge (on Windows) to connect a generic sensor such as a Modbus sensor to Azure *[OPTIONAL]*

This quickstart will take approximately 30 minutes (45 minutes with the optional Modbus portion).

At the end of this quickstart you will sucessfully configure and run the IoT Plug and Play bridge executable and be able to:
* See telemetry in an IoT Central Preview Application reported by the bridge  from a simulated [Environmental Sensor](https://github.com/Azure/IoTPlugandPlay/blob/master/samples/EnvironmentalSensor.interface.json)
* See telemetry in an IoT Central Preview Application reported by the bridge  from a physical [Modbus Sensor](https://www.icpdas-usa.com/dl_303.html) *[OPTIONAL]*

To complete this quickstart you will need: 
* Any hardware platform supporting and running one of the following OS:
    * Windows IoT Enterprise
    * Windows Server
    * Windows IoT Core 
    * Windows Desktop
    * Linux (Ubuntu)
* A development environment that supports compiling C++ such as: [Visual Studio (Community, Professional, or Enterprise)](https://visualstudio.microsoft.com/downloads/)- make sure that you include the NuGet package manager component and the Desktop Development with C++ workload when you install Visual Studio.
* [CMake](https://cmake.org/download/) - when you install CMake, select the option `Add CMake to the system PATH`.
* If you are building on Windows, you will also need to download Windows 17763 SDK: https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk
* To successfully complete the Modbus section of the quickstart guide, you will need to connect a  [DL-303 Modbus Sensor](https://www.icpdas-usa.com/dl_303.html) (such as a simple CO2 sensor) to the device running the bridge. *[OPTIONAL]*
  
## Connecting a Simple Environmental Sensor
The following section will cover the steps needed to connect a simple simulated [Environmental Sensor](https://github.com/Azure/IoTPlugandPlay/blob/master/samples/EnvironmentalSensor.interface.json) through the IoT Plug and Play bridge up to Azure IoT Central. First an IoT Central application is created. Then the bridge is configured with a simple JSON. Finally, the bridge is started and data is visible in the display.

Although IoT Central is covered in this demo, you can also use an Azure IoT Hub in a similar fashion. Simply use the connection details from your hub in **pnp_bridge_parameters** referenced below.

### Setting up Azure IoT Central (Environmental Sensor)
Complete the following to enable an IoT Central application to accept data from your bridge:

1. Complete the [Create an Azure IoT Central application (preview features)](https://docs.microsoft.com/azure/iot-central/quick-deploy-iot-central-pnp?toc=/azure/iot-central-pnp/toc.json&bc=/azure/iot-central-pnp/breadcrumb/toc.json) quickstart to create an IoT Central application using the Preview application template.

2. Complete [Create device template from scratch](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type#create-a-device-template-from-scratch), [Create a capability model](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type#create-a-capability-model), and [Publish a device template](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type#publish-a-device-template) sections, described in IoT Central's guide to [Build and Manage an IoT Device Template](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type). Instead of using the capability model provided in the instructions, use the capability model JSON provided [here](./docs/schemas/EnvironmentalSensorInline.capabilitymodel.json)

3. Obtain the connection information for your device using IoT Central documentation on [Device connectivity in Azure IoT Central](https://docs.microsoft.com/azure/iot-central/core/concepts-connectivity).


### Setting up the Configuration JSON (Environmental Sensor)
After cloning the Azure PnP Bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo. Modify the folowing parameters under **pnp_bridge_parameters** node in the config file  (%REPO_DIR%\pnpbridge\src\adapters\samples\environmental_sensor\config.json):

  Using Connection string (Note: the symmetric_key must match the SAS key in the connection string):

  ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[CONNECTION STRING]",
        "device_capability_model_uri": "[To fill in]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
        }
      }
    }
  }
  ```
  Or using DPS:

  ```JSON
  {
      "connection_parameters": {
        "connection_type" : "dps",
        "device_capability_model_uri": "[To fill in]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        },
        "dps_parameters" : {
          "global_prov_uri" : "[GLOBAL PROVISIONING URI]",
          "id_scope": "[IOT HUB ID SCOPE]",
          "device_id": "[DEVICE ID]"
        }
      }
    }
  }
  ```
  > Note: Refer to the [Azure IoT Central documentation on device connectivity](https://docs.microsoft.com/azure/iot-central/concepts-connectivity-pnp?toc=/azure/iot-central-pnp/toc.json&bc=/azure/iot-central-pnp/breadcrumb/toc.json) for how to generate the id_scope, device_id, and symmetric_key for your device. The device_capability_model_uri is the "CapabilityModelId" that is listed for your device's Device Capability Model in Azure IoT Central.
 
 Once filled in you the config file should resemble:
   ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[CONNECTION STRING]",
        "device_capability_model_uri": "urn:YOUR_COMPANY_NAME:sample_device:1",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
        }
      }
    }
  }
  ```
  Or using DPS:

  ```JSON
  {
      "connection_parameters": {
        "connection_type" : "dps",
        "device_capability_model_uri": "urn:YOUR_COMPANY_NAME:sample_device:1",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        },
        "dps_parameters" : {
          "global_prov_uri" : "[GLOBAL PROVISIONING URI] - typically it is global.azure-devices-provisioning.net",
          "id_scope": "[IOT HUB ID SCOPE]",
          "device_id": "[DEVICE ID]"
        }
      }
    }
  }
  ```

### Get the required dependencies (Enivronmental Sensor)
After cloning the Azure PnP Bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo:
```
%REPO_DIR%\> cd pnpbridge

%REPO_DIR%\pnpbridge\> git submodule update --init --recursive
```
>Note: If you run into issues with the git clone sub module update failing, this is a known issue with Windows file paths and git see: https://github.com/msysgit/git/pull/110 . You can try the following command to resolve the issue: `git config --system core.longpaths true`


### Build the IoT Plug and Play bridge (Environmental Sensor) on Windows

```
%REPO_DIR%\pnpbridge\> cd scripts\windows

%REPO_DIR%\pnpbridge\scripts\windows> build.cmd
```

### Start the IoT Plug and Play bridge (Environmental Sensor) on Windows
 Start the IoT Plug and Play bridge sample for Environmental sensors by running it in a command prompt:

    ```
    %REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86\src\adaptors\samples\environmental_sensor

    %REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\adaptors\samples\environmental_sensor>  Debug\pnpbridge_environmentalsensor.exe
    ```
### Completed Enviromental Sensor Demo: View Telemetry
The bridge will start to display informational messages indicating a successful connection and messaging with an Azure IoT Hub. Data should briefly start to appear in the dashboard that was created in "Setting up Azure IoT Central".


## Connecting a Generic Sensor (Modbus Example) *[OPTIONAL]*
The following section will cover the steps needed to connect a simple CO2 Modbus sensor through the IoT Plug and Play bridge up to Azure IoT Central. First an IoT Central application is created. Then the bridge is configured with a simple JSON. Finally, the bridge is started and data is visible in the display.

Although IoT Central is covered in this demo, you can also use an Azure IoT Hub in a similar fashion. Simply use the connection details from your hub in **pnp_bridge_parameters** referenced below.

For more specific information on the modbus adaptor being used in this example see [Modbus Adaptors](./docs/modbus_adapters.md)

>Note: Athough this example specifically covers Modbus; the steps followed apply generically for all adaptors.

### Setting up Azure IoT Central Generic Sensor (Modbus Example)
Complete the following to enable an IoT Central application to accept data from your bridge for a simple CO2 Modbus sensor:

1. Complete the [Create an Azure IoT Central application (preview features)](https://docs.microsoft.com/azure/iot-central/quick-deploy-iot-central-pnp?toc=/azure/iot-central-pnp/toc.json&bc=/azure/iot-central-pnp/breadcrumb/toc.json) quickstart to create an IoT Central application using the Preview application template.

2. Complete [Create device template from scratch](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type#create-a-device-template-from-scratch), [Create a capability model](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type#create-a-capability-model), and [Publish a device template](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type#publish-a-device-template) sections, described in IoT Central's guide to [Build and Manage an IoT Device Template](https://docs.microsoft.com/azure/iot-central/preview/tutorial-define-iot-device-type). Instead of using the capability model provided in the instructions, use the capability model provided [here](./docs/schemas/CO2SensorInline.capabilitymodel.json)

3. Obtain the connection information for your device using IoT Central documentation on [Device connectivity in Azure IoT Central](https://docs.microsoft.com/azure/iot-central/core/concepts-connectivity).

>Note: for other generic sensors you would follow similar steps; providing a customized capability model.


### Setting up the Configuration JSON for Generic Sensors (Modbus Example)
In this example we then further modified the configuration file to match the properties of the DL-303 sensor. An example of what the config file should look like is found here in this [**Sample Configuration File for Modbus**](./docs/schemas/config-modbus.json)

Modify the folowing parameters under **pnp_bridge_parameters** node in the config file  (%REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\pnpbridge\samples\console):

  Using Connection string (Note: the symmetric_key must match the SAS key in the connection string):

  ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[CONNECTION STRING]",
        "device_capability_model_uri": "[To fill in]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        }
      }
    }
  }
  ```
  Or using DPS:

  ```JSON
  {
      "connection_parameters": {
        "connection_type" : "dps",
        "device_capability_model_uri": "[To fill in]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
          "symmetric_key" : "[DEVICE KEY]"
        },
        "dps_parameters" : {
          "global_prov_uri" : "[GLOBAL PROVISIONING URI] - typically it is global.azure-devices-provisioning.net",
          "id_scope": "[IOT HUB ID SCOPE]",
          "device_id": "[DEVICE ID]"
        }
      }
    }
  }
  ```
  > Note: Refer to the [Azure IoT Central documentation on device connectivity](https://docs.microsoft.com/azure/iot-central/concepts-connectivity-pnp?toc=/azure/iot-central-pnp/toc.json&bc=/azure/iot-central-pnp/breadcrumb/toc.json) for how to generate the id_scope, device_id, and symmetric_key for your device. The device_capability_model_uri is the "CapabilityModelId" that is listed for your device's Device Capability Model in Azure IoT Central.

### Start the IoT Plug and Play bridge for Generic Sensors (Modbus Example) on Windows
Start PnpBridge by running it in a command prompt.

  ```
  %REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86\src\pnpbridge\samples\console

  %REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\pnpbridge\samples\console>    Debug\pnpbridge_bin.exe
  ```

  > Note: If you have either a built-in camera or a USB camera connected to your PC running the PnpBridge, you can start an application that uses camera, such as the built-in "Camera" app.  Once you started running the Camera app, PnpBridge console output window will show the monitoring stats and the frame rate of the camera will be reported through Azure IoT PnP interface to Azure.