#pragma once

#include "common.h"

#include "PnpBridgeCommon.h"

#include "DiscoveryInterface.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

#include "CoreDeviceManagement.h"

PPNP_INTERFACE_MODULE INTERFACE_MANIFEST[] = {
	//&CoreDeviceManagement,
	&SerialPnpImplementor
};

PNPBRIDGE_RESULT PnpAdapterManager_Create(PPNP_ADAPTER_MANAGER* adapter) {
    if (NULL == adapter) {
        return PNPBRIDGE_INVALID_ARGS;
    }

	//
	// Load a list of static modules
	//
	*adapter = (PPNP_ADAPTER_MANAGER) malloc(sizeof(PNP_ADAPTER_MANAGER));
    if (NULL == *adapter) {
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

	(*adapter)->PnpAdapterMap = Map_Create(NULL);

	// Build an interface map
	for (int i = 0; i < sizeof(INTERFACE_MANIFEST) / sizeof(PPNP_INTERFACE_MODULE); i++) {
		PPNP_INTERFACE_MODULE  d = INTERFACE_MANIFEST[i];
		char** filterFormatIds = NULL;
		int NumberOfFormats = 1;
		d->GetFilterFormatIds(&filterFormatIds, &NumberOfFormats);
		for (int j = 0; j < NumberOfFormats; j++) {
			Map_Add_Index((*adapter)->PnpAdapterMap, filterFormatIds[j], i);
		}
		//int nums = 2;	
	}

	// Build an interface map
	/*for (int i = 0; i < 1; i++) {
		 = INTERFACE_MANIFEST[i]->PnpAdapterMangager_GetInterfaceId;	
	}*/

	return PNPBRIDGE_OK;
}

void PnpAdapterManager_Release(PPNP_ADAPTER_MANAGER adapter) {
	free(adapter);
}

int PnpAdapterManager_SupportsFilterId(PPNP_ADAPTER_MANAGER adapter, JSON_Object* Message, bool* supported, int* key) {
	bool containsMessageKey = false;
	char* interfaceId = NULL;
	char* getFilterId = (char*) json_object_get_string(Message, "Identity");

	*supported = false;

	if ((Map_ContainsKey(adapter->PnpAdapterMap, getFilterId, &containsMessageKey) == MAP_OK) && containsMessageKey) {

		// Get the interface ID
		int index = Map_GetIndexValueFromKey(adapter->PnpAdapterMap, getFilterId);

		*supported = true;
		*key = index;
	}

	return 0;
}

/*
	Check if there is any module that supports this interface
*/
bool SupportsInterface(char* InterfaceId) {

	return false;
}

/*
	Once an interface is registerd with Azure PnpDeviceClient this 
	method will take care of binding it to a module implementing 
	PnP primitives
*/
int PnpAdapterManager_BindPnpInterface(PPNP_ADAPTER_MANAGER adapter, int key, PNP_INTERFACE_CLIENT_HANDLE* InterfaceClient, PDEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
	// Get the module using the key as index
	PPNP_INTERFACE_MODULE  d = INTERFACE_MANIFEST[key];

	PPNPADAPTER_INTERFACE pnpInterface = malloc(sizeof(PNPADAPTER_INTERFACE));

	pnpInterface->Interface = InterfaceClient;
	pnpInterface->key = key;

	// Invoke interface binding method
	d->BindPnpInterface(pnpInterface, DeviceChangePayload);

	//*InterfaceClient = (PNP_INTERFACE_CLIENT_HANDLE *) pnpInterface;

	return 0;
}

int PnpAdapterManager_ReleasePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNPADAPTER_INTERFACE_HANDLE Interface) {
	if (NULL == Interface) {
		return -1;
	}

	PPNPADAPTER_INTERFACE pnpInterface = (PPNPADAPTER_INTERFACE) Interface;

	// Get the module index
	PPNP_INTERFACE_MODULE  d = INTERFACE_MANIFEST[pnpInterface->key];
	d->ReleaseInterface(Interface);

	return 0;
}

PNP_INTERFACE_CLIENT_HANDLE
PnpAdapter_GetPnpInterface(PNPADAPTER_INTERFACE_HANDLE PnpInterfaceClient) {
	if (PnpInterfaceClient == NULL) {
		return NULL;
	}

	PPNPADAPTER_INTERFACE pnpInterfaceClient = (PPNPADAPTER_INTERFACE)PnpInterfaceClient;
	return pnpInterfaceClient->Interface;
}

int PnpAdapter_SetContext(PNPADAPTER_INTERFACE_HANDLE PnpInterfaceClient, void* Context) {
	if (PnpInterfaceClient == NULL) {
		return -1;
	}

	PPNPADAPTER_INTERFACE pnpInterfaceClient = (PPNPADAPTER_INTERFACE)PnpInterfaceClient;
	pnpInterfaceClient->Context = Context;
	return 0;
}

int PnpAdapterMangager_GetInterfaceId(PPNP_ADAPTER_MANAGER adapter, int key, JSON_Object* Message, char** InterfaceId) {
	PPNP_INTERFACE_MODULE  d = INTERFACE_MANIFEST[key];

	/*d->GetInterfaceId(Message, InterfaceId);*/
	//*InterfaceId = "http:////coredevice-interface//v1";

	return 0;
}
