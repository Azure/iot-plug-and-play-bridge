// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "PnpBridgeCommon.h"

PNPBRIDGE_RESULT PnpBridgeConfig_ReadConfigurationFromFile(const char *filename, JSON_Value** config) {
    if (NULL == filename || NULL == config) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    *config = json_parse_file(filename);
    if (*config == NULL) {
        return PNPBRIDGE_CONFIG_READ_FAILED;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpBridgeConfig_ReadConfigurationFromString(const char *configString, JSON_Value** config) {
    if (NULL == configString || NULL == config) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    *config = json_parse_string(configString);
    if (*config == NULL) {
        return PNPBRIDGE_CONFIG_READ_FAILED;
    }

    return PNPBRIDGE_OK;
}

JSON_Array* Configuration_GetConfiguredDevices(JSON_Value* config) {
    JSON_Object* jsonObject = json_value_get_object(config);
    JSON_Array *devices = json_object_dotget_array(jsonObject, "Devices");

    return devices;
}

JSON_Object* Configuration_GetPnpParametersForDevice(JSON_Object* device) {
    if (device == NULL) {
        return NULL;
    }

    JSON_Object* discoveryParams = json_object_dotget_object(device, "PnpParameters");
    return discoveryParams;
}


JSON_Object* Configuration_GetDiscoveryParametersForDevice(JSON_Object* device) {
    if (device == NULL) {
        return NULL;
    }

    JSON_Object* discoveryParams = json_object_dotget_object(device, "DiscoveryParameters");
    return discoveryParams;
}

const char* Configuration_GetConnectionString(JSON_Value* config) {
    JSON_Object* jsonObject = json_value_get_object(config);
    JSON_Object *pnpBridgeParams = json_object_dotget_object(jsonObject, "PnpBridgeParameters");
    if (NULL == pnpBridgeParams) {
        return NULL;
    }

    return json_object_dotget_string(pnpBridgeParams, "ConnectionString");
}

JSON_Object* Configuration_GetAdapterParameters(JSON_Value* config, const char* identity, const char* adapterType) {
    JSON_Object* jsonObject = json_value_get_object(config);
    JSON_Object *adapter = json_object_dotget_object(jsonObject, adapterType);
    if (NULL == adapter) {
        return NULL;
    }

    JSON_Array* params = json_object_dotget_array(adapter, "Parameters");
    if (NULL == params) {
        return NULL;
    }

    for (int j = 0; j < (int)json_array_get_count(params); j++) {
        JSON_Object *param = json_array_get_object(params, j);
        const char* id = json_object_dotget_string(param, "Identity");
        if (NULL != id) {
            if (strcmp(id, identity) == 0) {
                return param;
            }
        }
    }

    return NULL;
}

JSON_Object* Configuration_GetPnpParameters(JSON_Value* config, const char* identity) {
    return Configuration_GetAdapterParameters(config, identity, "PnpAdapters");
}

JSON_Object* Configuration_GetDiscoveryParameters(JSON_Value* config,const char* identity) {
    return Configuration_GetAdapterParameters(config, identity, "DiscoveryAdapters");
}

PNPBRIDGE_RESULT Configuration_IsDeviceConfigured(JSON_Value* config, JSON_Object* Message, JSON_Object** Device) {
    JSON_Array *devices = Configuration_GetConfiguredDevices(config);
    JSON_Object* discMatchParams;
    int res = -1;
    bool foundMatch = false;

   // const char* formatId = json_object_dotget_string(Message, "Identity");
    discMatchParams = json_object_get_object(Message, "MatchParameters");

    // There needs to be only one match at the end.
    for (int i = 0; i < (int)json_array_get_count(devices); i++) {
        JSON_Object *device = json_array_get_object(devices, i);
        JSON_Object* moduleParams = Configuration_GetPnpParametersForDevice(device);
        if (NULL == moduleParams) {
            continue;
        }

//        const char* deviceFormatId = json_object_dotget_string(moduleParams, "Identity");
        if (true) { //strcmp(deviceFormatId, formatId) == 0) {
            JSON_Object* matchCriteria = json_object_dotget_object(device, "MatchFilters");
            const char* matchType = json_object_get_string(matchCriteria, "MatchType");
            if (strcmp(matchType, "*") == 0) {
                if (true == foundMatch) {
                    LogError("Found multiple devices in the config that match the filter");
                    res = -1;
                    goto end;
                }

                foundMatch = true;
                res = 0;
                *Device = device;
                continue;
            }

            if (NULL == discMatchParams) {
                continue;
            }

            JSON_Object* matchParameters = json_object_dotget_object(matchCriteria, "MatchParameters");
            const size_t matchParameterCount = json_object_get_count(matchParameters);
            for (int j = 0; j < (int)matchParameterCount; j++) {
                const char* name = json_object_get_name(matchParameters, j);
                const char* value = json_object_get_string(matchParameters, name);

                if (!json_object_dothas_value(discMatchParams, name)) {
                    res = -1;
                    goto end;
                }

                const char* value2 = json_object_get_string(discMatchParams, name);
                if (strstr(value2, value) == 0) {
                    res = -1;
                    goto end;
                }
                else {
                    if (true == foundMatch) {
                        LogError("Found multiple devices in the config that match the filter");
                        res = -1;
                        goto end;
                    }

                    const char* interfaceId = json_object_dotget_string(device, "InterfaceId");
                    json_object_set_string(Message, "InterfaceId", interfaceId);
                    foundMatch = true;
                    *Device = device;
                    res = 0;
                }
            }
            res = 0;
            goto end;
        }
    }

end:
    return res;
}