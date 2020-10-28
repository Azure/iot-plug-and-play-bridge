// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
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

    void
    OnPnpPropertyCallback(
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version,
        void* userContextCallback
    );

    int
    OnPnpCommandCallback(
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize
    );

    void
    SetIotHubClientHandle(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle
    );

    void 
    StartTelemetry();

private:
    MqttConnectionManager*              s_ConnectionManager = nullptr;
    // Maps method name to 
    std::map<std::string, std::string>  s_Telemetry;
    std::map<std::string, std::pair<std::string, std::string>>
                                        s_Commands;
    JsonRpc*                            s_JsonRpc = nullptr;
    std::string                         s_ComponentName;
    PNP_BRIDGE_CLIENT_HANDLE            s_ClientHandle;
    PNP_BRIDGE_IOT_TYPE                 s_ClientType;
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