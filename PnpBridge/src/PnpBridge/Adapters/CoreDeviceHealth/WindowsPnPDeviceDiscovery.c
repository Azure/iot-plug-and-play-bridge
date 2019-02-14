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


const char* deviceChangeMessageformat = "{ \
                                           \"Identity\": \"windows-pnp-discovery\", \
                                           \"MatchParameters\": { \
                                               \"HardwareId\":%s, \"SymbolicLink\":%s \
                                            } \
                                         }";

PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeHandler = NULL;
SINGLYLINKEDLIST_HANDLE g_deviceWatchers;

DWORD 
OnDeviceNotification(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ PVOID context,
    _In_ CM_NOTIFY_ACTION action,
    _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
    _In_ DWORD eventDataSize)
{

    if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) {
        if (DeviceChangeHandler != NULL) {
            char msg[512];
            PNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload = { 0 };
            DEVPROPTYPE PropType;
            WCHAR hardwareIds[MAX_DEVICE_ID_LEN];
            ULONG BufferSize = MAX_DEVICE_ID_LEN * sizeof(WCHAR);
            CONFIGRET status;
            DEVINST Devinst;
            DEVPROPTYPE DevPropType;
            ULONG DevPropSize;
            WCHAR deviceInstance[MAX_DEVICE_ID_LEN];
            ULONG deviceInstanceSize = MAX_DEVICE_ID_LEN * sizeof(WCHAR);
            LPWSTR SingleDeviceId;
            int msgLen = 512;
            STRING_HANDLE asJson;

            payload.ChangeType = PNPBRIDGE_INTERFACE_CHANGE_ARRIVAL;

            // Find the hardware Id
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

            status = CM_Get_DevNode_Property(
                        Devinst,
                        &DEVPKEY_Device_HardwareIds,
                        &PropType,
                        (PBYTE)hardwareIds,
                        &BufferSize,
                        0);
            if (CR_SUCCESS != status) {
                hardwareIds[0] = L'\0';
            }

            SingleDeviceId = (LPWSTR)hardwareIds;
            size_t cLength = 0;
            cLength = wcslen(SingleDeviceId);

            while (*SingleDeviceId) {
                cLength = wcslen(SingleDeviceId);
                SingleDeviceId += cLength + 1;
                sprintf_s(msg, msgLen, "%S", SingleDeviceId);
                break;
            }

            LogInfo(msg);

            asJson = STRING_new_JSON(msg);

            char msg1[512];
            sprintf_s(msg1, msgLen, "%S", eventData->u.DeviceInterface.SymbolicLink);
            STRING_HANDLE asJson1 = STRING_new_JSON(msg1);
            sprintf_s(msg, msgLen, deviceChangeMessageformat, STRING_c_str(asJson), STRING_c_str(asJson1));

            payload.Message = msg;

            payload.Context = malloc(sizeof(eventData->u.DeviceInterface.ClassGuid));
            memcpy(payload.Context, &eventData->u.DeviceInterface.ClassGuid, sizeof(eventData->u.DeviceInterface.ClassGuid));

            DeviceChangeHandler(&payload);

            STRING_delete(asJson);
        }
    }
    return 0;
}

int WindowsPnp_StartDiscovery(PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback, const char* deviceArgs, const char* adapterArgs) {
    DWORD cmRet;
    CM_NOTIFY_FILTER cmFilter;
    HCMNOTIFICATION hNotifyCtx = NULL;
    JSON_Value* jmsg;
    JSON_Object* jobj;

    g_deviceWatchers = singlylinkedlist_create();
    if (NULL == g_deviceWatchers) {
        return -1;
    }

    jmsg = json_parse_string(adapterArgs);
    jobj = json_value_get_object(jmsg);
    JSON_Array* interfaceClasses =  json_object_dotget_array(jobj, "DeviceInterfaceClasses");

    DeviceChangeHandler = DeviceChangeCallback;

    for (int j = 0; j < (int)json_array_get_count(interfaceClasses); j++) {
        GUID guid = { 0 };
        const char *interfaceClass = json_array_get_string(interfaceClasses, j);
        if (UuidFromStringA((char *)interfaceClass, &guid) != RPC_S_OK) {
            return -1;
        }

        ZeroMemory(&cmFilter, sizeof(cmFilter));
        cmFilter.cbSize = sizeof(cmFilter);

        cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        cmFilter.u.DeviceInterface.ClassGuid = guid;
        cmRet = CM_Register_Notification(
                        &cmFilter,
                        NULL,
                        (PCM_NOTIFY_CALLBACK)OnDeviceNotification,
                        &hNotifyCtx
                        );

        if (cmRet != CR_SUCCESS) {
            return -1;
        }
        singlylinkedlist_add(g_deviceWatchers, hNotifyCtx);
    }

    return 0;
}

int WindowsPnp_StopDiscovery() {

    LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_deviceWatchers);
    while (interfaceItem != NULL) {
        HCMNOTIFICATION hNotifyCtx = (HCMNOTIFICATION) singlylinkedlist_item_get_value(interfaceItem);
        CM_Unregister_Notification(hNotifyCtx);
        interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
    }

    singlylinkedlist_destroy(g_deviceWatchers);
    return 0;
}

DISCOVERY_ADAPTER WindowsPnpDeviceDiscovery = {
    .Identity = "windows-pnp-discovery",
    .StartDiscovery = WindowsPnp_StartDiscovery,
    .StopDiscovery = WindowsPnp_StopDiscovery
};