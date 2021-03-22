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
    CURL_Static_Session_Data *curl_static_session;
    CURL_Stream_Session_Data *curl_stream_session;
} IMPINJ_READER, *PIMPINJ_READER;

typedef struct _IMPINJ_R700_MESSAGE_CONTEXT
{
    PIMPINJ_READER reader;
    IOTHUB_MESSAGE_HANDLE messageHandle;
} IMPINJ_R700_MESSAGE_CONTEXT, *PIMPINJ_R700_MESSAGE_CONTEXT;

typedef enum _R700_REST_REQUEST
{
    READER_STATUS_GET,
    READER_STATUS,
    HTTP_STREAM,
    MQTT,
    KAFKA,
    PROFILES,
    PROFILES_STOP,
    PROFILES_INVENTORY_PRESETS_SCHEMA,
    PROFILES_INVENTORY_PRESETS_IDS,
    PROFILES_INVENTORY_PRESETS_ID_GET,
    PROFILES_INVENTORY_PRESETS_ID_SET,
    PROFILES_INVENTORY_PRESETS_ID_DELETE,
    PROFILES_START,
    PROFILES_INVENTORY_TAG,
    SYSTEM,
    SYSTEM_HOSTNAME,
    SYSTEM_IMAGE,
    SYSTEM_IMAGE_UPGRADE_UPLOAD,
    SYSTEM_IMAGE_UPGRADE,
    SYSTEM_IMAGE_UPGRADE_GET,
    SYSTEM_NETORK_INTERFACES,
    SYSTEM_POWER,
    SYSTEM_POWER_SET,
    SYSTEM_REGION,
    SYSTEM_REGION_GET,
    SYSTEM_REBOOT,
    SYSTEM_RFID_LLRP,
    SYSTEM_RFID_INTERFACE,
    SYSTEM_TIME,
    SYSTEM_TIME_GET,
    SYSTEM_TIME_SET,
    UNSUPPORTED,
} R700_REST_REQUEST;

typedef enum _R700_DTDL_TYPE
{
    READONLY,
    WRITABLE,
    COMMAND,
    DTDL_NONE,
} R700_DTDL_TYPE;

typedef enum _R700_REST_TYPE
{
    GET,
    PUT,
    POST,
    DELETE,
    UNSUPPORTED_REST,
} R700_REST_TYPE;

#define R700_ENDPONT_LENGTH 48

typedef struct _IMPINJ_R700_REST
{
    R700_REST_REQUEST Request;
    R700_DTDL_TYPE DtdlType;
    R700_REST_TYPE RestType;
    char EndPoint[R700_ENDPONT_LENGTH];
    char Name[32];
} IMPINJ_R700_REST, *PIMPINJ_R700_REST;

