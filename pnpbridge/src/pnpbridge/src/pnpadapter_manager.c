// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "pnpadapter_api.h"
#include "pnpadapter_manager.h"

extern PPNP_ADAPTER PNP_ADAPTER_MANIFEST[];
extern const int PnpAdapterCount;
extern PPNP_BRIDGE g_PnpBridge;
extern PNP_BRIDGE_STATE g_PnpBridgeState;
extern bool g_PnpBridgeShutdown;

#ifdef USE_EDGE_MODULES
#include "pnpadapter_manager.h"
#endif

IOTHUB_CLIENT_RESULT PnpAdapterManager_ValidatePnpAdapter(
    PPNP_ADAPTER  pnpAdapter)
{

    if (NULL == pnpAdapter->identity) {
        LogError("PnpAdapter's Identity field is not initialized");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    if (NULL == pnpAdapter->createAdapter || NULL == pnpAdapter->createPnpComponent || NULL == pnpAdapter->startPnpComponent ||
        NULL == pnpAdapter->stopPnpComponent || NULL == pnpAdapter->destroyPnpComponent || NULL == pnpAdapter->destroyAdapter) {
        LogError("PnpAdapter's callbacks are not initialized");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_InitializeAdapter(
    PPNP_ADAPTER adapter,
    PPNP_ADAPTER_TAG adapterTag)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PPNP_ADAPTER_TAG adapterT = NULL;

    adapterT = calloc(1, sizeof(PNP_ADAPTER_TAG));
    if (NULL == adapterT) {
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    adapterT->adapter = adapter;
    adapterT->ComponentListLock = Lock_Init();
    adapterT->PnpComponentList = singlylinkedlist_create();
    if (NULL == adapterT->ComponentListLock) {
        result = IOTHUB_CLIENT_ERROR;
    }

    adapterTag = adapterT;
exit:
    return result;
}

void PnpAdapterManager_ReleaseAdapterComponents(
    PPNP_ADAPTER_TAG adapterTag)
{
    if (NULL == adapterTag) {
        return;
    }

    if (NULL != adapterTag->PnpComponentList) {
        SINGLYLINKEDLIST_HANDLE pnpInterfaces = adapterTag->PnpComponentList;
        Lock(adapterTag->ComponentListLock);
        LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(pnpInterfaces);
        while (NULL != handle) {
            PPNPADAPTER_COMPONENT_TAG componentHandle = (PPNPADAPTER_COMPONENT_TAG)singlylinkedlist_item_get_value(handle);
            adapterTag->adapter->destroyPnpComponent(componentHandle);
            handle = singlylinkedlist_get_next_item(handle);
        }
        Unlock(adapterTag->ComponentListLock);

        // This call to singlylinkedlist_destroy will free all resources 
        // associated with the list identified by the handle argument,
        // therefore each component's resources allocated during creation
        // will be destroyed after this call returns
        singlylinkedlist_destroy(adapterTag->PnpComponentList);
        Lock_Deinit(adapterTag->ComponentListLock);
    }
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_StopComponents(
        PPNP_ADAPTER_MANAGER adapterMgr)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

            LIST_ITEM_HANDLE componentHandleItem = singlylinkedlist_get_head_item(adapterHandle->adapter->PnpComponentList);
            while (NULL != componentHandleItem)
            {
                PPNPADAPTER_COMPONENT_TAG componentHandle = (PPNPADAPTER_COMPONENT_TAG)singlylinkedlist_item_get_value(componentHandleItem);
                result = adapterHandle->adapter->adapter->stopPnpComponent(componentHandle);
                if (result != IOTHUB_CLIENT_OK)
                {
                    LogError("PnpAdapterManager_StopComponents: Failed to stop component %s", componentHandle->componentName);
                }
                componentHandleItem = singlylinkedlist_get_next_item(componentHandleItem);
            }
            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
    }

    return result;
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_DestroyComponents(
        PPNP_ADAPTER_MANAGER adapterMgr)
{
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);
            if (adapterHandle->adapter)
            {
                // Destroy Components owned by each adapter
                PnpAdapterManager_ReleaseAdapterComponents(adapterHandle->adapter);
            }
            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_GetAdapterFromManifest(
    const char* adapterId,
    PPNP_ADAPTER* adapter)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_INVALID_ARG;
    PPNP_ADAPTER pnpAdapter = NULL;
    for (int i = 0; i < PnpAdapterCount; i++) {
        pnpAdapter = PNP_ADAPTER_MANIFEST[i];
        if (0 == strcmp(pnpAdapter->identity, adapterId))
        {
            *adapter = pnpAdapter;
            result = IOTHUB_CLIENT_OK;
        }
    }

    // Validate Pnp Adapter Methods
    result = PnpAdapterManager_ValidatePnpAdapter(pnpAdapter);
    if (IOTHUB_CLIENT_OK != result) {
        LogError("PnpAdapter structure is not initialized properly");
    }
    return result;
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_CreateAdapter(
    const char* adapterId,
    PPNP_ADAPTER_CONTEXT_TAG* adapterContext,
    JSON_Value* config)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    PPNP_ADAPTER_CONTEXT_TAG pnpAdapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)malloc(sizeof(PNP_ADAPTER_CONTEXT_TAG));
    if (pnpAdapterHandle == NULL)
    {
        result = IOTHUB_CLIENT_ERROR;
        LogError("Couldn't allocate memory for PNP_ADAPTER_CONTEXT_TAG");
        goto exit;
    }

    pnpAdapterHandle->context = NULL;
    pnpAdapterHandle->adapterGlobalConfig = NULL;
    // Get adapter structure from manifest
    pnpAdapterHandle->adapter = (PPNP_ADAPTER_TAG)malloc(sizeof(PNP_ADAPTER_TAG));
    if (pnpAdapterHandle->adapter == NULL)
    {
        result = IOTHUB_CLIENT_ERROR;
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
    pnpAdapterHandle->adapter->ComponentListLock = Lock_Init();
    pnpAdapterHandle->adapter->PnpComponentList = singlylinkedlist_create();
    if (NULL == pnpAdapterHandle->adapter->ComponentListLock) {
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    // Get global adapter parameters and allow the adapter to do internal setup

    pnpAdapterHandle->adapterGlobalConfig = Configuration_GetGlobalAdapterParameters(config, adapterId);
    if (NULL == pnpAdapterHandle->adapterGlobalConfig)
    {
        LogInfo("Adapter with identity %s does not have any associated global parameters. Proceeding with adapter creation.", adapterId);
    }
    result = pnpAdapterHandle->adapter->adapter->createAdapter(pnpAdapterHandle->adapterGlobalConfig, pnpAdapterHandle);
    if (!PNPBRIDGE_SUCCESS(result))
    {
        LogError("Adapter %s'couldn't be created.", adapterId);
        goto exit;
    }

    *adapterContext = pnpAdapterHandle;
exit:
    return result;
}

bool PnpAdapterManager_AdapterCreated(
    PPNP_ADAPTER_MANAGER adapterMgr,
    const char* adapterId)
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

IOTHUB_CLIENT_RESULT PnpAdapterManager_CreateManager(
    PPNP_ADAPTER_MANAGER* adapterMgr,
    JSON_Value* config)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PPNP_ADAPTER_MANAGER adapterManager = NULL;

    adapterManager = (PPNP_ADAPTER_MANAGER)malloc(sizeof(PNP_ADAPTER_MANAGER));
    if (NULL == adapterManager) {
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    adapterManager->NumComponents = 0;
    adapterManager->PnpAdapterHandleList = singlylinkedlist_create();
    JSON_Array* devices = Configuration_GetDevices(config);
    if (NULL == devices) {
        LogError("No configured devices in the pnpbridge config");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    for (size_t i = 0; i < json_array_get_count(devices); i++) {
        JSON_Object* device = json_array_get_object(devices, i);
        const char* adapterId = json_object_dotget_string(device, PNP_CONFIG_ADAPTER_ID);


        // Create and initialize adapter if it has never been done before

        if (!PnpAdapterManager_AdapterCreated(adapterManager, adapterId))
        {
            PPNP_ADAPTER_CONTEXT_TAG pnpAdapterHandle = NULL;

            result = PnpAdapterManager_CreateAdapter(adapterId, &pnpAdapterHandle, config);
            if (pnpAdapterHandle == NULL || result != IOTHUB_CLIENT_OK)
            {
                LogError("Adapter creation and initialization of a required adapter failed.");
                LogError("Destroying all adapters previously created.");
                goto exit;
            }

            // Add to list of manager's adapters
            singlylinkedlist_add(adapterManager->PnpAdapterHandleList, pnpAdapterHandle);

        }
    }

    *adapterMgr = adapterManager;

exit:
    if (!PNPBRIDGE_SUCCESS(result))
    {
        if (NULL != adapterManager)
        {
            PnpAdapterManager_ReleaseManager(adapterManager);
        }
    }
    return result;
}

void PnpAdapterManager_ReleaseManager(
    PPNP_ADAPTER_MANAGER adapterMgr)
{
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        // Free adapter resources
        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);
            if (adapterHandle->adapter)
            {
                // Clean up adapter's context
                adapterHandle->adapter->adapter->destroyAdapter(adapterHandle);
                free(adapterHandle->adapter);
            }

            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }

        // This call to singlylinkedlist_destroy will free all resources 
        // associated with the list identified by the handle argument,
        // therefore each adapter handle allocated during creation
        // will be destroyed after this call returns
        singlylinkedlist_destroy(adapterMgr->PnpAdapterHandleList);

        // Free components in model
        PnpAdapterManager_ReleaseComponentsInModel(adapterMgr);

        // Free adapter manager
        free(adapterMgr);
    }
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_GetAdapterHandle(
    PPNP_ADAPTER_MANAGER adapterMgr,
    const char* adapterIdentity,
    PPNP_ADAPTER_CONTEXT_TAG* adapterContext)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_INVALID_ARG;
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

            if (!strcmp(adapterHandle->adapter->adapter->identity, adapterIdentity))
            {
                *adapterContext = adapterHandle;
                result = IOTHUB_CLIENT_OK;
                break;
            }

            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
    }

    return result;

}

