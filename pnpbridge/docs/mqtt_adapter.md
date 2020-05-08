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
{
     "pnp_bridge_interface_components": [
       {
        "pnp_bridge_interface_id": "urn:contoso:com:mqtt-device:1",
        "pnp_bridge_component_name": "MqttComponent",
        "pnp_bridge_adapter_id": "mqtt-pnp-interface",
        "pnp_bridge_adapter_config": {
            "mqtt_port": "1",
            "mqtt_server": "1.1.1.1",
            "mqtt_protocol": "json_rpc",
            "mqtt_identity": "json_rpc_1"
        }
      }
    ]
}
```

The `pnp_bridge_adapter_config` section contains the configuration the MQTT adapter uses to connect to the broker and communicate with the downstream device/application.

The following fields are required:

| Field Name            | Description                              |
| --------------------- | ---------------------------------------- |
| `pnp_bridge_adapter_id` | Must be `mqtt-pnp-discovery`. |
| `pnp_bridge_interface_id`           | The URN of this interface as described in the DCM. |
| `pnp_bridge_component_name`      | The component name of this interface as described in the DCM. |
| `mqtt_server`         | IP address or domain name of the MQTT broker to connect to. |
| `mqtt_port`           | Port of the MQTT broker to connect to. |
| `mqtt_protocol`            | The protocol that will be used to communicate over MQTT. Only `json_rpc` is currently supported. |
| `mqtt_identity`              | Config specific to the protocol (e.g. JSON RPC). Please see Section 2 for more information. |

## 2. Protocol Configuration (JSON-RPC)

Currently, the JSON-RPC protocol is supported by the adapter. JSON-RPC defines payload structure, and allows for messages which require a response (normal Requests), and messages that do not require a response (Notifications). The adapter supports mapping commands to normal requests, and mapping notifications from a downstream device to telemetry. The adapter must specify this in its global adapter configuration section `pnp_bridge_adapter_global_configs`. Since the MQTT adapter currently only supports JSON RPC, `json_rpc_1` is the only supported `mqtt_identity`

```json
{
     "pnp_bridge_adapter_global_configs": {
      "mqtt-pnp-interface": {
        "json_rpc_1": [
            {
                "type": "telemetry",
                "json_rpc_method": "inventory_summary",
                "name": "inventory_summary",
                "rx_topic": "rfid/controller/events"
            },
            {
                "type": "telemetry",
                "json_rpc_method": "inventory_event",
                "name": "inventory_event",
                "rx_topic": "rfid/controller/events"
            },
            {
                "type": "command",
                "json_rpc_method": "inventory_get_tag_info",
                "name": "inventory_get_tag_info",
                "tx_topic": "rfid/controller/command",
                "rx_topic": "rfid/controller/response"
            },
            {
                "type": "command",
                "name": "inventory_unload",
                "json_rpc_method": "inventory_unload",
                "tx_topic": "rfid/controller/command",
                "rx_topic": "rfid/controller/response"
            },
            {
                "type": "command",
                "name": "scheduler_set_run_state",
                "json_rpc_method": "scheduler_set_run_state",
                "tx_topic": "rfid/controller/command",
                "rx_topic": "rfid/controller/response"
            }
        ]
    }
  }
}
```

The interface configuration section is made up of one or more entries, each of which may be `telemetry` or `command`. Each entry may have the following fields:

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