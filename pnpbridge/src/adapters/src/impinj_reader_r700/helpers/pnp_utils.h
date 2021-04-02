// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef R700_UTILS_H
#define R700_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <parson.h>
#include "azure_c_shared_utility/xlogging.h"
#include "../impinj_reader_r700.h"

static const char g_separator[] = "\r\n================================\r\n";

#define MAX_ANTENNA_PORT   4
#define MAX_PRESET         5
#define MAX_ANTENNA_CONFIG 255
#define MAX_PRESETID_LENGTH 128 + 1

// typedef struct _ANTENNA_PRESET
// {
//     char PresetId[MAX_PRESETID_LENGTH];
//     JSON_Value* AntennaConfig;
// } ANTENNA_PRESET, *PANTENNA_PRESET;

// typedef struct _ANTENNA_PORT
// {
//     ANTENNA_PRESET Preset[MAX_PRESET];
// } ANTENNA_PORT, *PANTENNA_PORT;

// typedef struct _ANTENNA_PORTS
// {
//     ANTENNA_PORT Port[MAX_ANTENNA_PORT];
// } ANTENNA_PORTS, *PANTENNA_PORTS;

// typedef struct _ANTENNA_PORTS
// {
//     ANTENNA_PORT Port[MAX_ANTENNA_PORT];
// } ANTENNA_PORTS, *PANTENNA_PORTS;

typedef struct _ANTENNA_PRESET
{
    char PresetId[MAX_PRESETID_LENGTH];
    JSON_Value* AntennaConfig[MAX_ANTENNA_PORT];
} ANTENNA_PRESET, *PANTENNA_PRESET;

typedef struct _ANTENNA_CONFIG
{
    ANTENNA_PRESET Preset[MAX_PRESET];
} ANTENNA_CONFIG, *PANTENNA_CONFIG;

void LogJsonPretty(
    const char* MsgFormat,
    JSON_Value* JsonValue, ...);

void LogJsonPrettyStr(
    const char* MsgFormat,
    char* JsonString, ...);

JSON_Value*
Remove_JSON_Array(
    char* Payload,
    const char* ArrayName);

JSON_Value*
JSONArray2DtdlMap(
    char* Payload,
    const char* ArrayName,
    const char* MapName);

char* DtdlMap2JSONArray(
    const char* Payload,
    const char* ArrayName);

const char*
GetStringFromPayload(
    JSON_Value* CommandValue,
    const char* ParamName);

const char*
GetObjectStringFromPayload(
    JSON_Value* Payload,
    const char* ParamName);

bool CleanAntennaConfig(
    JSON_Object* jsonObj_AntennaConfig);

void GetFirmwareVersion(
    PIMPINJ_READER Reader);

int PopulateAntennaSettings(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize);

#ifdef __cplusplus
}
#endif

#endif /* R700_UTILS_H */