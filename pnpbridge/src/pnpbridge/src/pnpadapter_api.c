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

void PnpComponentHandleSetContext(PNPBRIDGE_COMPONENT_HANDLE ComponentHandle, void* ComponentDeviceContext)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)ComponentHandle;
    interfaceContextTag->context = ComponentDeviceContext;
}

void* PnpComponentHandleGetContext(PNPBRIDGE_COMPONENT_HANDLE ComponentHandle)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)ComponentHandle;
    return (void*)interfaceContextTag->context;
}

void PnpComponentHandleSetPropertyUpdateCallback (PNPBRIDGE_COMPONENT_HANDLE ComponentHandle,
    PNPBRIDGE_COMPONENT_PROPERTY_CALLBACK PropertyUpdateCallback)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)ComponentHandle;
    interfaceContextTag->processPropertyUpdate = PropertyUpdateCallback;
}

void PnpComponentHandleSetCommandCallback (PNPBRIDGE_COMPONENT_HANDLE ComponentHandle,
    PNPBRIDGE_COMPONENT_METHOD_CALLBACK CommandCallback)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)ComponentHandle;
    interfaceContextTag->processCommand = CommandCallback;
}

IOTHUB_DEVICE_CLIENT_HANDLE PnpComponentHandleGetIotHubDeviceClient(PNPBRIDGE_COMPONENT_HANDLE ComponentHandle)
{
    PPNPADAPTER_INTERFACE_TAG interfaceContextTag = (PPNPADAPTER_INTERFACE_TAG)ComponentHandle;
    return interfaceContextTag->deviceClient;
}