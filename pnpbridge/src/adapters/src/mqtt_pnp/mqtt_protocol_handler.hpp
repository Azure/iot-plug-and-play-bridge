// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once
#include "pnpadapter_api.h"
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
    void SetIotHubClientHandle(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) = 0;

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