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

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_ValidatePnpAdapter(PPNP_ADAPTER  pnpAdapter) {

    if (NULL == pnpAdapter->identity) {
        LogError("PnpAdapter's Identity field is not initialized");
        return DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
    }

    if (NULL == pnpAdapter->createAdapter || NULL == pnpAdapter->createPnpInterface || NULL == pnpAdapter->startPnpInterface ||
        NULL == pnpAdapter->stopPnpInterface || NULL == pnpAdapter->destroyPnpInterface || NULL == pnpAdapter->destroyAdapter) {
        LogError("PnpAdapter's callbacks are not initialized");
        return DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
    }

    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_InitializeAdapter(PPNP_ADAPTER adapter, PPNP_ADAPTER_TAG adapterTag) {
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    PPNP_ADAPTER_TAG adapterT = NULL;

    adapterT = calloc(1, sizeof(PNP_ADAPTER_TAG));
    if (NULL == adapterT) {
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    adapterT->adapter = adapter;
    adapterT->InterfaceListLock = Lock_Init();
    adapterT->pnpInterfaceList = singlylinkedlist_create();
    if (NULL == adapterT->InterfaceListLock) {
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
    }

    adapterTag = adapterT;
exit:
    return result;
}

void PnpAdapterManager_ReleaseAdapterInterfaces(PPNP_ADAPTER_TAG adapterTag) {
    if (NULL == adapterTag) {
        return;
    }

    if (NULL != adapterTag->pnpInterfaceList) {
        SINGLYLINKEDLIST_HANDLE pnpInterfaces = adapterTag->pnpInterfaceList;
        Lock(adapterTag->InterfaceListLock);
        LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
        while (NULL != handle) {
            PPNPADAPTER_INTERFACE_TAG interfaceHandle = (PPNPADAPTER_INTERFACE_TAG)singlylinkedlist_item_get_value(handle);
            adapterTag->adapter->stopPnpInterface(interfaceHandle);
            adapterTag->adapter->destroyPnpInterface(interfaceHandle);
            handle = singlylinkedlist_get_next_item(handle);
        }
        Unlock(adapterTag->InterfaceListLock);

        singlylinkedlist_destroy(adapterTag->pnpInterfaceList);
        Lock_Deinit(adapterTag->InterfaceListLock);
    }
}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_GetAdapterFromManifest(const char* adapterId, PPNP_ADAPTER* adapter)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
    PPNP_ADAPTER pnpAdapter = NULL;
    for (int i = 0; i < PnpAdapterCount; i++) {
        pnpAdapter = PNP_ADAPTER_MANIFEST[i];
        if (0 == strcmp(pnpAdapter->identity, adapterId))
        {
            *adapter = pnpAdapter;
            result = DIGITALTWIN_CLIENT_OK;
        }
    }

    // Validate Pnp Adapter Methods
    result = PnpAdapterManager_ValidatePnpAdapter(pnpAdapter);
    if (DIGITALTWIN_CLIENT_OK != result) {
        LogError("PnpAdapter structure is not initialized properly");
    }
    return result;
}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_CreateAdapter(const char* adapterId, PPNP_ADAPTER_CONTEXT_TAG* adapterContext, JSON_Value* config)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;

    PPNP_ADAPTER_CONTEXT_TAG pnpAdapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)malloc(sizeof(PNP_ADAPTER_CONTEXT_TAG));
    if (pnpAdapterHandle == NULL)
    {
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        LogError("Couldn't allocate memory for PNP_ADAPTER_CONTEXT_TAG");
        goto exit;
    }

    pnpAdapterHandle->context = NULL;
    pnpAdapterHandle->adapterGlobalConfig = NULL;
    // Get adapter structure from manifest
    pnpAdapterHandle->adapter = (PPNP_ADAPTER_TAG)malloc(sizeof(PNP_ADAPTER_TAG));
    if (pnpAdapterHandle->adapter == NULL)
    {
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        LogError("Couldn't allocate memory for PNP_ADAPTER_TAG");
        goto exit;
    }
    PPNP_ADAPTER  pnpAdapter = NULL;
    result = PnpAdapterManager_GetAdapterFromManifest(adapterId, &pnpAdapter);
    if (!PNPBRIDGE_SUCCESS(result))
    {
        LogError("Adapter not setup correctly.");
        goto exit;
    }

    pnpAdapterHandle->adapter->adapter = pnpAdapter;
    pnpAdapterHandle->adapter->InterfaceListLock = Lock_Init();
    pnpAdapterHandle->adapter->pnpInterfaceList = singlylinkedlist_create();
    if (NULL == pnpAdapterHandle->adapter->InterfaceListLock) {
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    // Get global adapter parameters and allow the adapter to do internal setup

    pnpAdapterHandle->adapterGlobalConfig = Configuration_GetGlobalAdapterParameters(config, adapterId);
    if (NULL == pnpAdapterHandle->adapterGlobalConfig)
    {
        LogInfo("Adapter with identity %s does not have any associated global parameters. Proceeding with adapter creation.", adapterId);
    }
    result = pnpAdapterHandle->adapter->adapter->createAdapter(pnpAdapterHandle->adapterGlobalConfig, pnpAdapterHandle);
    if (!PNPBRIDGE_SUCCESS(result)) {
        LogError("Adapter %s'couldn't be created.", adapterId);
        goto exit;
    }

    *adapterContext = pnpAdapterHandle;
exit:
    return result;
}

