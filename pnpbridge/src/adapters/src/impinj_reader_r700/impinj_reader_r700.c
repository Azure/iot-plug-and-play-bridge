#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "impinj_reader_r700.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

#include "azure_c_shared_utility/const_defines.h"

#include "curl_wrapper/curl_wrapper.h"
#include <string.h>
#include "helpers/string_manipulation.h"
#include "helpers/led.h"

/****************************************************************
Helper function

Print outs JSON payload with indent
Works only with JSONObject
****************************************************************/
void ImpinjReader_LogJsonPretty(char *Message, JSON_Value *Json)
{
    char *buffer = NULL;
    size_t size;
    JSON_Object *jsonObj;

    if (Json == NULL)
    {
        return;
    }
    else if ((jsonObj = json_value_get_object(Json)) == NULL)
    {
        // Only supports JSONObject
        return;
    }
    else if (json_object_get_count(jsonObj) == 0)
    {
        // Emptry object.  Nothing to print
        return;
    }
    else if ((size = json_serialization_size_pretty(Json)) == 0)
    {
        LogError("R700 : Unable to retrieve buffer size of serialization");
    }
    else if ((buffer = malloc(size + 1)) == NULL)
    {
        LogError("R700 : Unable to allocate %ld bytes buffer for serialization", size + 1);
    }
    else if (JSONSuccess != json_serialize_to_buffer_pretty(Json, buffer, size))
    {
        LogError("Buffer too small.  Need %ld", json_serialization_size_pretty(Json));
    }
    else
    {
        LogInfo("%s\r\n%s", Message, buffer);
    }

    if (buffer)
    {
        free(buffer);
    }
}

void ImpinjReader_LogJsonString(char *Message, char *JsonString)
{
    JSON_Value *jsonValue;

    if ((jsonValue = json_parse_string(JsonString)) != NULL)
    {
        ImpinjReader_LogJsonPretty(Message, jsonValue);
        json_value_free(jsonValue);
    }
    else
    {
        LogError("R700 : Failed to allocation JSON Value for log. %s", JsonString);
    }
}

/****************************************************************
Process ErrorResponse and converts them to single string for properties
or JSON for command response.

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
char *ImpinjReader_ProcessErrorResponse(
    JSON_Value *JsonVal_ErrorResponse,
    R700_DTDL_TYPE DtdlType)
{
    JSON_Object *jsonObj_ErrorResponse = NULL;
    JSON_Value *jsonVal_Message = NULL;
    JSON_Value *jsonVal_InvalidPropertyId = NULL;
    JSON_Value *jsonVal_Details = NULL;
    const char *message = NULL;
    const char *invalidPropertyId = NULL;
    const char *detail = NULL;
    char *returnBuffer = NULL;
    char *messageBuffer = NULL;
    size_t size = 0;

    ImpinjReader_LogJsonPretty("R700 : Processing ErrorResponse", JsonVal_ErrorResponse);

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
        if ((messageBuffer = (char *)malloc(size + 1)) == NULL)
        {
            LogError("R700 : Unable to allocate buffer for description for %ld bytes", size);
        }
        else
        {
            messageBuffer[0] = '\0';
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
            messageBuffer[size] = '\0';
            LogInfo("R700 : Error Response %s", returnBuffer);
        }

        // Command responses must be JSON
        if (DtdlType == COMMAND)
        {
            if ((returnBuffer = (char *)malloc(size + strlen(g_errorResponseFormat) + 1)) == NULL)
            {
                LogError("R700 : Unable to allocate buffer for command response for %ld bytes", size);
            }
            else
            {
                sprintf(returnBuffer, g_errorResponseFormat, messageBuffer);
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
Process REST API Response
****************************************************************/
const char *ImpinjReader_ProcessResponse(
    IMPINJ_R700_REST *RestRequest,
    JSON_Value *JsonVal_Response,
    int HttpStatus)
{
    JSON_Value *jsonVal;
    JSON_Object *jsonObj;
    const char *descriptionBuffer = NULL;
    size_t size = 0;

    switch (HttpStatus)
    {
    case R700_STATUS_OK:
    case R700_STATUS_CREATED:
    case R700_STATUS_NO_CONTENT:
        size = strlen(g_successResponse) + 1;
        if ((descriptionBuffer = (char *)malloc(size)) == NULL)
        {
            LogError("R700 : Unable to allocate %ld bytes buffer for description for SYSTEM_POWER_SET", size);
        }
        mallocAndStrcpy_s((char**)&descriptionBuffer, &g_successResponse[0]);
        break;

    case R700_STATUS_ACCEPTED:

        switch (RestRequest->Request)
        {
        case SYSTEM_POWER_SET:
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
            else if ((descriptionBuffer = (char *)malloc(size + 1)) == NULL)
            {
                LogError("R700 : Unable to allocate  %ld bytes buffer for description for SYSTEM_POWER_SET", size);
            }
            else if ((jsonVal = json_object_get_value(jsonObj, g_statusResponseMessage)) == NULL)
            {
                LogError("R700 : Unable to retrieve JSON Value for %s", g_statusResponseMessage);
            }
            else if (json_value_get_type(jsonVal) != JSONString)
            {
                LogError("R700 : JSON field %s is not string", g_errorResponseMessage);
            }
            else if (json_serialize_to_buffer(jsonVal, (char*)descriptionBuffer, size + 1) != JSONSuccess)
            {
                LogError("R700 : Failed to serialize description to buffer");
            }
            break;
        }

        break;

    case R700_STATUS_BAD_REQUEST:
    case R700_STATUS_FORBIDDEN:
    case R700_STATUS_NOT_FOUND:
    case R700_STATUS_NOT_CONFLICT:
    case R700_STATUS_INTERNAL_ERROR:
        descriptionBuffer = ImpinjReader_ProcessErrorResponse(JsonVal_Response, RestRequest->DtdlType);
        break;
    }

    LogInfo("R700 : %s Http Status \"%d\" Response \"%s\"", __FUNCTION__, HttpStatus, descriptionBuffer);

    return descriptionBuffer;
}

/****************************************************************
Device Twin section
****************************************************************/
/****************************************************************
A callback from IoT Hub on Reported Property Update
****************************************************************/
static void ImpinjReader_ReportedPropertyCallback(
    int ReportedStatus,
    void *UserContextCallback)
{
    LogInfo("R700 : PropertyCallback called, result=%d, property name=%s",
            ReportedStatus,
            (const char *)UserContextCallback);
}

/****************************************************************
Processes Read Only Property Update
****************************************************************/
IOTHUB_CLIENT_RESULT ImpinjReader_UpdateReadOnlyReportProperty(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *ComponentName,
    char *PropertyName,
    JSON_Value *JsonVal_PropertyValue)
{
    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;
    STRING_HANDLE jsonToSend = NULL;
    char *propertyValue = NULL;

    if ((propertyValue = json_serialize_to_string(JsonVal_PropertyValue)) == NULL)
    {
        LogError("R700 : Unabled to serialize Read Only Property JSON payload");
    }
    else if ((jsonToSend = PnP_CreateReportedProperty(ComponentName, PropertyName, propertyValue)) == NULL)
    {
        LogError("R700 : Unable to build reported property response for Read Only Property %s, Value=%s",
                 PropertyName,
                 propertyValue);
    }
    else
    {
        PNP_BRIDGE_CLIENT_HANDLE clientHandle = PnpComponentHandleGetClientHandle(PnpComponentHandle);

        if ((iothubClientResult = PnpBridgeClient_SendReportedState(clientHandle,
                                                                    STRING_c_str(jsonToSend),
                                                                    STRING_length(jsonToSend),
                                                                    ImpinjReader_ReportedPropertyCallback,
                                                                    (void *)PropertyName)) != IOTHUB_CLIENT_OK)
        {
            LogError("R700 : Unable to send reported state for property=%s, error=%d",
                     PropertyName,
                     iothubClientResult);
        }
        else
        {
            LogInfo("R700 : Send Read Only Property %s to IoT Hub", PropertyName);
        }

        STRING_delete(jsonToSend);
    }

    if (propertyValue)
    {
        json_free_serialized_string(propertyValue);
    }

    return iothubClientResult;
}

