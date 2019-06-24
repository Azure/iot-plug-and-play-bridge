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
#include "azure_c_shared_utility/condition.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/doublylinkedlist.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/strings_types.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/lock.h"

#include <iothub.h>
#include <iothub_device_client.h>
#include "iothub_module_client.h"
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>

#include <digitaltwin_device_client.h>
#include <digitaltwin_module_client.h>
#include <digitaltwin_interface_client.h>

#include "parson.h"

#include <assert.h>

#define TRY
#define LEAVE   goto __tryLabel;
#define FINALLY goto __tryLabel; __tryLabel:
#undef __try
#undef __finally

#define PNPBRIDGE_RESULT_VALUES \
    PNPBRIDGE_OK, \
    PNPBRIDGE_INSUFFICIENT_MEMORY, \
    PNPBRIDGE_FILE_NOT_FOUND, \
    PNPBRIDGE_INVALID_ARGS, \
    PNPBRIDGE_CONFIG_READ_FAILED, \
    PNPBRIDGE_DUPLICATE_ENTRY, \
    PNPBRIDGE_FAILED

MU_DEFINE_ENUM(PNPBRIDGE_RESULT, PNPBRIDGE_RESULT_VALUES);

#define PNPBRIDGE_SUCCESS(Result) (Result == PNPBRIDGE_OK)
#include "configuration_parser.h"

MAP_RESULT Map_Add_Index(MAP_HANDLE handle, const char* key, int value);

int Map_GetIndexValueFromKey(MAP_HANDLE handle, const char* key);

#include <pnpbridge.h>

#define PNP_CONFIG_BRIDGE_PARAMETERS "pnp_bridge_parameters"
#define PNP_CONFIG_TRACE_ON "trace_on"
#define PNP_CONFIG_CONNECTION_PARAMETERS "connection_parameters"

#define PNP_CONFIG_CONNECTION_TYPE "connection_type"
#define PNP_CONFIG_CONNECTION_TYPE_STRING "connection_string"
#define PNP_CONFIG_CONNECTION_TYPE_DPS "dps"
#define PNP_CONFIG_CONNECTION_TYPE_EDGE_MODULE "edge_module"

#define PNP_CONFIG_CONNECTION_TYPE_CONFIG "config"
#define PNP_CONFIG_CONNECTION_TYPE_CONFIG_STRING "connection_string"

#define PNP_CONFIG_CONNECTION_TYPE_CONFIG_DPS "dps_parameters"
#define PNP_CONFIG_CONNECTION_DPS_GLOBAL_PROV_URI "global_prov_uri"
#define PNP_CONFIG_CONNECTION_DPS_ID_SCOPE "id_scope" 
#define PNP_CONFIG_CONNECTION_DPS_DEVICE_ID "device_id"
#define PNP_CONFIG_CONNECTION_DEVICE_CAPS_MODEL_URI "device_capability_model_uri"
#define PNP_CONFIG_CONNECTION_DPS_MODEL_REPO_URI "model_repository_uri"

#define PNP_CONFIG_CONNECTION_AUTH_PARAMETERS "auth_parameters"
#define PNP_CONFIG_CONNECTION_AUTH_TYPE "auth_type"
#define PNP_CONFIG_CONNECTION_AUTH_TYPE_SYMM "symmetric_key"
#define PNP_CONFIG_CONNECTION_AUTH_TYPE_X509 "x509"
#define PNP_CONFIG_CONNECTION_AUTH_TYPE_DEVICE_SYMM_KEY "symmetric_key"

