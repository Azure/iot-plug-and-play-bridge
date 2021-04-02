
#include "restapi.h"

MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(R700_REST_REQUEST, R700_REST_REQUEST_VALUES);

//#define DEBUG_REST

JSON_Value*
ImpinjReader_Convert_NetworkInterface(
    char* Json_String);

JSON_Value*
ImpinjReader_Convert_DeviceStatus(
    char* Json_String);

JSON_Value*
ImpinjReader_Convert_UpgradeStatus(
    char* Json_String);
/****************************************************************
REST APIS
****************************************************************/
JSON_Value*
ImpinjReader_RequestDelete(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    int* HttpStatus)
{
    char* jsonResult;
    JSON_Value* jsonVal = NULL;
    char endPointBuffer[128];
    char* endpoint;
    char* propertyName = R700_Request->Name;
    char api[]         = "DELETE";

    LogInfo("R700 : %s() API=%s", __FUNCTION__, MU_ENUM_TO_STRING(R700_REST_REQUEST, R700_Request->Request));

    if (Device->ApiVersion < R700_Request->ApiVersion)
    {
        LogError("R700 : API not supported.  Supported version %s", MU_ENUM_TO_STRING(R700_REST_VERSION, Device->ApiVersion));
        *HttpStatus = R700_STATUS_NOT_ALLOWED;
        return NULL;
    }

    if (Parameter)
    {
        snprintf(endPointBuffer, sizeof(endPointBuffer), R700_Request->EndPoint, Parameter);
        endpoint = (char*)&endPointBuffer;
    }
    else
    {
        endpoint = R700_Request->EndPoint;
    }

    LogInfo("R700 : Curl %s >> Endpoint \"%s\"", api, endpoint);
    jsonResult = curlStaticDelete(Device->curl_static_session, endpoint, HttpStatus);
    // LogInfo("R700 : Curl %s << Status %d", api, *HttpStatus);

    if (*HttpStatus >= R700_STATUS_BAD_REQUEST)
    {
        LogJsonPretty("R700 : Warn : %s HttpStatus %d Response Payload", jsonVal, api, *HttpStatus);
    }

    jsonVal = json_parse_string(jsonResult);

    return jsonVal;
}

JSON_Value*
ImpinjReader_RequestGet(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    int* HttpStatus)
{
    char* jsonResult;
    JSON_Value* jsonVal   = NULL;
    JSON_Object* jsonObj  = NULL;
    JSON_Array* jsonArray = NULL;
    JSON_Value_Type json_type;

    char endPointBuffer[128];
    char* endpoint;
    char* propertyName = R700_Request->Name;

    char buffer[R700_PRESET_ID_LENGTH];

    char api[] = "GET";

#ifdef DEBUG_REST
    LogInfo("R700 : %s() enter. API=%s API Ver=%s", __FUNCTION__, MU_ENUM_TO_STRING(R700_REST_REQUEST, R700_Request->Request), MU_ENUM_TO_STRING(R700_REST_VERSION, R700_Request->ApiVersion));
#endif

    if (Device->ApiVersion < R700_Request->ApiVersion)
    {
        LogError("R700 : API not supported.  Supported version %s", MU_ENUM_TO_STRING(R700_REST_VERSION, Device->ApiVersion));
        *HttpStatus = R700_STATUS_NOT_ALLOWED;
        return NULL;
    }

    if (Parameter)
    {
        snprintf(endPointBuffer, sizeof(endPointBuffer), R700_Request->EndPoint, Parameter);
        endpoint = (char*)&endPointBuffer;
    }
    else
    {
        endpoint = R700_Request->EndPoint;
    }

#ifdef DEBUG_REST
    LogInfo("R700 : Curl %s >> Endpoint \"%s\"", api, endpoint);
#endif

    if (R700_Request->Request == READER_STATUS_POLL)
    {
        jsonResult = curlStaticGet(Device->curl_polling_session, endpoint, HttpStatus);
    }
    else
    {
        jsonResult = curlStaticGet(Device->curl_static_session, endpoint, HttpStatus);
    }

    // LogInfo("R700 : Curl %s << Status %d", api, *HttpStatus);

    switch (R700_Request->Request)
    {
        case READER_STATUS_POLL:
        case READER_STATUS_GET:
        case READER_STATUS:
            jsonVal = ImpinjReader_Convert_DeviceStatus(jsonResult);
            break;

        case KAFKA:
            jsonVal = JSONArray2DtdlMap(jsonResult, g_kafkaBootstraps, "BootStraps");
            break;

        case PROFILES:
            jsonVal = JSONArray2DtdlMap(jsonResult, NULL, "Profile");
            break;

        case PROFILES_INVENTORY_PRESETS_SCHEMA:
            sprintf(buffer, g_presetSchemaFormat, g_NotSupportedMessage);
            jsonVal = json_parse_string(buffer);
            break;

        case PROFILES_INVENTORY_PRESETS_IDS:
            sprintf(buffer, g_presetsFormat, jsonResult);
            jsonVal = json_parse_string(buffer);
            break;

        case SYSTEM_IMAGE_UPGRADE_GET:
            jsonVal = ImpinjReader_Convert_UpgradeStatus(jsonResult);
            break;

        case SYSTEM_NETORK_INTERFACES: {
            // need to convert arrays to Map
            jsonVal = ImpinjReader_Convert_NetworkInterface(jsonResult);
            break;
        }
        case SYSTEM_REGION:
            jsonVal = Remove_JSON_Array(jsonResult, impinjReader_property_system_region_selectableRegions);
            break;

        default:
            jsonVal = json_parse_string(jsonResult);
            break;
    }

    if (*HttpStatus >= R700_STATUS_BAD_REQUEST)
    {
        LogJsonPretty("R700 : Warn : %s HttpStatus %d Response Payload", jsonVal, api, *HttpStatus);
    }

    return jsonVal;
}