/****************************************************************
Processes Writable Property
****************************************************************/
IOTHUB_CLIENT_RESULT ImpinjReader_UpdateWritableProperty(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_R700_REST RestRequest,
    JSON_Value *JsonVal_PropertyValue,
    int Ac,
    char *Ad,
    int Av)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;
    STRING_HANDLE jsonToSend = NULL;
    char *propertyName = RestRequest->Name;
        
    JSON_Object *jsonObj_Property = NULL;
    JSON_Value *jsonVal_Message = NULL;
    JSON_Value *jsonVal_Rest = NULL;
    const char *descToSend = Ad;
    char *propertyValue = NULL;
    int httpStatus;

    LogInfo("R700 : ImpinjReader_UpdateWritableProperty Name %s\r\n  AC : %d\r\n  AV : %d\r\n  AD : %s", propertyName, Ac, Av, Ad);
    ImpinjReader_LogJsonPretty("R700 : value", JsonVal_PropertyValue);

    // Check status (Ack Code)
    switch (Ac)
    {
    case R700_STATUS_NO_CONTENT:
        // Reader did not return any content
        // Read the current/new settings from HW
        assert(JsonVal_PropertyValue == NULL);
        jsonVal_Rest = ImpinjReader_RequestGet(PnpComponentHandle, device, RestRequest->Request, NULL, &httpStatus);
        descToSend = ImpinjReader_ProcessResponse(RestRequest, JsonVal_PropertyValue, Ac);
        break;

    case R700_STATUS_ACCEPTED:
        // Reader responded with some context
        assert(JsonVal_PropertyValue != NULL);
        jsonVal_Rest = ImpinjReader_RequestGet(PnpComponentHandle, device, RestRequest->Request, NULL, &httpStatus);
        descToSend = ImpinjReader_ProcessResponse(RestRequest, JsonVal_PropertyValue, Ac);
        break;

    case R700_STATUS_BAD_REQUEST:
    case R700_STATUS_FORBIDDEN:
    case R700_STATUS_NOT_FOUND:
    case R700_STATUS_NOT_CONFLICT:
    case R700_STATUS_INTERNAL_ERROR:
        // Errors
        assert(JsonVal_PropertyValue != NULL);
        jsonVal_Rest = ImpinjReader_RequestGet(PnpComponentHandle, device, RestRequest->Request, NULL, &httpStatus);
        descToSend = ImpinjReader_ProcessErrorResponse(JsonVal_PropertyValue, RestRequest->DtdlType);
        break;
    }

    if (jsonVal_Rest)
    {
        propertyValue = json_serialize_to_string(jsonVal_Rest);
    }
    else
    {
        propertyValue = json_serialize_to_string(JsonVal_PropertyValue);
    }

    if ((jsonToSend = PnP_CreateReportedPropertyWithStatus(device->SensorState->componentName,
                                                           propertyName,
                                                           propertyValue,
                                                           Ac,
                                                           descToSend,
                                                           Av)) == NULL)
    {
        LogError("R700 : Unable to build reported property response for Writable Property %s, Value=%s",
                 propertyName,
                 propertyValue);
    }
    else
    {
        PNP_BRIDGE_CLIENT_HANDLE clientHandle = PnpComponentHandleGetClientHandle(PnpComponentHandle);

        ImpinjReader_LogJsonString("R700 : Sending Reported Property Json", (char *)STRING_c_str(jsonToSend));

        if ((iothubClientResult = PnpBridgeClient_SendReportedState(clientHandle,
                                                                    STRING_c_str(jsonToSend),
                                                                    STRING_length(jsonToSend),
                                                                    ImpinjReader_ReportedPropertyCallback,
                                                                    (void *)propertyName)) != IOTHUB_CLIENT_OK)
        {
            LogError("R700 : Unable to send reported state for Writable Property %s :: error=%d",
                     propertyName,
                     iothubClientResult);
        }
        else
        {
            LogInfo("R700 : Sent reported state for Writable Property %s", propertyName);
        }
    }

    // clean up
    if (jsonToSend)
    {
        STRING_delete(jsonToSend);
    }

    if (jsonVal_Rest)
    {
        json_value_free(jsonVal_Rest);
    }

    if (propertyValue)
    {
        json_free_serialized_string(propertyValue);
    }

    return iothubClientResult;
}

/****************************************************************
GetDesiredJson retrieves JSON_Object* in the JSON tree
corresponding to the desired payload.
****************************************************************/
static JSON_Object *GetDesiredJson(DEVICE_TWIN_UPDATE_STATE updateState, JSON_Value *rootValue)
{
    JSON_Object *rootObject = NULL;
    JSON_Object *desiredObject;

    if ((rootObject = json_value_get_object(rootValue)) == NULL)
    {
        LogError("Unable to get root object of JSON");
        desiredObject = NULL;
    }
    else
    {
        if (updateState == DEVICE_TWIN_UPDATE_COMPLETE)
        {
            // For a complete update, the JSON from IoTHub will contain both "desired" and "reported" - the full twin.
            // We only care about "desired" in this sample, so just retrieve it.
            desiredObject = json_object_get_object(rootObject, g_IoTHubTwinDesiredObjectName);
        }
        else
        {
            // For a patch update, IoTHub does not explicitly put a "desired:" JSON envelope.  The "desired-ness" is implicit
            // in this case, so here we simply need the root of the JSON itself.
            desiredObject = rootObject;
        }
    }

    return desiredObject;
}

/****************************************************************
A callback for Property Update (DEVICE_TWIN_UPDATE_COMPLETE)
****************************************************************/
void ImpinjReader_OnPropertyCompleteCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const unsigned char *payload,
    size_t size,
    void *userContextCallback)
{
    JSON_Value *jsonVal_Rest = NULL;
    JSON_Value *jsonVal_Payload = NULL;
    JSON_Object *jsonObj_Desired;
    JSON_Object *jsonObj_Desired_Component = NULL;
    JSON_Object *jsonObj_Desired_Property;
    JSON_Value *jsonValue_Desired_Value = NULL;
    JSON_Value *versionValue = NULL;
    int status = PNP_STATUS_SUCCESS;
    int i = 0;
    int version;

    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    ImpinjReader_LogJsonString("R700 : ImpinjReader_OnPropertyCompleteCallback :: Payload", (char *)payload);

    if ((jsonVal_Payload = json_parse_string(payload)) == NULL)
    {
        LogError("R700 : Unable to parse twin JSON");
    }
    else if ((jsonObj_Desired = GetDesiredJson(DEVICE_TWIN_UPDATE_COMPLETE, jsonVal_Payload)) == NULL)
    {
        LogError("R700 : Unable to  retrieve desired JSON object");
    }
    else if ((versionValue = json_object_get_value(jsonObj_Desired, g_IoTHubTwinDesiredVersion)) == NULL)
    {
        LogError("R700 : Unable to retrieve %s for desired property", g_IoTHubTwinDesiredVersion);
    }
    else if (json_value_get_type(versionValue) != JSONNumber)
    {
        // The $version must be a number (and in practice an int) A non-numerical value indicates
        // something is fundamentally wrong with how we've received the twin and we should not proceed.
        LogError("R700 : %s is not JSONNumber", g_IoTHubTwinDesiredVersion);
    }
    else if ((version = (int)json_value_get_number(versionValue)) == 0)
    {
        LogError("R700 : Unable to retrieve %s", g_IoTHubTwinDesiredVersion);
    }
    else
    {
        // Device Twin may not have properties for this component right after provisioning
        if ((jsonObj_Desired_Component = json_object_get_object(jsonObj_Desired, device->ComponentName)) == NULL)
        {
            LogInfo("R700 : Unable to retrieve desired JSON Object for Component %s", device->ComponentName);
        }

        // Loop through APIs
        for (i = 0; i < (sizeof(R700_REST_LIST) / sizeof(IMPINJ_R700_REST)); i++)
        {
            jsonVal_Rest = NULL;
            jsonValue_Desired_Value = NULL;

            switch (R700_REST_LIST[i].DtdlType)
            {
            case COMMAND:
                break;

            case READONLY:
            {
                // Update Reported Properties
                jsonVal_Rest = ImpinjReader_RequestGet(PnpComponentHandle, device, i, NULL, &status);

                ImpinjReader_UpdateReadOnlyReportProperty(PnpComponentHandle, device->ComponentName, R700_REST_LIST[i].Name, jsonVal_Rest);

                break;
            }

            case WRITABLE:
                if ((jsonObj_Desired_Component == NULL) ||
                    ((jsonObj_Desired_Property = json_object_get_object(jsonObj_Desired_Component, R700_REST_LIST[i].Name)) == NULL))
                {
                    // No desired property for this property.  Send Reported Property with version = 1 with current values from reader
                    jsonVal_Rest = ImpinjReader_RequestGet(PnpComponentHandle, device, i, NULL, &status);

                    ImpinjReader_UpdateWritableProperty(PnpComponentHandle, &R700_REST_LIST[i], jsonVal_Rest, status, "Value from Reader", 1);
                }
                else
                {
                    // Desired Property found for this.
                    jsonValue_Desired_Value = json_object_get_value(jsonObj_Desired_Component, R700_REST_LIST[i].Name);
                    ImpinjReader_OnPropertyPatchCallback(PnpComponentHandle, R700_REST_LIST[i].Name, jsonValue_Desired_Value, version, NULL);
                }

                break;
            }
        }
    }

    // clean up
    if (jsonVal_Rest)
    {
        json_value_free(jsonVal_Rest);
    }

    if (jsonVal_Payload)
    {
        json_value_free(jsonVal_Payload);
    }
}

