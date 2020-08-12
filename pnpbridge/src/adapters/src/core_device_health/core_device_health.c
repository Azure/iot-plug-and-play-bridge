// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <PnpBridge.h>
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

#include "parson.h"

#include "core_device_health.h"
#include <assert.h>

#define DEVICE_INTERFACE_CLASSES "device_interface_classes"
#define DEVICE_INSTANCE_HARDWARE_ID "hardware_id"

static const char coreDeviceHealth_DeviceStateProperty[] = "activestate";
static const char coreDeviceHealth_DeviceStateActive[] = "true";
static const char coreDeviceHealth_DeviceStateInactive[] = "false";

void CoreDevice_SetActive(
    int pnpReportedStatus,
    void* userContextCallback)
{
    LogInfo("CoreDevice_ReportProperty called, result=%d, userContextCallback=%p", pnpReportedStatus, userContextCallback);
    PCORE_DEVICE_TAG device = userContextCallback;
    device->DeviceActive = true;
}

void CoreDevice_SetInactive(
    int pnpReportedStatus,
    void* userContextCallback)
{
    LogInfo("CoreDevice_ReportProperty called, result=%d, userContextCallback=%p", pnpReportedStatus, userContextCallback);
    PCORE_DEVICE_TAG device = userContextCallback;
    device->DeviceActive = false;
}

// Sends a reported property from device to cloud.
IOTHUB_CLIENT_RESULT CoreDevice_ReportProperty(
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
    const char * ComponentName,
    const char * PropertyName,
    const char * PropertyValue,
    IOTHUB_CLIENT_REPORTED_STATE_CALLBACK ReportedStateCallback,
    void * UserContext)
{
    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;
    STRING_HANDLE jsonToSend = NULL;

    if ((jsonToSend = PnP_CreateReportedProperty(ComponentName, PropertyName, PropertyValue)) == NULL)
    {
        LogError("Core Device Health: Unable to build reported property response for propertyName=%s, propertyValue=%s", PropertyName, PropertyValue);
        iothubClientResult = IOTHUB_CLIENT_ERROR;
        goto exit;
    }
    else
    {
        const char* jsonToSendStr = STRING_c_str(jsonToSend);
        size_t jsonToSendStrLen = strlen(jsonToSendStr);

        if ((iothubClientResult = IoTHubDeviceClient_SendReportedState(DeviceClient, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
            ReportedStateCallback, UserContext)) != IOTHUB_CLIENT_OK)
        {
            LogError("Core Device Health: Unable to send reported state for property=%s, error=%d",
                                PropertyName, iothubClientResult);
            goto exit;
        }
        else
        {
            LogInfo("Core Device Health: Sending device information property to IoTHub. propertyName=%s, propertyValue=%s",
                        PropertyName, PropertyValue);
        }

        STRING_delete(jsonToSend);
    }
exit:
    return iothubClientResult;
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
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PCORE_DEVICE_TAG device = context;

    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(eventDataSize);

    if (action == CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED)
    {
        LogInfo("Core Device health: Device connected %S", eventData->u.DeviceInterface.SymbolicLink);

        CoreDevice_SendConnectionEventAsync(device, "DeviceStatus", "Connected");

        result = CoreDevice_ReportProperty(device->DeviceClient, device->ComponentName, coreDeviceHealth_DeviceStateProperty,
            coreDeviceHealth_DeviceStateActive, CoreDevice_SetActive, (void*)device);

        if (result != IOTHUB_CLIENT_OK)
        {
            LogError("CoreDevice_OnDeviceNotification: Reporting property=<%s> failed, error=<%d>", coreDeviceHealth_DeviceStateProperty, result);
        }
        else
        {
            LogInfo("CoreDevice_OnDeviceNotification: Queued async active state property for %s to true", coreDeviceHealth_DeviceStateProperty);
        }
    }
    else if (action == CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED)
    {
        LogInfo("Core Device health: Device removed %S", eventData->u.DeviceInterface.SymbolicLink);

        CoreDevice_SendConnectionEventAsync(device, "DeviceStatus", "Disconnected");

        result = CoreDevice_ReportProperty(device->DeviceClient, device->ComponentName, coreDeviceHealth_DeviceStateProperty,
            coreDeviceHealth_DeviceStateInactive, CoreDevice_SetInactive, (void*)device);

        if (result != IOTHUB_CLIENT_OK)
        {
            LogError("CoreDevice_OnDeviceNotification: Reporting property=<%s> failed, error=<%d>", coreDeviceHealth_DeviceStateProperty, result);
        }
        else
        {
            LogInfo("CoreDevice_OnDeviceNotification: Queued async active state property for %s to false", coreDeviceHealth_DeviceStateProperty);
        }
    }

    return 0;
}

