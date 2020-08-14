#pragma once
#include "pnpbridge.h"
class MqttProtocolHandler {
public:
    virtual
    void
    OnReceive(
        const char*     Topic,
        const char*     Message,
        size_t          MessageSize
    ) = 0;

    virtual
    void
    Initialize(
        class MqttConnectionManager*
                                ConnectionManager,
        JSON_Value*             ProtocolHandlerConfig
    ) = 0;

    virtual
    void SetIotHubDeviceClientHandle(
        IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle) = 0;

    virtual
    void OnPnpPropertyCallback(
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version,
        void* userContextCallback
    ) = 0;

    virtual 
    int OnPnpCommandCallback(
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize
    ) = 0;


    virtual
    void StartTelemetry() = 0;

};