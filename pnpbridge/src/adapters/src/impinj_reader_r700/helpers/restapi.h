// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#pragma once

#ifndef R700_RESTAPI_H
#define R700_RESTAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <parson.h>
#include "azure_c_shared_utility/xlogging.h"
#include "../curl_wrapper/curl_wrapper.h"
#include "../impinj_reader_r700.h"
#include "pnp_utils.h"

#define R700_STATUS_OK              200
#define R700_STATUS_CREATED         201
#define R700_STATUS_ACCEPTED        202 // StatusResponse
#define R700_STATUS_NO_CONTENT      204
#define R700_STATUS_BAD_REQUEST     400 // ErrorResponse
#define R700_STATUS_FORBIDDEN       403 // ErrorResponse
#define R700_STATUS_NOT_FOUND       404 // ErrorResponse
#define R700_STATUS_NOT_ALLOWED     405 // ErrorResponse
#define R700_STATUS_NOT_CONFLICT    409 // ErrorResponse
#define R700_STATUS_INTERNAL_ERROR  500 // ErrorResponse
#define R700_STATUS_NOT_IMPLEMENTED 501 // ErrorResponse

static const char impinjReader_property_system_region_selectableRegions[] = "selectableRegions";

static const char g_presetSchemaFormat[]         = "{\"PresetScheme\":%s}";
static const char g_presetsFormat[]              = "{\"PresetsIds\":%s}";
static const char g_emptyCommandResponse[]       = "{}";
static const char g_successResponse[]            = "Operation Success";
static const char g_errorResponseToDescription[] = "%s %s %s";
static const char g_unsupportedApiResponse[]     = "{\"status\":\"API Not Supported\"}";
static const char g_notImplementedApiResponse[]  = "{\"status\":\"API Not Implemented\"}";

// Array in Kafka payload
static const char g_kafkaBootstraps[] = "bootstraps";

// For Error Response
static const char g_errorResponseMessage[]           = "message";
static const char g_errorResponseInvalidPropertyId[] = "invalidPropertyId";
static const char g_errorResponseDetail[]            = "detail";
static const char g_responseFormat[]                 = "{\"message\":\"%s\"}";
static const char g_NotSupportedMessage[]            = "\"Not Supported\"";
// For Status Response
static const char g_statusResponseMessage[] = "message";

// Preset ID command payload
static const char g_presetId[] = "presetId";
static const char g_presetObjectJSON[] = "presetObjectJSON";

// Preset ID configuration
static const char g_presetObject[]                                = "presetObject";
static const char g_presetObjectAntennaConfigs[]                  = "presetObject.antennaConfigs";
static const char g_presetObjectAntennaConfigsTagAuthentication[] = "tagAuthentication";
static const char g_presetObjectAntennaConfigsPowerSweeping[]     = "powerSweeping";
static const char g_antennaConfigFiltering[]                      = "filtering";
static const char g_antennaConfigFilters[]                        = "filters";

#define R700_PRESET_ID_LENGTH 128 + 32

#define R700_REST_REQUEST_VALUES              \
    READER_STATUS_GET,                        \
        READER_STATUS,                        \
        READER_STATUS_POLL,                   \
        HTTP_STREAM,                          \
        MQTT,                                 \
        KAFKA,                                \
        PROFILES,                             \
        PROFILES_STOP,                        \
        PROFILES_INVENTORY_PRESETS_SCHEMA,    \
        PROFILES_INVENTORY_PRESETS_IDS,       \
        PROFILES_INVENTORY_PRESETS_ID_GET,    \
        PROFILES_INVENTORY_PRESETS_ID_SET,    \
        PROFILES_INVENTORY_PRESETS_ID_SET_PASSTHROUGH,    \
        PROFILES_INVENTORY_PRESETS_ID_DELETE, \
        PROFILES_START,                       \
        PROFILES_INVENTORY_TAG,               \
        SYSTEM,                               \
        SYSTEM_HOSTNAME,                      \
        SYSTEM_IMAGE,                         \
        SYSTEM_IMAGE_UPGRADE_UPLOAD,          \
        SYSTEM_IMAGE_UPGRADE,                 \
        SYSTEM_IMAGE_UPGRADE_GET,             \
        SYSTEM_NETORK_INTERFACES,             \
        SYSTEM_POWER,                         \
        SYSTEM_POWER_SET,                     \
        SYSTEM_REGION,                        \
        SYSTEM_REGION_GET,                    \
        SYSTEM_REBOOT,                        \
        SYSTEM_RFID_LLRP,                     \
        SYSTEM_RFID_INTERFACE,                \
        SYSTEM_TIME,                          \
        SYSTEM_TIME_GET,                      \
        SYSTEM_TIME_SET,                      \
        SYSTEM_TIME_NTP,                      \
        SYSTEM_TIME_NTP_SET,                  \
        R700_REST_MAX

MU_DEFINE_ENUM_WITHOUT_INVALID(R700_REST_REQUEST, R700_REST_REQUEST_VALUES);

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
    R700_REST_VERSION ApiVersion;
    R700_REST_REQUEST Request;
    R700_DTDL_TYPE DtdlType;
    R700_REST_TYPE RestType;
    char EndPoint[R700_ENDPONT_LENGTH];
    char Name[64];
} IMPINJ_R700_REST, *PIMPINJ_R700_REST;

