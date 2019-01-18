// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "common.h"
#include "DiscoveryAdapterInterface.h"
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>

/* A5DCBF10-6530-11D2-901F-00C04FB951ED */
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, \
    0xC0, 0x4F, 0xB9, 0x51, 0xED);

char* deviceChangeMessageformat = "{ \"HardwareId\":%s, \"Identity\":\"core-device-health\"  }";

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
            char* msg;
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
            JSON_Value* json;
            JSON_Object* jsonObject;

            msg = malloc(msgLen * sizeof(char));
            if (NULL == msg) {
                LogError("Failed to allocate memory for PnpDeviceDiscovery change notification");
                return -1;
            }

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
            sprintf_s(msg, msgLen, deviceChangeMessageformat, STRING_c_str(asJson));

            json = json_parse_string(msg);
            jsonObject = json_value_get_object(json);

            payload.Message = jsonObject;

            DeviceChangeHandler(&payload);

            STRING_delete(asJson);
        }
    }
    return 0;
}

int PnpStartDiscovery(PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback, JSON_Object* deviceArgs, JSON_Object* adapterArgs) {
    DWORD cmRet;
    CM_NOTIFY_FILTER cmFilter;
    HCMNOTIFICATION hNotifyCtx = NULL;

    g_deviceWatchers = singlylinkedlist_create();
    if (NULL == g_deviceWatchers) {
        return -1;
    }

    JSON_Array* interfaceClasses =  json_object_dotget_array(adapterArgs, "DeviceInterfaceClasses");

    for (int j = 0; j < json_array_get_count(interfaceClasses); j++) {
        const char *interfaceClass = json_array_get_string(interfaceClasses, j);
        GUID guid;
        if (UuidFromStringA((char *)interfaceClass, &guid) != RPC_S_OK) {
            return -1;
        }

        ZeroMemory(&cmFilter, sizeof(cmFilter));
        cmFilter.cbSize = sizeof(cmFilter);

        cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        cmFilter.u.DeviceInterface.ClassGuid = guid;
        cmRet = CM_Register_Notification(
                        &cmFilter,                      // PCM_NOTIFY_FILTER pFilter,
                        NULL,                           // PVOID pContext,
                        (PCM_NOTIFY_CALLBACK)OnDeviceNotification,                 // PCM_NOTIFY_CALLBACK pCallback,
                        &hNotifyCtx                     // PHCMNOTIFICATION pNotifyContext
                        );

        if (cmRet != CR_SUCCESS) {
            return -1;
        }
        singlylinkedlist_add(g_deviceWatchers, hNotifyCtx);
    }

    DeviceChangeHandler = DeviceChangeCallback;

    //EnumerateDevices()

    return 0;
}

int PnpStopDiscovery() {

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
    .Identity = "windows-pnp-device-discovery",
    .StartDiscovery = PnpStartDiscovery,
    .StopDiscovery = PnpStopDiscovery
};