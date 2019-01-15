#include "parson.h"

typedef enum _PNP_INTERFACE_CHANGE_TYPE {
	PNP_INTERFACE_ARRIVAL,
	PNP_INTERFACE_REMOVAL,
	PNP_INTERFACE_CUSTOM
} PNP_INTERFACE_CHANGE_TYPE;

typedef struct _DEVICE_CHANGE_PAYLOAD {
	PNP_INTERFACE_CHANGE_TYPE Type;
	JSON_Object* Message;
	void* Context;
} DEVICE_CHANGE_PAYLOAD, *PDEVICE_CHANGE_PAYLOAD;

typedef int(*NOTIFY_DEVICE_CHANGE)(PDEVICE_CHANGE_PAYLOAD DeviceChangePayload);

typedef int(*START_DISCOVER)(NOTIFY_DEVICE_CHANGE DeviceChangeCallback, JSON_Object* args);
typedef int(*STOP_DISCOVERY)();
typedef int(*GET_FILTER_FORMAT_IDS)(char*** filterFormatIds, int* NumberOfFormats);

typedef struct _DISCOVER_INTERFACE {
    const char* Identity;
	START_DISCOVER StartDiscovery;
	STOP_DISCOVERY StopDiscovery;
	GET_FILTER_FORMAT_IDS GetFilterFormatIds;
} DISCOVERY_INTERFACE, *PDISCOVERY_INTERFACE;

int GetDiscoveryModuleInfo(PDISCOVERY_INTERFACE DiscoryInterface);

int DeviceAggregator_DeviceChangeCallback(PDEVICE_CHANGE_PAYLOAD DeviceChangePayload);