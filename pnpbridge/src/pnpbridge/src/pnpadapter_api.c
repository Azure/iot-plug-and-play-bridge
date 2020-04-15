// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "pnpadapter_api.h"
#include "pnpadapter_manager.h"

void PnpAdapterHandleSetContext(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle, void* AdapterContext)
{
    PPNP_ADAPTER_CONTEXT_TAG adapterContextTag = (PPNP_ADAPTER_CONTEXT_TAG) AdapterHandle;
    adapterContextTag->context = AdapterContext;
}

void* PnpAdapterHandleGetContext(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    PPNP_ADAPTER_CONTEXT_TAG adapterContextTag = (PPNP_ADAPTER_CONTEXT_TAG)AdapterHandle;
    return (void*) adapterContextTag->context;

}

void PnpInterfaceHandleSetContext(PNPBRIDGE_INTERFACE_HANDLE InterfaceHandle, void* InterfaceContext)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)InterfaceHandle;
    interfaceContextTag->context = InterfaceContext;
}

void* PnpInterfaceHandleGetContext(PNPBRIDGE_INTERFACE_HANDLE InterfaceHandle)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)InterfaceHandle;
    return (void*)interfaceContextTag->context;
}

DIGITALTWIN_DEVICE_CLIENT_HANDLE
IotHandle_GetPnpDeviceClient(_In_ MX_IOT_HANDLE IotHandle)
{
    MX_IOT_HANDLE_TAG* iotHandle = (MX_IOT_HANDLE_TAG*)IotHandle;
    return iotHandle->u1.IotDevice.PnpDeviceClientHandle;
}