void
CoreDevice_EventCallbackSent(
    IOTHUB_CLIENT_CONFIRMATION_RESULT pnpSendEventStatus,
    void* userContextCallback)
{
    LogInfo("CoreDevice_EventCallbackSent called, result=%d, userContextCallback=%p",
                pnpSendEventStatus, userContextCallback);
}

IOTHUB_CLIENT_RESULT
CoreDevice_SendConnectionEventAsync(
    PCORE_DEVICE_TAG DeviceContext,
    char* EventName,
    char* EventData)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    if (DeviceContext->TelemetryStarted)
    {
        IOTHUB_MESSAGE_HANDLE messageHandle = NULL;
        char msg[CONN_EVENT_SIZE] = { 0 };

        sprintf_s(msg, CONN_EVENT_SIZE, CONN_FORMAT, EventName, EventData);

        if ((messageHandle = PnP_CreateTelemetryMessageHandle(DeviceContext->ComponentName, msg)) == NULL)
        {
            LogError("Core Device Health: PnP_CreateTelemetryMessageHandle failed.");
        }
        else if ((result = IoTHubDeviceClient_SendEventAsync(DeviceContext->DeviceClient, messageHandle,
                CoreDevice_EventCallbackSent, (void*)EventName)) != IOTHUB_CLIENT_OK)
        {
            LogError("Core Device Health: IoTHubDeviceClient_SendEventAsync failed, error=%d", result);
        }

        IoTHubMessage_Destroy(messageHandle);
    }

    return result;
}

DWORD 
CoreDevice_DeviceNotification(
    HCMNOTIFICATION hNotify,
    _In_opt_ PVOID context,
    _In_ CM_NOTIFY_ACTION action,
    _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
    _In_ DWORD eventDataSize)
{
    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(eventDataSize);
    UNREFERENCED_PARAMETER(eventData);
    PCORE_DEVICE_ADAPTER_CONTEXT adapterContext = (PCORE_DEVICE_ADAPTER_CONTEXT)context;
    // New device discovered
    if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) {
        wchar_t* interfaceId = NULL;
        mallocAndStrcpy_s((char**)&interfaceId, (const char*) eventData->u.DeviceInterface.SymbolicLink);
        singlylinkedlist_add(adapterContext->SupportedInterfaces, interfaceId);
    }

    return 0;
}

