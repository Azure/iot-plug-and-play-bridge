// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif


int 
IotComms_RegisterPnPInterfaces(
    MX_IOT_HANDLE_TAG* IotHandle,
    const char* ModelRepoId,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* interfaces,
    int InterfaceCount
    );

DIGITALTWIN_CLIENT_RESULT IotComms_InitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams);

void IotComms_DigitalTwinClient_Destroy(MX_IOT_HANDLE_TAG* IotHandle);

#ifdef __cplusplus
}
#endif