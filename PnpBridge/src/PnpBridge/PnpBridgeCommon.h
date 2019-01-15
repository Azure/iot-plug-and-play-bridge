#pragma once

// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* Shared structures and enums for iothub convenience layer and LL layer */

#ifndef PNPBRIDGE_COMMON_H
#define PNPBRIDGE_COMMON_H

#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/umock_c_prod.h"

#include "iothub_transport_ll.h"
#include "iothub_message.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PNPBRIDGE_RESULT_VALUES \
    PNPBRIDGE_OK, \
    PNPBRIDGE_INSUFFICIENT_MEMORY, \
    PNPBRIDGE_INVALID_ARGS, \
    PNPBRIDGE_CONFIG_READ_FAILED, \
    PNPBRIDGE_FAILED

    DEFINE_ENUM(PNPBRIDGE_RESULT, PNPBRIDGE_RESULT_VALUES);

#ifdef __cplusplus
}
#endif

#endif /* IOTHUB_CLIENT_CORE_COMMON_H */
