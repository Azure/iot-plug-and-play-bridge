# MQTT Adapter

The MQTT adapter is an IoT Plug and Play bridge adapter which allows the IoT Plug and Play bridge to communicate with applications and devices that are capable of communicating over the MQTT protocol. This makes MQTT interfaces accessible as IoT Plug and Play interfaces.

The MQTT adapter is configured with information about an existing MQTT broker and protocol for interfacing with a device, and it will publish/subscribe to the configured topics and publish the configured interface to Azure, allowing downstream MQTT devices/applications to have an IoT Plug and Play interface.

The adapter supports the following protocols:
- JSON RPC over MQTT

The adapter supports the following functionality:
- Subscribe to a topic and transmit JSON-RPC notifications as Telemetry to Azure
- Send commands from Azure as JSON-RPC calls on an MQTT channel, and send the response back to Azure

## 1. Adapter Configuration

The MQTT adapter is configured in `config.json`, and requires information about the MQTT broker/topics and the JSON-RPC commands and notifications it will use.

An example configuration for a temperature sensor device which connects to an MQTT broker is here:

```json
"devices": [
    {
        "_comment": "Sample MQTT Temperature Sensor",
        "self_describing": "true",
        "match_filters": {
            "match_type": "exact"
        },
        "pnp_parameters": {
            "identity": "mqtt-pnp-interface"
        },
        "discovery_parameters": {
            "identity": "mqtt-pnp-discovery",
            "interface":"urn:contoso:com:temperatureSensor:1",
            "component_name": "temperature_sensor",
            "mqtt_server":"192.168.1.1",
            "mqtt_port":1883,
            "protocol":"json_rpc",
            "config":[
                {
                    "type":"telemetry",
                    "json_rpc_method":"ambient_temperature",
                    "name":"ambient_temperature",
                    "rx_topic":"telemetry"
                },
                {
                    "type":"command",
                    "json_rpc_method":"set_telemetry_state",
                    "name":"set_telemetry_state",
                    "tx_topic":"control",
                    "rx_topic":"control"
                }
            ]
        }
    }
]
```

The `discovery_parameters` section contains the configuration the MQTT adapter uses to connect to the broker and communicate with the downstream device/application.

The following fields are required:

| Field Name            | Description                              |
| --------------------- | ---------------------------------------- |
| `identity`            | Must be `mqtt-pnp-discovery`. |
| `interface`           | The URN of this interface as described in the DCM. |
| `component_name`      | The component name of this interface as described in the DCM. |
| `mqtt_server`         | IP address or domain name of the MQTT broker to connect to. |
| `mqtt_port`           | Port of the MQTT broker to connect to. |
| `protocol`            | The protocol that will be used to communicate over MQTT. Only `json_rpc` is currently supported. |
| `config`              | Config specific to the protocol (e.g. JSON RPC). Please see Section 2 for more information. |

## 2. Protocol Configuration (JSON-RPC)

Currently, the JSON-RPC protocol is supported by the adapter. JSON-RPC defines payload structure, and allows for messages which require a response (normal Requests), and messages that do not require a response (Notifications). The adapter supports mapping commands to normal requests, and mapping notifications from a downstream device to telemetry.

The `config` section is made up of one or more entries, each of which may be `telemetry` or `command`. Each entry may have the following fields:

| Field Name            | Required          | Description                              |
| --------------------- | ----------------- | ---------------------------------------- |
| `type`                | Yes               | Must be `telemetry` or `command`, indicating the type of entry. |
| `json_rpc_method`     | Yes               | The `method` to be used for the JSON-RPC call. |
| `name`                | Yes               | The name of this entry as defined in the interface definition. |
| `tx_topic`            | Only for commands | The MQTT topic the JSON-RPC call will be sent on. |
| `rx_topic`            | Yes               | The MQTT topic the JSON-RPC notification or response will be recieved on. |

## 3. Extensibility

The adapter is designed to be easily extended to support new protocols over MQTT. The MQTT connection and message handling logic is abstracted, such that new protocols may be supported by creating a new class which implements `MqttProtocolHandler`. This class will recieve an instance of `MqttConnectionManager` in its `Initialize` method, which it may use to subscribe to topics and recieve callbacks.

`JsonRpcProtocolHandler` (`json_rpc_protocol_handler.cpp`) provides an example of how a protocol handler may be implemented.