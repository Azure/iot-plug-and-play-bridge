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