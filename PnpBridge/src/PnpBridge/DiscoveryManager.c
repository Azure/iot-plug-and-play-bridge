// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "common.h"
#include "PnpBridgeCommon.h"
#include "DiscoveryManager.h"
#include "DiscoveryAdapterInterface.h"
#include "WindowsPnpDeviceDiscovery.h"
#include "Adapters/Camera/CameraIotPnpAPIs.h"

DISCOVERY_ADAPTER discoveryInterface = { 0 };

PDISCOVERY_ADAPTER DISCOVERY_MANIFEST[] = {
    &CameraPnpAdapter,
    &SerialPnpDiscovery,
    &WindowsPnpDeviceDiscovery
};

void DiscoveryAdapterChangeHandler(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload);

PNPBRIDGE_RESULT DiscoveryAdapterManager_Create(PDISCOVERY_MANAGER* discoveryManager) {
    PDISCOVERY_MANAGER discoveryMgr;

    if (NULL == discoveryManager) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    discoveryMgr = malloc(sizeof(DISCOVERY_MANAGER));
    if (NULL == discoveryMgr) {
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

    discoveryMgr->DiscoveryModuleMap = Map_Create(NULL);
    if (NULL == discoveryMgr->DiscoveryModuleMap) {
        return PNPBRIDGE_FAILED;
    }

    discoveryMgr->startDiscoveryThreadHandles = singlylinkedlist_create();
    if (NULL == discoveryMgr->startDiscoveryThreadHandles) {
        return PNPBRIDGE_FAILED;
    }

    *discoveryManager = discoveryMgr;

    return PNPBRIDGE_OK;
}

typedef struct _START_DISCOVERY_PARAMS {
    PDISCOVERY_ADAPTER  discoveryInterface;
    JSON_Object* deviceParams;
    JSON_Object* adapterParams;
    int key;
    PDISCOVERY_MANAGER discoveryManager;
} START_DISCOVERY_PARAMS, *PSTART_DISCOVERY_PARAMS;

int DiscoveryManager_StarDiscovery_Worker_Thread(void* threadArgument)
{
    PSTART_DISCOVERY_PARAMS p = threadArgument;
    int result = p->discoveryInterface->StartDiscovery(DiscoveryAdapterChangeHandler, p->deviceParams, p->adapterParams);

    if (PNPBRIDGE_OK == result) {
        Map_Add_Index(p->discoveryManager->DiscoveryModuleMap, p->discoveryInterface->Identity, p->key);
    }

    free(threadArgument);

    return 0;
}

PNPBRIDGE_RESULT DiscoveryAdapterManager_Start(PDISCOVERY_MANAGER discoveryManager) {
    JSON_Array *devices = Configuration_GetConfiguredDevices();

    if (NULL == devices) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    for (int i = 0; i < sizeof(DISCOVERY_MANIFEST) / sizeof(PDISCOVERY_ADAPTER); i++) {
        PDISCOVERY_ADAPTER  discoveryInterface = DISCOVERY_MANIFEST[i];
        JSON_Object* deviceParams = NULL;

        for (int j = 0; j < json_array_get_count(devices); j++) {
            JSON_Object *device = json_array_get_object(devices, j);

            // For this Identity check if there is any device
            // TODO: Create an array of device
            JSON_Object* params = Configuration_GetDiscoveryParametersForDevice(device);
            if (NULL != params) {
                const char* deviceFormatId = json_object_get_string(params, "Identity");
                if (strcmp(deviceFormatId, discoveryInterface->Identity) == 0) {
                    deviceParams = params;
                    break;
                }
            }
        }

        JSON_Object* adapterParams = NULL;
        adapterParams = Configuration_GetDiscoveryParameters(discoveryInterface->Identity);

        PSTART_DISCOVERY_PARAMS threadContext;
        THREAD_HANDLE workerThreadHandle = NULL;

        // threadContext memory will be freed by the worker item when its done.
        threadContext = malloc(sizeof(START_DISCOVERY_PARAMS));
        if (NULL == threadContext) {
            return PNPBRIDGE_INSUFFICIENT_MEMORY;
        }

        threadContext->adapterParams = adapterParams;
        threadContext->deviceParams = deviceParams;
        threadContext->discoveryInterface = discoveryInterface;
        threadContext->key = i;
        threadContext->discoveryManager = discoveryManager;

        if (THREADAPI_OK != ThreadAPI_Create(&workerThreadHandle, DiscoveryManager_StarDiscovery_Worker_Thread, threadContext)) {
            LogError("Failed to create PnpBridge_Worker_Thread \n");
            workerThreadHandle = NULL;
        }

        singlylinkedlist_add(discoveryManager->startDiscoveryThreadHandles, workerThreadHandle);
    }

    return PNPBRIDGE_OK;
}

void DiscoveryAdapterManager_Stop(PDISCOVERY_MANAGER discoveryManager) {
    const char* const* keys;
    const char* const* values;
    size_t count;

    // Wait for all start discovery threads to join
    LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(discoveryManager->startDiscoveryThreadHandles);
    while (interfaceItem != NULL) {
        THREAD_HANDLE workerThreadHandle = (THREAD_HANDLE)singlylinkedlist_item_get_value(interfaceItem);
        ThreadAPI_Join(workerThreadHandle, NULL);
        interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
    }
    singlylinkedlist_destroy(discoveryManager->startDiscoveryThreadHandles);

    // Call shutdown on all interfaces
    if (Map_GetInternals(discoveryManager->DiscoveryModuleMap, &keys, &values, &count) != MAP_OK)
    {
        LogError("Map_GetInternals failed to get all pnp adapters");
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            int index = values[i][0];
            PDISCOVERY_ADAPTER  adapter = DISCOVERY_MANIFEST[index];
            adapter->StopDiscovery();
        }
    }

    Map_Destroy(discoveryManager->DiscoveryModuleMap);
    free(discoveryManager);
}

void DiscoveryAdapterChangeHandler(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
    PnpBridge_DeviceChangeCallback(DeviceChangePayload);
}