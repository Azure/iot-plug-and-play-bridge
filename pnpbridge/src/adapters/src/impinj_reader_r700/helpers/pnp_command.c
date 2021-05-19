#include "pnp_command.h"
#include "restapi.h"

/****************************************************************
Creates command response payload
****************************************************************/
int ImpinjReader_SetCommandResponse(
    unsigned char** CommandResponse,
    size_t* CommandResponseSize,
    const unsigned char* ResponseData)
{
    int result = PNP_STATUS_SUCCESS;

    if (ResponseData == NULL)
    {
        LogError("Impinj Reader Adapter:: Response Data is empty");
        *CommandResponseSize = 0;
        return PNP_STATUS_INTERNAL_ERROR;
    }

    *CommandResponseSize = strlen((char*)ResponseData);
    memset(CommandResponse, 0, sizeof(*CommandResponse));

    // Allocate a copy of the response data to return to the invoker. Caller will free this.
    if (mallocAndStrcpy_s((char**)CommandResponse, (char*)ResponseData) != 0)
    {
        LogError("Impinj Reader Adapter:: Unable to allocate response data");
        result = PNP_STATUS_INTERNAL_ERROR;
    }

    return result;
}

/****************************************************************
A callback for Direct Method
****************************************************************/
int OnCommandCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    int i;
    int httpStatus                   = PNP_STATUS_INTERNAL_ERROR;
    char* restBody                   = NULL;
    JSON_Value* jsonVal_RestResponse = NULL;
    char* restResponse               = NULL;
    char* restParameter              = NULL;
    const char* responseString       = NULL;
    PIMPINJ_R700_REST r700_Request   = NULL;

    LogJsonPretty("R700 : %s() enter.  Command Name=%s", CommandValue, __FUNCTION__, CommandName);

    for (i = 0; i < R700_REST_MAX; i++)
    {
        if (R700_REST_LIST[i].DtdlType != COMMAND)
        {
            continue;
        }
        else if (strcmp(CommandName, R700_REST_LIST[i].Name) == 0)  // find command name from list 
        {
            r700_Request = &R700_REST_LIST[i];

            LogInfo("R700 : Found Command %s : Property %s", CommandName, r700_Request->Name);

            if (device->ApiVersion < r700_Request->ApiVersion)
            {
                LogError("R700 : Unsupported API. Please upgrade firmware");
                httpStatus = R700_STATUS_NOT_ALLOWED;
                break;
            }

            switch (r700_Request->Request)
            {
                case PROFILES_INVENTORY_PRESETS_ID_SET:
                    // Receive Preset ID
                    restParameter = (char*)GetStringFromPayload(CommandValue, g_presetId);
                    restBody      = PreProcessSetPresetIdPayload(CommandValue);
                    break;
                
                case PROFILES_INVENTORY_PRESETS_ID_SET_PASSTHROUGH:
                    // Receive Preset ID
                    restParameter = (char*)GetStringFromPayload(CommandValue, g_presetId);
                    restBody      = (char*)GetStringFromPayload(CommandValue, g_presetObjectJSON);
                    break;

                case PROFILES_INVENTORY_PRESETS_ID_GET:
                case PROFILES_INVENTORY_PRESETS_ID_DELETE:
                case PROFILES_START: {
                    // Receive Preset ID
                    restParameter = (char*)GetStringFromPayload(CommandValue, g_presetId);
                    break;
                }

                case PROFILES_INVENTORY_TAG:
                    restParameter = (char*)PreProcessTagPresenceResponse(CommandValue);
                    break;

                case SYSTEM_POWER_SET:
                case SYSTEM_TIME_SET:
                    // takes JSON payload for REST API Body
                    restBody = json_serialize_to_string(CommandValue);
                    break;
            }

            // Call REST API
            switch (r700_Request->RestType)
            {
                case GET:
                    jsonVal_RestResponse = ImpinjReader_RequestGet(device, r700_Request, restParameter, &httpStatus);
                    break;
                case PUT:
                    jsonVal_RestResponse = ImpinjReader_RequestPut(device, r700_Request, restParameter, restBody, &httpStatus);
                    break;
                case POST:
                    jsonVal_RestResponse = ImpinjReader_RequestPost(device, r700_Request, restParameter, &httpStatus);
                    break;
                case DELETE:
                    jsonVal_RestResponse = ImpinjReader_RequestDelete(device, r700_Request, restParameter, &httpStatus);
                    break;
            }
            break;
        }
    }

    if (r700_Request != NULL)
    {
        LogJsonPretty("R700 : Command Response", jsonVal_RestResponse);

        switch (httpStatus)
        {
            case R700_STATUS_OK:
            case R700_STATUS_CREATED:
                restResponse = json_serialize_to_string(jsonVal_RestResponse);
                break;

            case R700_STATUS_ACCEPTED:
                restResponse = (char*)ImpinjReader_ProcessResponse(r700_Request, jsonVal_RestResponse, httpStatus);
                LogInfo("R700 : Command Response \"%s\"", restResponse);
                break;

            case R700_STATUS_BAD_REQUEST:
            case R700_STATUS_FORBIDDEN:
            case R700_STATUS_NOT_FOUND:
            case R700_STATUS_NOT_CONFLICT:
            case R700_STATUS_INTERNAL_ERROR:
                restResponse = ImpinjReader_ProcessErrorResponse(jsonVal_RestResponse, r700_Request->DtdlType);
                break;
            case R700_STATUS_NOT_ALLOWED:
                mallocAndStrcpy_s(&restResponse, g_unsupportedApiResponse);
                break;
        }
    }

    if (restResponse == NULL)
    {
        if (r700_Request->Request == SYSTEM_POWER_SET)
        {
            responseString = &g_powerSourceAlreadyConfigure[0];
        }
        else if (httpStatus == R700_STATUS_NO_CONTENT)
        {
            if (jsonVal_RestResponse)
            {
                restResponse = json_serialize_to_string(jsonVal_RestResponse);
            }
            else
            {
                responseString = &g_noContentResponse[0];
            }
        }
        else
        {
            responseString = &g_emptyCommandResponse[0];
        }
    }
    else
    {
        responseString = restResponse;
    }

    if (ImpinjReader_SetCommandResponse(CommandResponse, CommandResponseSize, responseString) != PNP_STATUS_SUCCESS)
    {
        httpStatus = PNP_STATUS_INTERNAL_ERROR;
    }

    // clean up
    if (restBody)
    {
        json_free_serialized_string(restBody);
    }

    if (jsonVal_RestResponse)
    {
        json_value_free(jsonVal_RestResponse);
    }

    if (restResponse)
    {
        free(restResponse);
    }

    return httpStatus;
}

