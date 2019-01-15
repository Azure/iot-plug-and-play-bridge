#pragma once

typedef struct _DEVICE_CHANGE_NOTIFY {
	JSON_Object* Message;
	void* Context;
} DEVICE_CHANGE_NOTIFY, *PDEVICE_CHANGE_NOTIFY;

typedef struct _PNPADAPTER_INTERFACE {
	PNP_INTERFACE_CLIENT_HANDLE Interface;
	void* Context;
	int key;
} PNPADAPTER_INTERFACE, *PPNPADAPTER_INTERFACE;

typedef void* PNPADAPTER_INTERFACE_HANDLE;

// Need to pass arguments (filter)
typedef int(*BIND_PNP_INTERFACE)(PNPADAPTER_INTERFACE_HANDLE Interface, PDEVICE_CHANGE_PAYLOAD args);

typedef int(*RELEASE_PNP_INTERFACE)(PNPADAPTER_INTERFACE_HANDLE Interface);

typedef int(*GET_SUPPORTED_PNP_INTEFACE)();

typedef int(*GET_PNP_FILTER_FORMAT_IDS)(char*** filterFormatIds, int* NumberOfFormats);

typedef int(*GET_PNP_FILTER_FORMAT_IDS)(char*** filterFormatIds, int* NumberOfFormats);

typedef int(*PNP_INTERFACE_MODULE_SUPPORTS_INTERFACE_ID)(char* InterfaceId, bool* supported);

typedef struct PNP_INTERFACE_MODULE {
	GET_SUPPORTED_PNP_INTEFACE GetSupportedInterfaces;
	BIND_PNP_INTERFACE BindPnpInterface;
	RELEASE_PNP_INTERFACE ReleaseInterface;
	GET_PNP_FILTER_FORMAT_IDS GetFilterFormatIds;
	PNP_INTERFACE_MODULE_SUPPORTS_INTERFACE_ID SupportsInterfaceId;
} PNP_INTERFACE_MODULE, *PPNP_INTERFACE_MODULE;


/* PnpAdapter API's*/

PNP_INTERFACE_CLIENT_HANDLE PnpAdapter_GetPnpInterface(PNPADAPTER_INTERFACE_HANDLE PnpInterfaceClient);

int PnpAdapter_SetContext(PNPADAPTER_INTERFACE_HANDLE PnpInterfaceClient, void* Context);