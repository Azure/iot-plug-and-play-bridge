// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <PnpBridge.h>
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>

#include "azure_c_shared_utility/base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

#include "parson.h"

#define DEVICE_INTERFACE_CLASSES "device_interface_classes"

const char* CoreDeviceDiscovery_PnpMessageformat =
                        "{ \
                           \"identity\": \"core-device-discovery\", \
                           \"match_parameters\": { \
                               \"hardware_id\":%s \
                           }, \
                           \"symbolic_link\":%s \
                         }";

SINGLYLINKEDLIST_HANDLE g_CoreDeviceDiscovery_DeviceWatchers;

void
CoreDeviceDiscovery_NotifyPnpBridge(
    wchar_t* SymbolicLink
    ) 
{
    CONFIGRET cmret;
    DEVINST Devinst;
    DEVPROPTYPE PropType;
    wchar_t* hardwareId = NULL;
    char *msg = NULL;
    size_t msgLen = 0;
    PNPMESSAGE payload = NULL; 
    STRING_HANDLE sHardwareId = NULL;
    STRING_HANDLE sSymbolicLink = NULL;
    DEVPROPTYPE devPropType;
    wchar_t hardwareIds[MAX_DEVICE_ID_LEN] = { 0 };
    wchar_t deviceInstance[MAX_DEVICE_ID_LEN];
    ULONG deviceInstanceSize = MAX_DEVICE_ID_LEN * sizeof(wchar_t);
    ULONG hardwareIdSize = MAX_DEVICE_ID_LEN * sizeof(wchar_t);
    char hardwareIdA[MAX_DEVICE_ID_LEN] = { 0 };
    char symbolicLinkA[MAX_DEVICE_ID_LEN] = { 0 };

    cmret = CM_Get_Device_Interface_PropertyW(
                SymbolicLink,
                &DEVPKEY_Device_InstanceId,
                &devPropType,
                (PBYTE)deviceInstance,
                &deviceInstanceSize,
                0);

    if (CR_SUCCESS != cmret) {
        return;
    }

    cmret = CM_Locate_DevNode(
                &Devinst,
                deviceInstance,
                CM_LOCATE_DEVNODE_NORMAL);

    if (CR_SUCCESS != cmret) {
        goto exit;
    }

    cmret = CM_Get_DevNode_PropertyW(
                Devinst,
                &DEVPKEY_Device_HardwareIds,
                &PropType,
                (PBYTE)hardwareIds,
                &hardwareIdSize,
                0);
    if (CR_SUCCESS != cmret) {
        goto exit;
    }

    hardwareId = hardwareIds;

    sprintf_s(hardwareIdA, MAX_DEVICE_ID_LEN, "%S", hardwareId);
    sprintf_s(symbolicLinkA, MAX_DEVICE_ID_LEN, "%S", deviceInstance);

    sHardwareId = STRING_new_JSON(hardwareIdA);
    if (NULL == sHardwareId) {
        goto exit;
    }

    sSymbolicLink = STRING_new_JSON(symbolicLinkA);
    if (NULL == sSymbolicLink) {
        goto exit;
    }

    msgLen = strlen(CoreDeviceDiscovery_PnpMessageformat) +
                 strlen(STRING_c_str(sHardwareId)) + strlen(STRING_c_str(sSymbolicLink)) + 1;

    msg = malloc(msgLen * sizeof(char));
    
    sprintf_s(msg, msgLen, CoreDeviceDiscovery_PnpMessageformat, 
                STRING_c_str(sHardwareId), STRING_c_str(sSymbolicLink));

    PnpMessage_CreateMessage(&payload);
    PnpMessage_SetMessage(payload, msg);

    PnpMessage_AccessProperties(payload)->ChangeType = PNPMESSAGE_CHANGE_ARRIVAL;

    DiscoveryAdapter_ReportDevice(payload);

    PnpMemory_ReleaseReference(payload);

exit:
    if (sHardwareId) {
        STRING_delete(sHardwareId);
    }

    if (sSymbolicLink) {
        STRING_delete(sSymbolicLink);
    }

    if (msg) {
        free(msg);
    }
}

