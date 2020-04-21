# Bluetooth Sensor Adapter

The Bluetooth sensor adapter is a Azure PnP bridge adapter for Bluetooth enabled sensors. It is capable of listening for Bluetooth advertisements, parsing the payload via a user configured descriptor, and sending it to IoT Hub.

The adapter is flexible enough to work with the majority of off-the-shelf Bluetooth enabled sensors provided they meet these requirements:

1. The sensor transmits via standard BLE (Bluetooth Low Energy) advertisements.
2. The sensor data to parse is contained within the manufacturer payload of the Bluetooth advertisement.

A generic data descriptor is also provided which allows the user to specify how the adapter should parse the manufacturer specific payload of each sensor into IoT Hub telemetry.

This README is composed of two main sections:

1. How to configure the payload descriptor.
2. How the Bluetooth sensor adapter is architected.

## 1. Configuration

### 1.1. Define the Sensor Payload Descriptor

Ensure the Bluetooth discovery adapter has been specified inside the `pnp_bridge_adapter_global_configs` array in `config.json`:

```cpp
"pnp_bridge_adapter_global_configs": {
    "bluetooth-sensor-pnp-adapter": {
      <...Described Below...>
    }
}
```

Then add the interface identities for each BLE sensor interface. This describes how the Bluetooth advertisement payload should be parsed:

```cpp
"pnp_bridge_adapter_global_configs": {
  "bluetooth-sensor-pnp-adapter": {
    "Ruuvi" : {
        "company_id": "0x499",
        "endianness": "big",
        "telemetry_descriptor": [
        {
          "telemetry_name": "lux",
          "data_parse_type": "uint16",
          "data_offset": 1,
          "conversion_bias": 0,
          "conversion_coefficient": 1.0
        },
        {
          "telemetry_name": "battery_level",
          "data_parse_type": "uint8",
          "data_offset": 3,
          "conversion_bias": 0,
          "conversion_coefficient": 0.5
        }
      ]
    }
  }
}
```

Each interface identity should contain the following properties:

| Field Name             | Data Type | Description                                                  |
| ---------------------- | --------- | ------------------------------------------------------------ |
| `company_id`           | string    | Bluetooth company identifier, the adapter will filter out advertisements that do not match this. |
| `endianness`           | string    | Describes the endianness of the Bluetooth advertisement payload, valid values are `little` and `big`. |
| `telemetry_descriptor` | array     | Array of descriptors describing how to parse the manufacturer payload of the Bluetooth advertisement for telemetry. |

Each item in `telemetry_descriptor` describes how to parse a single data field and should contain the following properties:

| Field Name               | Data Type | Description                                                  |
| ------------------------ | --------- | ------------------------------------------------------------ |
| `telemetry_name`         | string    | Name of the telemetry point, should match the corresponding telemetry name in IoT Hub. |
| `data_parse_type`        | string    | Data type the raw payload section should be converted to. Must be one of these values: `uint8`, `uint16`, `uint32`, `uint64`, `int8`, `int16`, `int32`, `int64`, `float32`, `float64`. Note this only describes how the raw data is initially parsed. After the `conversion_coefficient` is applied, the final data may have decimal precision or signedness introduced. Be sure to take this into account when selecting the telemetry data type in IoT Hub. |
| `data_offset`            | integer   | Offset in bytes of where the data field begins in the manufacturer payload. |
| `conversion_coefficient` | decimal   | Coefficient applied to the parsed data to obtain the actual value. |
| `conversion_bias`        | decimal   | Bias applied to the parsed data after the `conversion_coefficient` has been applied. |

### 1.2. Define the Sensor Devices

In the `devices` array, populate the sensor device instance:

```cpp
"pnp_bridge_interface_components": [
    {
        "_comment": "Component 5 - Ruuvi Instance 1",
        "pnp_bridge_interface_id": "urn:contoso:com:ruuvi:1",
        "pnp_bridge_component_name": "MyComponent5",
        "pnp_bridge_adapter_id": "bluetooth-sensor-pnp-adapter",
        "pnp_bridge_adapter_config": {
            "bluetooth_address": "251788162761875",
            "blesensor_identity" : "Ruuvi"
        }
    }
]
```

Take note of two important properties:

- `bluetooth_address`
  - The Bluetooth address of the sensor device instance to listen for.
- `blesensor_identity`
  - Must match one of the interface IDs defined above. This allows the adapter to know how the sensor's manufacturer payload is parsed.

### 1.3 Example

Take for example the following manufacturer payload: `00101001 00000001 00101000 11000111`

Using the interface defined in section 1.1 above, the following telemetry points will be reported:

- Lux

  1. Data payload section begins at offset byte 1 for a length of 16 bits: `00000001 00101000`.
  2. `00000001 00101000` is converted to an unsigned integer of 296.
  3. The parsed value is multiplied by the conversion coefficient to produce (296 * 1.0) = 296.
  4. A telemetry report with the name `lux` that has the value `296` will be fired.

- Battery Level

  1. Data payload section begins at offset byte 3 for a length of 8 bits: `11000111`.
  2. `11000111` is converted to an unsigned integer of 199.
  3. The parsed value is multiplied by the conversion coefficient to produce (199 * 0.5) = 99.5.
  4. A telemetry report with the name `battery_level` that has the value `99.5` will be fired.

Note: While battery level's `data_parse_type` was `unsigned integer`, the value that gets reported is `99.5`. This is because in some sensors the raw data is parsed as a simpler type, but the conversion coefficient can introduce decimal precision or signedness. When configuring these telemetry fields in IoT Hub, ensure the real value type is selected (e.g. "Double").

## 2. Design

The Bluetooth sensor adapter is composed of two main classes:

1. `BluetoothSensorPnPBridgeInterface`
  - Defines the `PNP_ADAPTER` interface for the Bluetooth sensor adapter.
  - Responsible for managing the `BluetoothSensorDeviceAdapter` for each device interface instance.

2. `BluetoothSensorDeviceAdapter`
  - Represents a Bluetooth sensor device instance.
  - Listens for Bluetooth advertisements, parses the manufacterer payload, and sends telemetry.

The implementation of `BluetoothSensorDeviceAdapter` is platform dependent as it must access lower level APIs for Bluetooth access. It is expected for each platform to have their own implementation of `BluetoothSensorDeviceAdapter`. All platform dependencies are abstracted out in `BluetoothSensorDeviceAdapter`, which allows the other classes in the Bluetooth sensor adapter to work platform agnostic.

### 2.1 Windows

On Windows, the `BluetoothSensorDeviceAdapter` is implemented as `BluetoothSensorDeviceAdapterWin` and uses the [BluetoothLEAdvertisementWatcher]( https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher) WinRT API to listen for Bluetooth advertisements.