static IMPINJ_R700_REST R700_REST_LIST[] =
{
    {READER_STATUS_GET,                    COMMAND,  GET,     "/status", "GetReaderStatus"},
    {READER_STATUS,                        READONLY, GET,     "/status", "ReaderStatus"},
    {HTTP_STREAM,                          WRITABLE, PUT,     "/http-stream", "StreamConfiguration"},
    {MQTT,                                 WRITABLE, PUT,     "/mqtt", "MqttConfiguration"},
    {KAFKA,                                WRITABLE, PUT,     "/kafka", "KafkaConfiguration"},
    {PROFILES,                             READONLY, GET,     "/profiles", "Profiles"},
    {PROFILES_STOP,                        COMMAND,  POST,    "/profiles/stop", "StopPreset"},
    {PROFILES_INVENTORY_PRESETS_SCHEMA,    COMMAND,  GET,     "/profiles/inventory/presets-schema", "PresetsSchema"},
    {PROFILES_INVENTORY_PRESETS_IDS,       COMMAND,  GET,     "/profiles/inventory/presets", "Presets"},
    {PROFILES_INVENTORY_PRESETS_ID_GET,    COMMAND,  GET,     "/profiles/inventory/presets/%s", "GetPresetId"},
    {PROFILES_INVENTORY_PRESETS_ID_SET,    COMMAND,  PUT,     "/profiles/inventory/presets/%s", "SetPresetId"},
    {PROFILES_INVENTORY_PRESETS_ID_DELETE, COMMAND,  DELETE,  "/profiles/inventory/presets/%s", "DeletePresetId"},
    {PROFILES_START,                       COMMAND,  POST,    "/profiles/inventory/presets/%s/start", "StartPreset"},
    {PROFILES_INVENTORY_TAG,               COMMAND,  GET,     "/profiles/inventory/tag%s", "TagPresenceResponse"},
    {SYSTEM,                               READONLY, GET,     "/system", "SystemInfo"},
    {SYSTEM_HOSTNAME,                      WRITABLE, PUT,     "/system/hostname", "Hostname"},
    {SYSTEM_IMAGE,                         READONLY, GET,     "/system/image", "SystemImage"},
    {SYSTEM_IMAGE_UPGRADE_UPLOAD,          COMMAND,  POST,    "/system/image/upgrade", "UpgradeUpload"},
    {SYSTEM_IMAGE_UPGRADE,                 READONLY, GET,     "/system/image/upgrade", "UpgradeStatus"},
    {SYSTEM_IMAGE_UPGRADE_GET,             COMMAND,  GET,     "/system/image/upgrade", "GetUpgradeStatus"},
    {SYSTEM_NETORK_INTERFACES,             READONLY, GET,     "/system/network/interfaces", "NetworkInterface"},
    {SYSTEM_POWER,                         READONLY, GET,     "/system/power", "PowerConfiguration"},
    {SYSTEM_POWER_SET,                     COMMAND,  PUT,     "/system/power", "SetPowerConfiguration"},
    {SYSTEM_REGION,                        WRITABLE, PUT,     "/system/region", "RegionInfo"},
    {SYSTEM_REGION_GET,                    COMMAND,  GET,     "/system/region", "GetRegionInfo"},
    {SYSTEM_REBOOT,                        COMMAND,  POST,    "/system/reboot", "Reboot"},
    {SYSTEM_RFID_LLRP,                     READONLY, GET,     "/system/rfid/llrp", "LlrpStatus"},
    {SYSTEM_RFID_INTERFACE,                WRITABLE, PUT,     "/system/rfid/interface", "RfidInterface"},
    {SYSTEM_TIME,                          READONLY, GET,     "/system/time", "TimeInfo"},
    {SYSTEM_TIME_GET,                      COMMAND,  GET,     "/system/time", "GetTimeInfo"},
    {SYSTEM_TIME_SET,                      COMMAND,  PUT,     "/system/time", "SetTimeInfo"},
    {UNSUPPORTED,                          DTDL_NONE,UNSUPPORTED_REST,    "", ""},
};

static const char g_IoTHubTwinDesiredVersion[] = "$version";
static const char g_IoTHubTwinDesiredObjectName[] = "desired";

static const char impinjReader_property_system_region_selectableRegions[] = "selectableRegions";

static const char impinjReader_property_http_stream[] = "HttpStream";
static const char impinjReader_property_mqtt[] = "Mqtt";
static const char impinjReader_property_kafka[] = "Kafka";
static const char impinjReader_property_hostname[] = "HostName";
static const char impinjReader_property_power[] = "PowerConfiguration";

static const char impinjReader_command_get_reader_status[] = "GetReaderStatus";
static const char impinjReader_command_preset_schema[] = "PresetsSchema";
static const char impinjReader_command_stop_preset[] = "StopPreset";
static const char impinjReader_command_start_preset[] = "StartPreset";

static const char g_presetSchemaFormat[] = "{\"PresetScheme\":%s}";
static const char g_emptyCommandResponse[] = "{}";
static const char g_successResponse[] = "Operation Success";
static const char g_errorResponseToDescription[] = "%s %s %s";
static const char g_errorResponseMessage[] ="message";
static const char g_errorResponseInvalidPropertyId[] ="invalidPropertyId";
static const char g_errorResponseDetail[] ="detail";
static const char g_statusResponseMessage[] ="message";
static const char g_presetId[] = "presetId";
static const char g_presetObject[] = "presetObject";
static const char g_presetObjectAntennaConfigs[] = "presetObject.antennaConfigs";
static const char g_presetObjectAntennaConfigsTagAuthentication[] = "tagAuthentication";
static const char g_presetObjectAntennaConfigsPowerSweeping[] = "powerSweeping";
static const char g_kafkaBootstraps[] = "bootstraps";
static const char g_errorResponseFormat[] = "{\"message\":\"%s\"}";
static const char g_powerSourceAlreadyConfigure[] = "{\"message\":\"The provided power source was already configured on the reader.\"}";
static const char g_antennaConfigFiltering[] = "filtering";
static const char g_antennaConfigFilters[] = "filters";

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

