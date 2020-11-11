// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include <assert.h>
#include <stdexcept>
#include <map>
#include <atomic>
#include <mutex>
#include <thread>
#include <list>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "azure_umqtt_c/mqtt_client.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/const_defines.h"
#include "azure_c_shared_utility/threadapi.h"
#include "parson.h"

#include "json_rpc_protocol_handler.hpp"
#include "mqtt_pnp.hpp"

class MqttPnpAdapter {
public:
    std::map<std::string, JSON_Value*>    s_AdapterConfigs;
};

MqttPnpInstance::MqttPnpInstance(
    const std::string& componentName):
    s_ComponentName(componentName),
    s_ProtocolHandler(nullptr)
{

}

IOTHUB_CLIENT_RESULT MqttPnp_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    printf("mqtt-pnp: destroying interface component\n");
    MqttPnpInstance* context = static_cast<MqttPnpInstance*>(PnpComponentHandleGetContext(PnpComponentHandle));
    if (NULL == context)
    {
        return IOTHUB_CLIENT_OK;
    }
    context->s_ConnectionManager.Disconnect();
    delete context;

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT MqttPnp_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle
)
{
    MqttPnpAdapter* adapterContext = reinterpret_cast<MqttPnpAdapter*>(PnpAdapterHandleGetContext(AdapterHandle));
    if (adapterContext == NULL)
    {
        return IOTHUB_CLIENT_OK;
    }

    adapterContext->s_AdapterConfigs.clear();

    delete adapterContext;
    return IOTHUB_CLIENT_OK;
}

static void
MqttPnp_OnPnpPropertyCallback(
    _In_ PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    _In_ const char* PropertyName,
    _In_ JSON_Value* PropertyValue,
    _In_ int version,
    _In_ void* userContextCallback
)
{
    MqttPnpInstance* pnpComponent = static_cast<MqttPnpInstance*>(PnpComponentHandleGetContext(PnpComponentHandle));
    if (pnpComponent != NULL)
    {
        return pnpComponent->s_ProtocolHandler->OnPnpPropertyCallback(PropertyName, PropertyValue, version, userContextCallback);
    }
}

static int
MqttPnp_OnPnpCommandCallback(
    _In_ PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    _In_ const char* CommandName,
    _In_ JSON_Value* CommandValue,
    _Out_ unsigned char** CommandResponse,
    _Out_ size_t* CommandResponseSize
)
{
    MqttPnpInstance* pnpComponent = static_cast<MqttPnpInstance*>(PnpComponentHandleGetContext(PnpComponentHandle));
    if (pnpComponent != NULL)
    {
        return pnpComponent->s_ProtocolHandler->OnPnpCommandCallback(CommandName, CommandValue, CommandResponse, CommandResponseSize);
    }
    
    return PNP_STATUS_SUCCESS;
}

IOTHUB_CLIENT_RESULT
MqttPnp_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    JSON_Value* adapterConfig = NULL;
    std::map<std::string, JSON_Value*>::iterator mapItem;

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        PnpComponentHandle = NULL;
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    if (NULL == AdapterComponentConfig)
    {
        LogError("Each component requiring the Mqtt adapter needs to specify local adapter args (pnp_bridge_adapter_config)");
        return IOTHUB_CLIENT_INVALID_ARG;
    }
    const char* mqtt_server = json_object_get_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_MQTT_SERVER);
    int mqtt_port = (int) json_object_get_number(AdapterComponentConfig, PNP_CONFIG_ADAPTER_MQTT_PORT);
    const char* protocol = json_object_get_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_MQTT_PROTOCOL);
    const char* mqttConfigId = json_object_get_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_MQTT_IDENTITY);

    MqttPnpAdapter* adapterContext = reinterpret_cast<MqttPnpAdapter*>(PnpAdapterHandleGetContext(AdapterHandle));
    if (adapterContext == NULL)
    {
        return IOTHUB_CLIENT_OK;
    }

    MqttPnpInstance* context = new MqttPnpInstance(ComponentName);
    LogInfo("mqtt-pnp: connecting to server %s:%d", mqtt_server, mqtt_port);

    try {
        context->s_ConnectionManager.Connect(mqtt_server, mqtt_port);
    } catch (const std::exception e) {
        LogError("mqtt-pnp: Error connecting to MQTT server: %s", e.what());
    }

    printf("mqtt-pnp: connected to mqtt server\n");

    if (strcmp(protocol, "json_rpc") == 0)
    {
        context->s_ProtocolHandler = (new JsonRpcProtocolHandler(ComponentName));
    }
    else
    {
        throw std::invalid_argument("Unsupported MQTT Protocol");
    }

    mapItem = adapterContext->s_AdapterConfigs.find(std::string(mqttConfigId));
    if (mapItem != adapterContext->s_AdapterConfigs.end())
    {
        adapterConfig =  mapItem->second;
    }
    else
    {
        throw std::invalid_argument("Mqtt adapter doesn't have a supported config");
    }

    context->s_ProtocolHandler->Initialize(&context->s_ConnectionManager, adapterConfig);

    PnpComponentHandleSetContext(PnpComponentHandle, context);
    PnpComponentHandleSetPropertyUpdateCallback(PnpComponentHandle, MqttPnp_OnPnpPropertyCallback);
    PnpComponentHandleSetCommandCallback(PnpComponentHandle, MqttPnp_OnPnpCommandCallback);

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT MqttPnp_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    MqttPnpAdapter* adapterContext = new MqttPnpAdapter();
    if (AdapterGlobalConfig == NULL)
    {
        LogError("Mqtt adapter requires associated global parameters in config");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    // Build map of interface configs supported by the adapter
    for (int i = 0; i < (int)json_object_get_count(AdapterGlobalConfig); i++)
    {
        const char* mqttConfigIdentity = json_object_get_name(AdapterGlobalConfig, i);
        JSON_Value* config = json_object_get_value(AdapterGlobalConfig, mqttConfigIdentity);
        adapterContext->s_AdapterConfigs.insert(std::pair<std::string, JSON_Value*>(std::string(mqttConfigIdentity), config));
    }

    PnpAdapterHandleSetContext(AdapterHandle, (void*)adapterContext);
exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        result = MqttPnp_DestroyPnpAdapter(AdapterHandle);
    }
    return result;
}

IOTHUB_CLIENT_RESULT
MqttPnp_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);

    LogInfo("Starting the PnP interface: %p", PnpComponentHandle);

    MqttPnpInstance* context = static_cast<MqttPnpInstance*>(PnpComponentHandleGetContext(PnpComponentHandle));

    context->s_ProtocolHandler->SetIotHubClientHandle(PnpComponentHandle);

    if (context)
    {
        context->s_ProtocolHandler->StartTelemetry();
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT MqttPnp_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(PnpComponentHandle);
    return IOTHUB_CLIENT_OK;
}

PNP_ADAPTER MqttPnpInterface = {
    "mqtt-pnp-interface",
    MqttPnp_CreatePnpAdapter,
    MqttPnp_CreatePnpComponent,
    MqttPnp_StartPnpComponent,
    MqttPnp_StopPnpComponent,
    MqttPnp_DestroyPnpComponent,
    MqttPnp_DestroyPnpAdapter
};
