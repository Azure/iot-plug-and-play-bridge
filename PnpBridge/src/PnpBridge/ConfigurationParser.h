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
PNPBRIDGE_RESULT PnpBridgeConfig_ReadConfigurationFromFile(const char *filename);

/**
* @brief    PnpBridgeConfig_ReadConfigurationFromString reads a PnpBridge JSON config from string.
*
* @param    config            String containing JSON config
*
* @returns  PNPBRIDGE_OK on success and other PNPBRIDGE_RESULT values on failure.
*/
PNPBRIDGE_RESULT PnpBridgeConfig_ReadConfigurationFromString(const char *config);

JSON_Object* Configuration_GetDiscoveryParametersPerDevice(JSON_Object* device);

JSON_Object* Configuration_GetPnpParametersForDevice(JSON_Object* device);

JSON_Object* Configuration_GetDiscoveryParameters(const char *identity);

JSON_Array* Configuration_GetConfiguredDevices();

PNPBRIDGE_RESULT Configuration_IsDeviceConfigured(JSON_Object* Message);

#ifdef __cplusplus
}
#endif