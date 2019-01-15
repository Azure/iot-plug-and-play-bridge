#pragma once

typedef struct _PNP_ADAPTER_MANAGER {
	SINGLYLINKEDLIST_HANDLE AdapterList;
	MAP_HANDLE PnpAdapterMap;
} PNP_ADAPTER_MANAGER, *PPNP_ADAPTER_MANAGER;


PNPBRIDGE_RESULT PnpAdapterManager_Create(PPNP_ADAPTER_MANAGER* adapter);
int PnpAdapterManager_SupportsFilterId(PPNP_ADAPTER_MANAGER adapter, JSON_Object* Message, bool* supported, int* key);
int PnpAdapterManager_BindPnpInterface(PPNP_ADAPTER_MANAGER adapter, int key, PNP_INTERFACE_CLIENT_HANDLE* InterfaceClient, PDEVICE_CHANGE_PAYLOAD DeviceChangePayload);
int PnpAdapterManager_ReleasePnpInterface(PPNP_ADAPTER_MANAGER adapter, PNPADAPTER_INTERFACE_HANDLE Interface);
void PnpAdapterManager_Release(PPNP_ADAPTER_MANAGER adapter);