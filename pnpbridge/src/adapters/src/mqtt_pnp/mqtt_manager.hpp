// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once
#include "mqtt_protocol_handler.hpp"
class MqttConnectionManager {
public:
    void
    Subscribe(
        const char*         Topic,
        MqttProtocolHandler *ProtocolHandler
    );

    void
    Publish(
        const char*         Topic,
        const char*         Message,
        size_t              MessageSize
    );

    void
    Connect(
        const char*         Server,
        int                 Port
    );

    void
    Disconnect();

private:
    MQTT_CLIENT_HANDLE      s_MqttClientHandle = nullptr;
    XIO_HANDLE              s_XioHandle = nullptr;
    bool                    s_ProcessOperation = false;
    bool                    s_OperationSuccess = false;
    std::atomic<uint16_t>   s_NextPacketId{0};
    std::map<std::string, MqttProtocolHandler*> s_Topics;
    std::thread             s_WorkerThread;
    bool                    s_RunWorker = true;

    void
    OnRecv(
        MQTT_MESSAGE_HANDLE         MessageHandle
    );

    void
    OnComplete(
        MQTT_CLIENT_HANDLE          ClientHandle,
        MQTT_CLIENT_EVENT_RESULT    Result, 
        const void*                 MessageInfo
    );

    void
    OnError(
        MQTT_CLIENT_HANDLE          ClientHandle,
        MQTT_CLIENT_EVENT_ERROR     Error
    );

    uint16_t
    GetNextPacketId();
};