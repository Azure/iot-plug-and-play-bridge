// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Implements a sample interface that integrates with an environmental sensor (for reporting 
// temperature and humidity over time). It also has basic commands and properties it can process,
// such as setting brightness of a light and blinking LEDs.

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "environmental_sensor.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

#include "azure_c_shared_utility/const_defines.h"

//
// Telemetry names for this interface
//
static const char* SampleEnvironmentalSensor_TemperatureTelemetry = "temp";
static const char* SampleEnvironmentalSensor_HumidityTelemetry = "humidity";

//
// Environmental sensor's read-only property, device state indiciating whether its online or not
//
static const char sampleDeviceStateProperty[] = "state";
static const unsigned char sampleDeviceStateData[] = "true";
static const int sampleDeviceStateDataLen = sizeof(sampleDeviceStateData) - 1;

//
// Callback command names for this interface.
//
static const char sampleEnvironmentalSensorCommandBlink[] = "blink";
static const char sampleEnvironmentalSensorCommandTurnOn[] = "turnon";
static const char sampleEnvironmentalSensorCommandTurnOff[] = "turnoff";

//
// Command status codes
//
static const int commandStatusSuccess = 200;
static const int commandStatusPending = 202;
static const int commandStatusNotPresent = 404;
static const int commandStatusFailure = 500;

//
// Response to various commands [Must be valid JSON]
//
static const unsigned char sampleEnviromentalSensor_BlinkResponse[] = "{ \"status\": 12, \"description\": \"leds blinking\" }";
static const unsigned char sampleEnviromentalSensor_TurnOnLightResponse[] = "{ \"status\": 1, \"description\": \"light on\" }";
static const unsigned char sampleEnviromentalSensor_TurnOffLightResponse[] = "{ \"status\": 1, \"description\": \"light off\" }";
static const unsigned char sampleEnviromentalSensor_EmptyBody[] = "\" \"";

static const unsigned char sampleEnviromentalSensor_OutOfMemory[] = "\"Out of memory\"";
static const unsigned char sampleEnviromentalSensor_NotImplemented[] = "\"Requested command not implemented on this interface\"";

//
// Property names that are updatable from the server application/operator
//
static const char sampleEnvironmentalSensorPropertyCustomerName[] = "name";
static const char sampleEnvironmentalSensorPropertyBrightness[] = "brightness";

//
// Response description is an optional, human readable message including more information
// about the setting of the property
//
static const char g_environmentalSensorPropertyResponseDescription[] = "success";

// Format of the body when responding to a targetTemperature 
static const char g_environmentalSensorBrightnessResponseFormat[] = "%.2d";


// SampleEnvironmentalSensor_SetCommandResponse is a helper that fills out a command response
static int SampleEnvironmentalSensor_SetCommandResponse(
    unsigned char** CommandResponse,
    size_t* CommandResponseSize,
    const unsigned char* ResponseData)
{
    int result = PNP_STATUS_SUCCESS;
    if (ResponseData == NULL)
    {
        LogError("Environmental Sensor Adapter:: Response Data is empty");
        *CommandResponseSize = 0;
        return PNP_STATUS_INTERNAL_ERROR;
    }

    *CommandResponseSize = strlen((char*)ResponseData);
    memset(CommandResponse, 0, sizeof(*CommandResponse));

    // Allocate a copy of the response data to return to the invoker. Caller will free this.
    if (mallocAndStrcpy_s((char**)CommandResponse, (char*)ResponseData) != 0)
    {
        LogError("Environmental Sensor Adapter:: Unable to allocate response data");
        result = PNP_STATUS_INTERNAL_ERROR;
    }

    return result;
}

// Implement the callback to process the command "blink". Information pertaining to the request is
// specified in the CommandValue parameter, and the callback fills out data it wishes to
// return to the caller on the service in CommandResponse.

