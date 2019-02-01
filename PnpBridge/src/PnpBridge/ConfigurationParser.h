// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/**
* @brief    PnpBridgeConfig_ReadConfigurationFromFile reads a PnpBridge JSON config file.
*
* @param    filename            Fully path to JSON config file.
*
* @returns  PNPBRIDGE_OK on success and other PNPBRIDGE_RESULT values on failure.
*/
PNPBRIDGE_RESULT PnpBridgeConfig_ReadConfigurationFromFile(const char *filename, JSON_Value** config);

/**
* @brief    PnpBridgeConfig_ReadConfigurationFromString reads a PnpBridge JSON config from string.
*
* @param    config            String containing JSON config
*
* @returns  PNPBRIDGE_OK on success and other PNPBRIDGE_RESULT values on failure.
*/
PNPBRIDGE_RESULT PnpBridgeConfig_ReadConfigurationFromString(const char *configString, JSON_Value** config);

JSON_Object* Configuration_GetDiscoveryParametersForDevice(JSON_Object* device);

JSON_Object* Configuration_GetPnpParametersForDevice(JSON_Object* device);

JSON_Object* Configuration_GetPnpParameters(JSON_Value* config, const char* identity);

JSON_Object* Configuration_GetDiscoveryParameters(JSON_Value* config, const char *identity);

JSON_Array* Configuration_GetConfiguredDevices(JSON_Value* config);

PNPBRIDGE_RESULT Configuration_IsDeviceConfigured(JSON_Value* config, JSON_Object* Message, JSON_Object** Device);

const char* Configuration_GetConnectionString(JSON_Value* config);

#ifdef __cplusplus
}
#endif