// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/strings_types.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/lock.h"

#include <iothub.h>
#include <iothub_device_client.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>

#include <pnp_device_client.h>
#include <pnp_interface_client.h>

#include "parson.h"

#include "PnpBridgeCommon.h"
#include "ConfigurationParser.h"

MAP_RESULT Map_Add_Index(MAP_HANDLE handle, const char* key, int value);

int Map_GetIndexValueFromKey(MAP_HANDLE handle, const char* key);

typedef enum _PNPBRIDGE_INTERFACE_CHANGE_TYPE {
    PNPBRIDGE_INTERFACE_CHANGE_INVALID,
    PNPBRIDGE_INTERFACE_CHANGE_ARRIVAL,
    PNPBRIDGE_INTERFACE_CHANGE_REMOVAL
} PNPBRIDGE_INTERFACE_CHANGE_TYPE;

typedef struct _PNPBRIDGE_DEVICE_CHANGE_PAYLOAD {
    JSON_Object* Message;
    PNPBRIDGE_INTERFACE_CHANGE_TYPE ChangeType;
    void* Context;
} PNPBRIDGE_DEVICE_CHANGE_PAYLOAD, *PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD;

#ifdef __cplusplus
}
#endif