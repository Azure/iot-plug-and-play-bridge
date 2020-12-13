// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pnpadapter_api.h>

#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/const_defines.h"


#include "parson.h"
#include "curl_wrapper/curl_wrapper.h"


//
// Application state associated with the particular component. In particular it contains 
// the resources used for responses in callbacks along with properties set
// and representations of the property update and command callbacks invoked on given component
//
typedef struct IMPINJ_READER_STATE_TAG
{
    char * componentName;
    int brightness;
    char* customerName;
    int numTimesBlinkCommandCalled;
    int blinkInterval;
} IMPINJ_READER_STATE, * PIMPINJ_READER_STATE;

typedef struct _IMPINJ_READER {
    THREAD_HANDLE WorkerHandle;
    volatile bool ShuttingDown;
    PIMPINJ_READER_STATE SensorState;
    PNP_BRIDGE_CLIENT_HANDLE ClientHandle;
} IMPINJ_READER, * PIMPINJ_READER;

// Sends  telemetry messages about current environment
// IOTHUB_CLIENT_RESULT ImpinjReader_SendTelemetryMessagesAsync(
//     PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);
// IOTHUB_CLIENT_RESULT ImpinjReader_ReportDeviceStateAsync(
//     PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
//     const char * ComponentName);
// IOTHUB_CLIENT_RESULT ImpinjReader_RouteReportedState(
//     void * ClientHandle,
//     PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
//     const unsigned char * ReportedState,
//     size_t Size,
//     IOTHUB_CLIENT_REPORTED_STATE_CALLBACK ReportedStateCallback,
//     void * UserContextCallback);
// IOTHUB_CLIENT_RESULT ImpinjReader_RouteSendEventAsync(
//     PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
//     IOTHUB_MESSAGE_HANDLE EventMessageHandle,
//     IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK EventConfirmationCallback,
//     void * UserContextCallback
//     );
void ImpinjReader_ProcessPropertyUpdate(
    void * ClientHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);
// int ImpinjReader_ProcessCommand(
//     PIMPINJ_READER ImpinjReader,
//     const char* CommandName,
//     JSON_Value* CommandValue,
//     unsigned char** CommandResponse,
//     size_t* CommandResponseSize);
int ImpinjReader_ProcessCommand(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize);


#ifdef __cplusplus
}
#endif