IOTHUB_CLIENT_RESULT PnpAdapterManager_CreateComponents(
    PPNP_ADAPTER_MANAGER adapterMgr, JSON_Value* config)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    JSON_Array* devices = Configuration_GetDevices(config);
    if (NULL == devices) {
        LogError("No configured devices in the pnpbridge config");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    for (size_t i = 0; i < json_array_get_count(devices); i++) {

        JSON_Object* device = json_array_get_object(devices, i);
        const char* adapterId = json_object_dotget_string(device, PNP_CONFIG_ADAPTER_ID);
        const char* componentName = json_object_dotget_string(device, PNP_CONFIG_COMPONENT_NAME);

        JSON_Object* deviceAdapterArgs = json_object_dotget_object(device, PNP_CONFIG_DEVICE_ADAPTER_CONFIG);
        PPNP_ADAPTER_CONTEXT_TAG adapterHandle = NULL;

        result = PnpAdapterManager_GetAdapterHandle(adapterMgr, adapterId, &adapterHandle);

        if (IOTHUB_CLIENT_OK == result)
        {
            PPNPADAPTER_COMPONENT_TAG componentHandle = (PPNPADAPTER_COMPONENT_TAG)malloc(sizeof(PNPADAPTER_COMPONENT_TAG));

            if (componentHandle != NULL)
            {

                componentHandle->componentName = componentName;
                componentHandle->adapterIdentity = adapterHandle->adapter->adapter->identity;
                result = adapterHandle->adapter->adapter->createPnpComponent(adapterHandle, componentName, deviceAdapterArgs,
                                                                                componentHandle);
                if (PNPBRIDGE_SUCCESS(result))
                {
                    singlylinkedlist_add(adapterHandle->adapter->PnpComponentList, componentHandle);
                    adapterMgr->NumComponents++;
                }
                else
                {
                    LogInfo("Interface component creation with instance name: %s failed.", componentName);
                    free(componentHandle);
                    goto exit;
                }
            }
            else
            {
                result = IOTHUB_CLIENT_ERROR;
                goto exit;
            }
        }
    }

exit:
    return result;
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_StartComponents(
    PPNP_ADAPTER_MANAGER adapterMgr)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    if (NULL != adapterMgr)
    {
        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

            LIST_ITEM_HANDLE componentHandleItem = singlylinkedlist_get_head_item(adapterHandle->adapter->PnpComponentList);
            while (NULL != componentHandleItem)
            {
                PPNPADAPTER_COMPONENT_TAG componentHandle = (PPNPADAPTER_COMPONENT_TAG)singlylinkedlist_item_get_value(componentHandleItem);
                componentHandle->deviceClient = g_PnpBridge->IotHandle.u1.IotDevice.deviceHandle;
                result = adapterHandle->adapter->adapter->startPnpComponent(adapterHandle, componentHandle);
                componentHandleItem = singlylinkedlist_get_next_item(componentHandleItem);
            }
            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
    }
    return result;
}

