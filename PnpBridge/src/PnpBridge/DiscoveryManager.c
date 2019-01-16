#include "common.h"
#include "PnpBridgeCommon.h"
#include "DiscoveryManager.h"
#include "DiscoveryInterface.h"
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

	// Build an interface map
	for (int i = 0; i < sizeof(DISCOVERY_MANIFEST)/sizeof(PDISCOVERY_ADAPTER); i++) {
		PDISCOVERY_ADAPTER  discoveryAdapter = DISCOVERY_MANIFEST[i];
        Map_Add(discoveryMgr->DiscoveryModuleMap, discoveryAdapter->Identity, (char *)discoveryAdapter);
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
		JSON_Object* discoveryParams = NULL;
        PNPBRIDGE_RESULT result;

		// For this FiterId check if there is any device
		for (int i = 0; i < json_array_get_count(devices); i++) {
			JSON_Object *device = json_array_get_object(devices, i);

			JSON_Object* params = Configuration_GetDiscoveryParameters(device);
            if (NULL != params) {
                const char* deviceFormatId = json_object_get_string(params, "Identity");
                if (strcmp(deviceFormatId, discoveryInterface->Identity) == 0) {
                    discoveryParams = params;
                    break;
                }
            }
		}

        result = discoveryInterface->StartDiscovery(DiscoveryAdapterChangeHandler, discoveryParams);

        if (PNPBRIDGE_OK == result) {
            Map_Add_Index(discoveryManager->DiscoveryModuleMap, discoveryInterface->Identity, i);
        }
	}

    return PNPBRIDGE_OK;
}

void DiscoveryAdapterManager_Stop(PDISCOVERY_MANAGER discoveryManager) {
	for (int i = 0; i < sizeof(DISCOVERY_MANIFEST) / sizeof(DISCOVERY_ADAPTER); i++) {
		PDISCOVERY_ADAPTER  adapter = DISCOVERY_MANIFEST[i];
		adapter->StopDiscovery();
	}
}

// CALLBACK1
void DiscoveryAdapterChangeHandler(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
    PnpBridge_DeviceChangeCallback(DeviceChangePayload);
}