#define MESSAGE_SPLIT_DELIMITER "\n\r"

static const char g_ReaderEvent[] = "ReaderEvent";

static const char g_ReaderEvent_TagInventoryEvent[] = "tagInventoryEvent";
static const char g_ReaderEvent_AntennaConnectedEvent[] = "antennaConnectedEvent";
static const char g_ReaderEvent_AntennaDisconnectedEvent[] = "antennaDisconnectedEvent";
static const char g_ReaderEvent_InventoryStatusEvent[] = "inventoryStatusEvent";
static const char g_ReaderEvent_InventoryTerminatedEvent[] = "inventoryTerminatedEvent";
static const char g_ReaderEvent_DiagnosticEvent[] = "diagnosticEvent";
static const char g_ReaderEvent_OverflowEvent[] = "overflowEvent";
static const char g_ReaderEvent_TagLocationEntryEvent[] = "tagLocationEntryEvent";
static const char g_ReaderEvent_TagLocationUpdateEvent[] = "tagLocationUpdateEvent";
static const char g_ReaderEvent_TagLocationExitEvent[] = "tagLocationExitEvent";

#define R700_STATUS_OK             200
#define R700_STATUS_CREATED        201
#define R700_STATUS_ACCEPTED       202 // StatusResponse
#define R700_STATUS_NO_CONTENT     204
#define R700_STATUS_BAD_REQUEST    400 // ErrorResponse
#define R700_STATUS_FORBIDDEN      403 // ErrorResponse
#define R700_STATUS_NOT_FOUND      404 // ErrorResponse
#define R700_STATUS_NOT_CONFLICT   409 // ErrorResponse
#define R700_STATUS_INTERNAL_ERROR 500 // ErrorResponse

#define R700_PRESET_ID_LENGTH 128
#define R700_PRESET_START_LENGTH R700_ENDPONT_LENGTH + R700_PRESET_ID_LENGTH

IOTHUB_CLIENT_RESULT ImpinjReader_ReportPropertyAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *ComponentName,
    char *propertyName,
    char *propertyValue);

IOTHUB_CLIENT_RESULT ImpinjReader_RouteReportedState(
    void *ClientHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const unsigned char *ReportedState,
    size_t Size,
    IOTHUB_CLIENT_REPORTED_STATE_CALLBACK ReportedStateCallback,
    void *UserContextCallback);

IOTHUB_CLIENT_RESULT ImpinjReader_ReportDeviceStateAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *ComponentName);

// Sends  telemetry messages about current environment
IOTHUB_CLIENT_RESULT
ImpinjReader_SendTelemetryMessagesAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);

IOTHUB_CLIENT_RESULT
ImpinjReader_RouteSendEventAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    IOTHUB_MESSAGE_HANDLE EventMessageHandle,
    IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK EventConfirmationCallback,
    void *UserContextCallback);

void ImpinjReader_OnPropertyPatchCallback(
    void *ClientHandle,
    const char *PropertyName,
    JSON_Value *PropertyValue,
    int version,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);

void ImpinjReader_OnPropertyCompleteCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const unsigned char *payload,
    size_t size,
    void *userContextCallback);

int ImpinjReader_OnCommandCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *CommandName,
    JSON_Value *CommandValue,
    unsigned char **CommandResponse,
    size_t *CommandResponseSize);

IOTHUB_CLIENT_RESULT ImpinjReader_UpdateWritableProperty(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_R700_REST RestRequest,
    JSON_Value *PropertyValue,
    int Ac,
    char *Ad,
    int Av);

JSON_Value *ImpinjReader_RequestDelete(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    int *httpStatus);

JSON_Value * ImpinjReader_RequestGet(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    int *httpStatus);

JSON_Value *ImpinjReader_RequestPut(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    const char *Body,
    int *httpStatus);

JSON_Value *ImpinjReader_RequestPost(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    int *httpStatus);

const char * ImpinjReader_ProcessResponse(
    IMPINJ_R700_REST *RestRequest,
    JSON_Value *JsonVal_Response,
    int HttpStatus);

char *ImpinjReader_ProcessErrorResponse(
    JSON_Value *JsonVal_ErrorResponse,
    R700_DTDL_TYPE DtdlType);

#ifdef __cplusplus
}
#endif