void PnpAdapterManager_ReleaseComponentsInModel(
        PPNP_ADAPTER_MANAGER adapterMgr)
{
    if (adapterMgr != NULL && adapterMgr->ComponentsInModel != NULL)
    {
        for (unsigned int i = 0; i < adapterMgr->NumComponents; i++)
        {
            free(adapterMgr->ComponentsInModel[i]);
        }
        free (adapterMgr->ComponentsInModel);
    }
}

IOTHUB_CLIENT_RESULT PnpAdapterManager_BuildComponentsInModel(
        PPNP_ADAPTER_MANAGER adapterMgr)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    unsigned int componentNumber = 0;
    if (NULL != adapterMgr)
    {
        adapterMgr->ComponentsInModel = malloc(adapterMgr->NumComponents * sizeof(char*));
        if (NULL == adapterMgr->ComponentsInModel)
        {
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(adapterMgr->PnpAdapterHandleList);

        while (NULL != adapterListItem) {

            PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

            LIST_ITEM_HANDLE componentHandleItem = singlylinkedlist_get_head_item(adapterHandle->adapter->PnpComponentList);
            while (NULL != componentHandleItem)
            {
                PPNPADAPTER_COMPONENT_TAG componentHandle = (PPNPADAPTER_COMPONENT_TAG)singlylinkedlist_item_get_value(componentHandleItem);
                mallocAndStrcpy_s((char**)&adapterMgr->ComponentsInModel[componentNumber++], componentHandle->componentName);
                componentHandleItem = singlylinkedlist_get_next_item(componentHandleItem);
            }
            adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
        }
        if (componentNumber != adapterMgr->NumComponents)
        {
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }
    }
exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        PnpAdapterManager_ReleaseComponentsInModel(adapterMgr);
    }
    return result;
}

