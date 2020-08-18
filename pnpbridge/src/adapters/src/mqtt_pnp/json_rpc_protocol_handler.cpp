// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include <stdexcept>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include "parson.h"
#include <pnpbridge.h>
#include "azure_umqtt_c/mqtt_client.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/condition.h"
#include "azure_c_shared_utility/lock.h"

#include "json_rpc_protocol_handler.hpp"

class JsonRpcCallContext {
public:
    COND_HANDLE Condition;
    LOCK_HANDLE Lock;
    unsigned char** CommandResponse;
    size_t* CommandResponseSize;
    int* CommandResponseStatus;
};

JsonRpcProtocolHandler::JsonRpcProtocolHandler(
        const std::string& ComponentName) :
        s_ComponentName(ComponentName),
        s_TelemetryStarted(false),
        s_DeviceClient(NULL)
{

}

void
JsonRpcProtocolHandler::OnReceive(
    const char*     Topic,
    const char*     Message,
    size_t          MessageSize
)
{
    try {
        s_JsonRpc->Process(Message, MessageSize);
    } catch (const std::exception e) {
        LogError("mqtt-pnp: Error parsing JSON-RPC on topic %s : %s", Topic, e.what());
    }
}

void
JsonRpcProtocolHandler::Initialize(
    class MqttConnectionManager*
                            ConnectionManager,
    JSON_Value*             ProtocolHandlerConfig
)
{
    s_ConnectionManager = ConnectionManager;
    s_JsonRpc = new JsonRpc(RpcNotificationCallback, nullptr, RpcResultCallback, this);

    // Parse config, save channel <-> command mappings
    // and subscribe to correct channels / topics.

    JSON_Array* conf = json_value_get_array(ProtocolHandlerConfig);

    for (size_t i = 0; i < json_array_get_count(conf); i++) {
        JSON_Object *co = json_array_get_object(conf, i);
        const char* type = json_object_get_string(co, "type");

        if (strcmp(type, "telemetry") == 0) {
            const char* name = json_object_get_string(co, "name");
            const char* method = json_object_get_string(co, "json_rpc_method");
            const char* topic = json_object_get_string(co, "rx_topic");

            s_Telemetry.insert(std::pair<std::string, std::string>(std::string(method),
                                                                   std::string(name)));

            s_ConnectionManager->Subscribe(topic, this);
        }

        else if (strcmp(type, "command") == 0) {
            const char* name = json_object_get_string(co, "name");
            const char* method = json_object_get_string(co, "json_rpc_method");
            const char* rx_topic = json_object_get_string(co, "rx_topic");
            const char* tx_topic = json_object_get_string(co, "tx_topic");

            s_Commands.insert(std::pair<std::string, std::pair<std::string, std::string>>(
                                std::string(name),
                                std::pair<std::string, std::string>(
                                    std::string(method), std::string(tx_topic)
                                )));

            s_ConnectionManager->Subscribe(rx_topic, this);
        }
    }
}

void JsonRpcProtocolHandler::OnPnpPropertyCallback(
    const char* /* PropertyName */,
    JSON_Value* /* PropertyValue */,
    int /* version */,
    void* /* userContextCallback */)
{
    // no-op
}

int JsonRpcProtocolHandler::OnPnpCommandCallback(
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    int result = PNP_STATUS_SUCCESS;
    const char* json_method = nullptr;
    const char* tx_topic = nullptr;
    const char* commandValueString = nullptr;


    if ((commandValueString = json_value_get_string(CommandValue)) == NULL)
    {
        LogError("Component %s: Cannot retrieve JSON string for command", s_ComponentName.c_str());
        result = PNP_STATUS_BAD_FORMAT;
    }

    printf("Incoming call to %s with %s\n", CommandName, commandValueString);

    auto iterator = s_Commands.find(CommandName);
    if (iterator != s_Commands.end()) {
        json_method = iterator->second.first.c_str();
        tx_topic = iterator->second.second.c_str();
    }

    JsonRpcCallContext call;
    int * responseStatus = &result;
    call.CommandResponse = CommandResponse;
    call.CommandResponseSize = CommandResponseSize;
    call.CommandResponseStatus = responseStatus;
    const char* call_str = s_JsonRpc->RpcCall(json_method, CommandValue, &call);

    printf("Generated JSON %s\n", call_str);


    // Create event
    call.Condition = Condition_Init();
    call.Lock = Lock_Init();

    // Send appropriate command over json rpc, return success
    printf("Publishing command call on %s : %s\n", tx_topic, call_str);
    s_ConnectionManager->Publish(tx_topic, call_str, strlen(call_str));
    json_free_serialized_string((char*) call_str);

    printf("Waiting for response\n");
    Lock(call.Lock);
    Condition_Wait(call.Condition, call.Lock, 0);
    Unlock(call.Lock);

    printf("Response length %d, %s\n", (int) *CommandResponseSize, *CommandResponse);
    Condition_Deinit(call.Condition);
    Lock_Deinit(call.Lock);

    result = *responseStatus;

    return result;
}