IOTHUB_CLIENT_RESULT
CoreDevice_EnumerateDevices(
    _In_ GUID* InterfaceClassGuid,
    _Inout_ SINGLYLINKEDLIST_HANDLE SupportedInterfaces
)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    CONFIGRET cmret = CR_SUCCESS;
    ULONG cchInterfaceIds = 0;
    wchar_t* interfaceIds = NULL;

    cmret = CM_Get_Device_Interface_List_Size(
                        &cchInterfaceIds,
                        InterfaceClassGuid,
                        NULL,
                        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    interfaceIds = calloc(cchInterfaceIds, sizeof(wchar_t));

    cmret = CM_Get_Device_Interface_List(
                    InterfaceClassGuid,
                    NULL,
                    interfaceIds,
                    cchInterfaceIds,
                    CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    singlylinkedlist_add(SupportedInterfaces, interfaceIds);

    if (cmret == CR_FAILURE) {
        result = IOTHUB_CLIENT_ERROR;
        LogInfo("CoreDevice_EnumerateDevices: CM_Get_Device_Interface_List failed");
    }

    return result;
}

IOTHUB_CLIENT_RESULT CoreDevice_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    PCORE_DEVICE_ADAPTER_CONTEXT adapterContext = PnpAdapterHandleGetContext(AdapterHandle);
    if (adapterContext == NULL)
    {
        return IOTHUB_CLIENT_OK;
    }

    if (adapterContext->DeviceWatchers != NULL)
    {
        LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(adapterContext->DeviceWatchers);
        while (interfaceItem != NULL) {
            HCMNOTIFICATION hNotifyCtx = (HCMNOTIFICATION) singlylinkedlist_item_get_value(interfaceItem);
            CM_Unregister_Notification(hNotifyCtx);
            interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
        }

        singlylinkedlist_destroy(adapterContext->DeviceWatchers);
    }

    if (adapterContext->SupportedInterfaces)
    {
        LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(adapterContext->SupportedInterfaces);
        while (interfaceItem != NULL) {
            wchar_t* interfaceList = (wchar_t*) singlylinkedlist_item_get_value(interfaceItem);
            if (interfaceList)
            {
                free(interfaceList);
            }
            interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
        }

        singlylinkedlist_destroy(adapterContext->SupportedInterfaces);
    }

    free (adapterContext);
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT CoreDevice_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    CM_NOTIFY_FILTER cmFilter;
    HCMNOTIFICATION notifyHandle = NULL;
    DWORD cmRet;
    PCORE_DEVICE_ADAPTER_CONTEXT adapterContext = calloc(1, sizeof(CORE_DEVICE_ADAPTER_CONTEXT));
    if (NULL == adapterContext)
    {
        LogError("CoreDevice_CreatePnpAdapter: Could not allocate memory for device context.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    adapterContext->DeviceWatchers = singlylinkedlist_create();
    adapterContext->SupportedInterfaces = singlylinkedlist_create();

    if (NULL == adapterContext->DeviceWatchers || NULL == adapterContext->SupportedInterfaces)
    {
        LogError("CoreDevice_CreatePnpAdapter: Could not allocate device watcher or supported interfaces list");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    if (AdapterGlobalConfig == NULL)
    {
        LogError("CoreDevice_CreatePnpAdapter: Missing associated global parameters in config");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    JSON_Array* interfaceClasses = json_object_dotget_array(AdapterGlobalConfig, DEVICE_INTERFACE_CLASSES);

    for (int j = 0; j < (int) json_array_get_count(interfaceClasses); j++) {
        GUID guid = { 0 };
        const char *interfaceClass = json_array_get_string(interfaceClasses, j);
        if (UuidFromStringA((RPC_CSTR)interfaceClass, &guid) != RPC_S_OK) {
            LogError("CoreDevice_CreatePnpAdapter: UuidFromStringA failed");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        ZeroMemory(&cmFilter, sizeof(cmFilter));
        cmFilter.cbSize = sizeof(cmFilter);

        cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        cmFilter.u.DeviceInterface.ClassGuid = guid;
        cmRet = CM_Register_Notification(
                        &cmFilter,
                        adapterContext,
                        (PCM_NOTIFY_CALLBACK)CoreDevice_DeviceNotification,
                        &notifyHandle
                        );

        if (cmRet != CR_SUCCESS) {
            LogError("CoreDevice_CreatePnpAdapter: CM_Register_Notification failed");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        singlylinkedlist_add(adapterContext->DeviceWatchers, notifyHandle);

        result = CoreDevice_EnumerateDevices(&guid, adapterContext->SupportedInterfaces);
    }

    PnpAdapterHandleSetContext(AdapterHandle, (void*)adapterContext);

exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        result = CoreDevice_DestroyPnpAdapter(AdapterHandle);
    }
    return result;
}

static void CoreDevice_InterfaceRegisteredCallback(
    IOTHUB_CLIENT_RESULT dtInterfaceStatus,
    void* userInterfaceContext)
{
    UNREFERENCED_PARAMETER(userInterfaceContext);
    if (dtInterfaceStatus == IOTHUB_CLIENT_OK)
    {
        LogInfo("Core Device Health: Interface registered.");
    }
    else if (dtInterfaceStatus == IOTHUB_CLIENT_ERROR)
    {
        LogInfo("Core Device Health: Interface received unregistering callback.");
    }
    else
    {
        LogError("Core Device Health: Interface received failed, status=<%d>.", dtInterfaceStatus);
    }
}

static void CoreDevice_ProcessPropertyUpdate(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    void* userContextCallback)
{
    UNREFERENCED_PARAMETER(PnpComponentHandle);
    UNREFERENCED_PARAMETER(PropertyName);
    UNREFERENCED_PARAMETER(PropertyValue);
    UNREFERENCED_PARAMETER(version);
    UNREFERENCED_PARAMETER(userContextCallback);
    LogInfo("CoreDevice_ProcessPropertyUpdate called.");
}

static int CoreDevice_ProcessCommandUpdate(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    UNREFERENCED_PARAMETER(PnpComponentHandle);
    UNREFERENCED_PARAMETER(CommandName);
    UNREFERENCED_PARAMETER(CommandValue);
    UNREFERENCED_PARAMETER(CommandResponse);
    UNREFERENCED_PARAMETER(CommandResponseSize);
    LogInfo("CoreDevice_ProcessCommandUpdate called.");
    return PNP_STATUS_SUCCESS;
}

IOTHUB_CLIENT_RESULT CoreDevice_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PCORE_DEVICE_TAG deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);

    if (NULL == deviceContext) {
        return IOTHUB_CLIENT_OK;
    }

    if (NULL != deviceContext->NotifyHandle) {
        CM_Unregister_Notification(deviceContext->NotifyHandle);
    }

    if (NULL != deviceContext->SymbolicLink) {
        free(deviceContext->SymbolicLink);
    }

    if (NULL != deviceContext->ComponentName)
    {
        free(deviceContext->ComponentName);
    }

    free(deviceContext);

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT CoreDevice_FindMatchingDeviceInstance(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* HardwareId,
    char* SymbolicLink
)
{
    CONFIGRET cmResult;
    DEVINST devinst;
    DEVPROPTYPE devPropType;
    DEVPROPTYPE PropType;
    wchar_t deviceInstance[MAX_DEVICE_ID_LEN];
    ULONG deviceInstanceSize = MAX_DEVICE_ID_LEN * sizeof(wchar_t);
    wchar_t hardwareIds[MAX_DEVICE_ID_LEN] = { 0 };
    ULONG hardwareIdSize = MAX_DEVICE_ID_LEN * sizeof(wchar_t);
    bool symbolicLinkFound = false;
    PCORE_DEVICE_ADAPTER_CONTEXT adapterContext = PnpAdapterHandleGetContext(AdapterHandle);
    if (adapterContext == NULL)
    {
        return IOTHUB_CLIENT_ERROR;
    }

    if (adapterContext->SupportedInterfaces)
    {
        LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(adapterContext->SupportedInterfaces);
        while (interfaceItem != NULL) {
            wchar_t* interfaceList = (wchar_t*) singlylinkedlist_item_get_value(interfaceItem);
            if (interfaceList)
            {
                for (wchar_t* pInterface = interfaceList; L'\0' != *pInterface; pInterface += wcslen(pInterface) + 1)
                {
                    cmResult = CM_Get_Device_Interface_PropertyW(
                                    pInterface,
                                    &DEVPKEY_Device_InstanceId,
                                    &devPropType,
                                    (PBYTE)deviceInstance,
                                    &deviceInstanceSize,
                                    0);

                    if (CR_SUCCESS != cmResult)
                    {
                        continue;
                    }

                    cmResult = CM_Locate_DevNode(
                                    &devinst,
                                    deviceInstance,
                                    CM_LOCATE_DEVNODE_NORMAL);

                    if (CR_SUCCESS != cmResult)
                    {
                        continue;
                    }

                    cmResult = CM_Get_DevNode_PropertyW(
                                    devinst,
                                    &DEVPKEY_Device_HardwareIds,
                                    &PropType,
                                    (PBYTE)hardwareIds,
                                    &hardwareIdSize,
                                    0);
                    wchar_t * currentHardwareId = hardwareIds;
                    if (!strcmp((const char*)currentHardwareId, HardwareId))
                    {
                        mallocAndStrcpy_s(&SymbolicLink, (const char*) pInterface);
                        symbolicLinkFound = true;
                        break;
                    }

                }
            }
            interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
        }
    }

    if (symbolicLinkFound)
    {
        return IOTHUB_CLIENT_OK;
    }

    return IOTHUB_CLIENT_ERROR;
}

IOTHUB_CLIENT_RESULT
CoreDevice_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    CM_NOTIFY_FILTER cmFilter;
    CONFIGRET cmResult;
    PCORE_DEVICE_TAG deviceContext = calloc(1, sizeof(CORE_DEVICE_TAG));
    if (NULL == deviceContext)
    {
        LogError("Could not allocate memory for device context.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        BridgeComponentHandle = NULL;
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    const char* hardwareId = json_object_dotget_string(AdapterComponentConfig, DEVICE_INSTANCE_HARDWARE_ID);

    if (NULL == hardwareId)
    {
        LogError("Device adapter config needs to specify hardware ID for device.");
        BridgeComponentHandle = NULL;
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    deviceContext->DeviceActive = false;
    mallocAndStrcpy_s((char**)&deviceContext->ComponentName, ComponentName);

    result = CoreDevice_FindMatchingDeviceInstance(AdapterHandle, hardwareId, deviceContext->SymbolicLink);

    if (IOTHUB_CLIENT_OK != result)
    {
        LogError("Core Device Health: Couldn't find device with hardware ID %s", hardwareId);
        goto exit;
    }

    ZeroMemory(&cmFilter, sizeof(cmFilter));
    cmFilter.cbSize = sizeof(cmFilter);
    cmFilter.Flags = 0;
    cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE;
    wsprintf(cmFilter.u.DeviceInstance.InstanceId, L"%S", deviceContext->SymbolicLink);

    cmResult = CM_Register_Notification(
                &cmFilter,
                (void*)deviceContext,
                CoreDevice_OnDeviceNotification,
                &deviceContext->NotifyHandle);

    if (CR_SUCCESS != cmResult)
    {
        LogError("Core Device Health: CM_Register_Notification failed");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    PnpComponentHandleSetContext(BridgeComponentHandle, deviceContext);
    PnpComponentHandleSetPropertyUpdateCallback(BridgeComponentHandle, CoreDevice_ProcessPropertyUpdate);
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, CoreDevice_ProcessCommandUpdate);

exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        CoreDevice_DestroyPnpComponent(BridgeComponentHandle);
    }
    return result;

}

IOTHUB_CLIENT_RESULT CoreDevice_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    UNREFERENCED_PARAMETER(AdapterHandle);
    PCORE_DEVICE_TAG device = PnpComponentHandleGetContext(PnpComponentHandle);
    IOTHUB_DEVICE_CLIENT_HANDLE deviceHandle = PnpComponentHandleGetIotHubDeviceClient(PnpComponentHandle);
    device->DeviceClient = deviceHandle;
    device->TelemetryStarted = true;
    return CoreDevice_SendConnectionEventAsync(device, "DeviceStatus", "Connected");
}

IOTHUB_CLIENT_RESULT CoreDevice_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PCORE_DEVICE_TAG device = PnpComponentHandleGetContext(PnpComponentHandle);
    device->TelemetryStarted = false;
    return IOTHUB_CLIENT_OK;
}

PNP_ADAPTER CoreDeviceHealth = {
    .identity = "core-device-health",
    .createAdapter = CoreDevice_CreatePnpAdapter,
    .createPnpComponent = CoreDevice_CreatePnpComponent,
    .startPnpComponent = CoreDevice_StartPnpComponent,
    .stopPnpComponent = CoreDevice_StopPnpComponent,
    .destroyPnpComponent = CoreDevice_DestroyPnpComponent,
    .destroyAdapter = CoreDevice_DestroyPnpAdapter
};