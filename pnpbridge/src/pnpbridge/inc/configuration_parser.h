// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"

// AUTH mechanisms for connecting to IOT device
typedef enum AUTH_TYPE {
    AUTH_TYPE_TPM,
    AUTH_TYPE_X509,
    AUTH_TYPE_SYMMETRIC_KEY
} AUTH_TYPE;

// Supported ways to get connection string
typedef enum IOTDEVICE_CONNECTION_TYPE  {
    CONNECTION_TYPE_CONNECTION_STRING,
    CONNECTION_TYPE_DPS,
    CONNECTION_TYPE_EDGE_MODULE
} IOTDEVICE_CONNECTION_TYPE;

// Union of parameters used for each type of secure device type
typedef struct _AUTH_PARAMETERS {
    union {
        const char* DeviceKey;
        struct {
            const char* X509certificate;
            const char* X509privatekey;
        } Cert;

        struct {
            uint32_t SlotNo;
        } Tpm;
    } u1;

    AUTH_TYPE AuthType;
} AUTH_PARAMETERS;

// DPS parameters to retrieve the connection string
typedef struct _DPS_PARAMETERS {
    const char* GlobalProvUri;
    const char* IdScope;
    const char* DeviceId;
    const char* RootInterfaceModelId;
} DPS_PARAMETERS;

// Transport used to connect with IoTHub Device/Module
typedef enum TRANSPORT_PROTOCOL {
    MQTT
} TRANSPORT_PROTOCOL;

typedef struct BridgeFeatures
{
    int ModuleClient : 1;
} BridgeFeatures;


// Connection parameters that will be populate from the PnpBridge config
typedef struct _CONNECTION_PARAMETERS {
    union {
        DPS_PARAMETERS Dps;
        TRANSPORT_PROTOCOL Transport;
        const char* ConnectionString;
    } u1;
    AUTH_PARAMETERS AuthParameters;
    IOTDEVICE_CONNECTION_TYPE ConnectionType;
    const char* RootInterfaceModelId;
    PNP_DEVICE_CONFIGURATION PnpDeviceConfiguration;
} CONNECTION_PARAMETERS, *PCONNECTION_PARAMETERS;

typedef struct PNPBRIDGE_CONFIGURATION {
    // PnpBridge config document
    JSON_Value* JsonConfig;

    // Connection parameters for the device/module
    PCONNECTION_PARAMETERS ConnParams;

    bool TraceOn;

    // Set of flags that indicate which features are available
    // The idea is that features could be conditionally compiled
    // and this value should be set accordingly. During runtime,
    // before a feature is used, a check should be made to see
    // if its available.
    BridgeFeatures Features;
} PNPBRIDGE_CONFIGURATION;

/**
* @brief    PnpBridgeConfig_GetJsonValueFromConfigFile reads a PnpBridge JSON config file.
*
* @param    Filename            Full path to JSON config file.
*
* @param    Config              JSON_Value representing the root of the config file
*
* @returns  IOTHUB_CLIENT_OK on success and other IOTHUB_CLIENT_RESULT values on failure.
*/
IOTHUB_CLIENT_RESULT PnpBridgeConfig_GetJsonValueFromConfigFile(const char* Filename, JSON_Value** Config);

/**
* @brief    PnpBridgeConfig_GetJsonValueFromString reads a PnpBridge JSON config from string.
*
* @param    ConfigString        String containing JSON config
*
* @returns  IOTHUB_CLIENT_OK on success and other IOTHUB_CLIENT_RESULT values on failure.
*/
IOTHUB_CLIENT_RESULT PnpBridgeConfig_GetJsonValueFromString(const char *ConfigString, JSON_Value** Config);

/**
* @brief    PnpBridgeConfig_GetConnectionDetails parses the connection parameters from 
*           JSON_Object representing the connection_parameters in PnpBridge config.
*
* @param    config            String containing JSON config
*
* @returns  IOTHUB_CLIENT_OK on success and other IOTHUB_CLIENT_RESULT values on failure.
*/
PCONNECTION_PARAMETERS PnpBridgeConfig_GetConnectionDetails(JSON_Object* ConnectionParams);

/**
* @brief    PnpBridgeConfig_RetrieveConfiguration retrieves and verifies the format 
*           of the PnpBridge configuration file. It also checks for mandatory fields.
*
* @param    config   JSON value of the config file from parson
*
* @returns  IOTHUB_CLIENT_OK on success and other IOTHUB_CLIENT_RESULT values on failure.
*/
MOCKABLE_FUNCTION(,
IOTHUB_CLIENT_RESULT,
PnpBridgeConfig_RetrieveConfiguration,
    JSON_Value*, ConfigJson,
    PNPBRIDGE_CONFIGURATION*, BridgeConfig
    );

MOCKABLE_FUNCTION(, 
JSON_Object*, 
Configuration_GetMatchParametersForDevice,
    JSON_Object*, device
    );

MOCKABLE_FUNCTION(, 
JSON_Object*,
Configuration_GetPnpParameters,
    JSON_Value*, config, const char*, identity
    );

MOCKABLE_FUNCTION(,
    JSON_Object*,
    Configuration_GetGlobalAdapterParameters,
    JSON_Value*, config, const char, *identity
);

MOCKABLE_FUNCTION(,
JSON_Array*,
Configuration_GetDevices, JSON_Value*, config
    );


#ifdef __cplusplus
}
#endif