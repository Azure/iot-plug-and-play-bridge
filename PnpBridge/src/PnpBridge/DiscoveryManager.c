#include "common.h"
#include "PnpBridgeCommon.h"
#include "DiscoveryManager.h"
#include "DiscoveryInterface.h" // Need to remove it
#include "WindowsPnpDeviceDiscovery.h"

DISCOVERY_INTERFACE discoveryInterface = { 0 };

PDISCOVERY_INTERFACE DISCOVERY_MANIFEST[] = {
	&WindowsPnpDeviceDiscovery,
	&ArduinoSerialDiscovery
};

PNPBRIDGE_RESULT DiscoveryExtensions_Create(PDISCOVERY_MANAGER* discoveryManager) {

	PDISCOVERY_MANAGER discoveryMgr = malloc(sizeof(DISCOVERY_MANAGER));
	discoveryMgr->DiscoveryModuleMap = Map_Create(NULL);

	// Build an interface map
	for (int i = 0; i < sizeof(DISCOVERY_MANIFEST)/sizeof(PDISCOVERY_INTERFACE); i++) {
		PDISCOVERY_INTERFACE  d = DISCOVERY_MANIFEST[i];
		char** filterFormatIds = NULL;
		int NumberOfFormats = 1;
		d->GetFilterFormatIds(&filterFormatIds, &NumberOfFormats);
		for (int i = 0; i < NumberOfFormats; i++) {
			Map_Add(discoveryMgr->DiscoveryModuleMap, filterFormatIds[i], (char *)d);
		}
	}

	*discoveryManager = discoveryMgr;

    return PNPBRIDGE_OK;
}

int DiscoverModuleChangeHandler(PDEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
	//
	// Filter the message based on configuration provided to gateway
	//
	if (Configuration_IsDeviceConfigured(DeviceChangePayload->Message) < 0) {
		return -2;
	}

	if (DeviceChangePayload->Type == PNP_INTERFACE_ARRIVAL) {
		DeviceAggregator_DeviceChangeCallback(DeviceChangePayload);
	}

	return 0;
}

PNPBRIDGE_RESULT DiscoveryExtensions_Start(PDISCOVERY_MANAGER discoveryManager, JSON_Object* args) {
	JSON_Array *devices = Configuration_GetConfiguredDevices();

	for (int i = 0; i < sizeof(DISCOVERY_MANIFEST) / sizeof(PDISCOVERY_INTERFACE); i++) {
		PDISCOVERY_INTERFACE  discoveryInterface = DISCOVERY_MANIFEST[i];
		JSON_Object* moduleParameters = NULL;
		char** filterFormatIds = NULL;
		int NumberOfFormats = 0;
		discoveryInterface->GetFilterFormatIds(&filterFormatIds, &NumberOfFormats);
		for (int j = 0; j < NumberOfFormats; j++) {
			Map_Add_Index(discoveryManager->DiscoveryModuleMap, filterFormatIds[j], i);

			// For this FiterId check if there is any device
			int res = 0;
			for (int i = 0; i < json_array_get_count(devices); i++) {
				JSON_Object *device = json_array_get_object(devices, i);

				JSON_Object* moduleParams = Configuration_GetModuleParameters(device);
				const char* deviceFormatId = json_object_get_string(moduleParams, "Identity");
				if (strcmp(deviceFormatId, filterFormatIds[j]) == 0) {
					//char* enumeration = json_object_get_string(device, "Enumeration");
				//	if (enumeration != NULL && strcmp(enumeration, "Static") == 0) {
					//	printf("static enumeration: %s\n", deviceFormatId);
						moduleParameters = moduleParams;
						break;
				//	}
				}
			}
		}

		discoveryInterface->StartDiscovery(DiscoverModuleChangeHandler, moduleParameters);
	}

    return PNPBRIDGE_OK;
}

void DiscoveryModules_Stop(PDISCOVERY_MANAGER discoveryManager) {
	for (int i = 0; i < sizeof(DISCOVERY_MANIFEST) / sizeof(DISCOVERY_INTERFACE); i++) {
		PDISCOVERY_INTERFACE  d = DISCOVERY_MANIFEST[i];
		d->StopDiscovery();
	}
}