static int SampleEnvironmentalSensor_BlinkCallback(
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    int result = PNP_STATUS_SUCCESS;
    int BlinkInterval = 0;

    LogInfo("Environmental Sensor Adapter:: Blink command invoked. It has been invoked %d times previously", EnvironmentalSensor->SensorState->numTimesBlinkCommandCalled);

    if (json_value_get_type(CommandValue) != JSONNumber)
    {
        LogError("Cannot retrieve blink interval for blink command");
        result = PNP_STATUS_BAD_FORMAT;
    }
    else
    {
        BlinkInterval = (int)json_value_get_number(CommandValue);
        LogInfo("Environmental Sensor Adapter:: Blinking with interval=%d second(s)", BlinkInterval);
        EnvironmentalSensor->SensorState->numTimesBlinkCommandCalled++;
        EnvironmentalSensor->SensorState->blinkInterval = BlinkInterval;

        result = SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_BlinkResponse);
    }

    return result;
}

// Implement the callback to process the command "turnon".
static int SampleEnvironmentalSensor_TurnOnLightCallback(
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    int result = PNP_STATUS_SUCCESS;
    AZURE_UNREFERENCED_PARAMETER(EnvironmentalSensor);
    AZURE_UNREFERENCED_PARAMETER(CommandValue);
    LogInfo("Environmental Sensor Adapter:: Turn on light command invoked");

    result = SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_TurnOnLightResponse);


    return result;
}

// Implement the callback to process the command "turnoff".
static int SampleEnvironmentalSensor_TurnOffLightCallback(
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    int result = PNP_STATUS_SUCCESS;
    AZURE_UNREFERENCED_PARAMETER(EnvironmentalSensor);
    AZURE_UNREFERENCED_PARAMETER(CommandValue);
    LogInfo("Environmental Sensor Adapter:: Turn off light command invoked");

    result = SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_TurnOffLightResponse);


    return result;
}

// SampleEnvironmentalSensor_PropertyCallback is invoked when a property is updated (or failed) going to server.
// In this sample, we route ALL property callbacks to this function and just have the userContextCallback set
// to the propertyName. Product code will potentially have context stored in this userContextCallback.
static void SampleEnvironmentalSensor_PropertyCallback(
    int ReportedStatus,
    void* UserContextCallback)
{
    LogInfo("PropertyCallback called, result=%d, property name=%s", ReportedStatus, (const char*)UserContextCallback);
}

// Processes a property update, which the server initiated, for customer name.
static void SampleEnvironmentalSensor_CustomerNameCallback(
    void * ClientHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT iothubClientResult;
    STRING_HANDLE jsonToSend = NULL;
    const char * PropertyValueString = json_value_get_string(PropertyValue);
    size_t PropertyValueLen = strlen(PropertyValueString);

    LogInfo("Environmental Sensor Adapter:: CustomerName property invoked...");
    LogInfo("Environmental Sensor Adapter:: CustomerName data=<%.*s>", (int)PropertyValueLen, PropertyValueString);

    PENVIRONMENT_SENSOR EnvironmentalSensor = PnpComponentHandleGetContext(PnpComponentHandle);

    if (EnvironmentalSensor->SensorState != NULL)
    {
        if (EnvironmentalSensor->SensorState->customerName != NULL)
        {
            free (EnvironmentalSensor->SensorState->customerName);
        }

        if ((EnvironmentalSensor->SensorState->customerName = (char*)malloc(PropertyValueLen + 1)) == NULL)
        {
            LogError("Environmental Sensor Adapter:: Out of memory updating CustomerName...");
        }
        else
        {
            strncpy(EnvironmentalSensor->SensorState->customerName, (char*) PropertyValueString, PropertyValueLen);
            EnvironmentalSensor->SensorState->customerName[PropertyValueLen] = 0;
            LogInfo("Environmental Sensor Adapter:: CustomerName sucessfully updated...");

            if ((jsonToSend = PnP_CreateReportedPropertyWithStatus(EnvironmentalSensor->SensorState->componentName, PropertyName, PropertyValueString, 
                                                                        PNP_STATUS_SUCCESS, g_environmentalSensorPropertyResponseDescription, version)) == NULL)
            {
                LogError("Unable to build reported property response");
            }
            else
            {
                const char* jsonToSendStr = STRING_c_str(jsonToSend);
                size_t jsonToSendStrLen = strlen(jsonToSendStr);

                if ((iothubClientResult = SampleEnvironmentalSensor_RouteReportedState(ClientHandle, PnpComponentHandle, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
                                            SampleEnvironmentalSensor_PropertyCallback,
                                            (void*) EnvironmentalSensor->SensorState->customerName)) != IOTHUB_CLIENT_OK)
                {
                    LogError("Environmental Sensor Adapter:: SampleEnvironmentalSensor_RouteReportedState for customer name failed, error=%d", iothubClientResult);
                }
                else
                {
                    LogInfo("Environmental Sensor Adapter:: Successfully queued Property update for CustomerName for component=%s", EnvironmentalSensor->SensorState->componentName);
                }

                STRING_delete(jsonToSend);
            }
        }
    
    }
}

