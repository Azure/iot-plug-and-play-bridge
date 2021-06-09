#include "pnp_utils.h"
#include "restapi.h"
#include "pnp_command.h"
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/****************************************************************
Helper function

Print outs JSON payload with indent
Works only with JSONObject
****************************************************************/
void LogJsonPretty(
    const char* MsgFormat,
    JSON_Value* JsonValue, ...)
{
    JSON_Value_Type jsonType = JSONError;
    char local_Buffer[128]   = {0};
    char* bufferFormat       = NULL;
    char* bufferJson         = NULL;
    char* bufferPtr;
    size_t json_Size = 0;

    va_list argList;
    int len;
    int written;
    JSON_Object* jsonObj  = NULL;
    JSON_Array* jsonArray = NULL;

    if (JsonValue == NULL)
    {
        //LogInfo("R700 : JsonValue = NULL");
    }
    else if ((jsonType = json_value_get_type(JsonValue)) == JSONError)
    {
        LogInfo("R700 : Unable to retrieve JSON type");
    }
    else if (jsonType == JSONNull)
    {
        LogInfo("R700 : JSON is Null type");
    }
    else if ((jsonType == JSONObject) && (jsonObj = json_value_get_object(JsonValue)) == NULL)
    {
        // Only supports JSONObject
        LogInfo("R700 : Unable to retrieve JSON object");
    }
    else if ((jsonType == JSONArray) && (jsonArray = json_value_get_array(JsonValue)) == NULL)
    {
        // Only supports JSONObject
        LogInfo("R700 : Unable to retrieve JSON array");
    }
    else if ((json_Size = json_serialization_size_pretty(JsonValue)) == 0)
    {
        LogError("R700 : Unable to retrieve buffer size of serialization");
    }
    else if ((bufferJson = json_serialize_to_string_pretty(JsonValue)) == NULL)
    {
        LogError("R700 : Unabled to serialize JSON payload");
    }

    va_start(argList, JsonValue);
    len = vsnprintf(NULL, 0, MsgFormat, argList);
    va_end(argList);

    if (len < 0)
    {
        LogError("R700 : Unnable to determine buffer size");
    }
    else
    {
        len = len + 1 + json_Size + 1;   // for null and CR

        if (len > sizeof(local_Buffer))
        {
            bufferFormat = malloc(len);
        }
        else
        {
            bufferFormat = &local_Buffer[0];
        }
    }

    va_start(argList, JsonValue);
    written = vsnprintf(bufferFormat, len, MsgFormat, argList);
    va_end(argList);

    if (written < 0)
    {
        LogError("R700 : Unnable to format with JSON");
        goto exit;
    }

    if (bufferJson != NULL)
    {
        bufferPtr = bufferFormat + written;
        written   = snprintf(bufferPtr, len - written, "\r\n%s", bufferJson);
    }

    LogInfo("%s", bufferFormat);

exit:

    if (bufferFormat != &local_Buffer[0])
    {
        free(bufferFormat);
    }

    if (bufferJson != NULL)
    {
        json_free_serialized_string(bufferJson);
    }

    return;
}

