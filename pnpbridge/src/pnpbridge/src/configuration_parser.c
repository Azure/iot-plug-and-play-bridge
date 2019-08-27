// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"

char *getcwd(char *buf, size_t size);

PCONNECTION_PARAMETERS PnpBridgeConfig_GetConnectionDetails(JSON_Object* ConnectionParams)
{
    PCONNECTION_PARAMETERS connParams = NULL;
    PNPBRIDGE_RESULT result = false;

    TRY {
        connParams = (PCONNECTION_PARAMETERS) calloc(1, sizeof(CONNECTION_PARAMETERS));
        if (NULL == connParams) {
            LogError("Failed to allocate CONNECTION_PARAMETERS");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }
        
        // Evaluate the connection_type
        {
            const char* connectionTypeStr = json_object_get_string(ConnectionParams, PNP_CONFIG_CONNECTION_TYPE);
            if (NULL == connectionTypeStr) {
                LogError("%s is not specified in config", PNP_CONFIG_CONNECTION_TYPE);
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            if (0 == strcmp(connectionTypeStr, PNP_CONFIG_CONNECTION_TYPE_STRING)) {
                connParams->ConnectionType = CONNECTION_TYPE_CONNECTION_STRING;
            }
            else if (0 == strcmp(connectionTypeStr, PNP_CONFIG_CONNECTION_TYPE_DPS)) {
                connParams->ConnectionType = CONNECTION_TYPE_DPS;
            }
            else if (0 == strcmp(connectionTypeStr, PNP_CONFIG_CONNECTION_TYPE_EDGE_MODULE)) {
                connParams->ConnectionType = CONNECTION_TYPE_EDGE_MODULE;
            }
            else {
                LogError("ConnectionType (%s) is not valid", connectionTypeStr);
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            LogInfo("Connection_type is [%s]", connectionTypeStr);
        }

        // Get device capability model URI
        connParams->DeviceCapabilityModelUri = json_object_get_string(ConnectionParams, PNP_CONFIG_CONNECTION_DEVICE_CAPS_MODEL_URI);
        if (NULL == connParams->DeviceCapabilityModelUri) {
            LogError("%s is missing in config", PNP_CONFIG_CONNECTION_DEVICE_CAPS_MODEL_URI);
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }
        
        // Get connection string parameters
        {
            if (CONNECTION_TYPE_CONNECTION_STRING == connParams->ConnectionType) {
                connParams->u1.ConnectionString = json_object_get_string(ConnectionParams, PNP_CONFIG_CONNECTION_TYPE_CONFIG_STRING);
                if (NULL == connParams->u1.ConnectionString) {
                    LogError("%s is missing", PNP_CONFIG_CONNECTION_TYPE_CONFIG_STRING);
                    result = PNPBRIDGE_INVALID_ARGS;
                    LEAVE;
                }
            }
        }

        // Get dps parameters
        {
            if (CONNECTION_TYPE_DPS == connParams->ConnectionType) {
                JSON_Object* dpsSettings = json_object_get_object(ConnectionParams, PNP_CONFIG_CONNECTION_TYPE_CONFIG_DPS);
                if (NULL == dpsSettings) {
                    LogError("%s are missing in config", PNP_CONFIG_CONNECTION_TYPE_CONFIG_DPS);
                    result = PNPBRIDGE_INVALID_ARGS;
                    LEAVE;
                }

                DPS_PARAMETERS* dpsParams = &connParams->u1.Dps;

                dpsParams->GlobalProvUri = json_object_get_string(dpsSettings, PNP_CONFIG_CONNECTION_DPS_GLOBAL_PROV_URI);
                if (NULL == dpsParams->GlobalProvUri) {
                    LogError("%s is missing in config", PNP_CONFIG_CONNECTION_DPS_GLOBAL_PROV_URI);
                    result = PNPBRIDGE_INVALID_ARGS;
                    LEAVE;
                }

                dpsParams->IdScope = json_object_get_string(dpsSettings, PNP_CONFIG_CONNECTION_DPS_ID_SCOPE);
                if (NULL == dpsParams->IdScope) {
                    LogError("%s is missing in config", PNP_CONFIG_CONNECTION_DPS_ID_SCOPE);
                    result = PNPBRIDGE_INVALID_ARGS;
                    LEAVE;
                }
                 
                dpsParams->DeviceId = json_object_get_string(dpsSettings, PNP_CONFIG_CONNECTION_DPS_DEVICE_ID);
                if (NULL == dpsParams->DeviceId) {
                    LogError("%s is missing in config", PNP_CONFIG_CONNECTION_DPS_DEVICE_ID);
                    result = PNPBRIDGE_INVALID_ARGS;
                    LEAVE;
                }

                dpsParams->DcmModelId = connParams->DeviceCapabilityModelUri;
            }
        }

        // Evaluate the auth_type
        {
            // Get auth_parameters object
            JSON_Object* authParameters = json_object_get_object(ConnectionParams, PNP_CONFIG_CONNECTION_AUTH_PARAMETERS);
            if (NULL == authParameters) {
                LogError("%s is missing in config", PNP_CONFIG_CONNECTION_AUTH_PARAMETERS);
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            const char* authType = json_object_get_string(authParameters, PNP_CONFIG_CONNECTION_AUTH_TYPE);
            if (NULL == authType) {
                LogError("%s is not specified in config", PNP_CONFIG_CONNECTION_AUTH_TYPE);
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            if (0 == strcmp(authType, PNP_CONFIG_CONNECTION_AUTH_TYPE_SYMM)) {
                connParams->AuthParameters.AuthType = AUTH_TYPE_SYMMETRIC_KEY;
            }
            else if (0 == strcmp(authType, PNP_CONFIG_CONNECTION_AUTH_TYPE_X509)) {
                connParams->AuthParameters.AuthType = AUTH_TYPE_X509;
            }
            else {
                LogError("auth_type (%s) is not valid", authType);
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            // Evaluate auth parameters
            {
                if (AUTH_TYPE_SYMMETRIC_KEY == connParams->AuthParameters.AuthType) {
                    const char* deviceKey = json_object_get_string(authParameters, PNP_CONFIG_CONNECTION_AUTH_TYPE_DEVICE_SYMM_KEY);
                    if (NULL == deviceKey) {
                        LogError("%s is missing in config", PNP_CONFIG_CONNECTION_AUTH_TYPE_DEVICE_SYMM_KEY);
                        result = PNPBRIDGE_INVALID_ARGS;
                        LEAVE;
                    }

                    connParams->AuthParameters.u1.DeviceKey = deviceKey;
                }
            }
        }
    } FINALLY {
        if (PNPBRIDGE_OK != result) {
            free(connParams);
            connParams = NULL;
        }
    }

    return connParams;
}

PNPBRIDGE_RESULT PnpBridgeConfig_GetJsonValueFromConfigFile(const char *filename, JSON_Value** config) 
{    
    if (NULL == filename) {
        LogError("filename is NULL");
        return PNPBRIDGE_INVALID_ARGS;
    }

    if (NULL == config) {
        LogError("config is NULL");
        return PNPBRIDGE_INVALID_ARGS;
    }

    // Check if file exists
    {
        FILE *file;
        file = fopen(filename, "r");
        if (NULL != file) {
            fclose(file);
        }
        else {
            char* workingDir;
            workingDir = getcwd(NULL, 0);
            LogError("Cannot find the config file %s. Current working directory %s", filename, workingDir);
            if (NULL != workingDir) {
                free(workingDir);
            }
            return PNPBRIDGE_FILE_NOT_FOUND;
        }
    }

    // Serialize the json file using parson
    *config = json_parse_file(filename);
    if (*config == NULL) {
        LogError("Failed to parse the config file. Please validate the JSON file in a JSON vailidator");
        return PNPBRIDGE_CONFIG_READ_FAILED;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpBridgeConfig_RetrieveConfiguration(JSON_Value* JsonConfig, PNPBRIDGE_CONFIGURATION* BridgeConfig)
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    PCONNECTION_PARAMETERS connParams = NULL;

    // Check for mandatory parameters
    TRY {
        if (NULL == JsonConfig) {
            LogError("JsonConfig is NULL");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        if (NULL == BridgeConfig) {
            LogError("BridgeConfig is NULL");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        // TODO: Validate json schema
        // json_validate(JsonConfig, SCHEMA);

        JSON_Object* jsonObject = json_value_get_object(JsonConfig);
        JSON_Object* pnpBridgeParameters = json_object_get_object(jsonObject, PNP_CONFIG_BRIDGE_PARAMETERS);
        if (NULL == pnpBridgeParameters) {
            LogError("%s is missing", PNP_CONFIG_BRIDGE_PARAMETERS);
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        // Read the trace flag option
        int traceOnBool = json_object_get_boolean(pnpBridgeParameters, PNP_CONFIG_TRACE_ON);
        BridgeConfig->TraceOn = (1 == traceOnBool);
        LogInfo("Tracing is %s", BridgeConfig->TraceOn ? "enabled" : "disabled");

        // TODO: Check for connection pcoarameters
        {
            JSON_Object* connectionParameters = json_object_get_object(pnpBridgeParameters, PNP_CONFIG_CONNECTION_PARAMETERS);
            if (NULL == connectionParameters) {
                LogError("%s is missing", PNP_CONFIG_CONNECTION_PARAMETERS);
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            connParams = PnpBridgeConfig_GetConnectionDetails(connectionParameters);
            if (NULL == connParams) {
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }
        }

        // Check for Device list
        {
            JSON_Array* devices = Configuration_GetConfiguredDevices(JsonConfig);
            for (int i = 0; i < (int)json_array_get_count(devices); i++) {
                JSON_Object *device = json_array_get_object(devices, i);

                // InterfaceId or SelfDescribing
                //
                // A device can publish 1 or multiple interface. In the simplest case, the 
                // device is required to sepcify the InterfaceId and the PnpAdapter is supposed
                // publish this interface. The SelfDescribing option allows a device to not specify
                // the interface and is used for following:
                //
                // 1. When an interface is not known beforehand i.e it is dynamically retrieved from a peripheral
                // 2. When a PnPAdapter wants to publish multiple interfaces
                //
                {
                    const char* selfDescribing = json_object_dotget_string(device, PNP_CONFIG_SELF_DESCRIBING);
                    const char* interfaceId = json_object_dotget_string(device, PNP_CONFIG_INTERFACE_ID);
                    if (NULL == selfDescribing) {
                        if (NULL == interfaceId) {
                            LogError("Device (%d) is missing %s", i, PNP_CONFIG_INTERFACE_ID);
                        }
                    }
                    else {
                        if (NULL != interfaceId) {
                            LogError("A %s device (%i) shouldn't have interface id", PNP_CONFIG_SELF_DESCRIBING, i);
                        }
                    }
                }

                // PnpParameters
                {
                    JSON_Object* pnpParams = Configuration_GetPnpParametersForDevice(device);
                    if (NULL == pnpParams) {
                        LogError("A device is missing PnpParameters");
                        result = PNPBRIDGE_INVALID_ARGS;
                        LEAVE;
                    }

                    // PnpParameters -> Idenitity
                    const char* identity = json_object_dotget_string(pnpParams, PNP_CONFIG_IDENTITY);
                    if (NULL == identity) {
                        LogError("PnpParameter is missing Adapter identity");
                        result = PNPBRIDGE_INVALID_ARGS;
                        LEAVE;
                    }
                }

                // DiscoveryParameters
                {
                    JSON_Object* discParams = Configuration_GetDiscoveryParametersForDevice(device);
                    if (discParams) {
                        // DiscoveryParameters -> Idenitity
                        const char* identity = json_object_dotget_string(discParams, PNP_CONFIG_IDENTITY);
                        if (NULL == identity) {
                            LogError("DiscoveryParameters is missing Adapter identity");
                            result = PNPBRIDGE_INVALID_ARGS;
                            LEAVE;
                        }
                    }
                }
            }
        }

        JSON_Array *devices = Configuration_GetConfiguredDevices(JsonConfig);
        if (NULL == devices) {
            LogError("No configured devices in the pnpbridge config");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }
        
        // Assign output values
        BridgeConfig->JsonConfig = JsonConfig;
        BridgeConfig->ConnParams = connParams;

    } FINALLY {
        if (!PNPBRIDGE_SUCCESS(result)) {
            if (connParams) {
                free(connParams);
            }
        }
    }

    return result;
}

PNPBRIDGE_RESULT PnpBridgeConfig_GetJsonValueFromString(const char *configString, JSON_Value** config)
{
    if (NULL == configString || NULL == config) {
        LogError("PnpBridgeConfig_GetJsonValueFromString: Invalid parameters");
        return PNPBRIDGE_INVALID_ARGS;
    }

    *config = json_parse_string(configString);
    if (*config == NULL) {
        LogError("PnpBridgeConfig_GetJsonValueFromString: Failed to parse config string");
        return PNPBRIDGE_CONFIG_READ_FAILED;
    }

    return PNPBRIDGE_OK;
}

JSON_Array* Configuration_GetConfiguredDevices(JSON_Value* config) {
    JSON_Object* jsonObject = json_value_get_object(config);
    JSON_Array *devices = json_object_dotget_array(jsonObject, PNP_CONFIG_DEVICES);

    return devices;
}

JSON_Object* Configuration_GetPnpParametersForDevice(JSON_Object* device) {
    
    if (device == NULL) {
        return NULL;
    }

    JSON_Object* pnpParams = json_object_dotget_object(device, PNP_CONFIG_PNP_PARAMETERS);
    return pnpParams;
}

JSON_Object* Configuration_GetMatchParametersForDevice(JSON_Object* device)
{
    if (device == NULL) {
        return NULL;
    }

    JSON_Object* filterParams = json_object_dotget_object(device, PNP_CONFIG_MATCH_FILTERS);
    JSON_Object* matchParams = json_object_dotget_object(filterParams, PNP_CONFIG_MATCH_PARAMETERS);
    return matchParams;
}

JSON_Object* 
Configuration_GetDiscoveryParametersForDevice(
    _In_ JSON_Object* device
    )
{    
    if (device == NULL) {
        return NULL;
    }

    JSON_Object* discoveryParams = json_object_dotget_object(device, PNP_CONFIG_DISCOVERY_PARAMETERS);
    return discoveryParams;
}

JSON_Object* Configuration_GetAdapterParameters(JSON_Value* config, const char* identity, const char* adapterType) {
    JSON_Object* jsonObject = json_value_get_object(config);
    JSON_Object *adapter = json_object_dotget_object(jsonObject, adapterType);
    if (NULL == adapter) {
        return NULL;
    }

    JSON_Array* params = json_object_dotget_array(adapter, PNP_CONFIG_PARAMETERS);
    if (NULL == params) {
        return NULL;
    }

    for (int j = 0; j < (int)json_array_get_count(params); j++) {
        JSON_Object *param = json_array_get_object(params, j);
        const char* id = json_object_dotget_string(param, PNP_CONFIG_IDENTITY);
        if (NULL != id) {
            if (strcmp(id, identity) == 0) {
                return param;
            }
        }
    }

    return NULL;
}

JSON_Object* Configuration_GetPnpParameters(JSON_Value* config, const char* identity) {
    return Configuration_GetAdapterParameters(config, identity, PNP_CONFIG_PNP_ADAPTERS);
}

JSON_Object* Configuration_GetDiscoveryParameters(JSON_Value* config, const char* identity) {
    return Configuration_GetAdapterParameters(config, identity, PNP_CONFIG_DISCOVERY_ADAPTERS);
}

PNPBRIDGE_RESULT Configuration_IsDeviceConfigured(JSON_Value* config, JSON_Object* message, JSON_Object** device) {
    JSON_Array *devices = Configuration_GetConfiguredDevices(config);
    JSON_Object* discMatchParams;
    char* pnpAdapterIdentity = NULL;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    *device = NULL;

    TRY {
        discMatchParams = json_object_get_object(message, PNP_CONFIG_MATCH_PARAMETERS);
        for (int i = 0; i < (int)json_array_get_count(devices); i++) {
            bool foundMatch = false;
            JSON_Object* dev = json_array_get_object(devices, i);
            JSON_Object* pnpParams = Configuration_GetPnpParametersForDevice(dev);
            if (NULL == pnpParams) {
                continue;
            }

            JSON_Object* matchCriteria = json_object_dotget_object(dev, PNP_CONFIG_MATCH_FILTERS);
          
            const char* matchType = json_object_get_string(matchCriteria, PNP_CONFIG_MATCH_TYPE);
            const char* currPnpAdapterId = json_object_get_string(pnpParams, PNP_CONFIG_IDENTITY);
            bool exactMatch = strcmp(matchType, PNP_CONFIG_MATCH_TYPE_EXACT) == 0;

            if (NULL != discMatchParams) {
                JSON_Object* matchParameters = json_object_dotget_object(matchCriteria, PNP_CONFIG_MATCH_PARAMETERS);
                const size_t matchParameterCount = json_object_get_count(matchParameters);
                for (int j = 0; j < (int)matchParameterCount; j++) {
                    const char* name = json_object_get_name(matchParameters, j);
                    const char* value1 = json_object_get_string(matchParameters, name);

                    if (!json_object_dothas_value(discMatchParams, name)) {
                        break;
                    }

                    const char* value2 = json_object_get_string(discMatchParams, name);
                    bool match;
                    if (exactMatch) {
                        match = 0 != strstr(value2, value1);
                    }
                    else {
                        match = 0 == strcmp(value2, value1);
                    }
                    if (false == match) {
                        foundMatch = false;
                        break;
                    }
                    else {
                        foundMatch = true;
                    }
                }
            }
            else {
                char* selfDescribing = (char*)json_object_get_string(dev, PNP_CONFIG_SELF_DESCRIBING);
                if (NULL != selfDescribing && 0 == strcmp(selfDescribing, PNP_CONFIG_TRUE)) {
                    JSON_Object* discAdapterParams = Configuration_GetDiscoveryParametersForDevice(dev);
                    const char* discAdapterId = json_object_get_string(discAdapterParams, PNP_CONFIG_IDENTITY);
                    const char* messageId = json_object_get_string(message, PNP_CONFIG_IDENTITY);
                    if (NULL != discAdapterId && NULL != messageId && 0 == strcmp(discAdapterId, messageId)) {
                        foundMatch = true;
                    }
                }
            }

            if (foundMatch) {
                pnpAdapterIdentity = (char*)currPnpAdapterId;
                *device = dev;
                result = PNPBRIDGE_OK;
                LEAVE;
            }
        }
    }
    FINALLY
    {
        if (NULL == *device) {
            result = PNPBRIDGE_INVALID_ARGS;
        }
    }

    return result;
}