bool PnpAdapterManager_AdapterCreated(PPNP_ADAPTER_MANAGER adapterMgr, const char* adapterId)
{
    LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

    while (NULL != adapterListItem) {

        PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);
        if (adapterHandle->adapter)
        {
            if (!strcmp(adapterHandle->adapter->adapter->identity, adapterId))
            {
                return true;
            }
        }
        adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
    }
    return false;
}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_CreateManager(PPNP_ADAPTER_MANAGER* adapterMgr, JSON_Value* config) {
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    PPNP_ADAPTER_MANAGER adapterManager = NULL;

    adapterManager = (PPNP_ADAPTER_MANAGER)malloc(sizeof(PNP_ADAPTER_MANAGER));
    if (NULL == adapterManager) {
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    adapterManager->NumInterfaces = 0;
    adapterManager->PnpAdapterHandleList = singlylinkedlist_create();
    JSON_Array* devices = Configuration_GetDevices(config);
    if (NULL == devices) {
        LogError("No configured devices in the pnpbridge config");
        result = DIGITALTWIN_CLIENT_ERROR;
        goto exit;
    }

    // Pre-allocate contiguous memory for maximum number of registerable devices
    adapterManager->PnpInterfacesRegistrationList = calloc(json_array_get_count(devices), sizeof(DIGITALTWIN_INTERFACE_CLIENT_HANDLE));

    for (size_t i = 0; i < json_array_get_count(devices); i++) {
        JSON_Object* device = json_array_get_object(devices, i);
        const char* adapterId = json_object_dotget_string(device, PNP_CONFIG_ADAPTER_ID);


        // Create and initialize adapter if it has never been done before

        if (!PnpAdapterManager_AdapterCreated(adapterManager, adapterId))
        {
            PPNP_ADAPTER_CONTEXT_TAG pnpAdapterHandle = NULL;

            result = PnpAdapterManager_CreateAdapter(adapterId, &pnpAdapterHandle, config);
            if (pnpAdapterHandle == NULL || result != DIGITALTWIN_CLIENT_OK)
            {
                LogError("Adapter creation and initialization failed. Destroying all adapters previously created.");
                goto exit;
            }

            // Add to list of manager's adapters
            singlylinkedlist_add(adapterManager->PnpAdapterHandleList, pnpAdapterHandle);

        }
    }

    *adapterMgr = adapterManager;

exit:
    if (!PNPBRIDGE_SUCCESS(result)) {
        if (NULL != adapterManager) {
            PnpAdapterManager_ReleaseManager(adapterManager);
        }
    }
    return result;
}

void PnpAdapterManager_ReleaseManager(PPNP_ADAPTER_MANAGER adapterMgr) {
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);
            if (adapterHandle->adapter)
            {
                // Clean up adapter interfaces
                PnpAdapterManager_ReleaseAdapterInterfaces(adapterHandle->adapter);

                // Clean up adapter's context
                adapterHandle->adapter->adapter->destroyAdapter(adapterHandle);

                free(adapterHandle->adapter);
            }

            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
        singlylinkedlist_destroy(adapterMgr->PnpAdapterHandleList);

        if (adapterMgr->PnpInterfacesRegistrationList)
        {
            free(adapterMgr->PnpInterfacesRegistrationList);
        }
        free(adapterMgr);
    }
}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_GetAdapterHandle(PPNP_ADAPTER_MANAGER adapterMgr, const char* adapterIdentity, PPNP_ADAPTER_CONTEXT_TAG* adapterContext)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

            if (!strcmp(adapterHandle->adapter->adapter->identity, adapterIdentity))
            {
                *adapterContext = adapterHandle;
                result = DIGITALTWIN_CLIENT_OK;
                break;
            }

            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
    }

    return result;

}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_CreateInterfaces(PPNP_ADAPTER_MANAGER adapterMgr, JSON_Value* config)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;

    JSON_Array* devices = Configuration_GetDevices(config);
    if (NULL == devices) {
        LogError("No configured devices in the pnpbridge config");
        result = DIGITALTWIN_CLIENT_ERROR;
        goto exit;
    }

    for (size_t i = 0; i < json_array_get_count(devices); i++) {

        JSON_Object* device = json_array_get_object(devices, i);
        const char* adapterId = json_object_dotget_string(device, PNP_CONFIG_ADAPTER_ID);
        const char* interfaceId = json_object_dotget_string(device, PNP_CONFIG_INTERFACE_ID);
        const char* interfaceName = json_object_dotget_string(device, PNP_CONFIG_COMPONENT_NAME);

        JSON_Object* deviceAdapterArgs = json_object_dotget_object(device, PNP_CONFIG_DEVICE_ADAPTER_CONFIG);
        PPNP_ADAPTER_CONTEXT_TAG adapterHandle = NULL;

        result = PnpAdapterManager_GetAdapterHandle(adapterMgr, adapterId, &adapterHandle);

        if (DIGITALTWIN_CLIENT_OK == result)
        {
            PPNPADAPTER_INTERFACE_TAG interfaceHandle = (PPNPADAPTER_INTERFACE_TAG)malloc(sizeof(PNPADAPTER_INTERFACE_TAG));

            if (interfaceHandle != NULL)
            {
                DIGITALTWIN_INTERFACE_CLIENT_HANDLE digitalTwinInterfaceHandle;

                interfaceHandle->interfaceId = interfaceId;
                interfaceHandle->interfaceName = interfaceName;
                interfaceHandle->adapterIdentity = adapterHandle->adapter->adapter->identity;
                result = adapterHandle->adapter->adapter->createPnpInterface(adapterHandle, interfaceId, interfaceName, deviceAdapterArgs,
                                                                                interfaceHandle, &digitalTwinInterfaceHandle);
                if (result == DIGITALTWIN_CLIENT_OK)
                {
                    if (digitalTwinInterfaceHandle != NULL)
                    {
                        interfaceHandle->pnpInterfaceClient = digitalTwinInterfaceHandle;
                        singlylinkedlist_add(adapterHandle->adapter->pnpInterfaceList, interfaceHandle);
                        adapterMgr->PnpInterfacesRegistrationList[adapterMgr->NumInterfaces] = digitalTwinInterfaceHandle;
                        adapterMgr->NumInterfaces++;
                    }
                    else
                    {
                        LogError("The adapter returned from successful interface creation but interface client handle is invalid");
                        result = DIGITALTWIN_CLIENT_ERROR;
                        goto exit;
                    }
                }
                else
                {
                    LogInfo("Interface with ID: %s and instance name: %s failed.", interfaceId, interfaceName);
                    free(interfaceHandle);
                    goto exit;
                }
            }
            else
            {
                result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
                goto exit;
            }
        }
    }