#define PNP_CONFIG_DEVICES "devices"
#define PNP_CONFIG_IDENTITY "identity"
#define PNP_CONFIG_INTERFACE_ID "interface_id"
#define PNP_CONFIG_PUBLISH_MODE "publish_mode"
#define PNP_CONFIG_MATCH_FILTERS "match_filters"
#define PNP_CONFIG_MATCH_TYPE "match_type"
#define PNP_CONFIG_MATCH_TYPE_EXACT "exact"
#define PNP_CONFIG_MATCH_PARAMETERS "match_parameters"
#define PNP_CONFIG_DISCOVERY_ADAPTER "discovery_adapter"
#define PNP_CONFIG_PARAMETERS "parameters"
#define PNP_CONFIG_PNP_PARAMETERS "pnp_parameters"
#define PNP_CONFIG_DISCOVERY_PARAMETERS "discovery_parameters"
#define PNP_CONFIG_PNP_ADAPTERS "pnp_adapters"
#define PNP_CONFIG_DISCOVERY_ADAPTERS "discovery_adapters"
#define PNP_CONFIG_SELF_DESCRIBING "self_describing"
#define PNP_CONFIG_PUBLISH_MODE_ALWAYS "always"
#define PNP_CONFIG_TRUE "true"
#define PNP_CONFIG_FALSE "false"

#define PNPBRIDGE_MAX_PATH 2048

// Mode agnostic iot and pnp handle
typedef struct _MX_IOT_HANDLE_TAG {
    union {
        struct IotDevice {
            // connection to iot device
            IOTHUB_DEVICE_CLIENT_HANDLE deviceHandle;

            // Handle representing PnpDeviceClient
            DIGITALTWIN_DEVICE_CLIENT_HANDLE PnpDeviceClientHandle;
        } IotDevice;

        struct IotModule {
            // connection to iot device
            IOTHUB_MODULE_CLIENT_HANDLE moduleHandle;

            // Handle representing PnpDeviceClient
            DIGITALTWIN_MODULE_CLIENT_HANDLE pnpModuleClientHandle;
        } IotModule;
    } u1;

    // Change this to enum
    bool IsModule;
    bool DeviceClientInitialized;
    bool DigitalTwinClientInitialized;
} MX_IOT_HANDLE_TAG;

typedef struct _PNPBRIDGE_CHANGE_PAYLOAD {
    const char* Message;
    int MessageLength;
    char* InterfaceId;
    PNPMEMORY Self;
    DLIST_ENTRY Entry;
    PNPMESSAGE_PROPERTIES Properties;
} PNPBRIDGE_CHANGE_PAYLOAD, *PPNPBRIDGE_CHANGE_PAYLOAD;

typedef struct _MESSAGE_QUEUE {
    DLIST_ENTRY IngressQueue;

    DLIST_ENTRY PublishQueue;

    LOCK_HANDLE QueueLock;

    int PublishCount;

    THREAD_HANDLE Worker;

    LOCK_HANDLE WaitConditionLock;

    COND_HANDLE WaitCondition;

    bool TearDown;
} MESSAGE_QUEUE, *PMESSAGE_QUEUE;


// PnpMessage Queue APIs
PNPBRIDGE_RESULT 
PnpMessageQueue_Create(
    PMESSAGE_QUEUE* PnpMessageQueue
    );

// Thread entry callback
int
PnpMessageQueue_Worker(
    void* threadArgument
    );

PNPBRIDGE_RESULT 
PnpMesssageQueue_Add(
    PMESSAGE_QUEUE Queue,
    PNPMESSAGE PnpMessage
    );

PNPBRIDGE_RESULT 
PnpMesssageQueue_Remove(
    PMESSAGE_QUEUE Queue,
    PNPMESSAGE* PnpMessage
    );

PDLIST_ENTRY
PnpMesssageQueue_GetPublishQ(
    PMESSAGE_QUEUE Queue
    );

PNPBRIDGE_RESULT
PnpMesssageQueue_AddToPublishQ(
    PMESSAGE_QUEUE Queue,
    PNPMESSAGE PnpMessage
);

void
PnpMessageQueue_Release(
    PMESSAGE_QUEUE Queue
    );

int PnpBridge_ProcessPnpMessage(PNPMESSAGE PnpMessage);

int
DiscoveryAdapter_PublishInterfaces(
    void
    );

int
PnpBridge_ReinitPnpAdapters(
    void
    );

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_COMMON_H */
