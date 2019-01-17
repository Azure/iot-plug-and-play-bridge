#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <DiscoveryAdapterInterface.h>
#include <PnpAdapterInterface.h>

#include "pch.h"

// Camera discovery API entry points.
int
CameraPnpStartDiscovery(
    _In_ PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback,
    _In_ JSON_Object* deviceArgs, 
    _In_ JSON_Object* adapterArgs
    );

int 
CameraPnpStopDiscovery(
    );

int
CameraGetFilterFormatIds(
    _Out_ char*** filterFormatIds,
    _Out_ int* numberOfFormats
    );


// Interface functions.
PNPBRIDGE_RESULT
CameraPnpInterfaceInitialize(
    _In_ JSON_Object* adapterArgs
    );

PNPBRIDGE_RESULT
CameraPnpInterfaceShutdown(
    );

PNPBRIDGE_RESULT
CameraPnpInterfaceBind(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface, 
    _In_ PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, 
    _In_ PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload
    );

PNPBRIDGE_RESULT
CameraPnpInterfaceRelease(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface
    );

#ifdef __cplusplus
}
#endif
