// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "discoveryadapter_manager.h"
#include "discoveryadapter_api.h"

extern PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[];
extern const int DiscoveryAdapterCount;

void DiscoveryAdapterChangeHandler(PNPMESSAGE DeviceChangePayload);

PNPBRIDGE_RESULT DiscoveryAdapterManager_ValidateDiscoveryAdapter(PDISCOVERY_ADAPTER  discAdapter, MAP_HANDLE discAdapterMap) {
    bool containsKey = false;
    if (NULL == discAdapter->Identity) {
        LogError("DiscoveryAdapter's Identity field is not initialized");
        return PNPBRIDGE_INVALID_ARGS;
    }

    if (MAP_OK != Map_ContainsKey(discAdapterMap, discAdapter->Identity, &containsKey)) {
        LogError("Map_ContainsKey failed");
        return PNPBRIDGE_FAILED;
    }

    if (containsKey) {
        LogError("Found duplicate discovery adapter identity %s", discAdapter->Identity);
        return PNPBRIDGE_DUPLICATE_ENTRY;
    }

    if (NULL == discAdapter->StartDiscovery || NULL == discAdapter->StopDiscovery) {
        LogError("DiscoveryAdapter's callbacks are not initialized");
        return PNPBRIDGE_INVALID_ARGS;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT DiscoveryAdapterManager_Create(PDISCOVERY_MANAGER* discoveryManager) {
    PDISCOVERY_MANAGER discoveryMgr;

    if (NULL == discoveryManager) {
        LogError("discoveryManager parameter is NULL");
        return PNPBRIDGE_INVALID_ARGS;
    }

    discoveryMgr = malloc(sizeof(DISCOVERY_MANAGER));
    if (NULL == discoveryMgr) {
        LogError("Failed to allocate memory for discoveryManager");
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

    discoveryMgr->DiscoveryAdapterMap = Map_Create(NULL);
    if (NULL == discoveryMgr->DiscoveryAdapterMap) {
        return PNPBRIDGE_FAILED;
    }

    *discoveryManager = discoveryMgr;

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT 
DiscoveryManager_StarDiscoveryAdapter(
    PDISCOVERY_MANAGER discoveryManager,
    PDISCOVERY_ADAPTER  discoveryInterface,
    SINGLYLINKEDLIST_HANDLE DeviceAdapterParamsList,
    int DeviceAdapterParamsCount,
    JSON_Object* adapterParams,
    int key
    )
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    char* adapterParamString = NULL;
    PNPMEMORY adapterParamsMemory = NULL;
    PNPMEMORY deviceParamsMemory = NULL;
    char** deviceParams = NULL;

    TRY {
        result = DiscoveryAdapterManager_ValidateDiscoveryAdapter(discoveryInterface, 
                    discoveryManager->DiscoveryAdapterMap);
        if (PNPBRIDGE_OK != result) {
            LogError("Parson failed to serialize adapterParams");
            LEAVE;
        }

        // Serialize device discovery adapter parameters 
        if (NULL != DeviceAdapterParamsList && DeviceAdapterParamsCount > 0) {
            deviceParams = malloc(sizeof(char*) * DeviceAdapterParamsCount);
            if (NULL == deviceParams) {
                LogError("Failed to allocate memory for deviceParams");
                result = PNPBRIDGE_INSUFFICIENT_MEMORY;
                LEAVE;
            }

            LIST_ITEM_HANDLE handle = singlylinkedlist_get_head_item(DeviceAdapterParamsList);
            int x = 0;
            int stringSize = 0;
            while (NULL != handle) {
                JSON_Object* deviceAdpaterParam = (JSON_Object*)singlylinkedlist_item_get_value(handle);
                deviceParams[x] = json_serialize_to_string(json_object_get_wrapping_value(deviceAdpaterParam));
                if (NULL == deviceParams[x]) {
                    LogError("Failed to serialize deviceAdpaterParam");
                    result = PNPBRIDGE_INSUFFICIENT_MEMORY;
                    LEAVE;
                }
                
                stringSize += (int)strlen(deviceParams[x]) + 1;

                handle = singlylinkedlist_get_next_item(handle);
                x++;
            }

            // Create PNPMEMORY for the deviceParamString
            PDEVICE_ADAPTER_PARMAETERS deviceAdapterParams;
            PnpMemory_Create(PNPMEMORY_NO_OBJECT_PARAMS, sizeof(DEVICE_ADAPTER_PARMAETERS) + sizeof(char*) * x +  sizeof(char) *  stringSize, &deviceParamsMemory);\

            deviceAdapterParams = (PDEVICE_ADAPTER_PARMAETERS) PnpMemory_GetBuffer(deviceParamsMemory, NULL);

            deviceAdapterParams->Count = x;

            // Copy serialized device adapter parameters to PNPMEMORY
            char* offset = (char*)(&deviceAdapterParams->AdapterParameters + x);
            for (int i = 0; i < DeviceAdapterParamsCount; i++) {
                deviceAdapterParams->AdapterParameters[i] = offset;
                int size = (int) strlen(deviceParams[i]) + 1;
                strcpy_s(deviceAdapterParams->AdapterParameters[i], size, deviceParams[i]);

                offset += size;
            }
        }

        if (adapterParams) {
            // Serialize the adapter parameters
            adapterParamString = json_serialize_to_string(json_object_get_wrapping_value(adapterParams));
            if (NULL == adapterParamString) {
                LogError("Parson failed to serialize adapterParams");
                result = PNPBRIDGE_INSUFFICIENT_MEMORY;
                LEAVE;
            }

            // Create PNPMEMORY for the adapterParamString
            int length = (int)(strlen(adapterParamString) + 1);
            PnpMemory_Create(PNPMEMORY_NO_OBJECT_PARAMS, sizeof(char) *  length, &adapterParamsMemory);
            strcpy_s(PnpMemory_GetBuffer(adapterParamsMemory, NULL), length, adapterParamString);
        }

        // Invoke the adapters StartDiscovery
        int adapterReturn = discoveryInterface->StartDiscovery(deviceParamsMemory, adapterParamsMemory);
        if (adapterReturn < 0) {
            LogError("DiscoveryAdapter (%s) StartDiscovery failed", discoveryInterface->Identity);
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        Map_Add_Index(discoveryManager->DiscoveryAdapterMap, discoveryInterface->Identity, key);

    } FINALLY{
        if (adapterParamString) {
            json_free_serialized_string(adapterParamString);
        }

        if (adapterParamsMemory) {
            PnpMemory_ReleaseReference(adapterParamsMemory);
        }

        if (deviceParamsMemory) {
            PnpMemory_ReleaseReference(deviceParamsMemory);
        }

        if (deviceParams) {
            for (int i = 0; i < DeviceAdapterParamsCount; i++) {
                json_free_serialized_string(deviceParams[i]);
            }
            free(deviceParams);
        }
    }

    return PNPBRIDGE_OK;
}

//PNPBRIDGE_RESULT DiscoveryAdapterManager_PublishAlwaysInterfaces(PDISCOVERY_MANAGER discoveryManager, JSON_Value* config) {
//    AZURE_UNREFERENCED_PARAMETER(discoveryManager);
//    JSON_Array *devices = Configuration_GetConfiguredDevices(config);
//
//    for (int i = 0; i < (int)json_array_get_count(devices); i++) {
//        JSON_Object* device = json_array_get_object(devices, i);
//        const char* publishModeString = json_object_get_string(device, PNP_CONFIG_PUBLISH_MODE);
//        if (NULL != publishModeString && strcmp(publishModeString, PNP_CONFIG_PUBLISH_MODE_ALWAYS) == 0) {
//            PNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload = { 0 };
//            payload.ChangeType = PNPBRIDGE_INTERFACE_CHANGE_PERSIST;
//
//            // Get the match filters and post a device change message
//            JSON_Object* matchParams = Configuration_GetMatchParametersForDevice(device);
//            const char* interfaceId = json_object_dotget_string(device, PNP_CONFIG_INTERFACE_ID);
//            if (NULL != interfaceId && NULL != matchParams) {
//                char msg[512] = { 0 };
//                sprintf_s(msg, 512, DISCOVERY_MANAGER_ALWAY_PUBLISH_PAYLOAD, interfaceId, json_serialize_to_string(json_object_get_wrapping_value(matchParams)));
//                    
//                payload.Message = msg;
//                payload.MessageLength = (int)strlen(msg);
//
//                DiscoveryAdapterChangeHandler(&payload);
//            }
//        }
//    }
//
//    return 0;
//}

PNPBRIDGE_RESULT DiscoveryAdapterManager_Start(PDISCOVERY_MANAGER DiscoveryManager, PNPBRIDGE_CONFIGURATION * Config)
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    JSON_Array *devices = NULL;
   
   
    TRY {
        devices = Configuration_GetConfiguredDevices(Config->JsonConfig);
        
        // For each discovery adapter, get the adapter parameters and device then
        // invoke the StartDiscovery
        for (int i = 0; i < DiscoveryAdapterCount; i++) {
            PDISCOVERY_ADAPTER  discoveryInterface = DISCOVERY_ADAPTER_MANIFEST[i];
            SINGLYLINKEDLIST_HANDLE deviceParamsList = NULL;
            int deviceParamsCount = 0;

            // Create device parameter list
            deviceParamsList = singlylinkedlist_create();
            if (NULL == deviceParamsList) {
                // Report error
            }

            for (int j = 0; j < (int)json_array_get_count(devices); j++) {
                JSON_Object *device = json_array_get_object(devices, j);

                // Check if the discovery adapter identity matches
                JSON_Object* params = Configuration_GetDiscoveryParametersForDevice(device);
                if (NULL != params) {
                    const char* discoveryIdentity = json_object_get_string(params, PNP_CONFIG_IDENTITY);
                    if (NULL != discoveryIdentity) {
                        if (0 == strcmp(discoveryIdentity, discoveryInterface->Identity)) {
                            // Add it to the device parameter list
                            singlylinkedlist_add(deviceParamsList, params);
                            deviceParamsCount++;
                        }
                    }
                }
            }

            JSON_Object* adapterParams = NULL;
            adapterParams = Configuration_GetDiscoveryParameters(Config->JsonConfig, discoveryInterface->Identity);

            result = DiscoveryManager_StarDiscoveryAdapter(DiscoveryManager, discoveryInterface, 
                            deviceParamsList, deviceParamsCount, adapterParams, i);
            if (PNPBRIDGE_OK != result) {
                LEAVE;
            }
        }
    } FINALLY {
    }

    return result;
}

void DiscoveryAdapterManager_Release(PDISCOVERY_MANAGER discoveryManager) {
    const char* const* keys;
    const char* const* values;
    size_t count;

    // Wait for all start discovery threads to join
   /* LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(discoveryManager->startDiscoveryThreadHandles);
    while (interfaceItem != NULL) {
        PSTART_DISCOVERY_PARAMS threadArgument = (PSTART_DISCOVERY_PARAMS)singlylinkedlist_item_get_value(interfaceItem);
        ThreadAPI_Join(threadArgument, NULL);
        interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
        free(threadArgument);
    }
    singlylinkedlist_destroy(discoveryManager->startDiscoveryThreadHandles);*/

    // Call shutdown on all interfaces
    if (Map_GetInternals(discoveryManager->DiscoveryAdapterMap, &keys, &values, &count) != MAP_OK)
    {
        LogError("Map_GetInternals failed to get all pnp adapters");
    }
    else
    {
        for (int i = 0; i < (int)count; i++)
        {
            int index = atoi(values[i]);
            PDISCOVERY_ADAPTER  adapter = DISCOVERY_ADAPTER_MANIFEST[index];
            adapter->StopDiscovery();
        }
    }

    Map_Destroy(discoveryManager->DiscoveryAdapterMap);
    free(discoveryManager);
}

void DiscoveryAdapterChangeHandler(PNPMESSAGE DeviceChangePayload) {
    PnpBridge_ProcessPnpMessage(DeviceChangePayload);
}