void LogJsonPrettyStr(
    const char* MsgFormat,
    char* JsonString, ...)
{
    JSON_Value_Type jsonType = JSONError;
    char local_Buffer[128]   = {0};
    char* bufferFormat       = NULL;
    char* bufferJson         = NULL;
    char* bufferPtr;
    size_t json_Size = 0;
    JSON_Value* jsonValue;

    va_list argList;
    int len;
    int written;
    JSON_Object* jsonObj  = NULL;
    JSON_Array* jsonArray = NULL;

    if (JsonString == NULL)
    {
        jsonValue = NULL;
    }
    else if ((jsonValue = json_parse_string(JsonString)) == NULL)
    {
        LogError("R700 : Failed to allocation JSON Value for log. %s", JsonString);
    }
    else if ((jsonType = json_value_get_type(jsonValue)) == JSONError)
    {
        LogInfo("R700 : Unable to retrieve JSON type");
    }
    else if (jsonType == JSONNull)
    {
        LogInfo("R700 : JSON is Null type");
    }
    else if ((jsonType == JSONObject) && (jsonObj = json_value_get_object(jsonValue)) == NULL)
    {
        // Only supports JSONObject
        LogInfo("R700 : Unable to retrieve JSON object");
    }
    else if ((jsonType == JSONArray) && (jsonArray = json_value_get_array(jsonValue)) == NULL)
    {
        // Only supports JSONObject
        LogInfo("R700 : Unable to retrieve JSON array");
    }
    else if ((json_Size = json_serialization_size_pretty(jsonValue)) == 0)
    {
        LogError("R700 : Unable to retrieve buffer size of serialization");
    }
    else if ((bufferJson = json_serialize_to_string_pretty(jsonValue)) == NULL)
    {
        LogError("R700 : Unabled to serialize JSON payload");
    }

    va_start(argList, JsonString);
    len = vsnprintf(NULL, 0, MsgFormat, argList);
    va_end(argList);

    if (len < 0)
    {
        LogError("R700 : Unnable to determine buffer size");
    }
    else
    {
        len = len + 1 + json_Size + 1;   // for null and CR

        if (len > sizeof(local_Buffer))
        {
            bufferFormat = malloc(len);
        }
        else
        {
            bufferFormat = &local_Buffer[0];
        }
    }

    va_start(argList, JsonString);
    written = vsnprintf(bufferFormat, len, MsgFormat, argList);
    va_end(argList);

    if (written < 0)
    {
        LogError("R700 : Unnable to format with JSON");
        goto exit;
    }

    if (bufferJson != NULL)
    {
        bufferPtr = bufferFormat + written;
        written   = snprintf(bufferPtr, len - written, "\r\n%s", bufferJson);
    }

    LogInfo("%s", bufferFormat);

exit:

    if (bufferFormat != &local_Buffer[0])
    {
        free(bufferFormat);
    }

    if (bufferJson != NULL)
    {
        json_free_serialized_string(bufferJson);
    }

    if (jsonValue)
    {
        json_value_free(jsonValue);
    }
}


/****************************************************************
Removes specified JSON Array from JSON string
****************************************************************/
JSON_Value*
Remove_JSON_Array(
    char* Payload,
    const char* ArrayName)
{
    JSON_Value* jsonVal = NULL;
    JSON_Object* jsonObj;
    JSON_Array* jsonArray;

    if ((jsonVal = json_parse_string(Payload)) == NULL)
    {
        LogError("R700 : Unable to parse GET Response JSON");
    }
    else if ((jsonObj = json_value_get_object(jsonVal)) == NULL)
    {
        LogError("R700 : Cannot retrieve GET Response JSON object");
    }
    else if ((jsonArray = json_object_get_array(jsonObj, ArrayName)) == NULL)
    {
        LogError("R700 : Cannot retrieve JSON Array '%s'", ArrayName);
    }
    else if (json_object_remove(jsonObj, ArrayName) != JSONSuccess)
    {
        LogError("R700 : Cannot remove JSON Array '%s'", ArrayName);
    }
    else
    {
        return jsonVal;
    }

    if (jsonVal)
    {
        json_value_free(jsonVal);
        jsonVal = NULL;
    }

    return jsonVal;
}

