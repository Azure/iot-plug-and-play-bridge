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

#include "CoreDeviceHealth.h"

void CoreDevice_EventCallbackSent(DIGITALTWIN_CLIENT_RESULT pnpSendEventStatus, void* userContextCallback)
{
    LogInfo("WindowsPnpSendEventCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int CoreDevice_SendEventAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpinterfaceHandle, char* eventName, char* data)
{
    int result;
    DIGITALTWIN_CLIENT_RESULT pnpClientResult;

    if (pnpinterfaceHandle == NULL) {
        return 0;
    }

    char msg[512];
    sprintf_s(msg, 512, "{\"%s\":\"%s\"}", eventName, data);

    if ((pnpClientResult = DigitalTwin_InterfaceClient_SendTelemetryAsync(pnpinterfaceHandle, eventName, (const char*)msg, CoreDevice_EventCallbackSent, NULL)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("PnP_InterfaceClient_SendEventAsync failed, result=%d\n", pnpClientResult);
        result = -1;// __FAILURE__;
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
        if (device->state && stricmp(buff, device->SymbolicLink)  == 0) {
            device->state = false;

            LogInfo("device removed %S", eventData->u.DeviceInterface.SymbolicLink);
            CoreDevice_SendEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Disconnected");
        }
    }
    else if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) {
        if (!device->state && stricmp(buff, device->SymbolicLink) == 0) {
            device->state = true;
            LogInfo("device connected %S", eventData->u.DeviceInterface.SymbolicLink);
            CoreDevice_SendEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Connected");
        }
    }
    return 0;
}


int CoreDevice_ReleaseInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface) {
    PCORE_DEVICE_TAG device = PnpAdapterInterface_GetContext(pnpInterface);

    if (NULL != device) {
        if (NULL != device->NotifyHandle) {
            CM_Unregister_Notification(device->NotifyHandle);
        }
        if (NULL != device->SymbolicLink) {
            free(device->SymbolicLink);
        }
        free(device);
    }
    return 0;
}

int CoreDevice_CreatePnpInterface(PNPADAPTER_CONTEXT adapterHandle, PNPMESSAGE param) {
    DWORD cmRet;
    CM_NOTIFY_FILTER cmFilter;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    JSON_Value* jvalue = json_parse_string(PnpMessage_GetMessage(param));
    JSON_Object* jmsg = json_value_get_object(jvalue);
    JSON_Object* args = jmsg;
    PCORE_DEVICE_TAG device = NULL;
    const char* interfaceId = json_object_get_string(args, "InterfaceId");
    //const char* hardwareId = json_object_get_string(args, "HardwareId");
    const char* symbolicLink = json_object_get_string(args, "SymbolicLink");
    const char* publishMode = json_object_get_string(args, "PublishMode");
    int result = 0;
    PNPMESSAGE_PROPERTIES* props = PnpMessage_AccessProperties(param);
    DIGITALTWIN_CLIENT_RESULT dtres;

    device = calloc(1, sizeof(CORE_DEVICE_TAG));
    if (NULL != device) {
        result = -1;
        goto end;
    }

    dtres = DigitalTwin_InterfaceClient_Create(interfaceId, NULL, NULL, NULL, &pnpInterfaceClient);
    if (DIGITALTWIN_CLIENT_OK != dtres) {
        result = -1;
        goto end;
    }

    // Create PnpAdapter Interface
    {
        PNPADPATER_INTERFACE_PARAMS interfaceParams = { 0 };
        PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;
        PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, adapterHandle, pnpInterfaceClient);

        interfaceParams.ReleaseInterface = CoreDevice_ReleaseInterface;
        interfaceParams.InterfaceId = (char*)interfaceId;

        result = PnpAdapterInterface_Create(&interfaceParams, &pnpAdapterInterface);
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

    device->SymbolicLink = malloc(strlen(symbolicLink)+1);
    strcpy_s(device->SymbolicLink, strlen(symbolicLink) + 1, symbolicLink);

    ZeroMemory(&cmFilter, sizeof(cmFilter));
    cmFilter.cbSize = sizeof(cmFilter);
    cmFilter.Flags = 0;
    cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    cmFilter.u.DeviceInterface.ClassGuid = *((GUID *)props->Context);

    cmRet = CM_Register_Notification(
        &cmFilter,
        (void*)device,
        CoreDevice_OnDeviceNotification,
        &device->NotifyHandle
        );

    if (cmRet != CR_SUCCESS) {
        result = -1;
        goto end;
    }

    device->DigitalTwinInterface = pnpInterfaceClient;
    device->state = true;

    CoreDevice_SendEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Connected");
end:
    if (result < 0) {
        if (NULL != device) {
            if (NULL != device->SymbolicLink) {
                free(device->SymbolicLink);
            }
            free(device);
        }
    }
    return 0;
}

// AssetTrackerNewDataSendEventCallback is invoked when an event has been processed by Azure IoT or else has failed.
void CoreDeviceNewDataSendEventCallback(DIGITALTWIN_CLIENT_RESULT pnpClientResult, void* userContextCallback)
{
    LogInfo("CoreDeviceNewDataSendEventCallback called, result=%d, userContextCallback=%p", pnpClientResult, userContextCallback);
}

// SerialPnp_SendEventAsync demonstrates sending a PnP event to Azure IoT
int SendDeviceConnectedEventAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceCoreDevice)
{
    int result;
    const char* data = "DataToSend";
    DIGITALTWIN_CLIENT_RESULT pnpClientResult;


    if ((pnpClientResult = DigitalTwin_InterfaceClient_SendTelemetryAsync(pnpInterfaceCoreDevice, "NewData", (const char*)data, CoreDeviceNewDataSendEventCallback, NULL)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("DigitalTwin_InterfaceClient_SendTelemetryAsync failed, result=%d\n", pnpClientResult);
        result = -1;// __FAILURE__;
    }
    else
    {
        result = 0;
    }

    return result;
}


// SerialPnp_SendEventAsync demonstrates sending a PnP event to Azure IoT
int SendDeviceDisconnectedEventAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceCoreDevice)
{
    int result;
    const char* data = "DataToSend";
    DIGITALTWIN_CLIENT_RESULT pnpClientResult;


    if ((pnpClientResult = DigitalTwin_InterfaceClient_SendTelemetryAsync(pnpInterfaceCoreDevice, "NewData", (const char*)data, CoreDeviceNewDataSendEventCallback, NULL)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("PnP_InterfaceClient_SendEventAsync failed, result=%d\n", pnpClientResult);
        result = -1;// __FAILURE__;
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