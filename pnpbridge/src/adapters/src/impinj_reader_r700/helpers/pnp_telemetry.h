// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef R700_TELEMETRY_H
#define R700_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <parson.h>
#include <pnpadapter_api.h>
#include "pnp_utils.h"

// ENUM value to add to message property based on Reader Event
typedef enum _R700_READER_EVENT
{
    TagInventory,
    AntennaConnected,
    AntennaDisconnected,
    InventoryStatus,
    InventoryTerminated,
    Diagnostic,
    Overflow,
    TagLocationEntry,
    TagLocationUpdate,
    TagLocationExit,
    EventNone,
} R700_READER_EVENT;

static const char g_ReaderEvent[]                          = "ReaderEvent";
static const char g_ReaderEvent_TagInventoryEvent[]        = "tagInventoryEvent";
static const char g_ReaderEvent_AntennaConnectedEvent[]    = "antennaConnectedEvent";
static const char g_ReaderEvent_AntennaDisconnectedEvent[] = "antennaDisconnectedEvent";
static const char g_ReaderEvent_InventoryStatusEvent[]     = "inventoryStatusEvent";
static const char g_ReaderEvent_InventoryTerminatedEvent[] = "inventoryTerminatedEvent";
static const char g_ReaderEvent_DiagnosticEvent[]          = "diagnosticEvent";
static const char g_ReaderEvent_OverflowEvent[]            = "overflowEvent";
static const char g_ReaderEvent_TagLocationEntryEvent[]    = "tagLocationEntryEvent";
static const char g_ReaderEvent_TagLocationUpdateEvent[]   = "tagLocationUpdateEvent";
static const char g_ReaderEvent_TagLocationExitEvent[]     = "tagLocationExitEvent";

#define MESSAGE_SPLIT_DELIMITER "\n\r"

IOTHUB_CLIENT_RESULT
ProcessReaderTelemetry(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);

// Context for message
typedef struct _IMPINJ_R700_MESSAGE_CONTEXT
{
    PIMPINJ_READER reader;
    IOTHUB_MESSAGE_HANDLE messageHandle;
} IMPINJ_R700_MESSAGE_CONTEXT, *PIMPINJ_R700_MESSAGE_CONTEXT;

IOTHUB_CLIENT_RESULT
SendTelemetryMessages(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    char* Message);

static PIMPINJ_R700_MESSAGE_CONTEXT
CreateMessageInstance(
    IOTHUB_MESSAGE_HANDLE MessageHandle,
    PIMPINJ_READER Reader);

IOTHUB_MESSAGE_RESULT
AddMessageProperty(
    IOTHUB_MESSAGE_HANDLE MessageHandle,
    char* Message);

static void
TelemetryCallback(
    IOTHUB_CLIENT_CONFIRMATION_RESULT TelemetryStatus,
    void* UserContextCallback);

int
ImpinjReader_TelemetryWorker(
    void* context);

#ifdef __cplusplus
}
#endif

#endif /* R700_TELEMETRY_H */