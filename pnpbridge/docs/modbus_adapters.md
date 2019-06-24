# Modbus Adapter for PnP Bridge
This a README for the Modbus adapter on PnPBridge

## Table of Contents
- [Modbus Adapter for PnP Bridge](#modbus-adapter-for-pnp-bridge)
  - [Table of Contents](#table-of-contents)
  - [Adapter Configuration](#adapter-configuration)
    - [`DeviceConfig`](#deviceconfig)
    - [`interfaceConfig`](#interfaceconfig)
  - [Reference](#reference)
    - [Modbus](#modbus)

## Adapter Configuration
The adapter configuration follows the format defined by [Modbus Gateway configruation interface](./schemas/modbusGateway.interface.json), an Azure PnP Interface. Follow the steps below to update `config.json` to add a Modbus device to your PnPBrdige.

1. Open `config.json` for the PnPBridge.
2. Add the following json payload to `Devices`.
3. Modify `deviceConfig` and `interfaceConfig` objects under `DiscoveryParameters` to match your Modbus device configuration. Refer to following sections for more detailed descriptions of each field in these two `JSON` objects.

```json
{
    "_comment": "My Modbus Device",
    "MatchFilters": {
        "MatchType": "Exact"
    },
    "SelfDescribing": "true",
    "PnpParameters": {
        "Identity": "modbus-pnp-interface"
    },
    "DiscoveryParameters": {
        "Identity": "modbus-pnp-discovery",
        "deviceConfig": {
            "unitId": 1,
            "rtu": {
                "port": "COM5",
                "baudRate": "9600",
                "dataBits": 8,
                "stopBits": "ONE",
                "parity": "NONE"
            },
            "tcp": null
        },
        "interfaceConfig": {
            "interfaceId": "http://contoso.com/Co2Detector/1.0.0",
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
                    "defaultFrequency": 30000,
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
```

### `DeviceConfig`
DeviceConfig provides the physical connection information between the Modbus gateway and the Modbus edge devices. This part of configuration provides TCP/IP or serial connection information (must pick either one). You may provide `null` value for the connection you do not use.

```json
"deviceConfig": {
    "unitId": 1,
    "rtu": null,
    "tcp": {
        "host": "192.168.1.111",
        "port": 502
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

### `interfaceConfig`
Interface Configuration provides a mapping of Modbus device's memory addresses to P&P interfaces. This sample configuration is based on the [schema of a sample CO₂ detector](./schemas/Co2Detector.interface.json). The interfaceId and capability names provided here should match those of device interfaces schema.

``` json
"interfaceConfig": {
    "interfaceId": "http://contoso.com/Co2Detector/1.0.0",
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
            "defaultFrequency": 30000,
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
```
Configuration Definition:

|Field|Data Type|Description|
|:---|:---:|:---|
|`interfaceId`|string|The interfaceID of the Modbus device. This should map the device type of the Modbus devices listed in `deviceMap.json` |
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