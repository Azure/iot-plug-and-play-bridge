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

#ifdef __cplusplus
}
#endif
