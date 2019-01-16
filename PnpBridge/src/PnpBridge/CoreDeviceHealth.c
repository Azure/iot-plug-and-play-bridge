#pragma once

#include "common.h"

#include "DiscoveryInterface.h"
#include "PnPAdapterInterface.h"

#include <Windows.h>
#include <cfgmgr32.h>

#include <initguid.h>


PNP_INTERFACE_CLIENT_HANDLE pnpinterfaceHandle = NULL;
WCHAR* hardwareid = NULL;


void WindowsPnpSendEventCallback(PNP_SEND_TELEMETRY_STATUS pnpSendEventStatus, void* userContextCallback)
{
	LogInfo("WindowsPnpSendEventCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int Sample_SendEventAsync(char* eventName, char* data)
{
	int result;
	PNP_CLIENT_RESULT pnpClientResult;

	if (pnpinterfaceHandle == NULL) {
		return 0;
	}

	char msg[512];
	sprintf_s(msg, 512, "{\"%s\":\"%s\"}", eventName, data);

	if ((pnpClientResult = PnP_InterfaceClient_SendTelemetryAsync(pnpinterfaceHandle, eventName, (const unsigned char*)msg, strlen(msg), WindowsPnpSendEventCallback, NULL)) != PNP_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_SendEventAsync failed, result=%d\n", pnpClientResult);
		result = __FAILURE__;
	}
	else
	{
		result = 0;
	}

	return result;
}

volatile bool state = false;

DWORD
__stdcall
CoreDevice_OnDeviceNotification(
	_In_ HCMNOTIFICATION hNotify,
	_In_opt_ PVOID context,
	_In_ CM_NOTIFY_ACTION action,
	_In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
	_In_ DWORD eventDataSize)
{
	if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
		if (state && wcsstr(eventData->u.DeviceInterface.SymbolicLink, L"VID_045E") != NULL &&
			wcsstr(eventData->u.DeviceInterface.SymbolicLink, L"PID_0779") != NULL &&
			wcsstr(eventData->u.DeviceInterface.SymbolicLink, L"MI_00") != NULL) {
			state = false;
			LogInfo("device removed %S", eventData->u.DeviceInterface.SymbolicLink);
			Sample_SendEventAsync("camconn", "Disconnected");
		}
	}
	else if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) {
		if (!state && wcsstr(eventData->u.DeviceInterface.SymbolicLink, L"VID_045E") != NULL &&
			wcsstr(eventData->u.DeviceInterface.SymbolicLink, L"PID_0779") != NULL &&
			wcsstr(eventData->u.DeviceInterface.SymbolicLink, L"MI_00") != NULL) {
			state = true;
			LogInfo("device connected %S", eventData->u.DeviceInterface.SymbolicLink);
			Sample_SendEventAsync("camconn", "Connected");
		}
	}
	return 0;
}

HCMNOTIFICATION CoreDevice_hNotifyCtx = NULL;

int CoreDevice_BindPnpInterface(PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceClient, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
	DWORD cmRet;
	CM_NOTIFY_FILTER    cmFilter;

	JSON_Object* args = DeviceChangePayload->Message;

	if (pnpInterfaceClient == NULL) {
		return -1;
	}

	ZeroMemory(&cmFilter, sizeof(cmFilter));
	cmFilter.cbSize = sizeof(cmFilter);
	cmFilter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
	cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;

	cmRet = CM_Register_Notification(
		&cmFilter,                     
		NULL,                          
		CoreDevice_OnDeviceNotification,         
		&CoreDevice_hNotifyCtx                   
	);
	if (cmRet != CR_SUCCESS)
	{
		//Log::Error(String().Format(L"CM_Register_Notification failed, error %d", cmRet));
		return -1;
	}

	hardwareid = L"b94d388a-7331-4e5e-8b0f-08160ea1f706";//json_object_dotget_string(args, "HardwareId");

	pnpinterfaceHandle = PnpAdapter_GetPnpInterface(pnpInterfaceClient);
	state = true;
	Sample_SendEventAsync("camconn", "Connected");

	return 0;
}

