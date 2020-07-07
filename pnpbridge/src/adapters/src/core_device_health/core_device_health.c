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
static const unsigned char coreDeviceHealth_DeviceStateActive[] = "true";
static const unsigned char coreDeviceHealth_DeviceStateInactive[] = "false";

void CoreDevice_SetActive(
    DIGITALTWIN_CLIENT_RESULT pnpReportedStatus,
    void* userContextCallback)
{
    LogInfo("CoreDevice_ReportPropertyUpdatedCallback called, result=%d, userContextCallback=%p", pnpReportedStatus, userContextCallback);
    PCORE_DEVICE_TAG device = userContextCallback;
    device->DeviceActive = true;
}

void CoreDevice_SetInactive(
    DIGITALTWIN_CLIENT_RESULT pnpReportedStatus,
    void* userContextCallback)
{
    LogInfo("CoreDevice_ReportPropertyUpdatedCallback called, result=%d, userContextCallback=%p", pnpReportedStatus, userContextCallback);
    PCORE_DEVICE_TAG device = userContextCallback;
    device->DeviceActive = false;
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
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    PCORE_DEVICE_TAG device = context;

    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(eventDataSize);

    if (action == CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED) {
        LogInfo("device connected %S", eventData->u.DeviceInterface.SymbolicLink);
        CoreDevice_SendConnectionEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Connected");
        result = DigitalTwin_InterfaceClient_ReportPropertyAsync(device->DigitalTwinInterface, coreDeviceHealth_DeviceStateProperty, coreDeviceHealth_DeviceStateActive,
        sizeof((char*)coreDeviceHealth_DeviceStateActive), NULL, CoreDevice_SetActive, (void*)device);
        if (result != DIGITALTWIN_CLIENT_OK)
        {
            LogError("CoreDevice_OnDeviceNotification: Reporting property=<%s> failed, error=<%d>", coreDeviceHealth_DeviceStateProperty, result);
        }
        else
        {
            LogInfo("CoreDevice_OnDeviceNotification: Queued async active state property for %s to true", coreDeviceHealth_DeviceStateProperty);
        }
    }
    else if (action == CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED) {
        
        LogInfo("device removed %S", eventData->u.DeviceInterface.SymbolicLink);
        CoreDevice_SendConnectionEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Disconnected");
        result = DigitalTwin_InterfaceClient_ReportPropertyAsync(device->DigitalTwinInterface, coreDeviceHealth_DeviceStateProperty, coreDeviceHealth_DeviceStateInactive,
        strlen((char*)coreDeviceHealth_DeviceStateInactive), NULL, CoreDevice_SetInactive, (void*)device);
        if (result != DIGITALTWIN_CLIENT_OK)
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
    DIGITALTWIN_CLIENT_RESULT pnpSendEventStatus,
    void* userContextCallback)
{
    LogInfo("CoreDevice_EventCallbackSent called, result=%d, userContextCallback=%p",
                pnpSendEventStatus, userContextCallback);
}

int
CoreDevice_SendConnectionEventAsync(
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinInterface,
    char* EventName,
    char* EventData)
{
    int result = 0;
    char msg[CONN_EVENT_SIZE] = { 0 };
    DIGITALTWIN_CLIENT_RESULT dtResult;

    sprintf_s(msg, CONN_EVENT_SIZE, CONN_FORMAT, EventName, EventData);

    dtResult = DigitalTwin_InterfaceClient_SendTelemetryAsync(
                    DigitalTwinInterface, 
                    (unsigned char*) msg,
                    strlen(msg),
                    CoreDevice_EventCallbackSent,
                    (void*)EventName);
    if (DIGITALTWIN_CLIENT_OK != dtResult) {
        LogError("CoreDevice_SendEventAsync failed, result=%d\n", dtResult);
        result = -1;
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

DIGITALTWIN_CLIENT_RESULT
CoreDevice_EnumerateDevices(
    _In_ GUID* InterfaceClassGuid,
    _Inout_ SINGLYLINKEDLIST_HANDLE SupportedInterfaces
)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
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
        result = DIGITALTWIN_CLIENT_ERROR;
        LogInfo("CoreDevice_EnumerateDevices: CM_Get_Device_Interface_List failed");
    }

    return result;
}

DIGITALTWIN_CLIENT_RESULT CoreDevice_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    PCORE_DEVICE_ADAPTER_CONTEXT adapterContext = PnpAdapterHandleGetContext(AdapterHandle);
    if (adapterContext == NULL)
    {
        return DIGITALTWIN_CLIENT_OK;
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
    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT CoreDevice_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    CM_NOTIFY_FILTER cmFilter;
    HCMNOTIFICATION notifyHandle = NULL;
    DWORD cmRet;
    PCORE_DEVICE_ADAPTER_CONTEXT adapterContext = calloc(1, sizeof(CORE_DEVICE_ADAPTER_CONTEXT));
    if (NULL == adapterContext)
    {
        LogError("CoreDevice_CreatePnpAdapter: Could not allocate memory for device context.");
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    adapterContext->DeviceWatchers = singlylinkedlist_create();
    adapterContext->SupportedInterfaces = singlylinkedlist_create();

    if (NULL == adapterContext->DeviceWatchers || NULL == adapterContext->SupportedInterfaces)
    {
        LogError("CoreDevice_CreatePnpAdapter: Could not allocate device watcher or supported interfaces list");
        result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    if (AdapterGlobalConfig == NULL)
    {
        LogError("CoreDevice_CreatePnpAdapter: Missing associated global parameters in config");
        result = DIGITALTWIN_CLIENT_ERROR;
        goto exit;
    }

    JSON_Array* interfaceClasses = json_object_dotget_array(AdapterGlobalConfig, DEVICE_INTERFACE_CLASSES);

    for (int j = 0; j < (int) json_array_get_count(interfaceClasses); j++) {
        GUID guid = { 0 };
        const char *interfaceClass = json_array_get_string(interfaceClasses, j);
        if (UuidFromStringA((RPC_CSTR)interfaceClass, &guid) != RPC_S_OK) {
            LogError("CoreDevice_CreatePnpAdapter: UuidFromStringA failed");
            result = DIGITALTWIN_CLIENT_ERROR;
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
            result = DIGITALTWIN_CLIENT_ERROR;
            goto exit;
        }

        singlylinkedlist_add(adapterContext->DeviceWatchers, notifyHandle);

        result = CoreDevice_EnumerateDevices(&guid, adapterContext->SupportedInterfaces);
    }

    PnpAdapterHandleSetContext(AdapterHandle, (void*)adapterContext);

exit:
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        result = CoreDevice_DestroyPnpAdapter(AdapterHandle);
    }
    return result;
}

static void CoreDevice_InterfaceRegisteredCallback(
    DIGITALTWIN_CLIENT_RESULT dtInterfaceStatus,
    void* userInterfaceContext)
{
    UNREFERENCED_PARAMETER(userInterfaceContext);
    if (dtInterfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        LogInfo("Core Device Health: Interface registered.");
    }
    else if (dtInterfaceStatus == DIGITALTWIN_CLIENT_ERROR_INTERFACE_UNREGISTERING)
    {
        LogInfo("Core Device Health: Interface received unregistering callback.");
    }
    else
    {
        LogError("Core Device Health: Interface received failed, status=<%d>.", dtInterfaceStatus);
    }
}

static void CoreDevice_ProcessPropertyUpdate(
    const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate,
    void* userInterfaceContext)
{
    UNREFERENCED_PARAMETER(dtClientPropertyUpdate);
    UNREFERENCED_PARAMETER(userInterfaceContext);
    LogInfo("CoreDevice_ProcessPropertyUpdate called.");
}

static void CoreDevice_ProcessCommandUpdate(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtCommandRequest,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse,
    void* userInterfaceContext)
{
    UNREFERENCED_PARAMETER(dtCommandRequest);
    UNREFERENCED_PARAMETER(dtCommandResponse);
    UNREFERENCED_PARAMETER(userInterfaceContext);
    LogInfo("CoreDevice_ProcessCommandUpdate called.");
}

DIGITALTWIN_CLIENT_RESULT CoreDevice_DestroyPnpInterface(
    PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle)
{
    PCORE_DEVICE_TAG deviceContext = PnpInterfaceHandleGetContext(PnpInterfaceHandle);

    if (NULL == deviceContext) {
        return DIGITALTWIN_CLIENT_OK;
    }

    if (NULL != deviceContext->DigitalTwinInterface) {
        DigitalTwin_InterfaceClient_Destroy(deviceContext->DigitalTwinInterface);
    }

    if (NULL != deviceContext->NotifyHandle) {
        CM_Unregister_Notification(deviceContext->NotifyHandle);
    }

    if (NULL != deviceContext->SymbolicLink) {
        free(deviceContext->SymbolicLink);
    }

    free(deviceContext);

    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT CoreDevice_FindMatchingDeviceInstance(
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
        return DIGITALTWIN_CLIENT_ERROR;
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
        return DIGITALTWIN_CLIENT_OK;
    }

    return DIGITALTWIN_CLIENT_ERROR;
}

DIGITALTWIN_CLIENT_RESULT
CoreDevice_CreatePnpInterface(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName,
    const JSON_Object* AdapterInterfaceConfig,
    PNPBRIDGE_INTERFACE_HANDLE BridgeInterfaceHandle,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* PnpInterfaceClient)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    CM_NOTIFY_FILTER cmFilter;
    CONFIGRET cmResult;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    PCORE_DEVICE_TAG deviceContext = calloc(1, sizeof(CORE_DEVICE_TAG));
    if (NULL == deviceContext)
    {
        LogError("Could not allocate memory for device context.");
        return DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
    }

    const char* hardwareId = json_object_dotget_string(AdapterInterfaceConfig, DEVICE_INSTANCE_HARDWARE_ID);

    if (NULL == hardwareId)
    {
        LogError("Device adapter config needs to specify hardware ID for device.");
        return DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
    }

    deviceContext->DeviceActive = false;
    result = DigitalTwin_InterfaceClient_Create(ComponentName,
                CoreDevice_InterfaceRegisteredCallback, (void*)deviceContext, &pnpInterfaceClient);

    if (DIGITALTWIN_CLIENT_OK != result)
    {
        LogError("Core Device Health: Error registering pnp interface component %s", ComponentName);
        goto exit;
    }
    else if ((result = DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback(
                                pnpInterfaceClient,
                                CoreDevice_ProcessPropertyUpdate,
                                (void*)&deviceContext)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("Core Device Health: DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback failed. error=<%d>", result);
        goto exit;
    }
    else if ((result = DigitalTwin_InterfaceClient_SetCommandsCallback(
                                pnpInterfaceClient,
                                CoreDevice_ProcessCommandUpdate,
                                (void*)&deviceContext)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("Core Device Health: DigitalTwin_InterfaceClient_SetCommandsCallback failed. error=<%d>", result);
        goto exit;
    }

    result = CoreDevice_FindMatchingDeviceInstance(AdapterHandle, hardwareId, deviceContext->SymbolicLink);

    if (DIGITALTWIN_CLIENT_OK != result)
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
        result = DIGITALTWIN_CLIENT_ERROR;
        goto exit;
    }

    *PnpInterfaceClient = pnpInterfaceClient;
    deviceContext->DigitalTwinInterface = pnpInterfaceClient;

    PnpInterfaceHandleSetContext(BridgeInterfaceHandle, deviceContext);

exit:
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        CoreDevice_DestroyPnpInterface(BridgeInterfaceHandle);
    }
    return result;

}

DIGITALTWIN_CLIENT_RESULT CoreDevice_StartPnpInterface(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle)
{
    UNREFERENCED_PARAMETER(AdapterHandle);
    PCORE_DEVICE_TAG device = PnpInterfaceHandleGetContext(PnpInterfaceHandle);
    CoreDevice_SendConnectionEventAsync(device->DigitalTwinInterface, "DeviceStatus", "Connected");

    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT CoreDevice_StopPnpInterface(
    PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle)
{
    UNREFERENCED_PARAMETER(PnpInterfaceHandle);
    return DIGITALTWIN_CLIENT_OK;
}

PNP_ADAPTER CoreDeviceHealth = {
    .identity = "core-device-health",
    .createAdapter = CoreDevice_CreatePnpAdapter,
    .createPnpInterface = CoreDevice_CreatePnpInterface,
    .startPnpInterface = CoreDevice_StartPnpInterface,
    .stopPnpInterface = CoreDevice_StopPnpInterface,
    .destroyPnpInterface = CoreDevice_DestroyPnpInterface,
    .destroyAdapter = CoreDevice_DestroyPnpAdapter
};