PPNPADAPTER_COMPONENT_TAG PnpAdapterManager_GetComponentHandleFromComponentName(const char * ComponentName, size_t ComponentNameSize)
{
    PPNPADAPTER_COMPONENT_TAG componentHandle = NULL;
    bool componentFound = false;
    if (NULL != ComponentName)
    {
        if ((g_PnpBridge != NULL) && (PNP_BRIDGE_INITIALIZED == g_PnpBridgeState) && (g_PnpBridge->PnpMgr != NULL))
        {
            LIST_ITEM_HANDLE adapterListItem = singlylinkedlist_get_head_item(g_PnpBridge->PnpMgr->PnpAdapterHandleList);

            while (NULL != adapterListItem && !componentFound)
            {
                PPNP_ADAPTER_CONTEXT_TAG adapterHandle = (PPNP_ADAPTER_CONTEXT_TAG)singlylinkedlist_item_get_value(adapterListItem);

                LIST_ITEM_HANDLE componentHandleItem = singlylinkedlist_get_head_item(adapterHandle->adapter->PnpComponentList);
                while (NULL != componentHandleItem)
                {
                    PPNPADAPTER_COMPONENT_TAG componentHandleIterator = (PPNPADAPTER_COMPONENT_TAG)singlylinkedlist_item_get_value(componentHandleItem);
                    if (0 == strncmp(ComponentName, componentHandleIterator->componentName, ComponentNameSize))
                    {
                        componentHandle = componentHandleIterator;
                        componentFound = true;
                        break;
                    }
                    componentHandleItem = singlylinkedlist_get_next_item(componentHandleItem);
                }
                adapterListItem = singlylinkedlist_get_next_item(adapterListItem);
            }
        }
    }
    return componentHandle;
}

