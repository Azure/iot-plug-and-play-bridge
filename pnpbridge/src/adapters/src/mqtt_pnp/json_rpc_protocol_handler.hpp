#pragma once
#include "mqtt_manager.hpp"
#include "json_rpc.hpp"

class JsonRpcProtocolHandler : public MqttProtocolHandler {
public:

    JsonRpcProtocolHandler(
        const std::string& ComponentName);
    void
    OnReceive(
        const char*     Topic,
        const char*     Message,
        size_t          MessageSize
    );

    void
    Initialize(
        class MqttConnectionManager*
                                ConnectionManager,
        JSON_Value*             ProtocolHandlerConfig
    );

    static void
    OnPnpPropertyCallback(
        _In_ PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        _In_ const char* PropertyName,
        _In_ JSON_Value* PropertyValue,
        _In_ int version,
        _In_ void* userContextCallback
    );

    static int
    OnPnpCommandCallback(
        _In_ PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        _In_ const char* CommandName,
        _In_ JSON_Value* CommandValue,
        _Out_ unsigned char** CommandResponse,
        _Out_ size_t* CommandResponseSize
    );

    void
    SetIotHubDeviceClientHandle(
        IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle
    );

    void 
    StartTelemetry();

    MqttConnectionManager* GetConnectionManager() { return s_ConnectionManager; }
    std::map<std::string, std::pair<std::string, std::string>> GetCommands() { return s_Commands; }
    JsonRpc* GetJsonRpc() { return s_JsonRpc; }

private:
    MqttConnectionManager*              s_ConnectionManager = nullptr;
    // Maps method name to 
    std::map<std::string, std::string>  s_Telemetry;
    std::map<std::string, std::pair<std::string, std::string>>
                                        s_Commands;
    JsonRpc*                            s_JsonRpc = nullptr;
    std::string                         s_ComponentName;
    IOTHUB_DEVICE_CLIENT_HANDLE         s_DeviceClient;
    bool                                s_TelemetryStarted;

    static
    void
    RpcResultCallback(
        void*           Context,
        bool            Success,
        JSON_Value*     Parameters,
        void*           CallContext
    );

    static
    void
    RpcNotificationCallback(
        void*           Context,
        const char*     Method,
        JSON_Value*     Parameters
    );
};