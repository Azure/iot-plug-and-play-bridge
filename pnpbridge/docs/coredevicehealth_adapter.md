# Core Device Health Adapter
This a brief README for the core device health adapter on the IoT Plug and Play bridge. The adapter enables connecting devices of a specific interface class through the Plug and Play Bridge.

## Table of Contents
- [Core Device Health Adapter](#core-device-health-adapter)
  - [Table of Contents](#table-of-contents)
  - [Adapter Configuration](#adapter-configuration)
    - [PnP Bridge Adapter Config](#pnp-bridge-adapter-config)
    - [PnP Bridge Adapter Global Configs](#pnp-bridge-adapter-global-configs)

## Adapter Configuration
Follow the steps below to update `config.json` to add a device that uses the core device health adapter to connect to Azure IoT with IoT Plug and Play bridge.
1. Open `config.json` for the IoT Plug and Play bridge.
2. Add the json payload corresponding the the device
```json
{
    "_comment": "Core Device Component - USB device",
    "pnp_bridge_component_name": "coredevicehealth1",
    "pnp_bridge_adapter_id": "core-device-health",
    "pnp_bridge_adapter_config": {
        "hardware_id": "USB\\VID_06CB&PID_00B3"
    }
}
```

### PnP Bridge Adapter Config
`pnp_bridge_adapter_config` is used to provide the adapter with the hardware ID of the device that intends to be enumerated as a PnP Bridge component with the help of the core device health adapter.

```json
{
    "pnp_bridge_adapter_config":
    {
        "hardware_id": "USB\\VID_06CB&PID_00B3"
    }
}
```

Configuration Definition:

|Field|Data Type|Description|
|:---|:---:|:---|
|`hardware_id`|string|Hardware Id of the device|

### PnP Bridge Adapter Global Configs
PnP Bridge Adapter Global Configs provide PnP Bridge Adapters to optionally list supported interface configurations. These interface configurations are identified with a key that a core device health interface component uses to identify what type of device this is. Each key specified a list of device interface class GUIDs that are supported.

``` json
{
    "core-device-health": {
        "device_interface_classes": [
            "A5DCBF10-6530-11D2-901F-00C04FB951ED"
        ]
    }
}
```
Configuration Definition:

|Field|Data Type|Description|
|:---|:---:|:---|
|`device_interface_classes`|strings|Supported device interface class GUIDs |