JSON_Value*
ImpinjReader_RequestPut(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    const char* Body,
    int* HttpStatus)
{
    char* body       = NULL;
    char* bodyToSend = NULL;
    char* jsonResult;
    char* endpoint      = R700_Request->EndPoint;
    JSON_Value* jsonVal = NULL;
    char endPointBuffer[128];
    char api[] = "PUT";

#ifdef DEBUG_REST
    LogInfo("R700 : %s() API=%s", __FUNCTION__, MU_ENUM_TO_STRING(R700_REST_REQUEST, R700_Request->Request));
#endif

    if (Device->ApiVersion < R700_Request->ApiVersion)
    {
        LogError("R700 : API not supported.  Supported version %s", MU_ENUM_TO_STRING(R700_REST_VERSION, Device->ApiVersion));
        *HttpStatus = R700_STATUS_NOT_ALLOWED;
        return NULL;
    }

    if (Parameter)
    {
        snprintf(endPointBuffer, sizeof(endPointBuffer), R700_Request->EndPoint, Parameter);
        endpoint = (char*)&endPointBuffer;
    }
    else
    {
        endpoint = R700_Request->EndPoint;
    }

    switch (R700_Request->Request)
    {
        case KAFKA:
            body = DtdlMap2JSONArray(Body, g_kafkaBootstraps);
            break;
    }

    if (body)
    {
        bodyToSend = body;
    }
    else
    {
        bodyToSend = (char*)Body;
    }
#ifdef DEBUG_REST
    LogInfo("R700 : Curl %s Body %s >> Endpoint '%s'", api, bodyToSend, endpoint);
#endif
    jsonResult = curlStaticPut(Device->curl_static_session, endpoint, bodyToSend, HttpStatus);
    // LogInfo("R700 : Curl %s << Status %d", api, *HttpStatus);

    if (*HttpStatus >= R700_STATUS_BAD_REQUEST)
    {
        LogJsonPretty("R700 : Warn : %s HttpStatus %d Response Payload", jsonVal, api, *HttpStatus);
    }

    jsonVal = json_parse_string(jsonResult);

    if (body)
    {
        json_free_serialized_string(body);
    }

    return jsonVal;
}