exit:
    return result;
}

DIGITALTWIN_CLIENT_RESULT PnPAdapterManager_RegisterInterfaces(PPNP_ADAPTER_MANAGER adapterMgr)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    if (NULL != adapterMgr)
    {
        LogInfo("Publishing %d Azure Pnp Interface(s)", adapterMgr->NumInterfaces);
        result = PnpBridge_RegisterInterfaces(adapterMgr->PnpInterfacesRegistrationList, adapterMgr->NumInterfaces);
        if (result != DIGITALTWIN_CLIENT_OK)
        {
            LogError("Registration failed.");
            goto exit;
        }
    }

exit:
    return result;
}

DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_StartInterfaces(PPNP_ADAPTER_MANAGER adapterMgr)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

            LIST_ITEM_HANDLE interfaceHandleItem = singlylinkedlist_get_head_item(adapterHandle->adapter->pnpInterfaceList);
            while (NULL != interfaceHandleItem)
            {
                PPNPADAPTER_INTERFACE_TAG interfaceHandle = (PPNPADAPTER_INTERFACE_TAG)singlylinkedlist_item_get_value(interfaceHandleItem);
                result = adapterHandle->adapter->adapter->startPnpInterface(adapterHandle, interfaceHandle);
                interfaceHandleItem = singlylinkedlist_get_next_item(interfaceHandleItem);
            }
            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
    }
    return result;
}