/****************************************************************
Converts specified JSON Array to DTDL Map
****************************************************************/
JSON_Value*
JSONArray2DtdlMap(
    char* Payload,
    const char* ArrayName,
    const char* MapName)
{
    JSON_Value* jsonVal_Array  = NULL;
    JSON_Object* jsonObj_Array = NULL;
    JSON_Array* jsonArray      = NULL;
    JSON_Value* jsonVal_Map    = NULL;
    JSON_Object* jsonObj_Map   = NULL;
    int arrayCount;
    JSON_Value_Type json_Type = JSONError;
    int i;

    LogInfo("R700 : %s() enter Array Name=%s Map Name=%s", __FUNCTION__, ArrayName, MapName);

    if ((jsonVal_Array = json_parse_string(Payload)) == NULL)
    {
        LogError("R700 : Unable to parse Payload to JSON_Value");
    }
    else if ((json_Type = json_value_get_type(jsonVal_Array)) == JSONError)
    {
        LogError("R700 : Unable to determine JSON Value type");
    }
    else if (json_Type == JSONArray)
    {
        if ((jsonArray = json_value_get_array(jsonVal_Array)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON Array");
        }
        else if ((arrayCount = json_array_get_count(jsonArray)) == 0)
        {
            LogError("R700 : JSON Array empty");
        }
        else if ((jsonVal_Map = json_value_init_object()) == NULL)
        {
            LogError("R700 : Cannot create JSON Value for Profiles Map");
        }
        else if ((jsonObj_Map = json_value_get_object(jsonVal_Map)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON Object for Profiles Map");
        }
    }
    else if (json_Type == JSONObject)
    {
        assert(ArrayName != NULL);

        if ((jsonObj_Array = json_value_get_object(jsonVal_Array)) == NULL)
        {
            LogError("R700 : Cannot create copy of Array Value");
        }
        else if ((jsonArray = json_object_get_array(jsonObj_Array, ArrayName)) == NULL)
        {
            LogInfo("R700 : Did not find %s Array in payload", ArrayName);
        }
        else if ((jsonVal_Map = json_value_deep_copy(jsonVal_Array)) == NULL)
        {
            LogError("R700 : Cannot create copy of Array Value");
        }
        else if ((jsonObj_Map = json_value_get_object(jsonVal_Map)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON object for Array");
        }
        else if ((arrayCount = json_array_get_count(jsonArray)) == 0)
        {
            LogError("R700 : JSON Object Array empty");
        }
        else if (json_object_remove(jsonObj_Map, ArrayName) != JSONSuccess)
        {
            LogError("R700 : Failed to remove Array from payload");
        }
    }

    for (i = 0; i < arrayCount; i++)
    {
        char buffer[32] = {0};
        if (ArrayName)
        {
            JSON_Value* jsonVal_ArrayValue     = NULL;
            JSON_Value* jsonVal_ArrayValueCopy = NULL;
            sprintf(buffer, "%s.%s%d", ArrayName, MapName, i);

            if ((jsonVal_ArrayValue = json_array_get_value(jsonArray, i)) == NULL)
            {
                LogError("R700 : Unable to retrieve JSON Value from Object Array %s", ArrayName);
                break;
            }
            else if ((jsonVal_ArrayValueCopy = json_value_deep_copy(jsonVal_ArrayValue)) == NULL)
            {
                LogError("R700 : Cannot create a copy of bootstrap Value");
                break;
            }
            else if ((json_object_dotset_value(jsonObj_Map, buffer, jsonVal_ArrayValueCopy)) != JSONSuccess)
            {
                LogError("R700 : Unable to retrieve JSON Value from Object Array %s", ArrayName);
                break;
            }
        }
        else
        {
            const char* profilevalue = json_array_get_string(jsonArray, i);
            sprintf(buffer, "%s%d", MapName, i);
            json_object_set_string(jsonObj_Map, buffer, profilevalue);
        }
    }

    if (jsonVal_Array)
    {
        json_value_free(jsonVal_Array);
    }

    return jsonVal_Map;
}

/****************************************************************
Converts DTDL Map to JSON Array
****************************************************************/
char* DtdlMap2JSONArray(
    const char* Payload,
    const char* ArrayName)
{
    JSON_Value* jsonVal_Payload = NULL;
    JSON_Object* jsonObj_Payload;
    JSON_Value* jsonVal_Map;
    JSON_Object* jsonObj_Map;
    int mapCount;
    JSON_Value* jsonVal_Array = NULL;
    JSON_Object* jsonObj_Array;
    JSON_Value* jsonVal_ArrayValue;
    JSON_Array* jsonArray_ArrayValue;
    bool bSuccess    = false;
    char* arrayValue = NULL;

    LogInfo("R700 : %s() enter Array Name=%s", __FUNCTION__, ArrayName);
    LogJsonPrettyStr("Map Payload", (char*)Payload);

    if ((jsonVal_Payload = json_parse_string(Payload)) == NULL)
    {
        LogError("R700 : Unable to parse Map Payload to JSON_Value");
    }
    else if ((jsonObj_Payload = json_value_get_object(jsonVal_Payload)) == NULL)
    {
        LogError("R700 : Cannot retrieve JSON object for Kafka payload");
    }
    else if (json_object_has_value(jsonObj_Payload, ArrayName) == 0)
    {
        LogInfo("R700 : DTDL Map %s not found in  payload", ArrayName);
    }
    else if ((jsonVal_Map = json_object_get_value(jsonObj_Payload, ArrayName)) == NULL)
    {
        LogError("R700 : Cannnot retrieve JSON Value for %s", ArrayName);
    }
    else if (json_value_get_type(jsonVal_Map) != JSONObject)
    {
        LogError("R700 : Map %s is not JSON Object", ArrayName);
    }
    else if ((jsonObj_Map = json_value_get_object(jsonVal_Map)) == NULL)
    {
        LogError("R700 : Cannnot retrieve JSON Object for Kafka Bootstraps");
    }
    else if ((mapCount = json_object_get_count(jsonObj_Map)) == 0)
    {
        LogInfo("R700 : Map %s empty", ArrayName);
    }
    else if ((jsonVal_Array = json_value_deep_copy(jsonVal_Payload)) == NULL)
    {
        LogError("R700 : Cannot create copy of Kafka JSON Value");
    }
    else if ((jsonObj_Array = json_value_get_object(jsonVal_Array)) == NULL)
    {
        LogError("R700 : Cannnot retrieve JSON Object for Kafka Bootstraps copy");
    }
    else if (json_object_remove(jsonObj_Array, ArrayName) != JSONSuccess)
    {
        LogError("R700 : Failed to remove %s", ArrayName);
    }
    else if ((jsonVal_ArrayValue = json_value_init_array()) == NULL)
    {
        LogError("R700 : Failed to create JSON Value for %s array", ArrayName);
    }
    else if ((jsonArray_ArrayValue = json_value_get_array(jsonVal_ArrayValue)) == NULL)
    {
        LogError("R700 : Failed to retrieve %s array", ArrayName);
    }
    else
    {
        int i;

        for (i = 0; i < mapCount; i++)
        {
            JSON_Value* jsonVal_MapValue;
            JSON_Value* jsonVal_MapValueCopy;

            if ((jsonVal_MapValue = json_object_get_value_at(jsonObj_Map, i)) == NULL)
            {
                LogError("R700 : Unable to retrieve %s index %d", ArrayName, i);
                goto exit;
            }
            else if ((jsonVal_MapValueCopy = json_value_deep_copy(jsonVal_MapValue)) == NULL)
            {
                LogError("R700 : Unable to append bootstrap value index %d", i);
                goto exit;
            }
            else if ((json_array_append_value(jsonArray_ArrayValue, jsonVal_MapValueCopy)) != JSONSuccess)
            {
                LogError("R700 : Unable to append bootstrap value index %d", i);
                goto exit;
            }
        }

        json_object_set_value(jsonObj_Array, ArrayName, json_array_get_wrapping_value(jsonArray_ArrayValue));
        arrayValue = json_serialize_to_string(jsonVal_Array);
        LogJsonPretty("R700 : Array Payload", jsonVal_Array);
    }

exit:

    if (jsonVal_Payload)
    {
        json_value_free(jsonVal_Payload);
    }

    if (jsonVal_Array)
    {
        json_value_free(jsonVal_Array);
    }

    return arrayValue;
}

const char*
GetStringFromPayload(
    JSON_Value* Payload,
    const char* ParamName)
{
    JSON_Object* jsonObj_Payload;
    JSON_Value* jsonVal_Payload;
    const char* stringValue = NULL;

    if ((jsonObj_Payload = json_value_get_object(Payload)) == NULL)
    {
        LogError("R700 : Unable to retrieve JSON object for %s", ParamName);
    }
    else if ((jsonVal_Payload = json_object_get_value(jsonObj_Payload, ParamName)) == NULL)
    {
        LogError("R700 : Unable to retrieve JSON Value for %s.", ParamName);
    }
    else if ((stringValue = json_value_get_string(jsonVal_Payload)) == NULL)
    {
        LogError("R700 : Unable to retrieve string for %s.", ParamName);
    }

    return stringValue;
}


//
// Remove empty objects from Antenna Configuration
//
bool CleanAntennaConfig(
    JSON_Object* jsonObj_AntennaConfig)
{
    JSON_Object* jsonObj_Filtering         = json_object_get_object(jsonObj_AntennaConfig, g_antennaConfigFiltering);
    JSON_Object* jsonObj_TagAuthentication = NULL;
    JSON_Object* jsonObj_PowerSweeping     = NULL;

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

    return true;
}


const char*
GetObjectStringFromPayload(
    JSON_Value* Payload,
    const char* ParamName)
{
    JSON_Object* jsonObj_Payload;
    JSON_Value* jsonVal_Payload;
    const char* stringValue = NULL;

    if ((jsonObj_Payload = json_value_get_object(Payload)) == NULL)
    {
        LogError("R700 : Unable to retrieve JSON object for %s", ParamName);
    }
    else if ((jsonVal_Payload = json_object_get_value(jsonObj_Payload, ParamName)) == NULL)
    {
        LogError("R700 : Unable to retrieve JSON Value for %s.", ParamName);
    }
    else if ((stringValue = json_serialize_to_string(jsonVal_Payload)) == NULL)
    {
        LogError("R700 : Unable to serialize for %s.", ParamName);
    }

    return stringValue;
}

/****************************************************************
Get Firmware Version from reader
****************************************************************/
void GetFirmwareVersion(
    PIMPINJ_READER Reader)
{
    // get firmware version
    PIMPINJ_R700_REST r700_GetSystemImageRequest = &R700_REST_LIST[SYSTEM_IMAGE];
    JSON_Value* jsonVal_Image               = NULL;
    JSON_Value* jsonVal_Firmware            = NULL;
    JSON_Object* jsonObj_Image;
    int httpStatus;
    const char* firmware = NULL;
    int i;
    char* jsonResult;

    Reader->ApiVersion = V_Unknown;

    jsonResult = curlStaticGet(Reader->curl_static_session, r700_GetSystemImageRequest->EndPoint, &httpStatus);

    if ((jsonVal_Image = json_parse_string(jsonResult)) == NULL)
    {
        LogInfo("R700 :  Unable to retrieve JSON Value for Image information from reader.  Check that HTTPS is enabled.");
    }
    else if ((jsonObj_Image = json_value_get_object(jsonVal_Image)) == NULL)
    {
        LogInfo("R700 :  Unable to retrieve JSON Object for Image information. Check that HTTPS is enabled.");
    }
    else if ((firmware = json_object_get_string(jsonObj_Image, "primaryFirmware")) == NULL)
    {
        LogInfo("R700 :  Unable to retrieve primaryFirmware.  Check that HTTPS is enabled.");
    }
    else
    {
        long major[4]                = {0, 0, 0, 0};
        char* next                   = (char*)firmware;
        R700_REST_VERSION apiVersion = V1_0;

        for (i = 0; i < 4 && *next != '\0'; i++)
        {
            major[i] = strtol(next, &next, 10);
            next     = next + 1;
        }

        for (i = 0; i < sizeof(IMPINJ_R700_API_MAPPING) / sizeof(IMPINJ_R700_API_VERSION); i++)
        {
            if (major[0] < IMPINJ_R700_API_MAPPING[i].Firmware.major)
            {
                continue;
            }
            else if (major[1] < IMPINJ_R700_API_MAPPING[i].Firmware.minor)
            {
                continue;
            }
            else
            {
                apiVersion = IMPINJ_R700_API_MAPPING[i].RestVersion;
            }
        }

        LogInfo("R700 : REST API Version %s\r\n", MU_ENUM_TO_STRING(R700_REST_VERSION, apiVersion));
        Reader->ApiVersion = apiVersion;
    }
}

/****************************************************************
Get Interface Type
****************************************************************/
void CheckInterfaceType(
    PIMPINJ_READER Reader)
{
    // get firmware version
    PIMPINJ_R700_REST r700_GetSetInterfaceRequest = &R700_REST_LIST[SYSTEM_RFID_INTERFACE];
    JSON_Value* jsonVal_Interface           = NULL;
    JSON_Object* jsonObj_Interface;
    int httpStatus;
    const char* interface = NULL;
    char* jsonResult;

    jsonResult = curlStaticGet(Reader->curl_static_session, r700_GetSetInterfaceRequest->EndPoint, &httpStatus);

    if ((jsonVal_Interface = json_parse_string(jsonResult)) == NULL)
    {
        LogInfo("R700 :  Unable to retrieve JSON Value for Image information from reader.  Check that HTTPS is enabled.");
    }
    else if ((jsonObj_Interface = json_value_get_object(jsonVal_Interface)) == NULL)
    {
        LogInfo("R700 :  Unable to retrieve JSON Object for Image information. Check that HTTPS is enabled.");
    }
    else if ((interface = json_object_get_string(jsonObj_Interface, "rfidInterface")) == NULL)
    {
        LogInfo("R700 :  Unable to retrieve rfidInterface.  Check that HTTPS is enabled.");
    }
    else
    {
        LogInfo("R700 : Current Interface is %s\r\n", interface);
        if (strcmp(interface, "llrp") == 0)
        {
            char* jsonResultSet;
            LogInfo("R700 : Setting to REST API");
            char body[] = "{\"rfidInterface\":\"rest\"}";
            jsonResultSet = curlStaticPut(Reader->curl_static_session, r700_GetSetInterfaceRequest->EndPoint, body, &httpStatus);

        }
    }
}

PDOWNLOAD_DATA ParseUrl(const char* Url)
{
    PDOWNLOAD_DATA pUrl_Data = (PDOWNLOAD_DATA)calloc(1, sizeof(DOWNLOAD_DATA));
    const char* pStart;
    char* pEnd;
    char* pSeparator;
    int length;

    pStart = Url;

    // scheme
    pEnd = strstr(pStart, "://");
    if (pEnd == 0)
    {
        goto errorExit;
    }

    strncpy(pUrl_Data->url, pStart, strlen(pStart));
    LogInfo("R700 : Url %s", pUrl_Data->url);

    length = pEnd - pStart;

    strncpy(pUrl_Data->scheme, pStart, length);
    pUrl_Data->scheme[length] = 0;

    LogInfo("R700 : Scheme %s", pUrl_Data->scheme);

    pStart = pEnd + 3;   // for ://

    // check if user name & password are specified
    // http://username:password@example.com
    //
    pEnd = strchr(pStart, '@');
    if (pEnd)
    {
        pSeparator = strchr(pStart, ':');
        if (pSeparator && pSeparator < pEnd)
        {   // username:password
            length = pSeparator - pStart;

            if (length > 255)
            {
                LogError("R700 : User Name too long : Length %d", length);
                goto errorExit;
            }

            strncpy(pUrl_Data->username, pStart, length);
            pUrl_Data->username[length] = 0;

            length = pEnd - pSeparator;

            if (length > 255)
            {
                LogError("R700 : Password too long : Length %d", length);
                goto errorExit;
            }
            strncpy(pUrl_Data->password, pSeparator + 1, length - 1);
            pUrl_Data->password[length - 1] = 0;
        }
        else
        {   // only username
            length = pEnd - pStart;

            if (length > 255)
            {
                LogError("R700 : User Name too long : Length %d", length);
                goto errorExit;
            }

            strncpy(pUrl_Data->username, pStart, length);
            pUrl_Data->username[length] = 0;
        }

        LogInfo("R700 : User Name %s", pUrl_Data->username);
        LogInfo("R700 : Password  %s", pUrl_Data->password);

        pStart = pEnd + 1;   // for @
    }

    // Hostname
    pEnd = strchr(pStart, '/');
    if (pEnd == NULL)
    {
        LogError("R700 : Upgrade File URL does not contain path");
        goto errorExit;
    }
    else
    {
        pSeparator = strchr(pStart, ':');
        if (pSeparator && pSeparator < pEnd)
        {
            // contains port number
            // Copy Host Name
            length = pSeparator - pStart;
            if (length > 255)
            {
                LogError("R700 : Host Name too long : Length %d", length);
                goto errorExit;
            }
            strncpy(pUrl_Data->hostname, pStart, length);
            pUrl_Data->hostname[length] = 0;

            // Port number
            length = pEnd - pSeparator - 1;
            if (length > 5)
            {
                LogError("R700 : Port Number too long : Length %d", length);
                goto errorExit;
            }
            strncpy(pUrl_Data->port, pSeparator + 1, length);
            pUrl_Data->port[length] = 0;
        }
        else
        {
            // Only Host Name
            length = pEnd - pStart;
            if (length > 255)
            {
                LogError("R700 : Host Name too long : Length %d", length);
                goto errorExit;
            }
            strncpy(pUrl_Data->hostname, pStart, length);
            pUrl_Data->hostname[length] = 0;
        }
        LogInfo("R700 : Host Name %s", pUrl_Data->hostname);
        LogInfo("R700 : Port Number %s", pUrl_Data->port);
    }

    pStart = pEnd + 1;   // for /

    length = strlen(pStart);

    if (length == 0 || length > 1024)
    {
        LogError("R700 : URL Path too short or too long : Length %d", length);
        goto errorExit;
    }
    strncpy(pUrl_Data->path, pStart, length);
    pUrl_Data->path[length] = 0;

    LogInfo("R700 : URL Path %s", pUrl_Data->path);

    // get file name
    pSeparator = strrchr(pStart, '/');
    sprintf(pUrl_Data->outFileName, "/tmp/%s", pSeparator + 1);

    LogInfo("R700 : Output File %s", pUrl_Data->outFileName);

    return pUrl_Data;

errorExit:

    if (pUrl_Data)
    {
        free(pUrl_Data);
    }

    return NULL;
}

PDOWNLOAD_DATA DownloadFile(const char* Url)
{
    struct addrinfo* addrInfo;
    struct sockaddr_in* socketAddrHost;
    struct sockaddr_in socketAddrClient;
    char* hostAddr;
    int sock;
    PDOWNLOAD_DATA pUrl_Data;
    int statusCode = R700_STATUS_INTERNAL_ERROR;

    pUrl_Data = ParseUrl(Url);

    if (pUrl_Data == NULL)
    {
        goto exit;
    }

    statusCode = curlGetDownload(pUrl_Data);

    LogInfo("R700 : Download %d", statusCode);

    if (statusCode == R700_STATUS_OK)
    {
        // Upload File
    }
    else
    {
        //
    }

    // if (0 != getaddrinfo(pUrl_Data->hostname, NULL, NULL, &addrInfo))
    // {
    //     LogError("R700 : Error in resolving hostname %s\n", pUrl_Data->hostname);
    //     goto exit;
    // }

    // // Resolve host address as string
    // socketAddrHost = (struct sockaddr_in*)addrInfo->ai_addr;
    // hostAddr   = inet_ntoa(socketAddrHost->sin_addr);

    // LogInfo("R700 : Download Host Address %s", hostAddr);

    // // Create a socket of stream type
    // sock = socket(AF_INET, SOCK_STREAM, 0);
    // if (sock < 0)
    // {
    //     perror("Error opening channel");
    //     goto exit;
    // }

    // // Define properties required to connect to the socket
    // bzero(&socketAddrClient, sizeof(socketAddrClient));
    // socketAddrClient.sin_family      = AF_INET;
    // socketAddrClient.sin_addr.s_addr = inet_addr(hostAddr);
    // socketAddrClient.sin_port        = htons(80);

exit:

    close(sock);

    return pUrl_Data;
}