JSON_Value*
ImpinjReader_RequestPost(
    PIMPINJ_READER Device,
    PIMPINJ_R700_REST R700_Request,
    const char* Parameter,
    int* HttpStatus)
{
    char* jsonResult;
    char* endpoint = R700_Request->EndPoint;
    char* postData;
    JSON_Value* jsonVal = NULL;
    char api[]          = "POST";

#ifdef DEBUG_REST
    LogInfo("R700 : %s() API=%s", __FUNCTION__, MU_ENUM_TO_STRING(R700_REST_REQUEST, R700_Request->Request));
#endif

    if (Device->ApiVersion < R700_Request->ApiVersion)
    {
        LogError("R700 : API not supported.  Supported version %s", MU_ENUM_TO_STRING(R700_REST_VERSION, Device->ApiVersion));
        *HttpStatus = R700_STATUS_NOT_ALLOWED;
        return NULL;
    }

    if (R700_Request->Request == PROFILES_START)
    {
        char endPointData[200] = {0};
        sprintf(endPointData, R700_Request->EndPoint, Parameter);
        endpoint = (char*)&endPointData;
        postData = NULL;
    }
    else
    {
        endpoint = R700_Request->EndPoint;
        postData = (char*)Parameter;
    }
#ifdef DEBUG_REST
    LogInfo("R700 : Curl %s >> Endpoint \"%s\"", api, endpoint);
#endif
    jsonResult = curlStaticPost(Device->curl_static_session, endpoint, postData, HttpStatus);
    // LogInfo("R700 : Curl %s << Status %d", api, *HttpStatus);

    if (*HttpStatus >= R700_STATUS_BAD_REQUEST)
    {
        LogJsonPretty("R700 : Warn : %s HttpStatus %d Response Payload", jsonVal, api, *HttpStatus);
    }

    jsonVal = json_parse_string(jsonResult);

    return jsonVal;
}

/****************************************************************
Process REST API Response
Caller must free char* buffer
****************************************************************/
const char*
ImpinjReader_ProcessResponse(
    IMPINJ_R700_REST* RestRequest,
    JSON_Value* JsonVal_Response,
    int HttpStatus)
{
    JSON_Value* jsonVal;
    JSON_Object* jsonObj;
    char* descriptionBuffer = NULL;
    size_t size             = 0;
#ifdef DEBUG_REST
    LogJsonPretty("R700 : %s() enter HTTP Status=%d", JsonVal_Response, __FUNCTION__, HttpStatus);
#endif
    switch (HttpStatus)
    {
        case R700_STATUS_OK:
        case R700_STATUS_CREATED:
        case R700_STATUS_NO_CONTENT:
            size = strlen(g_successResponse) + 1;
            if ((descriptionBuffer = (char*)malloc(size)) == NULL)
            {
                LogError("R700 : Unable to allocate %ld bytes buffer for "
                         "description for SYSTEM_POWER_SET",
                         size);
            }
            mallocAndStrcpy_s((char**)&descriptionBuffer, &g_successResponse[0]);
            break;

        case R700_STATUS_ACCEPTED:
            // receives StatusResponse
            // {
            //   "message": "string"
            // }
            if ((size = json_serialization_size(JsonVal_Response)) == 0)
            {
                LogError("R700 : Unable to get serialization buffer size");
            }
            else if ((jsonObj = json_value_get_object(JsonVal_Response)) == NULL)
            {
                LogError("R700 : Unable to retrieve JSON Object for %s", g_statusResponseMessage);
            }
            else if (json_object_get_count(jsonObj) == 0)
            {
                LogInfo("R700 : Empty Status Response");
            }
            else if ((jsonVal = json_object_get_value(jsonObj, g_statusResponseMessage)) == NULL)
            {
                LogError("R700 : Unable to retrieve JSON Value for %s", g_statusResponseMessage);
            }
            else if (json_value_get_type(jsonVal) != JSONString)
            {
                LogError("R700 : JSON field %s is not string", g_errorResponseMessage);
            }
            else
            {
                if (RestRequest->DtdlType == COMMAND)
                {
                    // Command Response in JSON
                    if ((descriptionBuffer = (char*)calloc(1, size + 1)) == NULL)
                    {
                        LogError("R700 : Unable to allocate %ld bytes buffer "
                                 "for COMMAND response",
                                 size);
                    }
                    else if (json_serialize_to_buffer(jsonVal, (char*)descriptionBuffer, size + 1) != JSONSuccess)
                    {
                        LogError("R700 : Failed to serialize description to "
                                 "buffer for COMMAND");
                    }
                }
                else if (RestRequest->DtdlType == WRITABLE)
                {
                    // Writable Property Description in string
                    const char* tmpBuffer = NULL;
                    if ((tmpBuffer = json_object_get_string(jsonObj, g_errorResponseMessage)) == NULL)
                    {
                        LogError("R700 : Unable to retrive string from "
                                 "ResponseStatus for Writable Property");
                    }
                    else if (mallocAndStrcpy_s(&descriptionBuffer, tmpBuffer) != 0)
                    {
                        LogError("R700 : mallocAndStrcpy_s failed to copy "
                                 "description for Writable Property "
                                 "description for %ld bytes",
                                 strlen(tmpBuffer) + 1);
                    }
                }
            }

            break;

        case R700_STATUS_BAD_REQUEST:
        case R700_STATUS_FORBIDDEN:
        case R700_STATUS_NOT_FOUND:
        case R700_STATUS_NOT_CONFLICT:
        case R700_STATUS_INTERNAL_ERROR:
            // should not happen
            assert(false);
            // descriptionBuffer =
            // ImpinjReader_ProcessErrorResponse(JsonVal_Response,
            // RestRequest->DtdlType);
            break;
    }

    LogInfo("R700 : %s Http Status \"%d\" Response '%s'", __FUNCTION__, HttpStatus, descriptionBuffer);

    return descriptionBuffer;
}

