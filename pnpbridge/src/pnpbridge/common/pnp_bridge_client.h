// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PNP_BRIDGE_CLIENT_H
#define PNP_BRIDGE_CLIENT_H

#include "iothub_device_client.h"
#include "iothub_module_client.h"

#ifdef USE_MODULE_CLIENT
    typedef IOTHUB_MODULE_CLIENT_HANDLE PNP_BRIDGE_CLIENT_HANDLE;
    #define PnpBridgeClient_SendReportedState(iotHubClientHandle, reportedState, size, reportedStateCallback, userContextCallback) IoTHubModuleClient_SendReportedState(iotHubClientHandle, reportedState, size, reportedStateCallback, userContextCallback)
    #define PnpBridgeClient_SendEventAsync(iotHubClientHandle, eventMessageHandle, eventConfirmationCallback, userContextCallback) IoTHubModuleClient_SendEventAsync(iotHubClientHandle, eventMessageHandle, eventConfirmationCallback, userContextCallback)
#else
    typedef IOTHUB_DEVICE_CLIENT_HANDLE PNP_BRIDGE_CLIENT_HANDLE;
    #define PnpBridgeClient_SendReportedState(iotHubClientHandle, reportedState, size, reportedStateCallback, userContextCallback) IoTHubDeviceClient_SendReportedState(iotHubClientHandle, reportedState, size, reportedStateCallback, userContextCallback)
    #define PnpBridgeClient_SendEventAsync(iotHubClientHandle, eventMessageHandle, eventConfirmationCallback, userContextCallback) IoTHubDeviceClient_SendEventAsync(iotHubClientHandle, eventMessageHandle, eventConfirmationCallback, userContextCallback)
#endif

#endif /* PNP_BRIDGE_CLIENT_H */