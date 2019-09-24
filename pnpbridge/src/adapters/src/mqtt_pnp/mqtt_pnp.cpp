// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
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

#include <assert.h>

#include <stdexcept>
#include <map>
#include <atomic>
#include <mutex>
#include <thread>
#include <list>

#include <pnpbridge.h>

#include "mqtt_pnp.hpp"
#include "mqtt_protocol_handler.hpp"
#include "mqtt_manager.hpp"
#include "json_rpc.hpp"
#include "json_rpc_protocol_handler.hpp"

const char* mqttDeviceChangeMessage = "{ \
                                       \"identity\": \"mqtt-pnp-discovery\" \
                                       }";

class MqttPnpInstance {
public:
    MqttConnectionManager   s_ConnectionManager;
    MqttProtocolHandler*    s_ProtocolHandler;
};

std::list<MqttPnpInstance*> g_MqttPnpInstances;

int
MqttPnp_StartDiscovery(
    _In_ PNPMEMORY DeviceArgs,
    _In_ PNPMEMORY AdapterArgs
    )
{
    AZURE_UNREFERENCED_PARAMETER(AdapterArgs);

    if (DeviceArgs == nullptr) {
        LogError("mqtt-pnp: requires device arguments to be specified");
        return -1;
    }

    PnpMemory_AddReference(DeviceArgs);
    PDEVICE_ADAPTER_PARMAETERS deviceParams = (PDEVICE_ADAPTER_PARMAETERS) PnpMemory_GetBuffer(DeviceArgs, NULL);

    LogInfo("Starting mqtt-pnp: plugin with %d configurations", deviceParams->Count);

    // Dynamic discovery is not supported. For each device defined in DeviceArgs,
    // parse and instantiate the relevant classes, then report said device to the bridge.
    for (int i = 0; i < deviceParams->Count; i++) {
        JSON_Value* jvalue = json_parse_string(deviceParams->AdapterParameters[0]);
        JSON_Object* args = json_value_get_object(jvalue);

        // For each device we parse the interface and config space,
        // and report them in a PnPMessage.
        const char* interface = json_object_get_string(args, "interface");
        const char* mqtt_server = json_object_get_string(args, "mqtt_server");
        int mqtt_port = (int) json_object_get_number(args, "mqtt_port");
        const char* protocol = json_object_get_string(args, "protocol");
        const char* component = json_object_get_string(args, "component_name");

        JSON_Value* params = json_object_get_value(args, "config");

        MqttPnpInstance* context = new MqttPnpInstance();
        LogInfo("mqtt-pnp: connecting to server %s:%d", mqtt_server, mqtt_port);

        try {
            context->s_ConnectionManager.Connect(mqtt_server, mqtt_port);
        } catch (const std::exception e) {
            LogError("mqtt-pnp: Error connecting to MQTT server: %s", e.what());
        }

        printf("mqtt-pnp: connected to mqtt server\n");

        if (strcmp(protocol, "json_rpc") == 0) {
            context->s_ProtocolHandler = (new JsonRpcProtocolHandler());
        } else {
            throw std::invalid_argument("Unsupported MQTT Protocol");
        }

        context->s_ProtocolHandler->Initialize(&context->s_ConnectionManager, params);

        PNPMESSAGE payload = nullptr;
        PNPMESSAGE_PROPERTIES* pnpMsgProps = nullptr;
        PnpMessage_CreateMessage(&payload);

        //const char* interface_params = json_serialize_to_string(params);
        //PnpMessage_SetMessage(interface_params);
        PnpMessage_SetInterfaceId(payload, interface);
        PnpMessage_SetMessage(payload, mqttDeviceChangeMessage);
        pnpMsgProps = PnpMessage_AccessProperties(payload);
        pnpMsgProps->Context = context;
        mallocAndStrcpy_s(&pnpMsgProps->ComponentName, component);

        LogInfo("mqtt-pnp: reporting interface %s", interface);
        DiscoveryAdapter_ReportDevice(payload);

        //json_free_serialized_string((char*) interface_params)
        json_value_free(jvalue);
        // json_value_free(params);

        g_MqttPnpInstances.push_back(context);

        PnpMemory_ReleaseReference(payload);
    }

    // PnpMemory_ReleaseReference(AdapterArgs);

    return 0;
}