/****************************************************************
A callback for Property Update (DEVICE_TWIN_UPDATE_PARTIAL)
****************************************************************/
void ImpinjReader_OnPropertyPatchCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *PropertyName,
    JSON_Value *PropertyValue,
    int version,
    void *ClientHandle)
{
    JSON_Value *jsonVal_Rest = NULL;
    int r700_api = -1;
    int httpStatus = PNP_STATUS_SUCCESS;
    char *description = NULL;
    char *payload = NULL;

    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    ImpinjReader_LogJsonPretty("R700 : ImpinjReader_OnPropertyPatchCallback Payload", PropertyValue);

    for (int i = 0; i < (sizeof(R700_REST_LIST) / sizeof(IMPINJ_R700_REST)); i++)
    {
        if (R700_REST_LIST[i].DtdlType != WRITABLE)
        {
            continue;
        }
        else if (strcmp(PropertyName, R700_REST_LIST[i].Name) == 0)
        {
            r700_api = R700_REST_LIST[i].Request;
            assert(r700_api == i);
            break;
        }
    }

    if (r700_api != UNSUPPORTED)
    {
        payload = json_serialize_to_string(PropertyValue);

        if (R700_REST_LIST[r700_api].RestType == PUT)
        {
            jsonVal_Rest = ImpinjReader_RequestPut(PnpComponentHandle, device, r700_api, NULL, payload, &httpStatus);
        }
        else if (R700_REST_LIST[r700_api].RestType == POST)
        {
            jsonVal_Rest = ImpinjReader_RequestPost(PnpComponentHandle, device, r700_api, payload, &httpStatus);
        }
        else if (R700_REST_LIST[r700_api].RestType == DELETE)
        {
            jsonVal_Rest = ImpinjReader_RequestDelete(PnpComponentHandle, device, r700_api, payload, &httpStatus);
        }
        else
        {
            assert(false);
        }

        ImpinjReader_UpdateWritableProperty(PnpComponentHandle,
                                            &R700_REST_LIST[r700_api],
                                            jsonVal_Rest,
                                            httpStatus,
                                            description,
                                            version);

        if (description)
        {
            free(description);
        }

        if (payload)
        {
            json_free_serialized_string(payload);
        }

        if (jsonVal_Rest)
        {
            json_value_free(jsonVal_Rest);
        }
    }
}

/****************************************************************
Telemetry Section
****************************************************************/
char *ImpinjReader_CreateJsonResponse(
    char *propertyName,
    char *propertyValue)
{
    char jsonFirstChar = '{';
    char jsonToSendStr[2000] = "";

    strcat(jsonToSendStr, "{ \"");
    strcat(jsonToSendStr, propertyName);
    strcat(jsonToSendStr, "\": ");
    if (propertyValue[0] != jsonFirstChar)
    {
        strcat(jsonToSendStr, "\"");
    }
    strcat(jsonToSendStr, propertyValue);
    if (propertyValue[0] != jsonFirstChar)
    {
        strcat(jsonToSendStr, "\"");
    }
    strcat(jsonToSendStr, " }");

    char *response = Str_Trim(jsonToSendStr);

    // example JSON string: "{ \"status\": 12, \"description\": \"leds blinking\" }";

    return response;
}

// char *ImpinjReader_CreateTelemetryPayload(
//     char *TelemetryName,
//     char *TelemetryPayload)
// )
// {
// }

int ImpinjReader_TelemetryWorker(
    void *context)
{
    PNPBRIDGE_COMPONENT_HANDLE componentHandle = (PNPBRIDGE_COMPONENT_HANDLE)context;
    PIMPINJ_READER device = PnpComponentHandleGetContext(componentHandle);
    int httpStatus;
    long count = 0;
    char ledVal = '0';

    char *status = (char *)malloc(sizeof(char *) * 1000);
    char *statusNoTimePrev = (char *)malloc(sizeof(char *) * 1000);
    char *statusNoTime = (char *)malloc(sizeof(char *) * 1000);

    // Report telemetry every 5 seconds till we are asked to stop
    while (true)
    {
        if (count % 2)
        {
            ledVal = LED_ON;
        }
        else ledVal = LED_OFF;

        writeLed(SYSTEM_GREEN, ledVal);
        count++;
        
        if (device->ShuttingDown)
        {
            writeLed(SYSTEM_GREEN, LED_OFF);
            return IOTHUB_CLIENT_OK;
        }

        statusNoTimePrev = statusNoTime;

        int uSecInit = clock();
        int uSecTimer = 0;
        int uSecTarget = 10000;

        while (uSecTimer < uSecTarget)
        {
            if (device->ShuttingDown)
            {
                return IOTHUB_CLIENT_OK;
            }

            ImpinjReader_SendTelemetryMessagesAsync(componentHandle);
            ThreadAPI_Sleep(100);
            uSecTimer = clock() - uSecInit;
        }
    }

    free(status);

    return IOTHUB_CLIENT_OK;
}

static void ImpinjReader_TelemetryCallback(
    IOTHUB_CLIENT_CONFIRMATION_RESULT TelemetryStatus,
    void *UserContextCallback)
{
    PIMPINJ_R700_MESSAGE_CONTEXT callbackContext = (PIMPINJ_R700_MESSAGE_CONTEXT)UserContextCallback;
    PIMPINJ_READER device = callbackContext->reader;
    if (TelemetryStatus == IOTHUB_CLIENT_CONFIRMATION_OK)
    {
        LogInfo("R700 : Successfully delivered telemetry message for <%s>", (const char *)device->SensorState->componentName);
    }
    else
    {
        LogError("R700 : Failed delivered telemetry message for <%s>, error=<%d>", (const char *)device->SensorState->componentName, TelemetryStatus);
    }

    IoTHubMessage_Destroy(callbackContext->messageHandle);
    free(callbackContext);
}

IOTHUB_CLIENT_RESULT
ImpinjReader_RouteSendEventAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    IOTHUB_MESSAGE_HANDLE EventMessageHandle,
    IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK EventConfirmationCallback,
    void *UserContextCallback)
{
    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;
    PNP_BRIDGE_CLIENT_HANDLE clientHandle = PnpComponentHandleGetClientHandle(PnpComponentHandle);
    if ((iothubClientResult = PnpBridgeClient_SendEventAsync(clientHandle, EventMessageHandle,
                                                             EventConfirmationCallback, UserContextCallback)) != IOTHUB_CLIENT_OK)
    {
        LogError("IoTHub client call to _SendEventAsync failed with error code %d", iothubClientResult);
        goto exit;
    }
    else
    {
        LogInfo("IoTHub client call to _SendEventAsync succeeded");
    }

exit:
    return iothubClientResult;
}

void ImpinjReader_ProcessReaderEvent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    IOTHUB_MESSAGE_HANDLE MessageHandle,
    char *oneMessage)
{
    JSON_Value *jsonVal_Message = NULL;

    if ((jsonVal_Message = json_parse_string(oneMessage)) == NULL)
    {
        LogError("R700 : Unable to ReaderEvent JSON");
    }

    if (jsonVal_Message)
    {
        json_value_free(jsonVal_Message);
    }
}