void
JsonRpcProtocolHandler::RpcResultCallback(
    void*           /*Context*/,
    bool            Success,
    JSON_Value*     Parameters,
    void*           CallContext
)
{
    int result = PNP_STATUS_SUCCESS;
    printf("Got RPC result callback\n");
    JsonRpcCallContext* ctx = static_cast<JsonRpcCallContext*>(CallContext);

    char* response_str = json_serialize_to_string(Parameters);

    mallocAndStrcpy_s((char**) ctx->CommandResponse, response_str);
    *ctx->CommandResponseSize = strlen(response_str);

    if (!Success)
    {
        result = PNP_STATUS_INTERNAL_ERROR;
    }

    *ctx->CommandResponseStatus = result;

    json_free_serialized_string(response_str);

    Lock(ctx->Lock);
    Condition_Post(ctx->Condition);
    Unlock(ctx->Lock);

}

void
JsonRpcProtocolHandler::RpcNotificationCallback(
    void*           Context,
    const char*     Method,
    JSON_Value*     Parameters
)
{
    JsonRpcProtocolHandler *ph = static_cast<JsonRpcProtocolHandler*>(Context);
    const char* tname = nullptr;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    auto iterator = ph->s_Telemetry.find(Method);
    if (iterator != ph->s_Telemetry.end()) {
        tname = iterator->second.c_str();
    }

    if (tname)
    {
        if (ph->s_TelemetryStarted)
        {
            // Publish telemetry
            char *out = json_serialize_to_string(Parameters);
            const char* telemetryMessageFormat = "{\"%s\":%s}";
            size_t telemetryMessageLen = (strlen(tname) + strlen(out) + strlen(telemetryMessageFormat) + 1);
            char  * telemetryMessage = (char*) malloc(sizeof(char) * telemetryMessageLen);
            if (telemetryMessage)
            {
                sprintf(telemetryMessage, telemetryMessageFormat, tname, out);
            }

            if ((messageHandle = PnP_CreateTelemetryMessageHandle(ph->s_ComponentName.c_str(), telemetryMessage)) == NULL)
            {
                LogError("Mqtt Pnp Component: PnP_CreateTelemetryMessageHandle failed.");
            }
            else if ((result = IoTHubDeviceClient_SendEventAsync(ph->s_DeviceClient, messageHandle,
                    NULL, NULL)) != IOTHUB_CLIENT_OK)
            {
                LogError("Mqtt Pnp Component: IoTHubDeviceClient_SendEventAsync failed, error=%d", result);
            }
            else
            {
                LogInfo("Mqtt Pnp Component: Reported telemetry %s with parameters %s", tname, out);
            }

            IoTHubMessage_Destroy(messageHandle);
            json_free_serialized_string(out);
            if (telemetryMessage)
            {
                free(telemetryMessage);
            }
        }
    }
    else
    {
        LogInfo("Mqtt Pnp Component: Unknown RPC notification '%s' on topic", Method);
    }
}

void JsonRpcProtocolHandler::SetIotHubDeviceClientHandle(
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle)
{
    s_DeviceClient = DeviceClientHandle;
}

void JsonRpcProtocolHandler::StartTelemetry()
{
    s_TelemetryStarted = true;
}