/****************************************************************
Process ErrorResponse and converts them to single string for properties
or JSON for command response.

Caller must free char* buffer

{
  "message": "Message String.",
  "invalidPropertyId": "Invalid ID",
  "detail": "Detail"
}

to

"Message String. Invalid ID. Detail."

or

"{"message":"Message String. Invalid ID. Detail."}"

****************************************************************/
char* ImpinjReader_ProcessErrorResponse(
    JSON_Value* JsonVal_ErrorResponse,
    R700_DTDL_TYPE DtdlType)
{
    JSON_Object* jsonObj_ErrorResponse    = NULL;
    JSON_Value* jsonVal_Message           = NULL;
    JSON_Value* jsonVal_InvalidPropertyId = NULL;
    JSON_Value* jsonVal_Details           = NULL;
    const char* message                   = NULL;
    const char* invalidPropertyId         = NULL;
    const char* detail                    = NULL;
    char* returnBuffer                    = NULL;
    char* messageBuffer                   = NULL;
    size_t size                           = 0;

    LogJsonPretty("R700 : Processing ErrorResponse", JsonVal_ErrorResponse, JsonVal_ErrorResponse);

    if ((jsonObj_ErrorResponse = json_value_get_object(JsonVal_ErrorResponse)) == NULL)
    {
        LogError("R700 : Unable to get Error Response JSON object");
    }
    else
    {
        // message
        if ((jsonVal_Message = json_object_get_value(jsonObj_ErrorResponse, g_errorResponseMessage)) != NULL)
        {
            if (json_value_get_type(jsonVal_Message) != JSONString)
            {
                LogError("R700 : JSON field %s is not string", g_errorResponseMessage);
            }
            else
            {
                message = json_value_get_string(jsonVal_Message);
                size += strlen(message);
            }
        }
        // invalidPropertyId
        if ((jsonVal_InvalidPropertyId = json_object_get_value(jsonObj_ErrorResponse, g_errorResponseInvalidPropertyId)) != NULL)
        {
            if (json_value_get_type(jsonVal_InvalidPropertyId) != JSONString)
            {
                LogError("R700 : JSON field %s is not string", g_errorResponseInvalidPropertyId);
            }
            else
            {
                invalidPropertyId = json_value_get_string(jsonVal_InvalidPropertyId);
                size += strlen(invalidPropertyId);
            }
        }
        // details
        if ((jsonVal_Details = json_object_get_value(jsonObj_ErrorResponse, g_errorResponseDetail)) != NULL)
        {
            if (json_value_get_type(jsonVal_Details) != JSONString)
            {
                LogError("R700 : JSON field %s is not string", g_errorResponseDetail);
            }
            else
            {
                detail = json_value_get_string(jsonVal_Details);
                size += strlen(detail);
            }
        }

        // construct message
        if ((messageBuffer = (char*)calloc(1, size + 1)) == NULL)
        {
            LogError("R700 : Unable to allocate buffer for description for %ld "
                     "bytes",
                     size);
        }
        else
        {
            if (message)
            {
                strcat(messageBuffer, message);
            }

            if (invalidPropertyId)
            {
                strcat(messageBuffer, invalidPropertyId);
            }

            if (detail)
            {
                strcat(messageBuffer, detail);
            }
            LogInfo("R700 : Error Response %s", messageBuffer);
        }

        // Command responses must be JSON
        if (DtdlType == COMMAND)
        {
            if ((returnBuffer = (char*)malloc(size + strlen(g_responseFormat) + 1)) == NULL)
            {
                LogError("R700 : Unable to allocate buffer for command "
                         "response for %ld bytes",
                         size);
            }
            else
            {
                sprintf(returnBuffer, g_responseFormat, messageBuffer);
                free(messageBuffer);
            }
        }
        else
        {
            returnBuffer = messageBuffer;
        }
    }

    return returnBuffer;
}

