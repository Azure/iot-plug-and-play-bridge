# Quick start

This quick start guides you through:

* Configure the IoT Plug and Play bridge (on Windows) to connect a simulated [Environmental Sensor](https://github.com/Azure/IoTPlugandPlay/blob/master/samples/EnvironmentalSensor.interface.json) to Azure

This quickstart will take approximately 20 minutes.

At the end of this quick start you will successfully configure and run the IoT Plug and Play bridge executable and be able to:

* See telemetry in Azure IoT explorer reported by the bridge  from a simulated [Environmental Sensor](./docs/schemas/EnvironmentalSensorInline.capabilitymodel.json)

## Pre-requisites
To complete this quickstart you will need: 

### OS Platform

One of the following supported OS platforms and versions:

|Platform  |Supported Versions  |
|---------|---------|
|Windows 10     |     All Windows 10 SKUs are supported. For example:<li>IoT Enterprise</li><li>Server</li><li>Desktop</li><li>IoT Core</li>  |
|Linux     |Tested and Supported on Ubuntu 18.04, functionality on other distributions has not been tested.         |
||

### Hardware

- Any hardware platform capable of supporting the above OS SKUs and versions.

### Development Environment

- A development environment that supports compiling C++ such as: [Visual Studio (Community, Professional, or Enterprise)](https://visualstudio.microsoft.com/downloads/)- make sure that you include the **Desktop Development with C++** workload when you install Visual Studio.
- [CMake](https://cmake.org/download/) - when you install CMake, select the option `Add CMake to the system PATH`.
- If you are building on Windows, you will also need to download Windows 17763 SDK: [https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)

- [Azure IoT Hub Device C SDK](https://github.com/Azure/azure-iot-sdk-c/tree/public-preview). The included build scripts in this repo will automatically clone the required Azure IoT C SDK for you.

### Azure IoT Products and Tools

- **Azure IoT Hub** - You'll need an [Azure IoT Hub](https://docs.microsoft.com/en-us/azure/iot-hub/) in your Azure subscription to connect your device to. If you don't have an Azure subscription, [create a free account](https://azure.microsoft.com/free/) before you begin. If you don't have an IoT Hub, [follow these instructions to create one](https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-create-using-cli).

>Note: IoT Plug and Play is currently available on IoT Hubs created in the Central US, North Europe, and East Japan regions. IoT Plug and Play support is not included in basic-tier IoT Hubs.

-  **Azure IoT explorer** - To interact with the your IoT Plug and Play device, you can use the Azure IoT explorer tool. [Download and install the latest release of Azure IoT explorer](https://docs.microsoft.com/en-us/azure/iot-pnp/howto-use-iot-explorer) for your operating system.
  
## Setting up IoT Hub and Configurations
The following section will cover the steps needed to connect a simple simulated [Environmental Sensor](./docs/schemas/EnvironmentalSensorInline.capabilitymodel.json) through the IoT Plug and Play bridge up to an Azure IoT Hub. First an IoT Hub is prepared. Then the bridge is configured with a simple JSON using the connection details from your hub in **pnp_bridge_parameters** referenced below. Finally, the bridge is started and data is visible in the display.


### Prepare an IoT hub

You need an Azure IoT hub in your Azure subscription to complete the steps in this article. If you don't have an Azure subscription, create a [free account](https://azure.microsoft.com/free/?WT.mc_id=A261C142F) before you begin.

If you're using the Azure CLI locally, first sign in to your Azure subscription using `az login`. If you're running these commands in the Azure Cloud Shell, you're signed in automatically.

If you're using the Azure CLI locally, the `az` version should be **2.8.0** or later; the Azure Cloud Shell uses the latest version. Use the `az --version` command to check the version installed on your machine.

Run the following command to add the Microsoft Azure IoT Extension for Azure CLI to your instance:

```azurecli-interactive
az extension add --name azure-iot
```

If you don't already have an IoT hub to use, run the following commands to create a resource group and a free-tier IoT hub in your subscription. Replace `<YourIoTHubName>` with a hub name of your choice:

```azurecli-interactive
az group create --name my-pnp-resourcegroup \
    --location centralus
az iot hub create --name <YourIoTHubName> \
    --resource-group my-pnp-resourcegroup --sku F1
```

> [!NOTE]
> IoT Plug and Play is currently available on IoT Hubs created in the Central US, North Europe, and East Japan regions. IoT Plug and Play support is not included in basic-tier IoT Hubs.

Run the following command to create the device identity in your IoT hub. Replace the `<YourIoTHubName>` and `<YourDeviceID>` placeholders with your own _IoT Hub name_ and a _device ID_ of your choice.

```azurecli-interactive
az iot hub device-identity create --hub-name <YourIoTHubName> --device-id <YourDeviceID>
```

Run the following command to get the _IoT hub connection string_ for your hub. Make a note of this connection string, you use it later in this quickstart:

```azurecli-interactive
az iot hub show-connection-string --hub-name <YourIoTHubName> --output table
```

> [!TIP]
> You can also use the Azure IoT explorer tool to find the IoT hub connection string.

Run the following command to get the _device connection string_ for the device you added to the hub. Make a note of this connection string, you use it later in this quickstart:

```azurecli-interactive
az iot hub device-identity show-connection-string --hub-name <YourIoTHubName> --device-id <YourDeviceID> --output table
```


### Setting up the Configuration JSON (Environmental Sensor)
After cloning the Azure PnP Bridge repo to your machine, open the "Developer Command Prompt for VS 2017" and navigate to the directory of the cloned repo. Modify the folowing parameters under **pnp_bridge_parameters** node in the config file  (%REPO_DIR%\pnpbridge\src\adapters\samples\environmental_sensor\config.json):

  Using Connection string (Note: the symmetric_key must match the SAS key in the connection string):

  ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[CONNECTION STRING]",
        "root_interface_model_id": "[To fill in]",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
        }
      }
    }
  }
  ```

 
 Once filled in you the config file should resemble:
   ```JSON
    {
      "connection_parameters": {
        "connection_type" : "connection_string",
        "connection_string" : "[CONNECTION STRING]",
        "root_interface_model_id": "dtmi:com:example:SampleDevice;1",
        "auth_parameters" : {
          "auth_type" : "symmetric_key",
        }
      }
    }
  }
  ```

## Get the required dependencies (Environmental Sensor)

### Download the models

Later, you use Azure IoT explorer to view the device when it connects to your IoT hub. Azure IoT explorer needs a local copy of the model file that matches the **Model ID** your device sends. The model file lets Azure IoT explorer display the telemetry, properties, and commands that your device implements.

If you haven't already downloaded the sample model files:

1. Create a folder called models on your local machine.
1. Right-click [Environmental Sensor](https://raw.githubusercontent.com/Azure/AzurePnPBridgePreview/pprupdatesforddtlv2/pnpbridge/docs/schemas/EnvironmentalSensorInline.capabilitymodel.json) and save the JSON file to the models folder.
### Download the code 
After cloning the IoT Plug and Play bridge repo to your machine, open an administrative cmd prompt and navigate to the directory of the cloned repo:
```
%REPO_DIR%\> cd pnpbridge

%REPO_DIR%\pnpbridge\> git submodule update --init --recursive
```
>Note: If you run into issues with the git clone sub module update failing, this is a known issue with Windows file paths and git see: https://github.com/msysgit/git/pull/110 . You can try the following command to resolve the issue: `git config --system core.longpaths true`


## Build the IoT Plug and Play bridge

On Windows run the following:
```
%REPO_DIR%\pnpbridge\> cd scripts\windows

%REPO_DIR%\pnpbridge\scripts\windows> build.cmd
```

Similarly on Linux run the following:

```bash
 /%REPO_DIR%/pnpbridge/ $ cd scripts/linux

 /%REPO_DIR%/pnpbridge/scripts/linux $ ./setup.sh

 /%REPO_DIR%/pnpbridge/scripts/linux $ ./build.sh
```
## Start the IoT Plug and Play bridge (Environmental Sensor)
 Start the IoT Plug and Play bridge sample for Environmental sensors by running it in a command prompt:

    ```
    %REPO_DIR%\pnpbridge\> cd cmake\pnpbridge_x86\src\adapters\samples\environmental_sensor

    %REPO_DIR%\pnpbridge\cmake\pnpbridge_x86\src\adapters\samples\environmental_sensor>  Debug\pnpbridge_environmentalsensor.exe
    ```
## View Telemetry in Azure IoT explorer 
The bridge will start to display informational messages indicating a successful connection and messaging with an Azure IoT Hub. Data should briefly start to appear in the dashboard that was created in "Setting up Azure IoT Central".

1. Open Azure IoT explorer.

1. On the **IoT hubs** page, if you haven't already added a connection to your IoT hub, select **+ Add connection**. Enter the connection string for the IoT hub you created previously and select **Save**.

1. On the **IoT Plug and Play Settings** page, select **+ Add > Local folder** and select the local *models* folder where you saved your model files.

1. On the **IoT hubs** page, click on the name of the hub you want to work with. You see a list of devices registered to the IoT hub.

1. Click on the **Device ID** of the device you created previously.

1. The menu on the left shows the different types of information available for the device.

1. Select **IoT Plug and Play components** to view the model information for your device.

1. You can view the different components of the device. The default component and any additional ones. Select a component to work with.

1. Select the **Telemetry** page and then select **Start** to view the telemetry data the device is sending for this component.

1. Select the **Properties (read-only)** page to view the read-only properties reported for this component.

1. Select the **Properties (writable)** page to view the writable properties you can update  for this component.

1. Select a property by it's **name**, enter a new value for it, and select **Update desired value**.

1. To see the new value show up  select the **Refresh** button.

1. Select the **Commands** page to view all the commands for this component.

1. Select the command you want to test set the parameter if any. Select **Send command** to call the command on the device. You can see your device respond to the command in the command prompt window where the sample code is running.