static IMPINJ_R700_REST R700_REST_LIST[] = {
    {V1_0, READER_STATUS_GET, COMMAND, GET, "/status", "GetReaderStatus"},
    {V1_0, READER_STATUS, READONLY, GET, "/status", "ReaderStatus"},
    {V1_0, READER_STATUS_POLL, READONLY, GET, "/status", "ReaderStatus"},
    {V1_0, HTTP_STREAM, WRITABLE, PUT, "/http-stream", "StreamConfiguration"},
    {V1_0, MQTT, WRITABLE, PUT, "/mqtt", "MqttConfiguration"},
    {V1_2, KAFKA, WRITABLE, PUT, "/kafka", "KafkaConfiguration"},
    {V1_0, PROFILES, READONLY, GET, "/profiles", "Profiles"},
    {V1_0, PROFILES_STOP, COMMAND, POST, "/profiles/stop", "StopPreset"},
    {V1_0, PROFILES_INVENTORY_PRESETS_SCHEMA, COMMAND, GET, "/profiles/inventory/presets-schema", "PresetsSchema"},
    {V1_0, PROFILES_INVENTORY_PRESETS_IDS, COMMAND, GET, "/profiles/inventory/presets", "Presets"},
    {V1_0, PROFILES_INVENTORY_PRESETS_ID_GET, COMMAND, GET, "/profiles/inventory/presets/%s", "GetPresetId"},
    {V1_0, PROFILES_INVENTORY_PRESETS_ID_SET, COMMAND, PUT, "/profiles/inventory/presets/%s", "SetPresetId"},
    {V1_0, PROFILES_INVENTORY_PRESETS_ID_SET_PASSTHROUGH, COMMAND, PUT, "/profiles/inventory/presets/%s", "SetPresetIdPassthrough"},
    {V1_0, PROFILES_INVENTORY_PRESETS_ID_DELETE, COMMAND, DELETE, "/profiles/inventory/presets/%s", "DeletePresetId"},
    {V1_0, PROFILES_START, COMMAND, POST, "/profiles/inventory/presets/%s/start", "StartPreset"},
    {V1_2, PROFILES_INVENTORY_TAG, COMMAND, GET, "/profiles/inventory/tag%s", "TagPresenceResponse"},
    {V1_3, SYSTEM, READONLY, GET, "/system", "SystemInfo"},
    {V1_3, SYSTEM_HOSTNAME, WRITABLE, PUT, "/system/hostname", "Hostname"},
    {V1_3, SYSTEM_IMAGE, READONLY, GET, "/system/image", "SystemImage"},
    {V1_3, SYSTEM_IMAGE_UPGRADE_UPLOAD, COMMAND, POST, "/system/image/upgrade", "UpgradeUpload"},
    {V1_3, SYSTEM_IMAGE_UPGRADE, READONLY, GET, "/system/image/upgrade", "UpgradeStatus"},
    {V1_3, SYSTEM_IMAGE_UPGRADE_GET, COMMAND, GET, "/system/image/upgrade", "GetUpgradeStatus"},
    {V1_3, SYSTEM_NETORK_INTERFACES, READONLY, GET, "/system/network/interfaces", "NetworkInterface"},
    {V1_3, SYSTEM_POWER, READONLY, GET, "/system/power", "PowerConfiguration"},
    {V1_3, SYSTEM_POWER_SET, COMMAND, PUT, "/system/power", "SetPowerConfiguration"},
    {V1_3, SYSTEM_REGION, WRITABLE, PUT, "/system/region", "RegionInfo"},
    {V1_3, SYSTEM_REGION_GET, COMMAND, GET, "/system/region", "GetRegionInfo"},
    {V1_3, SYSTEM_REBOOT, COMMAND, POST, "/system/reboot", "Reboot"},
    {V1_3, SYSTEM_RFID_LLRP, READONLY, GET, "/system/rfid/llrp", "LlrpStatus"},
    {V1_3, SYSTEM_RFID_INTERFACE, WRITABLE, PUT, "/system/rfid/interface", "RfidInterface"},
    {V1_3, SYSTEM_TIME, READONLY, GET, "/system/time", "TimeInfo"},
    {V1_3, SYSTEM_TIME_GET, COMMAND, GET, "/system/time", "GetTimeInfo"},
    {V1_3, SYSTEM_TIME_SET, COMMAND, PUT, "/system/time", "SetTimeInfo"},
    {V1_4, SYSTEM_TIME_NTP, WRITABLE, PUT, "/system/time/ntp", "Ntp"},
    {V1_4, SYSTEM_TIME_NTP_SET, COMMAND, PUT, "/system/time/ntp", "SetNtp"},
};

JSON_Value*
ImpinjReader_RequestDelete(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    int* HttpStatus);

JSON_Value*
ImpinjReader_RequestGet(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    int* HttpStatus);

JSON_Value*
ImpinjReader_RequestPut(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    const char* Body,
    int* HttpStatus);

JSON_Value*
ImpinjReader_RequestPost(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    int* HttpStatus);

const char*
ImpinjReader_ProcessResponse(
    IMPINJ_R700_REST* RestRequest,
    JSON_Value* JsonVal_Response,
    int HttpStatus);

char* ImpinjReader_ProcessErrorResponse(
    JSON_Value* JsonVal_ErrorResponse,
    R700_DTDL_TYPE DtdlType);

#ifdef __cplusplus
}
#endif

#endif /* R700_RESTAPI_H */