/****************************************************************
Process Network Interface

Payload Example
[
    {
        "interfaceId": 1,
        "interfaceName": "eth0",
        "status": "connected",
        "hardwareAddress": "00:16:25:14:04:63",
        "networkAddress": [
            {
                "protocol": "ipv4",
                "address": "192.168.120.101",
                "prefix": 24,
                "gateway": "192.168.120.1"
            },
            {
                "protocol": "ipv6",
                "address": "fe80::18b6:7071:535d:16df",
                "prefix": 64,
                "gateway": ""
            }
        ]
    }
]
****************************************************************/
JSON_Value*
ImpinjReader_Convert_NetworkInterface(
    char* Json_String)
{
    int networkInterfaceCount;
    JSON_Value* jsonVal_NetworkInterfaces = NULL;
    JSON_Array* jsonArray_NetworkInterfaces;
    JSON_Object* jsonObj_NetworkInterfaces;

    JSON_Value* jsonVal_NetworkInterfaces_Map = NULL;
    JSON_Object* jsonObj_NetworkInterfaces_Map;
    char buffer[32];

    LogJsonPrettyStr("R700 : %s() enter", Json_String, __FUNCTION__);

    if ((jsonVal_NetworkInterfaces = json_parse_string(Json_String)) == NULL)
    {
        LogError("R700 : Unable to parse Network Interface payload");
    }
    else if ((jsonArray_NetworkInterfaces = json_value_get_array(jsonVal_NetworkInterfaces)) == NULL)
    {
        LogError("R700 : Cannot retrieve array in Network Interface payload");
    }
    else if ((networkInterfaceCount = json_array_get_count(jsonArray_NetworkInterfaces)) == 0)
    {
        LogInfo("R700 : Empty Network Interface Array in Network Interface payload");
    }
    else if ((jsonVal_NetworkInterfaces_Map = json_value_init_object()) == NULL)
    {
        LogError("R700 : Cannot create JSON Value for Network Interface Map");
    }
    else if ((jsonObj_NetworkInterfaces_Map = json_value_get_object(jsonVal_NetworkInterfaces_Map)) == NULL)
    {
        LogError("R700 : Cannot retrieve JSON Object for Network Interface Map");
    }
    else
    {
        // loop Network Interfaces
        int i;

        for (i = 0; i < networkInterfaceCount; i++)
        {
            char* networkAddress = NULL;
            JSON_Value* jsonVal_NetworkInterface;
            JSON_Value* jsonVal_NetworkAddress_Map;

            if ((jsonVal_NetworkInterfaces = json_array_get_value(jsonArray_NetworkInterfaces, i)) == NULL)
            {
                LogError("R700 : Cannot retrieve Network Interface Array index %d", i);
            }
            else if ((networkAddress = json_serialize_to_string(jsonVal_NetworkInterfaces)) == NULL)
            {
                LogError("R700 : Cannot serialize Network Interface Array index %d", i);
            }
            else if ((jsonVal_NetworkAddress_Map = JSONArray2DtdlMap(networkAddress, "networkAddress", "networkAddress")) == NULL)
            {
                LogError("R700 : Cannot convert array to map for Network Interface Array index %d", i);
            }
            else
            {
                //LogJsonPretty("R700 : Network Address Map", jsonVal_NetworkAddress_Map);
                sprintf(buffer, "NetworkInterface%d", i);
                json_object_set_value(jsonObj_NetworkInterfaces_Map, buffer, jsonVal_NetworkAddress_Map);
            }

            if (networkAddress)
            {
                json_free_serialized_string(networkAddress);
            }
        }
    }

    if (jsonVal_NetworkInterfaces)
    {
        json_value_free(jsonVal_NetworkInterfaces);
    }

    //LogJsonPretty("R700 : Network Interface Map", jsonVal_NetworkInterfaces_Map);
    return jsonVal_NetworkInterfaces_Map;
}

