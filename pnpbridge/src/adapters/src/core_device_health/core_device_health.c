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

#include "core_device_health.h"
#include <assert.h>

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

    if (action == CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED) {
        LogInfo("device removed %S", eventData->u.DeviceInterface.SymbolicLink);
        CoreDevice_SendConnectionEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Disconnected");
    }
    else if (action == CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED) {
        LogInfo("device connected %S", eventData->u.DeviceInterface.SymbolicLink);
        CoreDevice_SendConnectionEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Connected");
    }

    return 0;
}

void
CoreDevice_EventCallbackSent(
    DIGITALTWIN_CLIENT_RESULT pnpSendEventStatus,
    void* userContextCallback
    )
{
    LogInfo("CoreDevice_EventCallbackSent called, result=%d, userContextCallback=%p",
                pnpSendEventStatus, userContextCallback);
}

int
CoreDevice_SendConnectionEventAsync(
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinInterface,
    char* EventName,
    char* EventData
    )
{
    int result = 0;
    char msg[CONN_EVENT_SIZE] = { 0 };
    DIGITALTWIN_CLIENT_RESULT dtResult;

    sprintf_s(msg, CONN_EVENT_SIZE, CONN_FORMAT, EventName, EventData);

    dtResult = DigitalTwin_InterfaceClient_SendTelemetryAsync(
                        DigitalTwinInterface, EventName, (const char*)msg,
                        CoreDevice_EventCallbackSent, NULL);
    if (DIGITALTWIN_CLIENT_OK != dtResult) {
        LogError("CoreDevice_SendEventAsync failed, result=%d\n", dtResult);
        result = -1;
    }

    return result;
}

void
CoreDevice_ReleaseInterfaceImp(
    PCORE_DEVICE_TAG Device
    )
{
    assert(NULL != Device);

    if (NULL != Device->DigitalTwinInterface) {
        DigitalTwin_InterfaceClient_Destroy(Device->DigitalTwinInterface);
    }

    if (NULL != Device->NotifyHandle) {
        CM_Unregister_Notification(Device->NotifyHandle);
    }

    if (NULL != Device->SymbolicLink) {
        free(Device->SymbolicLink);
    }

    free(Device);
}

int 
CoreDevice_ReleaseInterface(
    PNPADAPTER_INTERFACE_HANDLE PnpInterface
    )
{
    PCORE_DEVICE_TAG device = NULL;
    
    device = (PCORE_DEVICE_TAG) PnpAdapterInterface_GetContext(PnpInterface);
    CoreDevice_ReleaseInterfaceImp(device);

    return 0;
}

int
CoreDevice_StartInterface(
    PNPADAPTER_INTERFACE_HANDLE pnpInterface
    )
{
    PCORE_DEVICE_TAG device = NULL;

    device = (PCORE_DEVICE_TAG) PnpAdapterInterface_GetContext(pnpInterface);
    CoreDevice_SendConnectionEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Connected");

    return 0;
}

int 
CoreDevice_CreatePnpInterface(
    PNPADAPTER_CONTEXT adapterHandle,
    PNPMESSAGE param
    )
{
    DWORD cmRet;
    int result = 0;
    CM_NOTIFY_FILTER cmFilter;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    DIGITALTWIN_CLIENT_RESULT dtres;
    PCORE_DEVICE_TAG device = NULL;
    JSON_Value* jvalue = json_parse_string(PnpMessage_GetMessage(param));
    JSON_Object* jmsg = json_value_get_object(jvalue);
    JSON_Object* args = jmsg;
    const char* interfaceId = PnpMessage_GetInterfaceId(param);
    const char* symbolicLink = json_object_get_string(args, "symbolic_link");
    const char* publishMode = json_object_get_string(args, "PublishMode");

    device = calloc(1, sizeof(CORE_DEVICE_TAG));
    if (NULL == device) {
        result = -1;
        goto exit;
    }

    dtres = DigitalTwin_InterfaceClient_Create(interfaceId, 
                NULL, NULL, &pnpInterfaceClient);
    if (DIGITALTWIN_CLIENT_OK != dtres) {
        result = -1;
        goto exit;
    }

    // Create PnpAdapter Interface
    {
        PNPADPATER_INTERFACE_PARAMS interfaceParams = { 0 };
        PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;
        PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, adapterHandle, pnpInterfaceClient);

        interfaceParams.StartInterface = CoreDevice_StartInterface;
        interfaceParams.ReleaseInterface = CoreDevice_ReleaseInterface;
        interfaceParams.InterfaceId = (char*)interfaceId;

        result = PnpAdapterInterface_Create(&interfaceParams, &pnpAdapterInterface);
        if (result < 0) {
            goto exit;
        }

        PnpAdapterInterface_SetContext(pnpAdapterInterface, device);
    }

    // Don't bind the interface for publishMode always. There will be a device
    // arrived notification from the discovery adapter.
    if (NULL != publishMode && stricmp(publishMode, "always") == 0) {
        result = -1;
        goto exit;
    }

    device->SymbolicLink = malloc(strlen(symbolicLink)+1);
    strcpy_s(device->SymbolicLink, strlen(symbolicLink) + 1, symbolicLink);

    ZeroMemory(&cmFilter, sizeof(cmFilter));
    cmFilter.cbSize = sizeof(cmFilter);
    cmFilter.Flags = 0;
    cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE;
    wsprintf(cmFilter.u.DeviceInstance.InstanceId, L"%S", device->SymbolicLink);

    cmRet = CM_Register_Notification(
                &cmFilter,
                (void*)device,
                CoreDevice_OnDeviceNotification,
                &device->NotifyHandle);

    if (CR_SUCCESS != cmRet) {
        result = -1;
        goto exit;
    }

    device->DigitalTwinInterface = pnpInterfaceClient;

exit:
    if (result < 0) {
        if (NULL != device) {
            CoreDevice_ReleaseInterfaceImp(device);
        }
    }

    return result;
}

int 
CoreDevice_Initialize(
    const char* AdapterArgs
    )
{
    UNREFERENCED_PARAMETER(AdapterArgs);
    return 0;
}

int 
CoreDevice_Shutdown() {
    return 0;
}

PNP_ADAPTER CoreDeviceHealth = {
    .identity = "core-device-health",
    .initialize = CoreDevice_Initialize,
    .shutdown = CoreDevice_Shutdown,
    .createPnpInterface = CoreDevice_CreatePnpInterface,
};