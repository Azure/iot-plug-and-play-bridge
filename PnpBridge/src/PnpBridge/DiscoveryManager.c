// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "common.h"
#include "PnpBridgeCommon.h"
#include "DiscoveryManager.h"
#include "DiscoveryAdapterInterface.h"
#include "WindowsPnpDeviceDiscovery.h"

DISCOVERY_ADAPTER discoveryInterface = { 0 };

PDISCOVERY_ADAPTER DISCOVERY_MANIFEST[] = {
	&WindowsPnpDeviceDiscovery,
	&ArduinoSerialDiscovery
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

	*discoveryManager = discoveryMgr;

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT DiscoveryAdapterManager_Start(PDISCOVERY_MANAGER discoveryManager) {
	JSON_Array *devices = Configuration_GetConfiguredDevices();

    if (NULL == devices) {
        return PNPBRIDGE_INVALID_ARGS;
    }

	for (int i = 0; i < sizeof(DISCOVERY_MANIFEST) / sizeof(PDISCOVERY_ADAPTER); i++) {
		PDISCOVERY_ADAPTER  discoveryInterface = DISCOVERY_MANIFEST[i];
		JSON_Object* deviceParams = NULL;
        PNPBRIDGE_RESULT result;

		for (int j = 0; j < json_array_get_count(devices); j++) {
			JSON_Object *device = json_array_get_object(devices, j);

            // For this Identity check if there is any device
            // TODO: Create an array of device
			JSON_Object* params = Configuration_GetDiscoveryParametersPerDevice(device);
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

        // TODO: Add validation to check minimum discovery interface methods
        result = discoveryInterface->StartDiscovery(DiscoveryAdapterChangeHandler, deviceParams, adapterParams);

        if (PNPBRIDGE_OK == result) {
            Map_Add_Index(discoveryManager->DiscoveryModuleMap, discoveryInterface->Identity, i);
        }
	}

    return PNPBRIDGE_OK;
}

void DiscoveryAdapterManager_Stop(PDISCOVERY_MANAGER discoveryManager) {
    const char* const* keys;
    const char* const* values;
    size_t count;

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