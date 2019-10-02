#include "parson.h"

#include <stdexcept>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>

#include <pnpbridge.h>
#include "azure_umqtt_c/mqtt_client.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/condition.h"
#include "azure_c_shared_utility/lock.h"

#include "mqtt_protocol_handler.hpp"
#include "mqtt_manager.hpp"
#include "json_rpc.hpp"
#include "json_rpc_protocol_handler.hpp"

class JsonRpcCallContext {
public:
    // void*                                       Event;
    COND_HANDLE Condition;
    LOCK_HANDLE Lock;
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST*   CommandRequest;
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE*        CommandResponse;
};

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

void
JsonRpcProtocolHandler::OnPnpMethodCall(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST*   CommandRequest,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE*        CommandResponse
)
{
    const char* json_method = nullptr;
    const char* tx_topic = nullptr;
    JSON_Value *commandParams = nullptr;

    printf("Incoming call to %s with %s\n", CommandRequest->commandName, CommandRequest->requestData);

    auto iterator = s_Commands.find(CommandRequest->commandName);
    if (iterator != s_Commands.end()) {
        json_method = iterator->second.first.c_str();
        tx_topic = iterator->second.second.c_str();
    }

    if (CommandRequest->requestData != nullptr) {
        commandParams = json_parse_string((const char*) CommandRequest->requestData);
    }

    JsonRpcCallContext call;
    call.CommandRequest = CommandRequest;
    call.CommandResponse = CommandResponse;
    const char* call_str = s_JsonRpc->RpcCall(json_method, commandParams, &call);
    // todo send params
    printf("Generated JSON %s\n", call_str);
    // if (commandParams) {
    //     json_value_free(commandParams);
    //     commandParams = nullptr;
    // }

    // Create event
    call.Condition = Condition_Init();
    call.Lock = Lock_Init();
    // call.Event = CreateEventA(NULL, TRUE, FALSE, NULL);

    // Send appropriate command over json rpc, return success
    printf("Publishing command call on %s : %s\n", tx_topic, call_str);
    s_ConnectionManager->Publish(tx_topic, call_str, strlen(call_str));
    json_free_serialized_string((char*) call_str);

    printf("Waiting for response\n");
    Lock(call.Lock);
    Condition_Wait(call.Condition, call.Lock, 0);
    Unlock(call.Lock);
    // WaitForSingleObject(call.Event, INFINITE);

    printf("Response length %d, %s\n", (int) CommandResponse->responseDataLen, CommandResponse->responseData);
    // CloseHandle(call.Event);
    Condition_Deinit(call.Condition);
    Lock_Deinit(call.Lock);
}

void
JsonRpcProtocolHandler::RpcResultCallback(
    void*           /*Context*/,
    bool            Success,
    JSON_Value*     Parameters,
    void*           CallContext
)
{
    printf("Got RPC result callback\n");
    JsonRpcCallContext* ctx = static_cast<JsonRpcCallContext*>(CallContext);

    char* response_str = json_serialize_to_string(Parameters);

    mallocAndStrcpy_s((char**) &ctx->CommandResponse->responseData, response_str);
    ctx->CommandResponse->responseDataLen = strlen(response_str);
    ctx->CommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;

    if (Success) {
        ctx->CommandResponse->status = 200;
    } else {
        ctx->CommandResponse->status = 500;
    }

    json_free_serialized_string(response_str);

    Lock(ctx->Lock);
    Condition_Post(ctx->Condition);
    Unlock(ctx->Lock);

    // SetEvent(ctx->Event);
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

    if (!ph->s_DtInterface) return;

    auto iterator = ph->s_Telemetry.find(Method);
    if (iterator != ph->s_Telemetry.end()) {
        tname = iterator->second.c_str();
    }

    if (tname) {
        // Publish telemetry
        char *out = json_serialize_to_string(Parameters);
        DIGITALTWIN_CLIENT_RESULT res;
        if ((res = DigitalTwin_InterfaceClient_SendTelemetryAsync(ph->s_DtInterface,
                                                                  tname,
                                                                  out,
                                                                  nullptr,
                                                                  nullptr)) != DIGITALTWIN_CLIENT_OK) {

            LogError("mqtt-pnp: Error sending telemetry %s, result=%d", tname, res);
        } else {
            LogInfo("mqtt-pnp: Reported telemetry %s with parameters %s", tname, out);
        }

        json_free_serialized_string(out);
    } else {
        LogInfo("mqtt-pnp: Unknown RPC notification '%s' on topic", Method);
    }
}

void
JsonRpcProtocolHandler::AssignDigitalTwin(
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE DtHandle
)
{
    s_DtInterface = DtHandle;
}