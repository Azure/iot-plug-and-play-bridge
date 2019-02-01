// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "PnpBridgeCommon.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

extern PPNP_ADAPTER PNP_ADAPTER_MANIFEST[];
extern const int PnpAdapterCount;

PNPBRIDGE_RESULT PnpAdapterManager_ValidatePnpAdapter(PPNP_ADAPTER  pnpAdapter, MAP_HANDLE pnpAdapterMap) {
    bool containsKey = false;
    if (NULL == pnpAdapter->Identity) {
        LogError("PnpAdapter's Identity filed is not initialized");
        return PNPBRIDGE_INVALID_ARGS;
    }
    if (MAP_OK != Map_ContainsKey(pnpAdapterMap, pnpAdapter->Identity, &containsKey)) {
        LogError("Map_ContainsKey failed");
        return PNPBRIDGE_FAILED;
    }
    if (containsKey) {
        LogError("Found duplicate pnp adapter identity %s", pnpAdapter->Identity);
        return PNPBRIDGE_DUPLICATE_ENTRY;
    }
    if (NULL == pnpAdapter->Initialize || NULL == pnpAdapter->Shutdown || NULL == pnpAdapter->CreatePnpInterface || NULL == pnpAdapter->ReleaseInterface) {
        LogError("PnpAdapter's callbacks are not initialized");
        return PNPBRIDGE_INVALID_ARGS;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpAdapterManager_Create(PPNP_ADAPTER_MANAGER* adapterMgr, JSON_Value* config) {
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    PPNP_ADAPTER_MANAGER adapter = NULL;

    if (NULL == adapterMgr) {
        result = PNPBRIDGE_INVALID_ARGS;
        goto exit;
    }

    adapter = (PPNP_ADAPTER_MANAGER) malloc(sizeof(PNP_ADAPTER_MANAGER));
    if (NULL == adapter) {
        result = PNPBRIDGE_INSUFFICIENT_MEMORY;
        goto exit;
    }

    adapter->PnpAdapterMap = Map_Create(NULL);
    if (NULL == adapter->PnpAdapterMap) {
        result = PNPBRIDGE_FAILED;
        goto exit;
    }

    // Load a list of static modules and build an interface map
    for (int i = 0; i < PnpAdapterCount; i++) {
        PPNP_ADAPTER  pnpAdapter = PNP_ADAPTER_MANIFEST[i];

        if (NULL == pnpAdapter->Identity) {
            LogError("Invalid Identity specified for a PnpAdapter");
            continue;
        }

        // Validate Pnp Adapter Methods
        result = PnpAdapterManager_ValidatePnpAdapter(pnpAdapter, adapter->PnpAdapterMap);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpAdapter structure is not initialized properly");
            goto exit;
        }

        JSON_Object* initParams = Configuration_GetPnpParameters(config, pnpAdapter->Identity);
        const char* initParamstring = NULL;
        if (initParams != NULL) {
            initParamstring = json_serialize_to_string(json_object_get_wrapping_value(initParams));
        }
        result = pnpAdapter->Initialize(initParamstring);
        if (PNPBRIDGE_OK != result) {
            LogError("Failed to initialze a PnpAdapter");
            continue;
        }

        Map_Add_Index(adapter->PnpAdapterMap, pnpAdapter->Identity, i);
    }

    *adapterMgr = adapter;

exit:
    return result;
}

void PnpAdapterManager_Release(PPNP_ADAPTER_MANAGER adapter) {
    const char* const* keys;
    const char* const* values;
    size_t count;

    // Call shutdown on all interfaces
    if (Map_GetInternals(adapter->PnpAdapterMap, &keys, &values, &count) != MAP_OK) {
        LogError("Map_GetInternals failed to get all pnp adapters");
    }
    else
    {
        for (int i = 0; i < (int)count; i++) {
            int index = values[i][0];
            PPNP_ADAPTER  pnpAdapter = PNP_ADAPTER_MANIFEST[index];
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
    JSON_Object* pnpParams = json_object_get_object(Message, "PnpParameters");
    char* getIdentity = (char*) json_object_get_string(pnpParams, "Identity");
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
    PPNP_ADAPTER  pnpAdapter = PNP_ADAPTER_MANIFEST[key];

    PPNPADAPTER_INTERFACE pnpInterface = malloc(sizeof(PNPADAPTER_INTERFACE));

    //pnpInterface->Interface = InterfaceClient;
    pnpInterface->key = key;

    // Invoke interface binding method
    int ret = pnpAdapter->CreatePnpInterface(pnpInterface, pnpDeviceClientHandle, DeviceChangePayload);
    if (ret < 0) {
        return PNPBRIDGE_FAILED;
    }

    *InterfaceClient = pnpInterface->Interface;

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpAdapterManager_ReleasePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNPADAPTER_INTERFACE_HANDLE interfaceClient) {
    if (NULL == interfaceClient) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    PPNPADAPTER_INTERFACE pnpInterface = (PPNPADAPTER_INTERFACE)interfaceClient;

    // Get the module index
    PPNP_ADAPTER  pnpAdapter = PNP_ADAPTER_MANIFEST[pnpInterface->key];
    pnpAdapter->ReleaseInterface(interfaceClient);

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