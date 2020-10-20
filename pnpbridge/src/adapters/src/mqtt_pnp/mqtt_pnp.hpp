// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include "mqtt_protocol_handler.hpp"
#include <pnpadapter_api.h>
#include <nosal.h>

#ifdef __cplusplus
extern "C" {
#endif
extern PNP_ADAPTER          MqttPnpInterface;

#ifdef __cplusplus
}
#endif

class MqttPnpInstance {
public:
    MqttPnpInstance(
        const std::string& componentName);
    MqttConnectionManager   s_ConnectionManager;
    MqttProtocolHandler* s_ProtocolHandler;
    std::string s_ComponentName;
};

// MQTT Adapter Config
#define PNP_CONFIG_ADAPTER_MQTT_IDENTITY "mqtt_identity"
#define PNP_CONFIG_ADAPTER_MQTT_PROTOCOL "mqtt_protocol"
#define PNP_CONFIG_ADAPTER_MQTT_SERVER "mqtt_server"
#define PNP_CONFIG_ADAPTER_MQTT_PORT "mqtt_port"
#define PNP_CONFIG_ADAPTER_MQTT_SUPPORTED_CONFIG "json_rpc_1"
