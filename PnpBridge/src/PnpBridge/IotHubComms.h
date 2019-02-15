// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

IOTHUB_DEVICE_HANDLE InitializeIotHubDeviceHandle(bool traceOn,const char* connectionString);

int AppRegisterPnPInterfacesAndWait(PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, PNP_INTERFACE_CLIENT_HANDLE* interfaces, int count);

IOTHUB_DEVICE_HANDLE PnPSampleDevice_InitializeIotHubViaProvisioning(bool traceOn, const char* globalProvUri, const char* idScope, const char* deviceId, const char* deviceKey, const char* dcmUri, const char* modelUri);

#ifdef __cplusplus
}
#endif