/****************************************************************
Adds EventType property to message for message routing
****************************************************************/
IOTHUB_MESSAGE_RESULT
ImpinjReader_AddMessageProperty(
    IOTHUB_MESSAGE_HANDLE MessageHandle,
    char *Message)
{
    IOTHUB_MESSAGE_RESULT result;
    JSON_Value *jsonVal_Message;
    JSON_Object *jsonObj_Message;
    JSON_Value *jsonVal_ReaderEvent;
    JSON_Object *jsonObj_ReaderEvent;

    R700_READER_EVENT eventType = EventNone;
    char buffer[3] = {0};

    if ((jsonVal_Message = json_parse_string(Message)) == NULL)
    {
        LogError("R700 : Unable to parse message JSON");
    }
    else if ((jsonObj_Message = json_value_get_object(jsonVal_Message)) == NULL)
    {
        LogError("R700 : Cannot retrieve message JSON object");
    }
    else if (json_object_has_value_of_type(jsonObj_Message, g_ReaderEvent, JSONObject) == 0)
    {
        LogError("R700 : Message does not contain Reader Event Object");
    }
    else if ((jsonVal_ReaderEvent = json_object_get_value(jsonObj_Message, g_ReaderEvent)) == NULL)
    {
        LogError("R700 : Cannot retrieve Reader Event JSON value");
    }
    else if ((jsonObj_ReaderEvent = json_value_get_object(jsonVal_ReaderEvent)) == NULL)
    {
        LogError("R700 : Cannot retrieve Reader Event JSON object");
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_TagInventoryEvent) == 1)
    {
        eventType = TagInventory;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_AntennaConnectedEvent) == 1)
    {
        eventType = AntennaConnected;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_AntennaDisconnectedEvent) == 1)
    {
        eventType = AntennaDisconnected;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_InventoryStatusEvent) == 1)
    {
        eventType = InventoryStatus;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_InventoryTerminatedEvent) == 1)
    {
        eventType = InventoryTerminated;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_DiagnosticEvent) == 1)
    {
        eventType = Diagnostic;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_OverflowEvent) == 1)
    {
        eventType = Overflow;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_TagLocationEntryEvent) == 1)
    {
        eventType = TagLocationEntry;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_TagLocationUpdateEvent) == 1)
    {
        eventType = TagLocationUpdate;
    }
    else if (json_object_has_value(jsonObj_ReaderEvent, g_ReaderEvent_TagLocationExitEvent) == 1)
    {
        eventType = TagLocationExit;
    }

    if (eventType != EventNone)
    {
        sprintf(buffer, "%d", eventType);
        result = IoTHubMessage_SetProperty(MessageHandle, "EventType", buffer);
    }

    if (jsonVal_Message)
    {
        json_value_free(jsonVal_Message);
    }

    return result;
}

/****************************************************************
Allocates a context for callback
****************************************************************/
static PIMPINJ_R700_MESSAGE_CONTEXT
CreateMessageInstance(
    IOTHUB_MESSAGE_HANDLE MessageHandle,
    PIMPINJ_READER Reader)
{
    PIMPINJ_R700_MESSAGE_CONTEXT messageContext = (PIMPINJ_R700_MESSAGE_CONTEXT)malloc(sizeof(IMPINJ_R700_MESSAGE_CONTEXT));
    if (NULL == messageContext)
    {
        LogError("R700 : Failed allocating 'IMPINJ_R700_MESSAGE_CONTEXT'");
    }
    else
    {
        memset(messageContext, 0, sizeof(IMPINJ_R700_MESSAGE_CONTEXT));

        if ((messageContext->messageHandle = IoTHubMessage_Clone(MessageHandle)) == NULL)
        {
            LogError("R700 : Failed clone nessage handle");
            free(messageContext);
            messageContext = NULL;
        }
        else
        {
            messageContext->reader = Reader;
        }
    }

    return messageContext;
}

IOTHUB_CLIENT_RESULT
ImpinjReader_SendTelemetryMessagesAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    PIMPINJ_R700_MESSAGE_CONTEXT callbackContext;

    // read curl stream
    int uSecInit = clock();
    int uSecTimer = 0;
    int uSecTarget = 5000;

    while (uSecTimer < uSecTarget)
    {
        // pull messages out of buffer for target time, then return to calling function
        CURL_Stream_Read_Data read_data = curlStreamReadBufferChunk(device->curl_stream_session);

        if (read_data.dataChunk == NULL)
        { // if no data in buffer, stop reading and return to calling function
            // LogInfo("No data returned from stream buffer.");
            IoTHubMessage_Destroy(messageHandle);
            return IOTHUB_CLIENT_OK;
        }

        char *oneMessage = strtok(read_data.dataChunk, MESSAGE_SPLIT_DELIMITER); // split data chunk by \n\r in case multiple reader events are contained in the same chunk

        int count = 0;

        while (oneMessage != NULL)
        {
            // send each event individually

            count++;

            LogInfo("R700 : TELEMETRY Message %d: %s", count, oneMessage);

            char *currentMessage = ImpinjReader_CreateJsonResponse("ReaderEvent", oneMessage); // TODO: Parameterize this property name

            if ((messageHandle = PnP_CreateTelemetryMessageHandle(device->SensorState->componentName, currentMessage)) == NULL)
            {
                LogError("R700 : PnP_CreateTelemetryMessageHandle failed.");
            }
            else if ((callbackContext = CreateMessageInstance(messageHandle, device)) == NULL)
            {
                LogError("R700 : Failed to allocate callback context");
            }
            else if (ImpinjReader_AddMessageProperty(messageHandle, currentMessage) != IOTHUB_MESSAGE_OK)
            {
                LogError("R700 : Failed to add message property");
            }
            else if ((result = ImpinjReader_RouteSendEventAsync(PnpComponentHandle, messageHandle,
                                                                ImpinjReader_TelemetryCallback, callbackContext)) != IOTHUB_CLIENT_OK)
            {
                IoTHubMessage_Destroy(messageHandle);
                LogError("R700 : ImpinjReader_RouteSendEventAsync failed, error=%d", result);
            }

            oneMessage = strtok(NULL, MESSAGE_SPLIT_DELIMITER); // continue splitting until all messages are sent individually
        }

        uSecTimer = clock() - uSecInit;
    }

    return result;
}

/****************************************************************
COMMAND Section
****************************************************************/
/****************************************************************
Creates command response payload
****************************************************************/
int ImpinjReader_SetCommandResponse(
    unsigned char **CommandResponse,
    size_t *CommandResponseSize,
    const unsigned char *ResponseData)
{
    int result = PNP_STATUS_SUCCESS;

    if (ResponseData == NULL)
    {
        LogError("Impinj Reader Adapter:: Response Data is empty");
        *CommandResponseSize = 0;
        return PNP_STATUS_INTERNAL_ERROR;
    }

    *CommandResponseSize = strlen((char *)ResponseData);
    memset(CommandResponse, 0, sizeof(*CommandResponse));

    LogInfo("ImpinjReader_SetCommandResponse Size %ld ResponseData \"%s\"", *CommandResponseSize, ResponseData);

    // Allocate a copy of the response data to return to the invoker. Caller will free this.
    if (mallocAndStrcpy_s((char **)CommandResponse, (char *)ResponseData) != 0)
    {
        LogError("Impinj Reader Adapter:: Unable to allocate response data");
        result = PNP_STATUS_INTERNAL_ERROR;
    }

    return result;
}

