#pragma once

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

#define DAG_RESULT_VALUES     \
    DAG_OK,                   \
    DAG_INVALID_ARG,		  \
    DAG_ERROR,			      \
    DAG_INVALID_SIZE		  \

/** @brief Enumeration specifying the status of calls to various APIs in this module.
*/

DEFINE_ENUM(DAG_RESULT, DAG_RESULT_VALUES);


MAP_RESULT Map_Add_Index(MAP_HANDLE handle, const char* key, int value);

int Map_GetIndexValueFromKey(MAP_HANDLE handle, const char* key);