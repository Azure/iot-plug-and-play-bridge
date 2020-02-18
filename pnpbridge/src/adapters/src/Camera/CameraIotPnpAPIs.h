// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <PnpBridge.h>

// Camera discovery API entry points.
int
CameraPnpStartDiscovery(
    _In_ PNPMEMORY deviceArgs,
    _In_ PNPMEMORY adapterArgs
    );

int 
CameraPnpStopDiscovery(
    );

// Interface functions.
int
CameraPnpInterfaceInitialize(
    _In_ const char* adapterArgs
    );

int
CameraPnpInterfaceShutdown(
    );

int
CameraPnpInterfaceBind(
    _In_ PNPADAPTER_CONTEXT adapterHandle,
    _In_ PNPMESSAGE payload
    );

int
CameraPnpInterfaceRelease(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface
    );

int
CamerePnpStartPnpInterface(
    _In_ PNPADAPTER_INTERFACE_HANDLE PnpInterface
    );


extern DISCOVERY_ADAPTER        CameraPnpAdapter;
extern PNP_ADAPTER     CameraPnpInterface;

// Declare camera command callbacks here.
void 
CameraPnpCallback_TakePhoto(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext, 
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, 
    void* userContextCallback
    );

void
CameraPnpCallback_TakeVideo(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    void* userContextCallback
);

/*
TODO: reenable after I figure out how to configure cmake to add the package dependencies
void
CameraPnpCallback_StartDetection(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    void* userContextCallback
);
*/

void
CameraPnpCallback_GetURI(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    void* userContextCallback
);


#ifdef __cplusplus
}
#endif
