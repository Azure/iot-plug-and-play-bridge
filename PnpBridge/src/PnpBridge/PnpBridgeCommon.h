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

#define TRY
#define LEAVE   goto __tryLabel;
#define FINALLY goto __tryLabel; __tryLabel:
#undef __try
#undef __finally

#define PNPBRIDGE_RESULT_VALUES \
    PNPBRIDGE_OK, \
    PNPBRIDGE_INSUFFICIENT_MEMORY, \
    PNPBRIDGE_INVALID_ARGS, \
    PNPBRIDGE_CONFIG_READ_FAILED, \
    PNPBRIDGE_DUPLICATE_ENTRY, \
    PNPBRIDGE_FAILED

    DEFINE_ENUM(PNPBRIDGE_RESULT, PNPBRIDGE_RESULT_VALUES);

#define PNPBRIDGE_SUCCESS(Result) (Result == PNPBRIDGE_OK)

#include "ConfigurationParser.h"

MAP_RESULT Map_Add_Index(MAP_HANDLE handle, const char* key, int value);

int Map_GetIndexValueFromKey(MAP_HANDLE handle, const char* key);

#include <PnpBridge.h>

#define PNP_CONFIG_IDENTITY_NAME "Identity"
#define PNP_CONFIG_NAME_INTERFACE_ID "InterfaceId"
#define PNP_CONFIG_PERSISTENT_NAME "Persistent"
#define PNP_CONFIG_MATCH_FILTERS_NAME "MatchFilters"
#define PNP_CONFIG_MATCH_TYPE_NAME "MatchType"
#define PNP_CONFIG_NAME_MATCH_PARAMETERS "MatchParameters"
#define PNP_CONFIG_DISCOVERY_ADAPTER_NAME "DiscoveryAdapter"
#define PNP_CONFIG_NAME_PNP_PARAMETERS "PnpParameters"
#define PNP_CONFIG_NAME_DISCOVERY_PARAMETERS "DiscoveryParameters"
#define PNP_CONFIG_NAME_PNP_ADAPTERS "PnpAdapters"
#define PNP_CONFIG_NAME_DISCOVERY_ADAPTERS "DiscoveryAdapters"
#define PNP_CONFIG_NAME_SELF_DESCRIBING "SelfDescribing"

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_COMMON_H */
