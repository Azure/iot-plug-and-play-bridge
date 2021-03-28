// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef R700_READER_H
#define R700_READER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <pnpadapter_api.h>
#include <parson.h>
#include "curl_wrapper/curl_wrapper.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/const_defines.h"
#include "azure_c_shared_utility/threadapi.h"
//
// Application state associated with the particular component. In particular it contains
// the resources used for responses in callbacks along with properties set
// and representations of the property update and command callbacks invoked on given component
//

typedef struct _IMPINJ_R700_FIRMWARE_VERSION
{
    long major;
    long minor;
    long build_major;
    long build_minor;
} IMPINJ_R700_FIRMWARE_VERSION, *PIMPINJ_R700_FIRMWARE_VERSION;

#define R700_REST_VERSION_VALUES \
    V_Unknown,                   \
        V1_0,                    \
        V1_2,                    \
        V1_3,                    \
        V1_4

MU_DEFINE_ENUM_WITHOUT_INVALID(R700_REST_VERSION, R700_REST_VERSION_VALUES);

typedef struct _IMPINJ_R700_API_VERSION
{
    IMPINJ_R700_FIRMWARE_VERSION Firmware;
    R700_REST_VERSION RestVersion;

} IMPINJ_R700_API_VERSION, *PIMPINJ_R700_API_VERSION;

static IMPINJ_R700_API_VERSION IMPINJ_R700_API_MAPPING[] = {
    {7, 1, 0, 0, V1_0},
    {7, 3, 0, 0, V1_2},
    {7, 4, 0, 0, V1_3},
    {7, 5, 0, 0, V1_4},
};

typedef struct IMPINJ_READER_STATE_TAG
{
    char *componentName;
    char *customerName;
    int statusHeartbeatSec;
    char *state;
    char *preset;

} IMPINJ_READER_STATE, *PIMPINJ_READER_STATE;

typedef struct _IMPINJ_READER
{
    R700_REST_VERSION ApiVersion;
    THREAD_HANDLE WorkerHandle;
    volatile bool ShuttingDown;
    PIMPINJ_READER_STATE SensorState;
    PNP_BRIDGE_CLIENT_HANDLE ClientHandle;
    const char *ComponentName;
    CURL_Static_Session_Data *curl_polling_session;
    CURL_Static_Session_Data *curl_static_session;
    CURL_Stream_Session_Data *curl_stream_session;
} IMPINJ_READER, *PIMPINJ_READER;

#ifdef __cplusplus
}
#endif

#endif /* R700_READER_H */