int PnpAdapterManager_DeviceMethodCallback(
    const char* methodName,
    const unsigned char* payload,
    size_t size,
    unsigned char** response,
    size_t* responseSize,
    void* userContextCallback)
{
    const char *componentName;
    size_t componentNameSize;
    const char *pnpCommandName;
    char* jsonStr = NULL;
    JSON_Value* commandValue = NULL;
    int result = PNP_STATUS_SUCCESS;

    // PnP APIs do not set userContextCallback for device method callbacks, ignore this
    AZURE_UNREFERENCED_PARAMETER(userContextCallback);

    // Parse the methodName into its PnP componentName and pnpCommandName.
    PnP_ParseCommandName(methodName, (const unsigned char**) (&componentName), &componentNameSize, &pnpCommandName);

    // Parse the JSON of the payload request.
    if ((jsonStr = PnP_CopyPayloadToString(payload, size)) == NULL)
    {
        LogError("Unable to allocate twin buffer");
        result = PNP_STATUS_INTERNAL_ERROR;
    }
    else if ((commandValue = json_parse_string(jsonStr)) == NULL)
    {
        LogError("Unable to parse twin JSON");
        result = PNP_STATUS_INTERNAL_ERROR;
    }
    else
    {
        if (componentName != NULL)
        {
            LogInfo("Received PnP command for component=%.*s, command=%s", (int)componentNameSize, componentName, pnpCommandName);

            PPNPADAPTER_COMPONENT_TAG componentHandle = PnpAdapterManager_GetComponentHandleFromComponentName(componentName, componentNameSize);
            if (componentHandle != NULL)
            {
                result = componentHandle->processCommand(componentHandle, pnpCommandName, commandValue, response, responseSize);
            }
        }
    }

    return result;
}

void PnpAdapterManager_DeviceTwinCallback(
    DEVICE_TWIN_UPDATE_STATE updateState,
    const unsigned char* payload,
    size_t size,
    void* userContextCallback)
{
    // Invoke PnP_ProcessTwinData to actualy process the data.  PnP_ProcessTwinData uses a visitor pattern to parse
    // the JSON and then visit each property, invoking PnpAdapterManager_RoutePropertyCallback on each element.
    if (PnP_ProcessTwinData(updateState, payload, size, (const char**) g_PnpBridge->PnpMgr->ComponentsInModel,
            g_PnpBridge->PnpMgr->NumComponents, PnpAdapterManager_RoutePropertyCallback, userContextCallback) == false)
    {
        // If we're unable to parse the JSON for any reason (typically because the JSON is malformed or we ran out of memory)
        // there is no action we can take beyond logging.
        LogError("Unable to process twin json. Ignoring any desired property update requests");
    }
}

// PnpAdapterManager_RoutePropertyCallback is the callback function that the PnP helper layer invokes per property update.
static void PnpAdapterManager_RoutePropertyCallback(
    const char* componentName,
    const char* propertyName,
    JSON_Value* propertyValue,
    int version,
    void* userContextCallback)
{

    if (componentName != NULL && propertyName != NULL)
    {
        LogInfo("Received PnP property update for component=%s, property=%s", componentName, propertyName);

        PPNPADAPTER_COMPONENT_TAG componentHandle = PnpAdapterManager_GetComponentHandleFromComponentName(componentName, strlen(componentName));
        if (componentHandle != NULL)
        {
            componentHandle->processPropertyUpdate(componentHandle, propertyName, propertyValue, version, userContextCallback);
        }
    }
}