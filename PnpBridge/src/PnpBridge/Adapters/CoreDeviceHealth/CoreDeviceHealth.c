// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <PnpBridge.h>

#include "azure_c_shared_utility/base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <initguid.h>

#include "parson.h"

#include "adapters/CoreDeviceHealth/CoreDeviceHealth.h"

void WindowsPnpSendEventCallback(PNP_SEND_TELEMETRY_STATUS pnpSendEventStatus, void* userContextCallback)
{
    LogInfo("WindowsPnpSendEventCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int Sample_SendEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpinterfaceHandle, char* eventName, char* data)
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

DWORD
__stdcall
CoreDevice_OnDeviceNotification(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ PVOID context,
    _In_ CM_NOTIFY_ACTION action,
    _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
    _In_ DWORD eventDataSize)
{
    PCORE_DEVICE_TAG device = context;

    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(eventDataSize);

    char buff[512];
    sprintf_s(buff, 512, "%S", eventData->u.DeviceInterface.SymbolicLink);

    if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
        if (device->state && stricmp(buff, device->symbolicLink)  == 0) {
            device->state = false;

            LogInfo("device removed %S", eventData->u.DeviceInterface.SymbolicLink);
            Sample_SendEventAsync(device->pnpinterfaceHandle, "DeviceStatus", "Disconnected");
        }
    }
    else if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) {
        if (!device->state && stricmp(buff, device->symbolicLink) == 0) {
            device->state = true;
            LogInfo("device connected %S", eventData->u.DeviceInterface.SymbolicLink);
            Sample_SendEventAsync(device->pnpinterfaceHandle, "DeviceStatus", "Connected");
        }
    }
    return 0;
}


int CoreDevice_ReleaseInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface) {
    PCORE_DEVICE_TAG device = PnpAdapterInterface_GetContext(pnpInterface);

    if (NULL != device) {
        if (NULL != device->hNotifyCtx) {
            CM_Unregister_Notification(device->hNotifyCtx);
        }
        if (NULL != device->symbolicLink) {
            free(device->symbolicLink);
        }
        free(device);
    }
    return 0;
}

int CoreDevice_CreatePnpInterface(PNPADAPTER_CONTEXT adapterHandle, PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD param) {
    DWORD cmRet;
    CM_NOTIFY_FILTER cmFilter;
    PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    JSON_Value* jvalue = json_parse_string(param->Message);
    JSON_Object* jmsg = json_value_get_object(jvalue);
    JSON_Object* args = jmsg;
    PCORE_DEVICE_TAG device = NULL;
    const char* interfaceId = json_object_get_string(args, "InterfaceId");
    //const char* hardwareId = json_object_get_string(args, "HardwareId");
    const char* symbolicLink = json_object_get_string(args, "SymbolicLink");
    const char* publishMode = json_object_get_string(args, "PublishMode");
    int result = 0;

    device = calloc(1, sizeof(CORE_DEVICE_TAG));
    if (NULL != device) {
        result = -1;
        goto end;
    }

    pnpInterfaceClient = PnP_InterfaceClient_Create(pnpDeviceClientHandle, interfaceId, NULL, NULL, NULL);
    if (NULL == pnpInterfaceClient) {
        result = -1;
        goto end;
    }

    // Create PnpAdapter Interface
    {
        PNPADPATER_INTERFACE_INIT_PARAMS interfaceParams = { 0 };
        interfaceParams.releaseInterface = CoreDevice_ReleaseInterface;
        PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;

        result = PnpAdapterInterface_Create(adapterHandle, interfaceId, pnpInterfaceClient, &pnpAdapterInterface, &interfaceParams);
        if (result < 0) {
            goto end;
        }
        PnpAdapterInterface_SetContext(pnpAdapterInterface, device);
    }

    // Don't bind the interface for publishMode always. There will be a device
    // arrived notification from the discovery adapter.
    if (NULL != publishMode && stricmp(publishMode, "always") == 0) {
        goto end;
    }

    device->symbolicLink = malloc(strlen(symbolicLink)+1);
    strcpy_s(device->symbolicLink, strlen(symbolicLink) + 1, symbolicLink);

    ZeroMemory(&cmFilter, sizeof(cmFilter));
    cmFilter.cbSize = sizeof(cmFilter);
    cmFilter.Flags = 0;
    cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    cmFilter.u.DeviceInterface.ClassGuid = *((GUID *)param->Context);

    cmRet = CM_Register_Notification(
        &cmFilter,
        (void*)device,
        CoreDevice_OnDeviceNotification,
        &device->hNotifyCtx
        );

    if (cmRet != CR_SUCCESS) {
        result = -1;
        goto end;
    }

    device->pnpinterfaceHandle = pnpInterfaceClient;
    device->state = true;

    Sample_SendEventAsync(device->pnpinterfaceHandle, "DeviceStatus", "Connected");
end:
    if (result < 0) {
        if (NULL != device) {
            if (NULL != device->symbolicLink) {
                free(device->symbolicLink);
            }
            free(device);
        }
    }
    return 0;
}

// AssetTrackerNewDataSendEventCallback is invoked when an event has been processed by Azure IoT or else has failed.
void CoreDeviceNewDataSendEventCallback(PNP_CLIENT_RESULT pnpClientResult, void* userContextCallback)
{
    LogInfo("CoreDeviceNewDataSendEventCallback called, result=%d, userContextCallback=%p", pnpClientResult, userContextCallback);
}

// SerialPnp_SendEventAsync demonstrates sending a PnP event to Azure IoT
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


// SerialPnp_SendEventAsync demonstrates sending a PnP event to Azure IoT
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

int CoreDevice_Initialize(const char* adapterArgs) {
    UNREFERENCED_PARAMETER(adapterArgs);
    return 0;
}

int CoreDevice_Shutdown() {
    return 0;
}

PNP_ADAPTER CoreDeviceHealthInterface = {
    .identity = "core-device-health",
    .initialize = CoreDevice_Initialize,
    .shutdown = CoreDevice_Shutdown,
    .createPnpInterface = CoreDevice_CreatePnpInterface,
};