// Validate Brightness Levels
static bool SampleEnvironmentalSensor_ValidateBrightness(
    double brightness)
{
    if (brightness < 0 || brightness > INT_MAX)
    {
        return false;
    }
    return true;
}

// Process a property update for bright level.
static void SampleEnvironmentalSensor_BrightnessCallback(
    void * ClientHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT iothubClientResult;
    STRING_HANDLE jsonToSend = NULL;
    char targetBrightnessString[32];

    LogInfo("Environmental Sensor Adapter:: Brightness property invoked...");

    PENVIRONMENT_SENSOR EnvironmentalSensor = PnpComponentHandleGetContext(PnpComponentHandle);

    if (json_value_get_type(PropertyValue) != JSONNumber)
    {
        LogError("JSON field %s is not a number", PropertyName);
    }
    else if(EnvironmentalSensor == NULL || EnvironmentalSensor->SensorState == NULL)
    {
        LogError("Environmental sensor device context not initialized correctly.");
    }
    else if (SampleEnvironmentalSensor_ValidateBrightness(json_value_get_number(PropertyValue)))
    {
        EnvironmentalSensor->SensorState->brightness = (int) json_value_get_number(PropertyValue);
        if (snprintf(targetBrightnessString, sizeof(targetBrightnessString), 
            g_environmentalSensorBrightnessResponseFormat, EnvironmentalSensor->SensorState->brightness) < 0)
        {
            LogError("Unable to create target brightness string for reporting result");
        }
        else if ((jsonToSend = PnP_CreateReportedPropertyWithStatus(EnvironmentalSensor->SensorState->componentName,
                    PropertyName, targetBrightnessString, PNP_STATUS_SUCCESS, g_environmentalSensorPropertyResponseDescription,
                    version)) == NULL)
        {
            LogError("Unable to build reported property response");
        }
        else
        {
            const char* jsonToSendStr = STRING_c_str(jsonToSend);
            size_t jsonToSendStrLen = strlen(jsonToSendStr);

            if ((iothubClientResult = SampleEnvironmentalSensor_RouteReportedState(ClientHandle, PnpComponentHandle, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
                                        SampleEnvironmentalSensor_PropertyCallback,
                                        (void*) &EnvironmentalSensor->SensorState->brightness)) != IOTHUB_CLIENT_OK)
            {
                LogError("Environmental Sensor Adapter:: SampleEnvironmentalSensor_RouteReportedState for brightness failed, error=%d", iothubClientResult);
            }
            else
            {
                LogInfo("Environmental Sensor Adapter:: Successfully queued Property update for Brightness for component=%s", EnvironmentalSensor->SensorState->componentName);
            }

            STRING_delete(jsonToSend);
        }
    }
}

