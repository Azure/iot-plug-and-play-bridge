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
    void StartTelemetry() = 0;

};