DWORD 
CoreDeviceDiscovery_DeviceNotification(
    HCMNOTIFICATION hNotify,
    _In_opt_ PVOID context,
    _In_ CM_NOTIFY_ACTION action,
    _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
    _In_ DWORD eventDataSize)
{
    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(eventDataSize);
    UNREFERENCED_PARAMETER(context);

    // New device discovered
    if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) {
        CoreDeviceDiscovery_NotifyPnpBridge(eventData->u.DeviceInterface.SymbolicLink);
    }

    return 0;
}

    
HRESULT
CoreDeviceDiscovery_Enumerate(
    _In_ GUID* InterfaceClassGuid
    )
{
    CONFIGRET cmret = CR_SUCCESS;
    ULONG cchInterfaceIds = 0;
    wchar_t* interfaceIds = NULL;
    wchar_t* interfaceIdDefault = NULL;

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

    interfaceIdDefault = interfaceIds;
    for (wchar_t* pInterface = interfaceIds; L'\0' != *pInterface;
            pInterface += wcslen(pInterface) + 1)
    {
        CoreDeviceDiscovery_NotifyPnpBridge(pInterface);
    }

    if (interfaceIds) {
        free(interfaceIds);
    }

    return S_OK;
}

int 
CoreDevice_StartDiscovery(
    const PNPMEMORY DeviceArgs,
    const PNPMEMORY AdapterArgs
    )
{
    DWORD cmRet;
    CM_NOTIFY_FILTER cmFilter;
    HCMNOTIFICATION notifyHandle = NULL;
    JSON_Value* jmsg;
    JSON_Object* jobj;
    char* adapterArgsJson = NULL;
    JSON_Array* interfaceClasses = NULL;

    UNREFERENCED_PARAMETER(DeviceArgs);

    if (NULL == AdapterArgs) {
        LogError("Missing adapter parameters");
        return -1;
    }

    g_CoreDeviceDiscovery_DeviceWatchers = singlylinkedlist_create();
    if (NULL == g_CoreDeviceDiscovery_DeviceWatchers) {
        LogError("Failed to allocate device watcher list");
        return -1;
    }

    adapterArgsJson = PnpMemory_GetBuffer(AdapterArgs, NULL);

    jmsg = json_parse_string(adapterArgsJson);
    jobj = json_value_get_object(jmsg);

    interfaceClasses =  json_object_dotget_array(jobj, DEVICE_INTERFACE_CLASSES);

    for (int j = 0; j < (int) json_array_get_count(interfaceClasses); j++) {
        GUID guid = { 0 };
        const char *interfaceClass = json_array_get_string(interfaceClasses, j);
        if (UuidFromStringA((RPC_CSTR)interfaceClass, &guid) != RPC_S_OK) {
            return -1;
        }

        ZeroMemory(&cmFilter, sizeof(cmFilter));
        cmFilter.cbSize = sizeof(cmFilter);

        cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        cmFilter.u.DeviceInterface.ClassGuid = guid;
        cmRet = CM_Register_Notification(
                        &cmFilter,
                        NULL,
                        (PCM_NOTIFY_CALLBACK)CoreDeviceDiscovery_DeviceNotification,
                        &notifyHandle
                        );

        if (cmRet != CR_SUCCESS) {
            return -1;
        }

        CoreDeviceDiscovery_Enumerate(&guid);

        singlylinkedlist_add(g_CoreDeviceDiscovery_DeviceWatchers, notifyHandle);
    }

    return 0;
}

int 
CoreDevice_StopDiscovery()
{
    LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_CoreDeviceDiscovery_DeviceWatchers);
    while (interfaceItem != NULL) {
        HCMNOTIFICATION hNotifyCtx = (HCMNOTIFICATION) singlylinkedlist_item_get_value(interfaceItem);
        CM_Unregister_Notification(hNotifyCtx);
        interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
    }

    singlylinkedlist_destroy(g_CoreDeviceDiscovery_DeviceWatchers);
    return 0;
}

DISCOVERY_ADAPTER CoreDeviceDiscovery = {
    .Identity = "core-device-discovery",
    .StartDiscovery = CoreDevice_StartDiscovery,
    .StopDiscovery = CoreDevice_StopDiscovery
};