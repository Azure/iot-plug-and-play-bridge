# Modbus Adapter for IoT Plug and Play bridge
This a README for the Modbus adapter on the IoT Plug and Play bridge

## Table of Contents
- [Modbus Adapter for PnP Bridge](#modbus-adapter-for-iot-plug-and-play-bridge)
  - [Table of Contents](#table-of-contents)
  - [Adapter Configuration](#adapter-configuration)
    - [PnP Bridge Adapter Config](#pnp-bridge-adapter-config)
    - [PnP Bridge Adapter Global Configs](#pnp-bridge-adapter-global-configs)
  - [Reference](#reference)
    - [Modbus](#modbus)

## Adapter Configuration
The adapter configuration follows the format defined by [Modbus Gateway configruation interface](./schemas/modbusGateway.interface.json), an Azure Digital Twin (IoT Plug and Play) Interface. Follow the steps below to update `config.json` to add a Modbus device to IoT Plug and Play bridge.

1. Open `config.json` for the IoT Plug and Play bridge.
2. Add the following json payload to `pnp_bridge_interface_components`.
3. Add any supported modbus interface configurations into the `pnp_bridge_adapter_global_configs` dictionary with a key that an interface component can use as `modbus_identity` under `pnp_bridge_adapter_config`. More information in the [PnP Bridge Adapter Global Configs](#pnp-bridge-adapter-global-configs) section.
4. Modify `rtu` or `tcp` and `modbus_identity` parameters under `pnp_bridge_adapter_config` to match your Modbus device configuration. Refer to following sections for more detailed descriptions of each field in these `JSON` objects. "DL679" is the name of the interface configuration that the Modbus Adapter supports, and that the device corresponding to the interface component specifying it, uses to interact with the adapter.

```json
{
    "pnp_bridge_interface_components": [
      {
          "_comment": "Component - Modbus Device",
          "pnp_bridge_interface_id": "urn:contoso:com:Co2Detector:1",
          "pnp_bridge_component_name": "ModbusComponent",
          "pnp_bridge_adapter_id": "modbus-pnp-interface",
          "pnp_bridge_adapter_config": {
              "unit_id": 1,
              "rtu": {
                "port": "COM5",
                "baudRate": "9600",
                "dataBits": 8,
                "stopBits": "ONE",
                "parity": "NONE"
              },
              "tcp": null,
              "modbus_identity": "DL679"
          }
      }
    ]
}
```

### PnP Bridge Adapter Config
`pnp_bridge_adapter_config` provides the physical connection information between the Modbus gateway and the Modbus edge devices. This part of configuration provides TCP/IP or serial connection information (must pick either one). You may provide `null` value for the connection you do not use.

```json
{
    "pnp_bridge_adapter_config": {
    "unitId": 1,
    "rtu": null,
    "tcp": {
        "host": "192.168.1.111",
        "port": 502
       }
    }
}
```

Configuration Definition:

|Field|Data Type|Description|
|:---|:---:|:---|
|`unitId`|integer|The id (or slave address) of the Modbus device|
|`rtu`|
|`port`|integer| Serial port name, ex: "/dev/ttys0" or "COM1".|
|`baudRate`|string|Baud rate of the serial port. Valid values: ..."9600", "14400", "19200"...|
|`dataBits`|integer|Data Bit for the serial port. Valid values: `7` and `8`.|
|`stopBits`|string|Stop Bit used for the serial port. Valid values: "ONE", "TWO", "OnePointFive". |
|`parity`|string|Serial port parity. Valid values: "ODD", "EVEN", and "NONE".|
|`tcp`|
|`host`|string|Ipv4 address of the Modbus device|
|`port`|integer|Port number of the Modbus device|

### PnP Bridge Adapter Global Configs
PnP Bridge Adapter Global Configs provide PnP Bridge Adapters to optionally list supported interface configurations. These interface configurations are identified with a key that a Modbus interface component uses to identify how the adapter must parse data coming from the device. The data sheet of the physical device would usually outline this information. This sample configuration is based on the [schema of a sample CO₂ detector](./schemas/Co2Detector.interface.json).

``` json
{
    ,
    "pnp_bridge_adapter_global_configs": {
      "modbus-pnp-interface": {
          "DL679": {
              "telemetry": {
                  "co2": {
                      "startAddress": "40001",
                      "length": 1,
                      "dataType": "integer",
                      "defaultFrequency": 5000,
                      "conversionCoefficient": 1
                  },
                  "temperature": {
                      "startAddress": "40003",
                      "length": 1,
                      "dataType": "decimal",
                      "defaultFrequency": 5000,
                      "conversionCoefficient": 0.01
                  }
              },
              "properties": {
                  "firmwareVersion": {
                      "startAddress": "40482",
                      "length": 1,
                      "dataType": "hexstring",
                      "defaultFrequency": 60000,
                      "conversionCoefficient": 1,
                      "access": 1
                  },
                  "modelName": {
                      "startAddress": "40483",
                      "length": 2,
                      "dataType": "string",
                      "defaultFrequency": 60000,
                      "conversionCoefficient": 1,
                      "access": 1
                  },
                  "alarmStatus_co2": {
                      "startAddress": "00305",
                      "length": 1,
                      "dataType": "boolean",
                      "defaultFrequency": 1000,
                      "conversionCoefficient": 1,
                      "access": 1
                  },
                  "alarmThreshold_co2": {
                      "startAddress": "40225",
                      "length": 1,
                      "dataType": "integer",
                      "defaultFrequency": 30000,
                      "conversionCoefficient": 1,
                      "access": 2
                  }
              },
              "commands": {
                  "clearAlarm_co2": {
                      "startAddress": "00305",
                      "length": 1,
                      "dataType": "flag",
                      "conversionCoefficient": 1
                  }
              }
          }
      }
    }
}
```
Configuration Definition:

|Field|Data Type|Description|
|:---|:---:|:---|
|`Capability Definition`|
|`startAddress`|integer|Starting address of the Modbus device to read from |
|`length`|integer| Number of bytes to read.|
|`dataType`|string|Data type that the raw Modbus response should convert to. Valid values: `"integer"`, `"decimal"`. <br> Experimental Data Types*: <br>`"string"`: returns Modbus byte array as ASCII string. <br>`"hexstring"`: return Modbus byte arry as hexadecimal string.|
|`defaultFrequency`|integer|For **telemetry** and **property** capability only. The time interval (in miliseconds) between each data pull from the Modbus device.|
|`conversionCoefficient`|decimal| The coefficient that the raw Modbus response should multiply to to get actual value. It is `1` by default.  </br>Ex. If the raw response of the temperature (in Celcius) reading from the Modbus device is `0x0935` (=`2357`), we need to mutiply the raw data to `0.01` to get the actual value (`23.57`) in Celcius. `0.01` is the `conversionCoefficient`.|
|`access`|integer|For **property** capability only. </br>`1` for read-only property  </br> `2` for writable property |

**\*** We understand that data type like "string" can be interpreted differently for each device manufacturer as Modbus does not provide a standard representation for "string". Please share your opnion on how these type of data should be generally converted

## Reference
### Modbus
A serial communication protocol that is commonly used in the industrial IoT world. This module supports two most common variants of the Modbus protocols: Modbus TCP/IP (communicates over TCP/IP networks) and Modbus RTU (communicates over RS485 connection).