int
MqttPnp_StopDiscovery()
{
    printf("mqtt-pnp: stop discovery\n");

    // for (auto it = g_MqttPnpInstances.begin(); it != g_MqttPnpInstances.end(); it++) {
    //     (*it)->s_ConnectionManager.Disconnect();
    //     delete (*it)->s_ProtocolHandler;
    //     (*it)->s_ProtocolHandler = nullptr;
    //     delete *it;
    // }

    // g_MqttPnpInstances = std::list<MqttPnpInstance*>();

    // printf("MQTTPNP stopped discovery\n");

    return 0;
}

int
MqttPnp_Initialize(const char* AdapterArgs)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterArgs);
    printf("mqtt-pnp: initializing\n");
    // if (platform_init() != 0) {
    //     LogError("mqtt platform_init failed\r\n");
    //     return -1;
    // }

    return 0;
}

int 
MqttPnp_Shutdown()
{
    printf("mqtt-pnp: shutdown\n");
    // platform_deinit();
    return 0;
}

int
MqttPnp_ReleaseInterface(
    _In_ PNPADAPTER_INTERFACE_HANDLE pnpInterface
    )
{
    printf("mqtt-pnp: releasing interface\n");
    MqttPnpInstance* context = static_cast<MqttPnpInstance*>(PnpAdapterInterface_GetContext(pnpInterface));

    context->s_ConnectionManager.Disconnect();
    delete context->s_ProtocolHandler;

    delete context;

    return 0;
}

int
MqttPnp_Bind(
    _In_ PNPADAPTER_CONTEXT AdapterHandle,
    _In_ PNPMESSAGE         Message
    )
{
    int result = 0;
    PNPMESSAGE_PROPERTIES* pnpMsgProps = nullptr;
    MqttPnpInstance* context = nullptr;
    DIGITALTWIN_CLIENT_RESULT dtres;

    pnpMsgProps = PnpMessage_AccessProperties(Message);
    context = static_cast<MqttPnpInstance*>(pnpMsgProps->Context);
    const char* interface = PnpMessage_GetInterfaceId(Message);

    DIGITALTWIN_INTERFACE_CLIENT_HANDLE digitalTwinInterface;
    PNPADPATER_INTERFACE_PARAMS interfaceParams = { 0 };
    PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;

    printf("mqtt-pnp: publishing interface %s, component %s\n", interface, pnpMsgProps->ComponentName);

    // Create Digital Twin Interface
    dtres = DigitalTwin_InterfaceClient_Create(interface,
                                               pnpMsgProps->ComponentName,
                                               nullptr,
                                               context, 
                                               &digitalTwinInterface);
    if (DIGITALTWIN_CLIENT_OK != dtres) {
        LogError("mqtt-pnp: Error registering pnp interface %s", interface);
        result = -1;
        goto exit;
    }

    DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback(digitalTwinInterface,
        [](
            const DIGITALTWIN_CLIENT_PROPERTY_UPDATE*   /*dtClientPropertyUpdate*/,
            void*                                       /*userContextCallback*/
        )
        {
            return;
        }
    );

    DigitalTwin_InterfaceClient_SetCommandsCallback(digitalTwinInterface,
        [](
            const DIGITALTWIN_CLIENT_COMMAND_REQUEST*   dtClientCommandContext,
            DIGITALTWIN_CLIENT_COMMAND_RESPONSE*        dtClientCommandResponseContext,
            void*                                       userContextCallback
        )
        {
            static_cast<MqttPnpInstance*>(userContextCallback)->s_ProtocolHandler->OnPnpMethodCall(dtClientCommandContext, dtClientCommandResponseContext);
        }
    );

    // Create PnpBridge-PnpAdapter Interface
    PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, AdapterHandle, digitalTwinInterface);

    interfaceParams.StartInterface = [](
        PNPADAPTER_INTERFACE_HANDLE /*PnpInterface*/
    )
    {
        return 0;
    };

    interfaceParams.ReleaseInterface = MqttPnp_ReleaseInterface;

    interfaceParams.InterfaceId = (char*) interface;

    result = PnpAdapterInterface_Create(&interfaceParams, &pnpAdapterInterface);
    // if (result < 0) {
    //     goto exit;
    // }

    PnpAdapterInterface_SetContext(pnpAdapterInterface, context);
    context->s_ProtocolHandler->AssignDigitalTwin(digitalTwinInterface);

exit:
    printf("mqtt-pnp: bound interface\n");
    return result;
}

DISCOVERY_ADAPTER MqttPnpAdapter = {
    "mqtt-pnp-discovery",
    MqttPnp_StartDiscovery,
    MqttPnp_StopDiscovery
};

PNP_ADAPTER MqttPnpInterface = {
    "mqtt-pnp-interface",
    MqttPnp_Initialize,
    MqttPnp_Bind,
    MqttPnp_Shutdown
};