char* PreProcessSetPresetIdPayload(
    JSON_Value* Payload)
{
    JSON_Object* jsonOjb_PresetId;
    JSON_Value* jsonVal_PresetId;
    JSON_Value* jsonVal_PresetObject    = NULL;
    JSON_Array* jsonArray_AntennaConfig = NULL;
    int i;

    if ((jsonOjb_PresetId = json_value_get_object(Payload)) == NULL)
    {
        LogError("R700 : Unable to retrieve command payload JSON object");
    }
    else
    {
        JSON_Object* jsonObj_TagAuthentication = NULL;
        JSON_Object* jsonObj_PowerSweeping     = NULL;

        if ((jsonVal_PresetObject = json_object_get_value(jsonOjb_PresetId, g_presetObject)) == NULL)
        {
            LogError("R700 : Unable to retrieve %s for Set Preset Id payload JSON.", g_presetObject);
        }
        else if ((jsonArray_AntennaConfig = json_object_dotget_array(jsonOjb_PresetId, g_presetObjectAntennaConfigs)) != NULL)
        {
            int arrayCount = json_array_get_count(jsonArray_AntennaConfig);

            for (i = 0; i < arrayCount; i++)
            {
                JSON_Object* jsonObj_AntennaConfig = json_array_get_object(jsonArray_AntennaConfig, i);
                JSON_Object* jsonObj_Filtering     = json_object_get_object(jsonObj_AntennaConfig, g_antennaConfigFiltering);

                if (jsonObj_Filtering)
                {
                    JSON_Array* jsonArray_Filters = json_object_get_array(jsonObj_Filtering, g_antennaConfigFilters);

                    if (jsonArray_Filters == NULL)
                    {
                        json_object_remove(jsonObj_AntennaConfig, g_antennaConfigFiltering);
                    }
                }
                // Remove empty tagAuthentication
                if ((jsonObj_TagAuthentication = json_object_get_object(jsonObj_AntennaConfig, g_presetObjectAntennaConfigsTagAuthentication)) != NULL)
                {
                    if (json_object_get_count(jsonObj_TagAuthentication) == 0)
                    {
                        json_object_remove(jsonObj_AntennaConfig, g_presetObjectAntennaConfigsTagAuthentication);
                    }
                }

                // Remove empty powerSweeping
                if ((jsonObj_PowerSweeping = json_object_get_object(jsonObj_AntennaConfig, g_presetObjectAntennaConfigsPowerSweeping)) != NULL)
                {
                    if (json_object_get_count(jsonObj_TagAuthentication) == 0)
                    {
                        json_object_remove(jsonObj_AntennaConfig, g_presetObjectAntennaConfigsPowerSweeping);
                    }
                }
            }
        }
    }

    if (jsonVal_PresetObject)
    {
        return json_serialize_to_string(jsonVal_PresetObject);
    }
    return NULL;
}

