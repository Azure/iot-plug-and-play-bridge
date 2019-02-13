#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <PnpBridge.h>

// Camera discovery API entry points.
int
CameraPnpStartDiscovery(
    _In_ PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback,
    _In_ const char* deviceArgs, 
    _In_ const char* adapterArgs
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
    _In_ PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, 
    _In_ PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload
    );

int
CameraPnpInterfaceRelease(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface
    );

extern DISCOVERY_ADAPTER        CameraPnpAdapter;
extern PNP_ADAPTER     CameraPnpInterface;

#ifdef __cplusplus
}
#endif