// AssetTrackerNewDataSendEventCallback is invoked when an event has been processed by Azure IoT or else has failed.
void CoreDeviceNewDataSendEventCallback(PNP_CLIENT_RESULT pnpClientResult, void* userContextCallback)
{
	LogInfo("AssetTrackerNewDataSendEventCallback called, result=%d, userContextCallback=%p", pnpClientResult, userContextCallback);
}

// PnP_Sample_SendEventAsync demonstrates sending a PnP event to Azure IoT
int SendDeviceConnectedEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceCoreDevice)
{
	int result;
	const char* data = "DataToSend";
	PNP_CLIENT_RESULT pnpClientResult;


	if ((pnpClientResult = PnP_InterfaceClient_SendTelemetryAsync(pnpInterfaceCoreDevice, "NewData", (const unsigned char*)data, sizeof(data), CoreDeviceNewDataSendEventCallback, NULL)) != PNP_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_SendEventAsync failed, result=%d\n", pnpClientResult);
		result = __FAILURE__;
	}
	else
	{
		result = 0;
	}

	return result;
}


// PnP_Sample_SendEventAsync demonstrates sending a PnP event to Azure IoT
int SendDeviceDisconnectedEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceCoreDevice)
{
	int result;
	const char* data = "DataToSend";
	PNP_CLIENT_RESULT pnpClientResult;


	if ((pnpClientResult = PnP_InterfaceClient_SendTelemetryAsync(pnpInterfaceCoreDevice, "NewData", (const unsigned char*)data, sizeof(data), CoreDeviceNewDataSendEventCallback, NULL)) != PNP_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_SendEventAsync failed, result=%d\n", pnpClientResult);
		result = __FAILURE__;
	}
	else
	{
		result = 0;
	}

	return result;
}



//static IOTHUB_CLIENT_RESULT StartWorkerThreadIfNeeded(IOTHUB_CLIENT_CORE_INSTANCE* iotHubClientInstance)
//{
//	if (ThreadAPI_Create(&iotHubClientInstance->ThreadHandle, ScheduleWork_Thread, iotHubClientInstance) != THREADAPI_OK)
//	{
//		LogError("ThreadAPI_Create failed");
//		iotHubClientInstance->ThreadHandle = NULL;
//		result = IOTHUB_CLIENT_ERROR;
//	}
//
//	IOTHUB_CLIENT_RESULT result;
//	if (iotHubClientInstance->TransportHandle == NULL)
//	{
//		if (iotHubClientInstance->ThreadHandle == NULL)
//		{
//			iotHubClientInstance->StopThread = 0;
//			if (ThreadAPI_Create(&iotHubClientInstance->ThreadHandle, ScheduleWork_Thread, iotHubClientInstance) != THREADAPI_OK)
//			{
//				LogError("ThreadAPI_Create failed");
//				iotHubClientInstance->ThreadHandle = NULL;
//				result = IOTHUB_CLIENT_ERROR;
//			}
//			else
//			{
//				result = IOTHUB_CLIENT_OK;
//			}
//		}
//		else
//		{
//			result = IOTHUB_CLIENT_OK;
//		}
//	}
//	else
//	{
//		/*Codes_SRS_IOTHUBCLIENT_17_012: [ If the transport connection is shared, the thread shall be started by calling IoTHubTransport_StartWorkerThread. ]*/
//		/*Codes_SRS_IOTHUBCLIENT_17_011: [ If the transport connection is shared, the thread shall be started by calling IoTHubTransport_StartWorkerThread*/
//		result = IoTHubTransport_StartWorkerThread(iotHubClientInstance->TransportHandle, iotHubClientInstance, ScheduleWork_Thread_ForMultiplexing);
//	}
//	return result;
//}


PNP_INTERFACE_MODULE CoreDeviceHealthInterface = {
    .Identity = "core-device-health",
	.BindPnpInterface = CoreDevice_BindPnpInterface,
	.ReleaseInterface = NULL,
};