char* PreProcessTagPresenceResponse(
    JSON_Value* Payload)
{
    JSON_Object* jsonObj_Tag;
    size_t count;
    int i;
    char tagQueryBuffer[1024] = {0};
    char tagAntennaNumber[3]  = {0};
    char* tagQueryBufferPtr   = &tagQueryBuffer[0];
    char* returnBuffer        = NULL;

    if ((jsonObj_Tag = json_value_get_object(Payload)) == NULL)
    {
        LogError("R700 : Unable to retrieve Tag Presence payload JSON object");
    }
    else if ((count = json_object_get_count(jsonObj_Tag)) == 0)
    {
        LogInfo("R700 : Missing parameters");
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            JSON_Value* jsonVal_TagParam;
            const char* tagName;
            const char* tagValue;
            int tagNumber;
            const char* prefix;

            if ((jsonVal_TagParam = json_object_get_value_at(jsonObj_Tag, i)) == NULL)
            {
                LogError("R700 : Unable to retrieve Inventory Tag parameter index %d", i);
            }
            else if ((tagName = json_object_get_name(jsonObj_Tag, i)) == NULL)
            {
                LogError("R700 : Unable to retrieve name for Inventory Tag index %d", i);
            }
            else
            {
                switch (json_value_get_type(jsonVal_TagParam))
                {
                    case JSONString:
                        if ((tagValue = json_object_get_string(jsonObj_Tag, tagName)) == NULL)
                        {
                            LogError("R700 : Unable to retrieve string for Inventory Tag index %d", i);
                            continue;
                        }

                        break;
                    case JSONNumber:
                        if ((tagNumber = (int)json_object_get_number(jsonObj_Tag, tagName)) == 0)
                        {
                            LogError("R700 : Unable to retrieve number for Inventory Tag index %d", i);
                            continue;
                        }

                        sprintf(tagAntennaNumber, "%d", tagNumber);
                        tagValue = tagAntennaNumber;

                        break;
                }

                if (i == 0)
                {
                    prefix = "?";
                }
                else
                {
                    prefix = "&";
                }
                sprintf(tagQueryBufferPtr, "%s%s=%s", prefix, tagName, tagValue);
                tagQueryBufferPtr = &tagQueryBuffer[0] + strlen(tagQueryBuffer);
            }
        }
        LogInfo("R700 : Inventory Tag Query %s", tagQueryBuffer);

        mallocAndStrcpy_s(&returnBuffer, tagQueryBuffer);
    }

    return returnBuffer;
}