// Routes the reported property for device or module client. This function can be called either by passing a valid client handle or by passing
// a NULL client handle after components have been started such that the client handle can be extracted from the PnpComponentHandle
IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_RouteReportedState(
    void * ClientHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const unsigned char * ReportedState,
    size_t Size,
    IOTHUB_CLIENT_REPORTED_STATE_CALLBACK ReportedStateCallback,
    void * UserContextCallback)
{
    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;

    PNP_BRIDGE_CLIENT_HANDLE clientHandle = (ClientHandle != NULL) ?
            (PNP_BRIDGE_CLIENT_HANDLE) ClientHandle : PnpComponentHandleGetClientHandle(PnpComponentHandle);

    if ((iothubClientResult = PnpBridgeClient_SendReportedState(clientHandle, ReportedState, Size,
            ReportedStateCallback, UserContextCallback)) != IOTHUB_CLIENT_OK)
    {
        LogError("IoTHub client call to _SendReportedState failed with error code %d", iothubClientResult);
        goto exit;
    }
    else
    {
        LogInfo("IoTHub client call to _SendReportedState succeeded");
    }

exit:
    return iothubClientResult;
}


// Sends a reported property for device state of this simulated device.
IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_ReportDeviceStateAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char * ComponentName)
{

    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;
    STRING_HANDLE jsonToSend = NULL;

    if ((jsonToSend = PnP_CreateReportedProperty(ComponentName, sampleDeviceStateProperty, (const char*) sampleDeviceStateData)) == NULL)
    {
        LogError("Unable to build reported property response for propertyName=%s, propertyValue=%s", sampleDeviceStateProperty, sampleDeviceStateData);
    }
    else
    {
        const char* jsonToSendStr = STRING_c_str(jsonToSend);
        size_t jsonToSendStrLen = strlen(jsonToSendStr);

        if ((iothubClientResult = SampleEnvironmentalSensor_RouteReportedState(NULL, PnpComponentHandle, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
            SampleEnvironmentalSensor_PropertyCallback, (void*)sampleDeviceStateProperty)) != IOTHUB_CLIENT_OK)
        {
            LogError("Environmental Sensor Adapter:: Unable to send reported state for property=%s, error=%d",
                                sampleDeviceStateProperty, iothubClientResult);
        }
        else
        {
            LogInfo("Environmental Sensor Adapter:: Sending device information property to IoTHub. propertyName=%s, propertyValue=%s",
                        sampleDeviceStateProperty, sampleDeviceStateData);
        }

        STRING_delete(jsonToSend);
    }

    return iothubClientResult;
}


// SampleEnvironmentalSensor_ProcessCommandUpdate receives commands from the server.  This implementation acts as a simple dispatcher
// to the functions to perform the actual processing.
int SampleEnvironmentalSensor_ProcessCommandUpdate(
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    if (strcmp(CommandName, sampleEnvironmentalSensorCommandBlink) == 0)
    {
        return SampleEnvironmentalSensor_BlinkCallback(EnvironmentalSensor, CommandValue, CommandResponse, CommandResponseSize);
    }
    else if (strcmp(CommandName, sampleEnvironmentalSensorCommandTurnOn) == 0)
    {
        return SampleEnvironmentalSensor_TurnOnLightCallback(EnvironmentalSensor, CommandValue, CommandResponse, CommandResponseSize);
    }
    else if (strcmp(CommandName, sampleEnvironmentalSensorCommandTurnOff) == 0)
    {
        return SampleEnvironmentalSensor_TurnOffLightCallback(EnvironmentalSensor, CommandValue, CommandResponse, CommandResponseSize);
    }
    else
    {
        // If the command is not implemented by this interface, by convention we return a 404 error to server.
        LogError("Environmental Sensor Adapter:: Command name <%s> is not associated with this interface", CommandName);
        return SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_NotImplemented);
    }
}

