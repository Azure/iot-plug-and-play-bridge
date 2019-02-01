// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef PNPBRIDGE_COMMON_H
#define PNPBRIDGE_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/umock_c_prod.h"

#include "iothub_transport_ll.h"
#include "iothub_message.h"

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


/** @brief Enumeration specifying the status of calls to various APIs in this module.
*/

#define PNPBRIDGE_RESULT_VALUES \
    PNPBRIDGE_OK, \
    PNPBRIDGE_INSUFFICIENT_MEMORY, \
    PNPBRIDGE_INVALID_ARGS, \
    PNPBRIDGE_CONFIG_READ_FAILED, \
    PNPBRIDGE_DUPLICATE_ENTRY, \
    PNPBRIDGE_FAILED

    DEFINE_ENUM(PNPBRIDGE_RESULT, PNPBRIDGE_RESULT_VALUES);

#include "ConfigurationParser.h"

MAP_RESULT Map_Add_Index(MAP_HANDLE handle, const char* key, int value);

int Map_GetIndexValueFromKey(MAP_HANDLE handle, const char* key);

#include <PnpBridge.h>

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_COMMON_H */
