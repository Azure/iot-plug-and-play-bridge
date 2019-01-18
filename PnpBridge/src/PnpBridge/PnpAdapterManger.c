// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "common.h"
#include "PnpBridgeCommon.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"
#include "CoreDeviceHealth.h"

// TODO: Decide where the extern are present for CoreDeviceHealth
PPNP_INTERFACE_MODULE INTERFACE_MANIFEST[] = {
    &CoreDeviceHealthInterface,
    &SerialPnpInterface
};

PNPBRIDGE_RESULT PnpAdapterManager_Create(PPNP_ADAPTER_MANAGER* adapterMgr) {
    PPNP_ADAPTER_MANAGER adapter;
    if (NULL == adapterMgr) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    adapter = (PPNP_ADAPTER_MANAGER) malloc(sizeof(PNP_ADAPTER_MANAGER));
    if (NULL == adapter) {
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

    adapter->PnpAdapterMap = Map_Create(NULL);
    if (NULL == adapter->PnpAdapterMap) {
        return PNPBRIDGE_FAILED;
    }

    // Load a list of static modules and build an interface map
    for (int i = 0; i < sizeof(INTERFACE_MANIFEST) / sizeof(PPNP_INTERFACE_MODULE); i++) {
        PPNP_INTERFACE_MODULE  pnpAdapter = INTERFACE_MANIFEST[i];
        PNPBRIDGE_RESULT result;

        if (NULL == pnpAdapter->Identity) {
            LogError("Invalid Identity specified for a PnpAdapter");
            continue;
        }

        if (NULL != pnpAdapter->Initialize) {
            // TODO: Extract args for the adapterMgr and pass it below
            result = pnpAdapter->Initialize(NULL);
            if (PNPBRIDGE_OK != result) {
                LogError("Failed to initialze a PnpAdapter");
                continue;
            }
        }

        Map_Add_Index(adapter->PnpAdapterMap, pnpAdapter->Identity, i);
    }

    *adapterMgr = adapter;

    return PNPBRIDGE_OK;
}

void PnpAdapterManager_Release(PPNP_ADAPTER_MANAGER adapter) {
    const char* const* keys;
    const char* const* values;
    size_t count;

    // Call shutdown on all interfaces
    if (Map_GetInternals(adapter->PnpAdapterMap, &keys, &values, &count) != MAP_OK)
    {
        LogError("Map_GetInternals failed to get all pnp adapters");
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            int index = values[i][0];
            PPNP_INTERFACE_MODULE  pnpAdapter = INTERFACE_MANIFEST[index];
            if (NULL != pnpAdapter->Shutdown) {
                pnpAdapter->Shutdown();
            }
        }
    }

    Map_Destroy(adapter->PnpAdapterMap);
    free(adapter);
}

PNPBRIDGE_RESULT PnpAdapterManager_SupportsIdentity(PPNP_ADAPTER_MANAGER adapter, JSON_Object* Message, bool* supported, int* key) {
    bool containsMessageKey = false;
    char* interfaceId = NULL;
    char* getIdentity = (char*) json_object_get_string(Message, "Identity");
    MAP_RESULT mapResult;

    *supported = false;

    mapResult = Map_ContainsKey(adapter->PnpAdapterMap, getIdentity, &containsMessageKey);
    if (MAP_OK != mapResult || !containsMessageKey) {
        return PNPBRIDGE_FAILED;
    }

    // Get the interface ID
    int index = Map_GetIndexValueFromKey(adapter->PnpAdapterMap, getIdentity);

    *supported = true;
    *key = index;

    return PNPBRIDGE_OK;
}

/*
    Once an interface is registerd with Azure PnpDeviceClient this 
    method will take care of binding it to a module implementing 
    PnP primitives
*/
PNPBRIDGE_RESULT PnpAdapterManager_CreatePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, int key, PNP_INTERFACE_CLIENT_HANDLE* InterfaceClient, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
    // Get the module using the key as index
    PPNP_INTERFACE_MODULE  pnpAdapter = INTERFACE_MANIFEST[key];

    PPNPADAPTER_INTERFACE pnpInterface = malloc(sizeof(PNPADAPTER_INTERFACE));

    //pnpInterface->Interface = InterfaceClient;
    pnpInterface->key = key;

    // Invoke interface binding method
    pnpAdapter->CreatePnpInterface(pnpInterface, pnpDeviceClientHandle, DeviceChangePayload);

    *InterfaceClient = pnpInterface->Interface;
    
    //*InterfaceClient = (PNP_INTERFACE_CLIENT_HANDLE *) pnpInterface;

    return 0;
}

PNPBRIDGE_RESULT PnpAdapterManager_ReleasePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNPADAPTER_INTERFACE_HANDLE npInterfaceClient) {
    if (NULL == npInterfaceClient) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    PPNPADAPTER_INTERFACE pnpInterface = (PPNPADAPTER_INTERFACE) npInterfaceClient;

    // Get the module index
    PPNP_INTERFACE_MODULE  pnpAdapter = INTERFACE_MANIFEST[pnpInterface->key];
    pnpAdapter->ReleaseInterface(npInterfaceClient);

    return PNPBRIDGE_OK;
}

PNP_INTERFACE_CLIENT_HANDLE PnpAdapter_GetPnpInterfaceClient(PNPADAPTER_INTERFACE_HANDLE pnpInterfaceClient) {
    if (pnpInterfaceClient == NULL) {
        return NULL;
    }

    PPNPADAPTER_INTERFACE interfaceClient = (PPNPADAPTER_INTERFACE)pnpInterfaceClient;
    return interfaceClient->Interface;
}

void PnpAdapter_SetPnpInterfaceClient(PNPADAPTER_INTERFACE_HANDLE pnpInterface, PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceClient) {
    if (pnpInterfaceClient == NULL) {
        LogError("pnpInterface argument is NULL");
        return;
    }

    PPNPADAPTER_INTERFACE interfaceClient = (PPNPADAPTER_INTERFACE)pnpInterface;
    interfaceClient->Interface = pnpInterfaceClient;
}

PNPBRIDGE_RESULT PnpAdapter_SetContext(PNPADAPTER_INTERFACE_HANDLE pnpInterfaceClient, void* Context) {
    if (NULL == pnpInterfaceClient) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    PPNPADAPTER_INTERFACE interfaceClient = (PPNPADAPTER_INTERFACE)pnpInterfaceClient;
    interfaceClient->Context = Context;

    return PNPBRIDGE_OK;
}

void* PnpAdapter_GetContext(PNPADAPTER_INTERFACE_HANDLE PnpInterfaceClient) {
    if (NULL == PnpInterfaceClient) {
        return NULL;
    }

    PPNPADAPTER_INTERFACE pnpInterfaceClient = (PPNPADAPTER_INTERFACE)PnpInterfaceClient;
    return pnpInterfaceClient->Context;
}