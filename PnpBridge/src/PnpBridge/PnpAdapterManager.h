#pragma once

typedef struct _PNP_ADAPTER_MANAGER {
	SINGLYLINKEDLIST_HANDLE AdapterList;
	MAP_HANDLE PnpAdapterMap;
} PNP_ADAPTER_MANAGER, *PPNP_ADAPTER_MANAGER;


/**
* @brief    PnpAdapterManager_Create creates the Azure Pnp Interface adapter manager.
*
* @remarks  PnpAdapterManager_Create initializes all the available Pnp Interface adapters by calling their
            initialize method, if implemented. The Pnp Interface adapter is added to a PnpAdapterMap,
            whose key will be the PnpAdapters identity. This identity will be used to find the PnpAdapter
            based on the discovery extension change message.

* @param    adapter           Pointer to get back an initialized PPNP_ADAPTER_MANAGER.
*
* @returns  PNPBRIDGE_OK on success and other PNPBRIDGE_RESULT values on failure.
*/
PNPBRIDGE_RESULT PnpAdapterManager_Create(PPNP_ADAPTER_MANAGER* adapter);

/**
* @brief    PnpAdapterManager_Release uninitializes the PnpAdapter resources.
*
* @remarks  PnpAdapterManager_Release calls shutdown on all the Pnp adapters and cleans up the PnpAdapterManager

* @param    adapter           Pointer to an initialized PPNP_ADAPTER_MANAGER.
*
* @returns  VOID.
*/
void PnpAdapterManager_Release(PPNP_ADAPTER_MANAGER adapter);


int PnpAdapterManager_SupportsIdentity(PPNP_ADAPTER_MANAGER adapter, JSON_Object* Message, bool* supported, int* key);
int PnpAdapterManager_CreatePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, int key, PNP_INTERFACE_CLIENT_HANDLE** InterfaceClient, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload);
int PnpAdapterManager_ReleasePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNPADAPTER_INTERFACE_HANDLE Interface);