/****************************************************************
Workaround for IoT Explorer.
Command response containing "status" confused IoT Explorer
****************************************************************/
JSON_Value*
ImpinjReader_Convert_DeviceStatus(
    char* Json_String)
{
    const char* deviceStatus          = NULL;
    JSON_Value* jsonVal_deviceStatus  = NULL;
    JSON_Object* jsonObj_deviceStatus = NULL;

#ifdef DEBUG_REST
    LogJsonPrettyStr("R700 : %s() enter", Json_String, __FUNCTION__);
#endif

    if ((jsonVal_deviceStatus = json_parse_string(Json_String)) == NULL)
    {
        LogError("R700 : Unable to parse Reader Status JSON");
    }
    else if ((jsonObj_deviceStatus = json_value_get_object(jsonVal_deviceStatus)) == NULL)
    {
        LogError("R700 : Cannot retrieve Reader Status JSON object");
    }
    else if ((deviceStatus = json_object_get_string(jsonObj_deviceStatus, "status")) == NULL)
    {
        LogError("R700 : Cannot retrieve status in Reader Status JSON object");
    }
    else if (json_object_set_string(jsonObj_deviceStatus, "deviceStatus", deviceStatus) != JSONSuccess)
    {
        LogError("R700 : Failed to add deviceStatus to Reader Status payload");
    }
    else if (json_object_remove(jsonObj_deviceStatus, "status") != JSONSuccess)
    {
        LogError("R700 : Failed to remove status from Reader Status payload");
    }

    return jsonVal_deviceStatus;
}

/****************************************************************
Workaround for IoT Explorer.
Command response containing "status" confused IoT Explorer
****************************************************************/
JSON_Value*
ImpinjReader_Convert_UpgradeStatus(
    char* Json_String)
{
    const char* deviceStatus          = NULL;
    JSON_Value* jsonVal_deviceStatus  = NULL;
    JSON_Object* jsonObj_deviceStatus = NULL;

    LogJsonPrettyStr("R700 : %s() enter", Json_String, __FUNCTION__);

    if ((jsonVal_deviceStatus = json_parse_string(Json_String)) == NULL)
    {
        LogError("R700 : Unable to parse Reader Status JSON");
    }
    else if ((jsonObj_deviceStatus = json_value_get_object(jsonVal_deviceStatus)) == NULL)
    {
        LogError("R700 : Cannot retrieve Reader Status JSON object");
    }
    else if ((deviceStatus = json_object_get_string(jsonObj_deviceStatus, "status")) == NULL)
    {
        LogError("R700 : Cannot retrieve status in Reader Status JSON object");
    }
    else if (json_object_set_string(jsonObj_deviceStatus, "upgradeStatus", deviceStatus) != JSONSuccess)
    {
        LogError("R700 : Failed to add deviceStatus to Reader Status payload");
    }
    else if (json_object_remove(jsonObj_deviceStatus, "status") != JSONSuccess)
    {
        LogError("R700 : Failed to remove status from Reader Status payload");
    }

    return jsonVal_deviceStatus;
}