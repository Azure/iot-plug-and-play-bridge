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

The Bluetooth sensor adapter is configured in `config.json` and is a two step process, described below.

### 1.1. Define the Sensor Payload Descriptor

Ensure the Bluetooth discovery adapter has been specified inside the `discovery_adapters` array in `config.json`:

```cpp
"discovery_adapters": {
  "parameters": [
    {
      "identity": "bluetooth-sensor-discovery-adapter",
      "_comment1": "Bluetooth Enabled Sensor Discovery Adapter"
    }
  ]
}
```

Then add and populate the `interface_descriptors` property to describe how the Bluetooth advertisement payload should be parsed for each sensor interface:

```cpp
"discovery_adapters": {
  "parameters": [
    {
      "identity": "bluetooth-sensor-discovery-adapter",
      "_comment1": "Bluetooth Enabled Sensor",
      "interface_descriptors": [
        {
          "interface_id": "urn:contoso:com:lightsensor:1",
          "company_id": "0x3FF",
          "endianness": "little",
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
      ]
    }
  ]
}
```

Each item in `interface_descriptors` should contain the following properties:

| Field Name             | Data Type | Description                                                  |
| ---------------------- | --------- | ------------------------------------------------------------ |
| `interface_id`         | string    | Unique identifier for the sensor interface.                  |
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

In the `devices` array, populate each sensor device instance:

```cpp
"devices": [
  {
    "_comment": "Light Sensor Instance 1",
    "match_filters": {
      "match_type": "exact",
      "match_parameters": {
        "bluetooth_address": "222939435"
      }
    },
    "interface_id": "urn:contoso:com:lightsensor:1",
    "component_name": "LightSensor1",
    "pnp_parameters": {
      "identity": "bluetooth-sensor-pnp-adapter"
    }
  },
  ...
```

Take note of two important properties:

- `bluetooth_address`
  - The Bluetooth address of the sensor device instance.
- `interface_id`
  - Must match one of the interface IDs defined in `interface_descriptors` above. This allows the adapter to know how the sensor's manufacturer payload is parsed.

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

The Bluetooth sensor adapter is composed of two main interfaces:

1. `BluetoothSensorDiscoveryAdapter`
   - Listens to BLE advertisements.
   - Creates a new `BluetoothSensorDeviceAdapter` instance when an advertisement from a Bluetooth address is received for the first time.
   - Forwards advertisements to the appropriate `BluetoothSensorDeviceAdapter` instance for parsing if an advertisement from the Bluetooth address has already occurred.
2. `BluetoothSensorDeviceAdapter`
   - Represents a Bluetooth sensor device.
   - Responsible for parsing the Bluetooth payload via the user defined descriptor and sending the corresponding telemetry.


The implementation of `BluetoothSensorDiscoveryAdapter` is platform dependent as it must access lower level APIs for Bluetooth access. Therefore it is expected for each platform to have their own implementation of `BluetoothSensorDiscoveryAdapter`. All platform dependencies are abstracted out in `BluetoothSensorDiscoveryAdapter`, which allows the other classes in the Bluetooth sensor adapter to work platform agnostic.

### 2.1 Windows

On Windows, the `BluetoothSensorDiscoveryAdapter` is implemented as `BluetoothSensorDiscoveryAdapterWin` and uses the [BluetoothLEAdvertisementWatcher]( https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher) WinRT API to listen for Bluetooth advertisements.

A [BluetoothLEAdvertisementWatcher]( https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher) is created for every `company_id` specified in `config.json` and its [BluetoothLEAdvertisement.ManufacturerData]( https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisement.manufacturerdata#Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisement_ManufacturerData) is set to its respective `company_id` to act as first level filtering. Once an advertisement does arrive, `BluetoothSensorDiscoveryAdapterWin`  will then manually validate filter out advertisements which did not come from a known `bluetooth_address` in `config.json`.