// SampleEnvironmentalSensor_ProcessPropertyUpdate receives updated properties from the server.  This implementation
// acts as a simple dispatcher to the functions to perform the actual processing.
void SampleEnvironmentalSensor_ProcessPropertyUpdate(
    void * ClientHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    if (strcmp(PropertyName, sampleEnvironmentalSensorPropertyCustomerName) == 0)
    {
        SampleEnvironmentalSensor_CustomerNameCallback(ClientHandle, PropertyName, PropertyValue, version, PnpComponentHandle);
    }
    else if (strcmp(PropertyName, sampleEnvironmentalSensorPropertyBrightness) == 0)
    {
        SampleEnvironmentalSensor_BrightnessCallback(ClientHandle, PropertyName, PropertyValue, version, PnpComponentHandle);
    }
    else if (strcmp(PropertyName, sampleDeviceStateProperty) == 0)
    {
        const char * PropertyValueString = json_value_get_string(PropertyValue);
        size_t PropertyValueLen = strlen(PropertyValueString);

        LogInfo("Environmental Sensor Adapter:: Property name <%s>, last reported value=<%.*s>",
            PropertyName, (int)PropertyValueLen, PropertyValueString);
    }
    else
    {
        // If the property is not implemented by this interface, presently we only record a log message but do not have a mechanism to report back to the service
        LogError("Environmental Sensor Adapter:: Property name <%s> is not associated with this interface", PropertyName);
    }
}

// Routes the sending asynchronous events for device or module client
IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_RouteSendEventAsync(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        IOTHUB_MESSAGE_HANDLE EventMessageHandle,
        IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK EventConfirmationCallback,
        void * UserContextCallback)
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


// SampleEnvironmentalSensor_TelemetryCallback is invoked when a telemetry message
// is either successfully delivered to the service or else fails. For this sample, the userContextCallback
// is simply a string pointing to the name of the message sent. More complex scenarios may include
// more detailed state information as part of this callback.
static void SampleEnvironmentalSensor_TelemetryCallback(
    IOTHUB_CLIENT_CONFIRMATION_RESULT TelemetryStatus,
    void* UserContextCallback)
{
    PENVIRONMENT_SENSOR device = (PENVIRONMENT_SENSOR) UserContextCallback;
    if (TelemetryStatus == IOTHUB_CLIENT_CONFIRMATION_OK)
    {
        LogInfo("Environmental Sensor Adapter:: Successfully delivered telemetry message for <%s>", (const char*)device->SensorState->componentName);
    }
    else
    {
        LogError("Environmental Sensor Adapter:: Failed delivered telemetry message for <%s>, error=<%d>", (const char*)device->SensorState->componentName, TelemetryStatus);
    }
}

//
// SampleEnvironmentalSensor_SendTelemetryMessagesAsync is periodically invoked by the caller to
// send telemetry containing the current temperature and humidity (in both cases random numbers
// so this sample will work on platforms without these sensors).
//
IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_SendTelemetryMessagesAsync(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;
    PENVIRONMENT_SENSOR device = PnpComponentHandleGetContext(PnpComponentHandle);

    float currentTemperature = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
    float currentHumidity = 60.0f + ((float)rand() / RAND_MAX) * 20.0f;

    char currentMessage[128];
    sprintf(currentMessage, "{\"%s\":%.3f, \"%s\":%.3f}", SampleEnvironmentalSensor_TemperatureTelemetry, 
            currentTemperature, SampleEnvironmentalSensor_HumidityTelemetry, currentHumidity);


    if ((messageHandle = PnP_CreateTelemetryMessageHandle(device->SensorState->componentName, currentMessage)) == NULL)
    {
        LogError("Environmental Sensor Adapter:: PnP_CreateTelemetryMessageHandle failed.");
    }
    else if ((result = SampleEnvironmentalSensor_RouteSendEventAsync(PnpComponentHandle, messageHandle,
            SampleEnvironmentalSensor_TelemetryCallback, device)) != IOTHUB_CLIENT_OK)
    {
        LogError("Environmental Sensor Adapter:: SampleEnvironmentalSensor_RouteSendEventAsync failed, error=%d", result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return result;
}
