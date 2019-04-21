// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "PnpBridgeCommon.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

int PnpAdapterInterface_Create(PNPADAPTER_CONTEXT adapterHandle, const char* interfaceId, 
                               PNP_INTERFACE_CLIENT_HANDLE pnpInterface,
                               PPNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface,
                               PPNPADPATER_INTERFACE_INIT_PARAMS params)
{
    int result = 0;
    PPNPADAPTER_INTERFACE_TAG interface = NULL;
    PPNP_ADAPTER_CONTEXT_TAG adapterContext = adapterHandle;

    TRY 
    {
        // validate params
        if (NULL == params || NULL == params->releaseInterface || NULL == pnpInterface) {
            result = -1;
            LEAVE;
        }

        // Create a pnp adapter interface
        interface = calloc(1, sizeof(PNPADAPTER_INTERFACE_TAG));
        if (NULL == interface) {
            result = -1;
            LEAVE;
        }

        interface->pnpInterfaceClient = pnpInterface;
        interface->interfaceId = malloc(strlen(interfaceId) + 1);
        if (NULL == interface->interfaceId) {
            result = -1;
            LEAVE;
        }

        // Make a copy of interface id
        strcpy_s(interface->interfaceId, strlen(interfaceId)+1, interfaceId);

        // Copy the params
        memcpy(&interface->params, params, sizeof(interface->params));

        // Copy adapter context
        interface->adapterContext = calloc(1, sizeof(PPNP_ADAPTER_CONTEXT_TAG));
        if (NULL == interface->adapterContext) {
            result = -1;
            LEAVE;
        }
        memcpy(interface->adapterContext, adapterContext, sizeof(interface->adapterContext));

        // Add this interface to the list of interfaces under the adapter context
        PnpAdapterManager_AddInterface(adapterContext->adapter, interface);

        *pnpAdapterInterface = interface;
    }
    FINALLY 
    {
        if (result < 0) {
           /* PnpAdapterManager_RemoveInterface(adapterContext->adapter, pnpAdapterInterface);*/
            PnpAdapterInterface_Destroy(interface);
        }
    }

    return result;
}

void PnpAdapterInterface_Destroy(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface) {
    if (NULL == pnpAdapterInterface) {
        return;
    }

    PPNPADAPTER_INTERFACE_TAG interface = (PPNPADAPTER_INTERFACE_TAG)pnpAdapterInterface;
    if (NULL != interface->interfaceId) {
        free(interface->interfaceId);
    }

    if (NULL != interface->adapterContext) {
        PPNP_ADAPTER_CONTEXT_TAG adapterContext = (PPNP_ADAPTER_CONTEXT_TAG)interface->adapterContext;
        if (NULL != interface->adapterEntry) {
            PnpAdapterManager_RemoveInterface(adapterContext->adapter, pnpAdapterInterface);
        }
        free(interface->adapterContext);
    }
}

PNP_INTERFACE_CLIENT_HANDLE PnpAdapterInterface_GetPnpInterfaceClient(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface) {
    if (NULL == pnpAdapterInterface) {
        return NULL;
    }

    PPNPADAPTER_INTERFACE_TAG interfaceClient = (PPNPADAPTER_INTERFACE_TAG)pnpAdapterInterface;
    return interfaceClient->pnpInterfaceClient;
}

int PnpAdapterInterface_SetContext(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface, void* context) {
    if (NULL == pnpAdapterInterface) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    PPNPADAPTER_INTERFACE_TAG interfaceClient = (PPNPADAPTER_INTERFACE_TAG)pnpAdapterInterface;
    interfaceClient->context = context;

    return PNPBRIDGE_OK;
}

void* PnpAdapterInterface_GetContext(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface) {
    if (NULL == pnpAdapterInterface) {
        return NULL;
    }

    PPNPADAPTER_INTERFACE_TAG adapterInterface = (PPNPADAPTER_INTERFACE_TAG)pnpAdapterInterface;
    return adapterInterface->context;
}
