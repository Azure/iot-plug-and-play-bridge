// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "pnpadapter_api.h"
#include "pnpadapter_manager.h"

extern PPNP_ADAPTER PNP_ADAPTER_MANIFEST[];
extern const int PnpAdapterCount;

#ifdef USE_EDGE_MODULES
#include "pnpadapter_manager.h"
#endif

PNPBRIDGE_RESULT PnpAdapterManager_ValidatePnpAdapter(PPNP_ADAPTER  pnpAdapter, MAP_HANDLE pnpAdapterMap) {
    bool containsKey = false;
    if (NULL == pnpAdapter->identity) {
        LogError("PnpAdapter's Identity filed is not initialized");
        return PNPBRIDGE_INVALID_ARGS;
    }
    if (MAP_OK != Map_ContainsKey(pnpAdapterMap, pnpAdapter->identity, &containsKey)) {
        LogError("Map_ContainsKey failed");
        return PNPBRIDGE_FAILED;
    }
    if (containsKey) {
        LogError("Found duplicate pnp adapter identity %s", pnpAdapter->identity);
        return PNPBRIDGE_DUPLICATE_ENTRY;
    }
    if (NULL == pnpAdapter->initialize || NULL == pnpAdapter->shutdown || NULL == pnpAdapter->createPnpInterface) {
        LogError("PnpAdapter's callbacks are not initialized");
        return PNPBRIDGE_INVALID_ARGS;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpAdapterManager_InitializeAdapter(PPNP_ADAPTER adapter, PPNP_ADAPTER_TAG* adapterTag) {
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    PPNP_ADAPTER_TAG adapterT = NULL;
    TRY
    {
        adapterT = calloc(1, sizeof(PNP_ADAPTER_TAG));
        if (NULL == adapterT) {
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        adapterT->adapter = adapter;
        adapterT->InterfaceListLock = Lock_Init();
        adapterT->pnpInterfaceList = singlylinkedlist_create();
        if (NULL == adapterT->InterfaceListLock) {
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        *adapterTag = adapterT;
    }
    FINALLY 
    {
        if (!PNPBRIDGE_SUCCESS(result)) {
            PnpAdapterManager_ReleaseAdapter(adapterT);
        }
    }

    return result;
}

void PnpAdapterManager_ReleaseAdapter(PPNP_ADAPTER_TAG adapterTag) {
    if (NULL == adapterTag) {
        return;
    }

    if (NULL != adapterTag->pnpInterfaceList) {
        SINGLYLINKEDLIST_HANDLE pnpInterfaces = adapterTag->pnpInterfaceList;
        Lock(adapterTag->InterfaceListLock);
        LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
        while (NULL != handle) {
            PPNPADAPTER_INTERFACE_TAG adapterInterface = (PPNPADAPTER_INTERFACE_TAG)singlylinkedlist_item_get_value(handle);
            // Disconnect the adapter interface from the list. The cleanup of adapte interface
            // will be done when destroy API is called
            if (NULL != adapterInterface->adapterEntry) {
                adapterInterface->adapterEntry = NULL;
            }
            adapterInterface->params.ReleaseInterface(adapterInterface);
            
            handle = singlylinkedlist_get_next_item(handle);
        }
        Unlock(adapterTag->InterfaceListLock);

        singlylinkedlist_destroy(adapterTag->pnpInterfaceList);
        Lock_Deinit(adapterTag->InterfaceListLock);
    }

    free(adapterTag);
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

    adapter->pnpAdapterMap = Map_Create(NULL);
    if (NULL == adapter->pnpAdapterMap) {
        result = PNPBRIDGE_FAILED;
        goto exit;
    }

    // Create PNP_ADAPTER_HANDLE's
    adapter->pnpAdapters = calloc(PnpAdapterCount, sizeof(PPNP_ADAPTER_TAG));
    if (NULL == adapter->pnpAdapters) {
        result = PNPBRIDGE_FAILED;
        goto exit;
    }

    // Load a list of static modules and build an interface map
    for (int i = 0; i < PnpAdapterCount; i++) {
        PPNP_ADAPTER  pnpAdapter = PNP_ADAPTER_MANIFEST[i];
        
        result = PnpAdapterManager_InitializeAdapter(pnpAdapter, &adapter->pnpAdapters[i]);
        if (!PNPBRIDGE_SUCCESS(result)) {
            LogError("Failed to initialize PnpAdapter %s", pnpAdapter->identity);
            continue;
        }

        if (NULL == pnpAdapter->identity) {
            LogError("Invalid Identity specified for a PnpAdapter");
            continue;
        }

        // Validate Pnp Adapter Methods
        result = PnpAdapterManager_ValidatePnpAdapter(pnpAdapter, adapter->pnpAdapterMap);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpAdapter structure is not initialized properly");
            goto exit;
        }

        JSON_Object* initParams = Configuration_GetPnpParameters(config, pnpAdapter->identity);
        const char* initParamstring = NULL;
        if (initParams != NULL) {
            initParamstring = json_serialize_to_string(json_object_get_wrapping_value(initParams));
        }
        result = pnpAdapter->initialize(initParamstring);
        if (PNPBRIDGE_OK != result) {
            LogError("Failed to initialze a PnpAdapter");
            continue;
        }

        Map_Add_Index(adapter->pnpAdapterMap, pnpAdapter->identity, i);
    }

    *adapterMgr = adapter;

exit:
    if (!PNPBRIDGE_SUCCESS(result)) {
        if (NULL != adapter) {
            PnpAdapterManager_Release(adapter);
        }
    }
    return result;
}

void PnpAdapterManager_Release(PPNP_ADAPTER_MANAGER adapterMgr) {
    const char* const* keys;
    const char* const* values;
    size_t count;

    if (NULL != adapterMgr->pnpAdapterMap) {
        // Call shutdown on all interfaces
        if (Map_GetInternals(adapterMgr->pnpAdapterMap, &keys, &values, &count) != MAP_OK) {
            LogError("Map_GetInternals failed to get all pnp adapters");
        }
        else
        {
            for (int i = 0; i < (int)count; i++) {
                int index = atoi(values[i]);
                PPNP_ADAPTER_TAG  adapterT = adapterMgr->pnpAdapters[index];
                if (NULL != adapterT) {
                    // Release all interfaces
                    PnpAdapterManager_ReleaseAdapter(adapterT);

                    PPNP_ADAPTER adapter = PNP_ADAPTER_MANIFEST[index];
                    if (NULL != adapter->shutdown) {
                        adapter->shutdown();
                    }
                }
            }
        }
    }

    if (NULL != adapterMgr->pnpAdapters) {
        free(adapterMgr->pnpAdapters);
    }

    Map_Destroy(adapterMgr->pnpAdapterMap);
    free(adapterMgr);
}

PNPBRIDGE_RESULT PnpAdapterManager_SupportsIdentity(PPNP_ADAPTER_MANAGER adapter, JSON_Object* Message, bool* supported, int* key) {
    bool containsMessageKey = false;
    JSON_Object* pnpParams = json_object_get_object(Message, PNP_CONFIG_PNP_PARAMETERS);
    char* getIdentity = (char*) json_object_get_string(pnpParams, PNP_CONFIG_IDENTITY);
    MAP_RESULT mapResult;

    *supported = false;

    mapResult = Map_ContainsKey(adapter->pnpAdapterMap, getIdentity, &containsMessageKey);
    if (MAP_OK != mapResult || !containsMessageKey) {
        LogError("PnpAdapter %s is not present in AdapterManifest", getIdentity);
        return PNPBRIDGE_FAILED;
    }

    // Get the interface ID
    int index = Map_GetIndexValueFromKey(adapter->pnpAdapterMap, getIdentity);

    *supported = true;
    *key = index;

    return PNPBRIDGE_OK;
}

/*
    Once an interface is registerd with Azure PnpDeviceClient this 
    method will take care of binding it to a module implementing 
    PnP primitives
*/
PNPBRIDGE_RESULT 
PnpAdapterManager_CreatePnpInterface(
    PPNP_ADAPTER_MANAGER AdapterMgr,
    MX_IOT_HANDLE_TAG* IotHandle,
    int key,
    JSON_Object* deviceConfig,
    PNPMESSAGE DeviceChangeMessage
    ) 
{
    // Get the module using the key as index
    PPNP_ADAPTER_TAG  adapterT = AdapterMgr->pnpAdapters[key];
    PNP_ADAPTER_CONTEXT_TAG context = { 0 };

    AZURE_UNREFERENCED_PARAMETER(IotHandle);

    context.adapter = adapterT;
    context.deviceConfig = deviceConfig;

    // Invoke interface binding method
    int ret = adapterT->adapter->createPnpInterface(&context, DeviceChangeMessage);
    if (ret < 0) {
        return PNPBRIDGE_FAILED;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpAdapterManager_GetAllInterfaces(PPNP_ADAPTER_MANAGER adapterMgr, DIGITALTWIN_INTERFACE_CLIENT_HANDLE** interfaces , int* count) {
    int n = 0;

    // Get the number of created interfaces
    for (int i = 0; i < PnpAdapterCount; i++) {
        PPNP_ADAPTER_TAG  pnpAdapter = adapterMgr->pnpAdapters[i];
        
        SINGLYLINKEDLIST_HANDLE pnpInterfaces = pnpAdapter->pnpInterfaceList;
        LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
        while (NULL != handle) {
            handle = singlylinkedlist_get_next_item(handle);
            n++;
        }
    }

    // create an array of interface handles
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* pnpClientHandles = NULL;
    {
        pnpClientHandles = calloc(n, sizeof(DIGITALTWIN_INTERFACE_CLIENT_HANDLE));
        int x = 0;
        for (int i = 0; i < PnpAdapterCount; i++) {
            PPNP_ADAPTER_TAG  pnpAdapter = adapterMgr->pnpAdapters[i];

            SINGLYLINKEDLIST_HANDLE pnpInterfaces = pnpAdapter->pnpInterfaceList;
            Lock(pnpAdapter->InterfaceListLock);
            LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
            while (NULL != handle) {
                PPNPADAPTER_INTERFACE_TAG adapterInterface = (DIGITALTWIN_INTERFACE_CLIENT_HANDLE) singlylinkedlist_item_get_value(handle);
                pnpClientHandles[x] = PnpAdapterInterface_GetPnpInterfaceClient(adapterInterface);
                handle = singlylinkedlist_get_next_item(handle);
                x++;
            }
            Unlock(pnpAdapter->InterfaceListLock);
        }
    }

    *count = n;
    *interfaces = pnpClientHandles;

    return 0;
}

void PnpAdapterManager_InvokeStartInterface(PPNP_ADAPTER_MANAGER adapterMgr) {
    for (int i = 0; i < PnpAdapterCount; i++) {
        PPNP_ADAPTER_TAG  pnpAdapter = adapterMgr->pnpAdapters[i];

        SINGLYLINKEDLIST_HANDLE pnpInterfaces = pnpAdapter->pnpInterfaceList;
        LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
        while (NULL != handle) {
            PPNPADAPTER_INTERFACE_TAG adapterInterface = (DIGITALTWIN_INTERFACE_CLIENT_HANDLE)singlylinkedlist_item_get_value(handle);
            // Invoke the startInterface callback
            int res = adapterInterface->params.StartInterface(adapterInterface);

            // TODO: If failed then clear the interface
            if (res < 0) {
                LogError("startInterface adapter callback failed for interface %s, %d",
                            adapterInterface->interfaceId, res);
                // TODO: Mark for removal
            }

            handle = singlylinkedlist_get_next_item(handle);
        }
    }

    // TODO: If any interfaces removed then republish it
}

bool PnpAdapterManager_IsInterfaceIdPublished(PPNP_ADAPTER_MANAGER adapterMgr, const char* interfaceId) {
    for (int i = 0; i < PnpAdapterCount; i++) {
        PPNP_ADAPTER_TAG  pnpAdapter = adapterMgr->pnpAdapters[i];

        SINGLYLINKEDLIST_HANDLE pnpInterfaces = pnpAdapter->pnpInterfaceList;
        Lock(pnpAdapter->InterfaceListLock);
        LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
        while (NULL != handle) {
            PPNPADAPTER_INTERFACE_TAG adapterInterface = (DIGITALTWIN_INTERFACE_CLIENT_HANDLE)singlylinkedlist_item_get_value(handle);
            if (0 == strcmp(adapterInterface->interfaceId, interfaceId)) {
                return true;
            }
            handle = singlylinkedlist_get_next_item(handle);
        }
        Unlock(pnpAdapter->InterfaceListLock);
    }

    return false;
}

// TODO: Prevent duplicate interfaces
void PnpAdapterManager_AddInterface(PPNP_ADAPTER_TAG adapter, PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface) {
    LIST_ITEM_HANDLE handle = NULL;
    Lock(adapter->InterfaceListLock);

    handle = singlylinkedlist_add(adapter->pnpInterfaceList, pnpAdapterInterface);
    Unlock(adapter->InterfaceListLock);

    if (NULL != handle) {
        PPNPADAPTER_INTERFACE_TAG interface = (PPNPADAPTER_INTERFACE_TAG)pnpAdapterInterface;
        interface->adapterEntry = handle;
    }
    // Handle failure case when singlylinkedlist_add returns NULL
}

void 
PnpAdapterManager_RemoveInterface(
    PPNP_ADAPTER_TAG Adapter,
    PNPADAPTER_INTERFACE_HANDLE PnpAdapterInterface
    ) 
{
    PPNPADAPTER_INTERFACE_TAG interface = (PPNPADAPTER_INTERFACE_TAG) PnpAdapterInterface;

    Lock(Adapter->InterfaceListLock);
    singlylinkedlist_remove(Adapter->pnpInterfaceList, interface->adapterEntry);
    Unlock(Adapter->InterfaceListLock);
}
