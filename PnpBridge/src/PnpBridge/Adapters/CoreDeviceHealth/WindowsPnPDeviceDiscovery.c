#include "common.h"
#include "DiscoveryAdapterInterface.h"
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>

/* A5DCBF10-6530-11D2-901F-00C04FB951ED */
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, \
    0xC0, 0x4F, 0xB9, 0x51, 0xED);

char* format = "{ \"HardwareId\":%s, \"Filter\":\"abc\", \"Identity\":\"core-device-health\"  }";

PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeHandler = NULL;

THREAD_HANDLE NotifyDevicePnpChanges;
void NotifyDevicePnpChanges_worker(void* context)
{
    PNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload = { 0 };
	//payload.Type = PNP_INTERFACE_ARRIVAL;

	JSON_Value* json = json_parse_string(context);
	JSON_Object* jsonObject = json_value_get_object(json);

	payload.Message = jsonObject;

	DeviceChangeHandler(&payload);

	free(context);
}

DWORD 
OnDeviceNotification(
	_In_ HCMNOTIFICATION hNotify,
	_In_opt_ PVOID context,
	_In_ CM_NOTIFY_ACTION action,
	_In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
	_In_ DWORD eventDataSize)
{

	if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) { // , "windows-pnp-module");
		if (DeviceChangeHandler != NULL) {
			char* msg = malloc(512*sizeof(char));
            PNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload = { 0 };
			//payload.Type = PNP_INTERFACE_ARRIVAL;

			DEVPROPTYPE PropType;
			WCHAR hardwareIds[MAX_DEVICE_ID_LEN];
			ULONG BufferSize = MAX_DEVICE_ID_LEN * sizeof(WCHAR);
			CONFIGRET status;

			DEVINST Devinst;
			DEVPROPTYPE DevPropType;
			ULONG DevPropSize;

			WCHAR deviceInstance[MAX_DEVICE_ID_LEN];
			ULONG deviceInstanceSize = MAX_DEVICE_ID_LEN * sizeof(WCHAR);

			DevPropSize = MAX_DEVICE_ID_LEN * sizeof(WCHAR);
			status = CM_Get_Device_Interface_Property(
							eventData->u.DeviceInterface.SymbolicLink,
							&DEVPKEY_Device_InstanceId,
							&DevPropType,
							(PBYTE)deviceInstance,
							&deviceInstanceSize,
							0);

			if (status != CR_SUCCESS) {
				return -1;
			}

			status = CM_Locate_DevNode(
						&Devinst,
						deviceInstance,
						CM_LOCATE_DEVNODE_NORMAL);

			if (status != CR_SUCCESS) {
				return -1;
			}
			/////////////


			/*status = CM_Get_Device_Interface_Property(eventData->u.DeviceInterface.SymbolicLink,
							&DEVPKEY_Device_HardwareIds,
							&PropType,
							(PBYTE)hardwareIds,
							&BufferSize,
							0);*/

			status = CM_Get_DevNode_Property(
						Devinst,
						&DEVPKEY_Device_HardwareIds,
						&PropType,
						(PBYTE)hardwareIds,
						&BufferSize,
						0);
			if (CR_SUCCESS != status)
			{
				hardwareIds[0] = L'\0';
			}

			LPWSTR SingleDeviceId = (LPWSTR)hardwareIds;
			size_t cLength = 0;
			cLength = wcslen(SingleDeviceId);
			while (*SingleDeviceId)
			{
				cLength = wcslen(SingleDeviceId);
				SingleDeviceId += cLength + 1;
				sprintf_s(msg, 512, "%S", SingleDeviceId);
				break;
			}

			LogInfo(msg);

			//sprintf_s(msg, 512, "%S", eventData->u.DeviceInstance.InstanceId);
			STRING_HANDLE asJson = STRING_new_JSON(msg);
			sprintf_s(msg, 512, format, STRING_c_str(asJson));

			JSON_Value* json = json_parse_string(msg);
			JSON_Object* jsonObject = json_value_get_object(json);

			payload.Message = jsonObject;

			/*if (ThreadAPI_Create(&NotifyDevicePnpChanges, NotifyDevicePnpChanges_worker, msg) != THREADAPI_OK)
			{
				LogError("ThreadAPI_Create failed");
			}*/

			DeviceChangeHandler(&payload);
			STRING_delete(asJson);
		}
	}
	return 0;
}

HCMNOTIFICATION hNotifyCtx = NULL;

PNPBRIDGE_RESULT PnpStartDiscovery(PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback, JSON_Object* deviceArgs, JSON_Object* adapterArgs) {
	DWORD cmRet;
	CM_NOTIFY_FILTER    cmFilter;

	ZeroMemory(&cmFilter, sizeof(cmFilter));
	cmFilter.cbSize = sizeof(cmFilter);
	//cmFilter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;

    cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
	cmFilter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_USB_DEVICE;

	cmRet = CM_Register_Notification(
				&cmFilter,                      // PCM_NOTIFY_FILTER pFilter,
				NULL,                           // PVOID pContext,
				(PCM_NOTIFY_CALLBACK) OnDeviceNotification,                 // PCM_NOTIFY_CALLBACK pCallback,
				&hNotifyCtx                     // PHCMNOTIFICATION pNotifyContext
				);
	if (cmRet != CR_SUCCESS)
	{
		return PNPBRIDGE_FAILED;
	}

	DeviceChangeHandler = DeviceChangeCallback;

    //EnumerateDevices(s_CameraInterfaceCategories[i])

	return 0;
}

int PnpStopDiscovery() {
	CM_Unregister_Notification(hNotifyCtx);
	return 0;
}

int GetDiscoveryModuleInfo(PDISCOVERY_ADAPTER DiscoryInterface) {
	if (NULL == DiscoryInterface) {
		return -1;
	}

	DiscoryInterface->StartDiscovery = PnpStartDiscovery;
	DiscoryInterface->StopDiscovery = PnpStopDiscovery;

	return 0;
}

DISCOVERY_ADAPTER WindowsPnpDeviceDiscovery = {
    .Identity = "windows-pnp-device-discovery",
	.StartDiscovery = PnpStartDiscovery,
	.StopDiscovery = PnpStopDiscovery
};