/****************************************************************
Processes Direct Method (or command)
****************************************************************/
int ImpinjReader_ProcessCommand(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *CommandName,
    JSON_Value *CommandValue,
    unsigned char **CommandResponse,
    size_t *CommandResponseSize)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    JSON_Array *jsonArray;
    JSON_Value *jsonVal_Rest = NULL;
    char *jsonString = NULL;
    char responseBuffer[128];
    int httpStatus = PNP_STATUS_INTERNAL_ERROR;
    const char *restParameter = NULL;
    char *restBody = NULL;
    const char *response;
    int i, j;
    int r700_api = UNSUPPORTED;

    LogInfo("R700 : Processing Command %s", CommandName);
    ImpinjReader_LogJsonPretty("R700 : Command Payload", CommandValue);

    // loop through
    for (i = 0; i < (sizeof(R700_REST_LIST) / sizeof(IMPINJ_R700_REST)); i++)
    {
        if (R700_REST_LIST[i].DtdlType != COMMAND)
        {
            continue;
        }
        else if (strcmp(CommandName, R700_REST_LIST[i].Name) == 0)
        {
            switch (R700_REST_LIST[i].Request)
            {
            case PROFILES_INVENTORY_PRESETS_ID_SET:
            case PROFILES_INVENTORY_PRESETS_ID_GET:
            case PROFILES_INVENTORY_PRESETS_ID_DELETE:
            case PROFILES_START:
            {
                JSON_Object *jsonOjb_PresetId;
                JSON_Value *jsonVal_PresetId;
                JSON_Value *jsonVal_PresetObject = NULL;
                JSON_Array *jsonArray_AntennaConfig = NULL;

                if ((jsonOjb_PresetId = json_value_get_object(CommandValue)) == NULL)
                {
                    httpStatus = R700_STATUS_BAD_REQUEST;
                    LogError("R700 : Unable to retrieve command payload JSON object");
                }
                else if ((jsonVal_PresetId = json_object_get_value(jsonOjb_PresetId, g_presetId)) == NULL)
                {
                    httpStatus = R700_STATUS_BAD_REQUEST;
                    LogError("R700 : Unable to retrieve command payload JSON Value for %s.", g_presetId);
                }
                else if ((restParameter = json_value_get_string(jsonVal_PresetId)) == NULL)
                {
                    httpStatus = R700_STATUS_BAD_REQUEST;
                    LogError("R700 : Unable to retrieve %s.", g_presetId);
                }
                else
                {
                    if (R700_REST_LIST[i].Request == PROFILES_INVENTORY_PRESETS_ID_SET)
                    {
                        JSON_Object *jsonObj_TagAuthentication = NULL;
                        JSON_Object *jsonObj_PowerSweeping = NULL;

                        if ((jsonVal_PresetObject = json_object_get_value(jsonOjb_PresetId, g_presetObject)) == NULL)
                        {
                            LogError("R700 : Unable to retrieve %s for command payload JSON.", g_presetObject);
                        }
                        else if ((jsonArray_AntennaConfig = json_object_dotget_array(jsonOjb_PresetId, g_presetObjectAntennaConfigs)) != NULL)
                        {
                            int arrayCount = json_array_get_count(jsonArray_AntennaConfig);

                            for (j = 0; j < arrayCount; j++)
                            {
                                JSON_Object *jsonObj_AntennaConfig = json_array_get_object(jsonArray_AntennaConfig, j);
                                JSON_Object *jsonObj_Filtering = json_object_get_object(jsonObj_AntennaConfig, g_antennaConfigFiltering);

                                if (jsonObj_Filtering)
                                {
                                    JSON_Array *jsonArray_Filters = json_object_get_array(jsonObj_Filtering, g_antennaConfigFilters);

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
                        ImpinjReader_LogJsonPretty("R700 : Create or replace inventory preset", jsonVal_PresetObject);
                        restBody = json_serialize_to_string(jsonVal_PresetObject);
                    }
                }
            }

            break;

            case PROFILES_INVENTORY_TAG:
            {
                JSON_Object *jsonObj_Tag;
                size_t count;
                int i;
                if ((jsonObj_Tag = json_value_get_object(CommandValue)) == NULL)
                {
                    httpStatus = R700_STATUS_BAD_REQUEST;
                    LogError("R700 : Unable to retrieve command payload JSON object");
                }
                else if ((count = json_object_get_count(jsonObj_Tag)) == 0)
                {
                    httpStatus = R700_STATUS_BAD_REQUEST;
                    LogInfo("R700 : Missing parameters");
                }
                else
                {
                    char tagQueryBuffer[1024] = {0};
                    char tagAntennaNumber[3] = {0};
                    char *tagQueryBufferPtr = &tagQueryBuffer[0];

                    for (i = 0; i < count; i++)
                    {
                        JSON_Value *jsonVal_TagParam;
                        const char *tagName;
                        const char *tagValue;
                        int tagNumber;
                        const char *prefix;

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
                    restParameter = tagQueryBuffer;
                }
            }

            break;

            case SYSTEM_POWER_SET:
            case SYSTEM_TIME_SET:
                restBody = json_serialize_to_string(CommandValue);
                break;
            }

            // Call REST API
            switch (R700_REST_LIST[i].RestType)
            {
            case GET:
                jsonVal_Rest = ImpinjReader_RequestGet(PnpComponentHandle, device, R700_REST_LIST[i].Request, restParameter, &httpStatus);
                break;
            case PUT:
                jsonVal_Rest = ImpinjReader_RequestPut(PnpComponentHandle, device, R700_REST_LIST[i].Request, restParameter, restBody, &httpStatus);
                break;
            case POST:
                jsonVal_Rest = ImpinjReader_RequestPost(PnpComponentHandle, device, R700_REST_LIST[i].Request, restParameter, &httpStatus);
                break;
            case DELETE:
                jsonVal_Rest = ImpinjReader_RequestDelete(PnpComponentHandle, device, R700_REST_LIST[i].Request, restParameter, &httpStatus);
                break;
            }

            LogInfo("R700 : Command %s HttpStatus %d", R700_REST_LIST[i].Name, httpStatus);
            ImpinjReader_LogJsonPretty("R700 : Response Payload", jsonVal_Rest);
            r700_api = i;
            break;
        }
    }

    if (r700_api != UNSUPPORTED)
    {
        ImpinjReader_LogJsonPretty("R700 : Command Rest Response", jsonVal_Rest);
        switch (httpStatus)
        {
        case R700_STATUS_OK:
        case R700_STATUS_CREATED:
            jsonString = json_serialize_to_string(jsonVal_Rest);
            break;

        case R700_STATUS_ACCEPTED:
            jsonString = (char *)ImpinjReader_ProcessResponse(&R700_REST_LIST[r700_api], jsonVal_Rest, httpStatus);
            LogInfo("R700 : Command Response \"%s\"", jsonString);
            break;

        case R700_STATUS_NO_CONTENT:
            break;
        case R700_STATUS_BAD_REQUEST:
        case R700_STATUS_FORBIDDEN:
        case R700_STATUS_NOT_FOUND:
        case R700_STATUS_NOT_CONFLICT:
        case R700_STATUS_INTERNAL_ERROR:
            jsonString = ImpinjReader_ProcessErrorResponse(jsonVal_Rest, R700_REST_LIST[r700_api].DtdlType);
            break;
        }

        ImpinjReader_LogJsonPretty("R700 : Command Response", jsonVal_Rest);

        switch (R700_REST_LIST[r700_api].Request)
        {
        case SYSTEM_POWER_SET:
        {
            JSON_Value *jsonVal_Power = NULL;
            int httpStatusGetPower;

            if (httpStatus != R700_STATUS_ACCEPTED)
            {
                break;
            }

            // update reported property
            if ((jsonVal_Power = ImpinjReader_RequestGet(PnpComponentHandle, device, R700_REST_LIST[SYSTEM_POWER].Request, NULL, &httpStatusGetPower)) != NULL)
            {
                if (httpStatusGetPower == R700_STATUS_OK)
                {
                    ImpinjReader_UpdateReadOnlyReportProperty(PnpComponentHandle,
                                                              device->ComponentName,
                                                              R700_REST_LIST[r700_api].Name,
                                                              jsonVal_Power);
                }
            }

            break;
        }
        }
    }

    LogInfo("R700 : jsonString %s", jsonString);

    if (jsonString == NULL)
    {
        if (r700_api == SYSTEM_POWER_SET)
        {
            response = &g_powerSourceAlreadyConfigure[0];
        }
        else
        {
            response = &g_emptyCommandResponse[0];
        }
    }
    else
    {
        response = jsonString;
    }

    if (ImpinjReader_SetCommandResponse(CommandResponse, CommandResponseSize, response) != PNP_STATUS_SUCCESS)
    {
        httpStatus = PNP_STATUS_INTERNAL_ERROR;
    }

    // clean up
    if (restBody)
    {
        json_free_serialized_string(restBody);
    }

    if (jsonVal_Rest)
    {
        json_value_free(jsonVal_Rest);
    }

    if (jsonString)
    {
        free(jsonString);
    }

    LogInfo("R700 : Processed Command %s Status %d", CommandName, httpStatus);

    return httpStatus;
}

int ImpinjReader_OnCommandCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char *CommandName,
    JSON_Value *CommandValue,
    unsigned char **CommandResponse,
    size_t *CommandResponseSize)
{
    return ImpinjReader_ProcessCommand(PnpComponentHandle, CommandName, CommandValue, CommandResponse, CommandResponseSize);
}

/****************************************************************
REST APIS
****************************************************************/
JSON_Value *ImpinjReader_RequestDelete(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    int *httpStatus)
{
    char *jsonResult;
    JSON_Value *jsonVal = NULL;
    char endPointBuffer[128];
    char *endpoint;
    char *propertyName = R700_REST_LIST[R700_Api].Name;
    bool bHasWritableProperty = false;

    if (Parameter)
    {
        snprintf(endPointBuffer, sizeof(endPointBuffer), R700_REST_LIST[R700_Api].EndPoint, Parameter);
        endpoint = (char *)&endPointBuffer;
    }
    else
    {
        endpoint = R700_REST_LIST[R700_Api].EndPoint;
    }

    LogInfo("R700 : Curl DELETE >> Endpoint \"%s\"", endpoint);
    jsonResult = curlStaticDelete(Device->curl_static_session, endpoint, httpStatus);
    LogInfo("R700 : Curl DELETE << HttpStatus %d", *httpStatus);

    switch (R700_Api)
    {
    case SYSTEM_REGION:
    case SYSTEM_REGION_GET:
        break;
    default:
        jsonVal = json_parse_string(jsonResult);
        break;
    }

    ImpinjReader_LogJsonPretty("R700 : DELETE Response Payload", jsonVal);

    return jsonVal;
}

JSON_Value *ImpinjReader_RequestGet(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    int *httpStatus)
{
    char *jsonResult;
    JSON_Value *jsonVal = NULL;
    JSON_Object *jsonObj = NULL;
    JSON_Array *jsonArray = NULL;
    JSON_Value_Type json_type;
    char endPointBuffer[128];
    char *endpoint;
    char *propertyName = R700_REST_LIST[R700_Api].Name;
    bool bHasWritableProperty = false;

    if (Parameter)
    {
        snprintf(endPointBuffer, sizeof(endPointBuffer), R700_REST_LIST[R700_Api].EndPoint, Parameter);
        endpoint = (char *)&endPointBuffer;
    }
    else
    {
        endpoint = R700_REST_LIST[R700_Api].EndPoint;
    }

    LogInfo("R700 : Curl GET >> %d Endpoint \"%s\"", R700_Api, endpoint);
    jsonResult = curlStaticGet(Device->curl_static_session, endpoint, httpStatus);
    LogInfo("R700 : Curl GET << HttpStatus %d", *httpStatus);

    switch (R700_Api)
    {
    case SYSTEM_REGION:
    {
        if ((jsonVal = json_parse_string(jsonResult)) == NULL)
        {
            LogError("R700 : Unable to parse GET Response JSON");
        }
        else if ((jsonObj = json_value_get_object(jsonVal)) == NULL)
        {
            LogError("R700 : Cannot retrieve GET Response JSON object");
        }
        else if (json_object_get_array(jsonObj, impinjReader_property_system_region_selectableRegions))
        {
            json_object_remove(jsonObj, impinjReader_property_system_region_selectableRegions);
        }

        break;
    }

    case KAFKA:
    {
        // convert KafkaBootstrapServerConfiguration array to Map
        int arrayCount;
        JSON_Value *jsonVal_Rest = NULL;
        JSON_Object *jsonObj_Rest;
        JSON_Array *jsonArray_BootStraps;
        JSON_Value *jsonVal_Kafka;
        JSON_Object *jsonObj_Kafka;

        if ((jsonVal_Rest = json_parse_string(jsonResult)) == NULL)
        {
            LogError("R700 : Unable to parse Kafka GET payload");
        }
        else if ((jsonObj_Rest = json_value_get_object(jsonVal_Rest)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON object for Kafka payload");
        }
        else if ((jsonArray_BootStraps = json_object_get_array(jsonObj_Rest, g_kafkaBootstraps)) == NULL)
        {
            LogInfo("R700 : Did not find bootstraps Array in Kafka GET payload");
        }
        else if ((arrayCount = json_array_get_count(jsonArray_BootStraps)) == 0)
        {
            LogInfo("R700 : Empty bootstraps array in Kafka GET payload");
        }
        else if ((jsonVal_Kafka = json_value_deep_copy(jsonVal_Rest)) == NULL)
        {
            LogError("R700 : Cannot create copy of Kafka JSON Value");
        }
        else if ((jsonObj_Kafka = json_value_get_object(jsonVal_Kafka)) == NULL)
        {
            LogError("R700 : Cannot create a copy of kafka Object");
        }
        else if (json_object_remove(jsonObj_Kafka, g_kafkaBootstraps) != JSONSuccess)
        {
            LogError("R700 : Failed to remove bootstraps from Kafka payload");
        }
        else
        {
            int i;

            for (i = 0; i < arrayCount; i++)
            {
                JSON_Value *jsonVal_BootStrap = NULL;
                JSON_Object *jsonObj_BootStrap = NULL;
                JSON_Value *jsonVal_BootStrapCopy = NULL;
                char buffer[32] = {0};

                if ((jsonVal_BootStrap = json_array_get_value(jsonArray_BootStraps, i)) == NULL)
                {
                    LogError("R700 : Cannot retrieve Kafka bootstrap Array index %d", i);
                }
                else if ((jsonObj_BootStrap = json_value_get_object(jsonVal_BootStrap)) == NULL)
                {
                    LogError("R700 : Cannot retrieve Kafka bootstrap Object index %d", i);
                }
                else if ((jsonVal_BootStrapCopy = json_value_deep_copy(jsonVal_BootStrap)) == NULL)
                {
                    LogError("R700 : Cannot create a copy of bootstrap Value");
                }
                else
                {
                    sprintf(buffer, "bootstraps.bootstrap%d", i);
                    json_object_dotset_value(jsonObj_Kafka, buffer, jsonVal_BootStrapCopy);
                }
            }
        }

        if (jsonVal_Rest)
        {
            json_value_free(jsonVal_Rest);
        }

        jsonVal = jsonVal_Kafka;

        break;
    }

    case PROFILES:
    {
        // convert profiles array to Map
        int arrayCount;
        JSON_Value *jsonVal_Rest;
        JSON_Array *jsonArray;
        JSON_Value *jsonVal_Profiles;
        JSON_Object *jsonObj_Profiles;

        if ((jsonVal_Rest = json_parse_string(jsonResult)) == NULL)
        {
            LogError("R700 : Unable to parse Profiles GET payload");
        }
        else if ((jsonArray = json_value_get_array(jsonVal_Rest)) == NULL)
        {
            LogError("R700 : Cannot retrieve Profiles JSON Array");
        }
        else if ((arrayCount = json_array_get_count(jsonArray)) == 0)
        {
            LogInfo("R700 : Profiles Array empty");
        }
        else if ((jsonVal_Profiles = json_value_init_object()) == NULL)
        {
            LogError("R700 : Cannot create JSON Value for Profiles Map");
        }
        else if ((jsonObj_Profiles = json_value_get_object(jsonVal_Profiles)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON Object for Profiles Map");
        }
        else
        {
            int i;

            for (i = 0; i < arrayCount; i++)
            {
                char buffer[32] = {0};
                const char *profilevalue = json_array_get_string(jsonArray, i);
                sprintf(buffer, "profile%d", i);
                json_object_set_string(jsonObj_Profiles, buffer, profilevalue);
            }
        }

        if (jsonVal_Rest)
        {
            json_value_free(jsonVal_Rest);
        }
        jsonVal = jsonVal_Profiles;

        break;
    }

    case PROFILES_INVENTORY_PRESETS_SCHEMA:
    {
        // char outputBuffer[8192] = {0};
        // char *output_ptr = &outputBuffer[0];
        // char *input_ptr = jsonResult;
        // int count = 0;
        // printf("!!!!!!!!!! Size %ld\r\n", strlen(jsonResult));
        // while (*input_ptr != '\0')
        // {
        //     if (*input_ptr < 0x20)
        //     {
        //         printf("!!!!!!!! Found 0xa at %d\r\n", count);
        //     }
        //     else{
        //         *output_ptr = *input_ptr;
        //         output_ptr++;
        //     }
        //     count++;
        //     input_ptr++;
        // }

        // printf("%d %ld !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n\r\n", count, strlen(outputBuffer));

        // jsonVal = json_parse_string(outputBuffer);

        // if (jsonVal == NULL)
        // {
        //     LogError("Json Value Null");
        // }
        break;
    }

    case SYSTEM_NETORK_INTERFACES:
    {
        int arrayCount;
        JSON_Value *jsonVal_Rest;
        JSON_Value *jsonVal_NetworkInterfaces;
        JSON_Object *jsonObj_NetworkInterfaces;

        if ((jsonVal_Rest = json_parse_string(jsonResult)) == NULL)
        {
            LogError("R700 : Unable to parse Network Interface GET payload");
        }
        else if ((jsonArray = json_value_get_array(jsonVal_Rest)) == NULL)
        {
            LogError("R700 : Cannot retrieve array in Network Interface GET payload");
        }
        else if ((arrayCount = json_array_get_count(jsonArray)) == 0)
        {
            LogInfo("R700 : Empty Network Interface Array in Network Interface payload");
        }
        else if ((jsonVal_NetworkInterfaces = json_value_init_object()) == NULL)
        {
            LogError("R700 : Cannot create JSON Value for Network Interface Map");
        }
        else if ((jsonObj_NetworkInterfaces = json_value_get_object(jsonVal_NetworkInterfaces)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON Object for Network Interface Map");
        }
        else
        {
            //convert array to map
            int i;

            for (i = 0; i < arrayCount; i++)
            {
                char buffer[32] = {0};
                JSON_Value *jsonVal_NetworkInterface = NULL;
                JSON_Object *jsonObj_NetworkInterface = NULL;
                JSON_Value *jsonVal_NetworkInterfaceCopy = NULL;
                JSON_Object *jsonObj_NetworkInterfaceCopy = NULL;

                if ((jsonVal_NetworkInterface = json_array_get_value(jsonArray, i)) == NULL)
                {
                    LogError("R700 : Cannot retrieve Network Interface Array index %d", i);
                }
                else if ((jsonObj_NetworkInterface = json_value_get_object(jsonVal_NetworkInterface)) == NULL)
                {
                    LogError("R700 : Cannot retrieve Network Interface Object index %d", i);
                }
                else if ((jsonVal_NetworkInterfaceCopy = json_value_deep_copy(jsonVal_NetworkInterface)) == NULL)
                {
                    LogError("R700 : Cannot create copy of Network Interface Object index %d", i);
                }
                else if ((jsonObj_NetworkInterfaceCopy = json_value_get_object(jsonVal_NetworkInterfaceCopy)) == NULL)
                {
                    LogError("R700 : Cannot retrieve copy of Network Interface Object index %d", i);
                }
                else if (json_object_remove(jsonObj_NetworkInterfaceCopy, "networkAddress") != JSONSuccess)
                {
                    LogError("R700 : Failed to remove networkAddress array from Network Interface");
                }
                else
                {
                    JSON_Array *jsonArray_NetworkAddress = NULL;
                    int networkAddressCount;
                    int j;

                    if ((jsonArray_NetworkAddress = json_object_get_array(jsonObj_NetworkInterface, "networkAddress")) == NULL)
                    {
                        LogInfo("R700 : Did not find networkAddress Array for Network Interface %d", i);
                    }
                    else if ((networkAddressCount = json_array_get_count(jsonArray_NetworkAddress)) == 0)
                    {
                        LogInfo("R700 : Empty networkAddress Array for Network Interface %d", i);
                    }
                    else
                    {
                        JSON_Value *jsonVal_NetworkAddressMaps;
                        JSON_Object *jsonObj_NetworkAddressMaps;

                        if ((jsonVal_NetworkAddressMaps = json_value_init_object()) == NULL)
                        {
                            LogError("R700 : Cannot create JSON Object for Network Maps");
                        }
                        else if ((jsonObj_NetworkAddressMaps = json_value_get_object(jsonVal_NetworkAddressMaps)) == NULL)
                        {
                            LogError("R700 : Cannot retrieve Network Address Maps Object");
                        }
                        else
                        {
                            for (j = 0; j < networkAddressCount; j++)
                            {
                                JSON_Value *jsonVal_NetworkAddress;
                                JSON_Object *jsonObj_NetworkAddress;

                                if ((jsonVal_NetworkAddress = json_array_get_value(jsonArray_NetworkAddress, j)) == NULL)
                                {
                                    LogError("R700 : Cannot retrieve Network Address Array index %d", j);
                                }
                                else if ((jsonObj_NetworkAddress = json_value_get_object(jsonVal_NetworkAddress)) == NULL)
                                {
                                    LogError("R700 : Cannot retrieve Network Address Object index %d", i);
                                }
                                else
                                {
                                    JSON_Value *jsonVal_NetworkAddressCopy;
                                    JSON_Value *jsonVal_NetworkAddressMap;
                                    JSON_Object *jsonObj_NetworkAddressMap;

                                    ImpinjReader_LogJsonPretty("R700 : Network Address", jsonVal_NetworkAddress);

                                    if ((jsonVal_NetworkAddressMap = json_value_init_object()) == NULL)
                                    {
                                        LogError("R700 : Cannot create JSON Object for Network Map index %d", j);
                                    }
                                    else if ((jsonObj_NetworkAddressMap = json_value_get_object(jsonVal_NetworkAddressMap)) == NULL)
                                    {
                                        LogError("R700 : Cannot retrieve Network Address Map Object index %d", i);
                                    }
                                    else if (json_object_has_value(jsonObj_NetworkAddress, "protocol") == 0)
                                    {
                                        LogError("R700 : Cannot find protocol in Network Address Object index %d", i);
                                    }
                                    else if ((jsonVal_NetworkAddressCopy = json_value_deep_copy(jsonVal_NetworkAddress)) == NULL)
                                    {
                                        LogError("R700 : Cannot create copy of Network Address index %d", i);
                                    }
                                    else
                                    {
                                        sprintf(buffer, "networkAddress.networkAddress%d", j);
                                        json_object_dotset_value(jsonObj_NetworkInterfaceCopy, buffer, jsonVal_NetworkAddressCopy);
                                    }
                                }
                            }
                        }
                    }
                }

                sprintf(buffer, "NetworkInterface%d", i);
                json_object_set_value(jsonObj_NetworkInterfaces, buffer, jsonVal_NetworkInterfaceCopy);
            }

            if (jsonVal_Rest)
            {
                json_value_free(jsonVal_Rest);
            }

            jsonVal = jsonVal_NetworkInterfaces;
        }

        break;
    }

    default:
        jsonVal = json_parse_string(jsonResult);
        break;
    }

    ImpinjReader_LogJsonPretty("R700 : GET Response Payload", jsonVal);
    return jsonVal;
}

JSON_Value *ImpinjReader_RequestPut(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    const char *Body,
    int *httpStatus)
{
    char *body = NULL;
    char *bodyToSend = NULL;
    char *jsonResult;
    char *endpoint = R700_REST_LIST[R700_Api].EndPoint;
    JSON_Value *jsonVal = NULL;
    char endPointBuffer[128];

    if (Parameter)
    {
        snprintf(endPointBuffer, sizeof(endPointBuffer), R700_REST_LIST[R700_Api].EndPoint, Parameter);
        endpoint = (char *)&endPointBuffer;
    }
    else
    {
        endpoint = R700_REST_LIST[R700_Api].EndPoint;
    }

    switch (R700_Api)
    {
    case KAFKA:
    {
        JSON_Value *jsonVal_Body;
        JSON_Object *jsonObj_Body;
        JSON_Value *jsonVal_Bootstraps;
        JSON_Object *jsonObj_Bootstraps;
        JSON_Value *jsonVal_BodyCopy;
        JSON_Object *jsonObj_BodyCopy;
        JSON_Value *jsonVal_BootstrapsArray;
        JSON_Array *jsonArray_BootStraps;
        size_t count;
        int i;

        // convert map to array
        if ((jsonVal_Body = json_parse_string(Body)) == NULL)
        {
            LogError("R700 : Unable to parse Kafka Body payload");
        }
        else if ((jsonObj_Body = json_value_get_object(jsonVal_Body)) == NULL)
        {
            LogError("R700 : Cannot retrieve JSON object for Kafka payload");
        }
        else if (json_object_has_value(jsonObj_Body, g_kafkaBootstraps) == 0)
        {
            LogInfo("R700 : Bootstraps not found in Kafka payload");
        }
        else if ((jsonVal_Bootstraps = json_object_get_value(jsonObj_Body, g_kafkaBootstraps)) == NULL)
        {
            LogError("R700 : Cannnot retrieve JSON Value for Kafka Bootstraps");
        }
        else if ((jsonObj_Bootstraps = json_value_get_object(jsonVal_Bootstraps)) == NULL)
        {
            LogError("R700 : Cannnot retrieve JSON Object for Kafka Bootstraps");
        }
        else if ((count = json_object_get_count(jsonObj_Bootstraps)) == 0)
        {
            LogInfo("R700 : Bootstraps empty");
        }
        else if ((jsonVal_BodyCopy = json_value_deep_copy(jsonVal_Body)) == NULL)
        {
            LogError("R700 : Cannot create copy of Kafka JSON Value");
        }
        else if ((jsonObj_BodyCopy = json_value_get_object(jsonVal_BodyCopy)) == NULL)
        {
            LogError("R700 : Cannnot retrieve JSON Object for Kafka Bootstraps copy");
        }
        else if (json_object_remove(jsonObj_BodyCopy, g_kafkaBootstraps) != JSONSuccess)
        {
            LogError("R700 : Failed to remove bootstraps from Kafka payload copy");
        }
        else if ((jsonVal_BootstrapsArray = json_value_init_array()) == NULL)
        {
            LogError("R700 : Failed to create JSON Value for bootstrap array");
        }
        else if ((jsonArray_BootStraps = json_value_get_array(jsonVal_BootstrapsArray)) == NULL)
        {
            LogError("R700 : Failed to retrieve Bootstrap array");
        }
        else
        {
            ImpinjReader_LogJsonPretty("R700 : Kafka payload", jsonVal_Body);
            ImpinjReader_LogJsonPretty("R700 : Kafka payloa Copy", jsonVal_BodyCopy);

            for (i = 0; i < count; i++)
            {
                JSON_Value *jsonVal_Bootstrap;
                JSON_Value *jsonVal_BootstrapCopy;

                if ((jsonVal_Bootstrap = json_object_get_value_at(jsonObj_Bootstraps, i)) == NULL)
                {
                    LogError("R700 : Unable to retrieve bootstrap index %d", i);
                }
                else if ((jsonVal_BootstrapCopy = json_value_deep_copy(jsonVal_Bootstrap)) == NULL)
                {
                    LogError("R700 : Unable to append bootstrap value index %d", i);
                }
                else if ((json_array_append_value(jsonArray_BootStraps, jsonVal_BootstrapCopy)) != JSONSuccess)
                {
                    LogError("R700 : Unable to append bootstrap value index %d", i);
                }
                else
                {
                    char *tmp = json_serialize_to_string_pretty(jsonVal_BootstrapCopy);
                }
            }
            json_object_set_value(jsonObj_BodyCopy, g_kafkaBootstraps, json_array_get_wrapping_value(jsonArray_BootStraps));

            body = json_serialize_to_string(jsonVal_BodyCopy);

            json_value_free(jsonVal_BodyCopy);
            json_value_free(jsonVal_Body);
        }

        break;
    }
    }

    if (body)
    {
        bodyToSend = body;
    }
    else
    {
        bodyToSend = (char *)Body;
    }

    LogInfo("R700 : Curl PUT >> %s Body %s", endpoint, bodyToSend);
    jsonResult = curlStaticPut(Device->curl_static_session, endpoint, bodyToSend, httpStatus);
    LogInfo("R700 : Curl PUT << Status %d", *httpStatus);

    if (body)
    {
        json_free_serialized_string(body);
    }

    jsonVal = json_parse_string(jsonResult);

    ImpinjReader_LogJsonPretty("R700 : PUT Response Payload", jsonVal);

    return jsonVal;
}

JSON_Value *ImpinjReader_RequestPost(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    PIMPINJ_READER Device,
    int R700_Api,
    const char *Parameter,
    int *httpStatus)
{
    char *jsonResult;
    char *endpoint = R700_REST_LIST[R700_Api].EndPoint;
    char *postData;
    JSON_Value *jsonVal = NULL;

    if (R700_Api == PROFILES_START)
    {
        char endPointData[R700_PRESET_START_LENGTH] = "";
        sprintf(endPointData, R700_REST_LIST[R700_Api].EndPoint, Parameter);
        // endPointData[R700_PRESET_START_LENGTH] = '\0';
        endpoint = (char *)&endPointData;
        postData = NULL;
    }
    else
    {
        endpoint = R700_REST_LIST[R700_Api].EndPoint;
        postData = (char *)Parameter;
    }

    LogInfo("R700 : Curl POST >> %s", endpoint);
    jsonResult = curlStaticPost(Device->curl_static_session, endpoint, postData, httpStatus);
    LogInfo("R700 : Curl POST << Status %d", *httpStatus);

    switch (R700_Api)
    {
    case SYSTEM_REGION:
    case SYSTEM_REGION_GET:
        break;
    default:
        jsonVal = json_parse_string(jsonResult);
        break;
    }
    ImpinjReader_LogJsonPretty("R700 : POST Response Payload", jsonVal);

    return jsonVal;
}

/****************************************************************
PnP Bridge
****************************************************************/
IOTHUB_CLIENT_RESULT ImpinjReader_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    // Store client handle before starting Pnp component
    device->ClientHandle = PnpComponentHandleGetClientHandle(PnpComponentHandle);

    // Set shutdown state
    device->ShuttingDown = false;
    LogInfo("Impinj Reader: Starting Pnp Component");

    PnpComponentHandleSetContext(PnpComponentHandle, device);

    curlStreamSpawnReaderThread(device->curl_stream_session);

    // Create a thread to periodically publish telemetry
    if (ThreadAPI_Create(&device->WorkerHandle, ImpinjReader_TelemetryWorker, PnpComponentHandle) != THREADAPI_OK)
    {
        LogError("ThreadAPI_Create failed");
        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    if (device)
    {
        device->ShuttingDown = true;
        curlStreamStopThread(device->curl_stream_session);
        ThreadAPI_Join(device->WorkerHandle, NULL);
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    if (device != NULL)
    {
        if (device->SensorState != NULL)
        {
            if (device->SensorState->customerName != NULL)
            {
                if (device->curl_static_session != NULL)
                {
                    curlStreamCleanup(device->curl_stream_session);
                    curlStaticCleanup(device->curl_static_session);
                    free(device->curl_static_session);
                }
                free(device->SensorState->customerName);
            }
            free(device->SensorState);
        }
        free(device);

        PnpComponentHandleSetContext(PnpComponentHandle, NULL);
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);

    writeLed(SYSTEM_GREEN, LED_OFF);
    curl_global_cleanup(); // cleanup cURL globally

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_CreatePnpAdapter(
    const JSON_Object *AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterGlobalConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);

    curl_global_init(CURL_GLOBAL_DEFAULT); // initialize cURL globally

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT
ImpinjReader_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char *ComponentName,
    const JSON_Object *AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterComponentConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PIMPINJ_READER device = NULL;

    /* print component creation message */
    char compCreateStr[] = "Creating Impinj Reader component: ";

    char *compHostname;
    char *http_user;
    char *http_pass;

    compHostname = (char *)json_object_dotget_string(AdapterComponentConfig, "hostname");
    http_user = (char *)json_object_dotget_string(AdapterComponentConfig, "username");
    http_pass = (char *)json_object_dotget_string(AdapterComponentConfig, "password");

    strcat(compCreateStr, ComponentName);
    strcat(compCreateStr, "\n       Hostname: ");
    strcat(compCreateStr, compHostname);

    LogInfo("%s", compCreateStr);

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        BridgeComponentHandle = NULL;
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    /* initialize base HTTP strings */

    char str_http[] = "https://";
    char str_basepath[] = "/api/v1";

    char build_str_url_always[100] = "";
    strcat(build_str_url_always, str_http);
    strcat(build_str_url_always, compHostname);
    strcat(build_str_url_always, str_basepath);

    char *http_basepath = Str_Trim(build_str_url_always);

    /* initialize cURL sessions */
    CURL_Static_Session_Data *curl_static_session = curlStaticInit(http_user, http_pass, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);
    CURL_Stream_Session_Data *curl_stream_session = curlStreamInit(http_user, http_pass, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

    device = calloc(1, sizeof(IMPINJ_READER));
    if (NULL == device)
    {

        LogError("Unable to allocate memory for Impinj Reader component.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    device->SensorState = calloc(1, sizeof(IMPINJ_READER_STATE));
    if (NULL == device)
    {
        LogError("Unable to allocate memory for Impinj Reader component state.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    mallocAndStrcpy_s(&device->SensorState->componentName, ComponentName);

    device->curl_static_session = curl_static_session;
    device->curl_stream_session = curl_stream_session;
    device->ComponentName = ComponentName;

    PnpComponentHandleSetContext(BridgeComponentHandle, device);
    PnpComponentHandleSetPropertyPatchCallback(BridgeComponentHandle, ImpinjReader_OnPropertyPatchCallback);
    PnpComponentHandleSetPropertyCompleteCallback(BridgeComponentHandle, ImpinjReader_OnPropertyCompleteCallback);
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, ImpinjReader_OnCommandCallback);

exit:
    return result;
}

PNP_ADAPTER ImpinjReaderR700 = {
    .identity = "impinj-reader-r700",
    .createAdapter = ImpinjReader_CreatePnpAdapter,
    .createPnpComponent = ImpinjReader_CreatePnpComponent,
    .startPnpComponent = ImpinjReader_StartPnpComponent,
    .stopPnpComponent = ImpinjReader_StopPnpComponent,
    .destroyPnpComponent = ImpinjReader_DestroyPnpComponent,
    .destroyAdapter = ImpinjReader_DestroyPnpAdapter};
