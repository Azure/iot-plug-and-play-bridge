// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Implements a sample interface that integrates with an environmental sensor (for reporting 
// temperature and humidity over time). It also has basic commands and properties it can process,
// such as setting brightness of a light and blinking LEDs. Because this sample is designed
// to be run anywhere, all of the sameple data and command processing is expressed simply with 
// random numbers and LogInfo() calls.

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "environmental_sensor.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

#include "azure_c_shared_utility/const_defines.h"

//
// Telemetry names for this interface.
//
static const char* SampleEnvironmentalSensor_TemperatureTelemetry = "temp";
static const char* SampleEnvironmentalSensor_HumidityTelemetry = "humidity";


//
// Property names and data for read-only properties for this interface.
//
// sampleDeviceState* represents the environmental sensor's read-only property, whether its online or not.
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
// What we respond to various commands with. Must be valid JSON.
//
static const unsigned char sampleEnviromentalSensor_BlinkResponse[] = "{ \"status\": 12, \"description\": \"leds blinking\" }";
static const unsigned char sampleEnviromentalSensor_TurnOnLightResponse[] = "{ \"status\": 1, \"description\": \"light on\" }";
static const unsigned char sampleEnviromentalSensor_TurnOffLightResponse[] = "{ \"status\": 1, \"description\": \"light off\" }";
static const unsigned char sampleEnviromentalSensor_EmptyBody[] = "\" \"";

static const unsigned char sampleEnviromentalSensor_OutOfMemory[] = "\"Out of memory\"";
static const unsigned char sampleEnviromentalSensor_NotImplemented[] = "\"Requested command not implemented on this interface\"";

//
// Property names that are updatebale from the server application/operator.
//

static const char sampleEnvironmentalSensorPropertyCustomerName[] = "name";
static const char sampleEnvironmentalSensorPropertyBrightness[] = "brightness";

// Response description is an optional, human readable message including more information
// about the setting of the property.
static const char g_environmentalSensorPropertyResponseDescription[] = "success";

// Format of the body when responding to a targetTemperature 
static const char g_environmentalSensorBrightnessResponseFormat[] = "%.2d";


// SampleEnvironmentalSensor_SetCommandResponse is a helper that fills out a command request
static int SampleEnvironmentalSensor_SetCommandResponse(
    unsigned char** CommandResponse,
    size_t* CommandResponseSize,
    const unsigned char* ResponseData)
{
    int result = PNP_STATUS_SUCCESS;
    if (ResponseData == NULL)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Response Data is empty");
        *CommandResponseSize = 0;
        return PNP_STATUS_INTERNAL_ERROR;
    }

    *CommandResponseSize = strlen((char*)ResponseData);
    memset(CommandResponse, 0, sizeof(*CommandResponse));

    // Allocate a copy of the response data to return to the invoker. Caller will free this.
    if (mallocAndStrcpy_s((char**)CommandResponse, (char*)ResponseData) != 0)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate response data");
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
    const char * BlinkRequest;

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Blink command invoked. It has been invoked %d times previously", EnvironmentalSensor->SensorState->numTimesBlinkCommandCalled);
    
    if ((BlinkRequest = json_value_get_string(CommandValue)) == NULL)
    {
        LogError("Cannot retrieve JSON string for command");
        result = PNP_STATUS_BAD_FORMAT;
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Blink data=<%.*s>", (int)strlen(BlinkRequest), BlinkRequest);
        EnvironmentalSensor->SensorState->numTimesBlinkCommandCalled++;
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
    const char * TurnOnLightRequest;
    AZURE_UNREFERENCED_PARAMETER(EnvironmentalSensor);
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn on light command invoked");
    if ((TurnOnLightRequest = json_value_get_string(CommandValue)) == NULL)
    {
        LogError("Cannot retrieve JSON string for command");
        result = PNP_STATUS_BAD_FORMAT;
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn on light data=<%.*s>", (int)strlen(TurnOnLightRequest), TurnOnLightRequest);
        result = SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_TurnOnLightResponse);
    }

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
    const char * TurnOffLightRequest;
    AZURE_UNREFERENCED_PARAMETER(EnvironmentalSensor);
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn off light command invoked");

    if ((TurnOffLightRequest = json_value_get_string(CommandValue)) == NULL)
    {
        LogError("Cannot retrieve JSON string for command");
        result = PNP_STATUS_BAD_FORMAT;
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn off light data=<%.*s>", (int)strlen(TurnOffLightRequest), TurnOffLightRequest);
        result = SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_TurnOffLightResponse);
    }

    return result;
}

// SampleEnvironmentalSensor_PropertyCallback is invoked when a property is updated (or failed) going to server.
// In this sample, we route ALL property callbacks to this function and just have the userContextCallback set
// to the propertyName. Product code will potentially have context stored in this userContextCallback.
static void SampleEnvironmentalSensor_PropertyCallback(
    int ReportedStatus,
    void* UserContextCallback)
{
    if (ReportedStatus == IOTHUB_CLIENT_OK)
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Updating property=<%s> succeeded", (const char*)UserContextCallback);
    }
    else
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Updating property property=<%s> failed, error=<%d>", (const char*)UserContextCallback, ReportedStatus);
    }
}

// Processes a property update, which the server initiated, for customer name.
static void SampleEnvironmentalSensor_CustomerNameCallback(
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version)
{
    IOTHUB_CLIENT_RESULT iothubClientResult;
    STRING_HANDLE jsonToSend = NULL;
    const char * PropertyValueString = json_value_get_string(PropertyValue);
    size_t PropertyValueLen = strlen(PropertyValueString);

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: CustomerName property invoked...");
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: CustomerName data=<%.*s>", (int)PropertyValueLen, PropertyValueString);

    if (EnvironmentalSensor->SensorState != NULL)
    {
        if (EnvironmentalSensor->SensorState->customerName != NULL)
        {
            free (EnvironmentalSensor->SensorState->customerName);
        }

        if ((EnvironmentalSensor->SensorState->customerName = (char*)malloc(PropertyValueLen + 1)) == NULL)
        {
            LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Out of memory updating CustomerName...");
        }
        else
        {
            strncpy(EnvironmentalSensor->SensorState->customerName, (char*) PropertyValueString, PropertyValueLen);
            EnvironmentalSensor->SensorState->customerName[PropertyValueLen] = 0;
            LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: CustomerName sucessfully updated...");

            if ((jsonToSend = PnP_CreateReportedPropertyWithStatus(EnvironmentalSensor->SensorState->componentName, PropertyName, PropertyValueString, 
                                                                        PNP_STATUS_SUCCESS, g_environmentalSensorPropertyResponseDescription, version)) == NULL)
            {
                LogError("Unable to build reported property response");
            }
            else
            {
                const char* jsonToSendStr = STRING_c_str(jsonToSend);
                size_t jsonToSendStrLen = strlen(jsonToSendStr);

                if ((iothubClientResult = IoTHubDeviceClient_SendReportedState(DeviceClient, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
                                            SampleEnvironmentalSensor_PropertyCallback,
                                            (void*) EnvironmentalSensor->SensorState->customerName)) != IOTHUB_CLIENT_OK)
                {
                    LogError("ENVIRONMENTAL_SENSOR_INTERFACE: IoTHubDeviceClient_SendReportedState for customer name failed, error=%d", iothubClientResult);
                }
                else
                {
                    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Successfully queued Property update for CustomerName for component=%s", EnvironmentalSensor->SensorState->componentName);
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
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version)
{
    IOTHUB_CLIENT_RESULT iothubClientResult;
    STRING_HANDLE jsonToSend = NULL;
    char targetBrightnessString[32];

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Brightness property invoked...");

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

            if ((iothubClientResult = IoTHubDeviceClient_SendReportedState(DeviceClient, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
                                        SampleEnvironmentalSensor_PropertyCallback,
                                        (void*) &EnvironmentalSensor->SensorState->brightness)) != IOTHUB_CLIENT_OK)
            {
                LogError("ENVIRONMENTAL_SENSOR_INTERFACE: IoTHubDeviceClient_SendReportedState for brightness failed, error=%d", iothubClientResult);
            }
            else
            {
                LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Successfully queued Property update for Brightness for component=%s", EnvironmentalSensor->SensorState->componentName);
            }

            STRING_delete(jsonToSend);
        }
    }
}

// Sends a reported property for device state of this simulated device.
IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_ReportDeviceStateAsync(
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
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

        if ((iothubClientResult = IoTHubDeviceClient_SendReportedState(DeviceClient, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
            SampleEnvironmentalSensor_PropertyCallback, (void*)sampleDeviceStateProperty)) != IOTHUB_CLIENT_OK)
        {
            LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to send reported state for property=%s, error=%d",
                                sampleDeviceStateProperty, iothubClientResult);
        }
        else
        {
            LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Sending device information property to IoTHub. propertyName=%s, propertyValue=%s",
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
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Command name <%s> is not associated with this interface", CommandName);
        return SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_NotImplemented);
    }
}

// SampleEnvironmentalSensor_ProcessPropertyUpdate receives updated properties from the server.  This implementation
// acts as a simple dispatcher to the functions to perform the actual processing.
void SampleEnvironmentalSensor_ProcessPropertyUpdate(
    PENVIRONMENT_SENSOR EnvironmentalSensor,
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version)
{
    const char * PropertyValueString = json_value_get_string(PropertyValue);
    size_t PropertyValueLen = strlen(PropertyValueString);

    if (strcmp(PropertyName, sampleEnvironmentalSensorPropertyCustomerName) == 0)
    {
        SampleEnvironmentalSensor_CustomerNameCallback(EnvironmentalSensor, DeviceClient, PropertyName, PropertyValue, version);
    }
    else if (strcmp(PropertyName, sampleEnvironmentalSensorPropertyBrightness) == 0)
    {
        SampleEnvironmentalSensor_BrightnessCallback(EnvironmentalSensor, DeviceClient, PropertyName, PropertyValue, version);
    }
    else if (strcmp(PropertyName, sampleDeviceStateProperty) == 0)
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Property name <%s>, last reported value=<%.*s>",
            PropertyName, (int)PropertyValueLen, PropertyValueString);
    }
    else
    {
        // If the property is not implemented by this interface, presently we only record a log message but do not have a mechanism to report back to the service
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Property name <%s> is not associated with this interface", PropertyName);
    }
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
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Successfully delivered telemetry message for <%s>", (const char*)device->SensorState->componentName);
    }
    else
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Failed delivered telemetry message for <%s>, error=<%d>", (const char*)device->SensorState->componentName, TelemetryStatus);
    }
}

//
// SampleEnvironmentalSensor_SendTelemetryMessagesAsync is periodically invoked by the caller to
// send telemetry containing the current temperature and humidity (in both cases random numbers
// so this sample will work on platforms without these sensors).
//
IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_SendTelemetryMessagesAsync(
    PENVIRONMENT_SENSOR EnvironmentalSensor)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

    float currentTemperature = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
    float currentHumidity = 60.0f + ((float)rand() / RAND_MAX) * 20.0f;

    char currentMessage[128];
    sprintf(currentMessage, "{\"%s\":%.3f, \"%s\":%.3f}", SampleEnvironmentalSensor_TemperatureTelemetry, 
            currentTemperature, SampleEnvironmentalSensor_HumidityTelemetry, currentHumidity);


    if ((messageHandle = PnP_CreateTelemetryMessageHandle(EnvironmentalSensor->SensorState->componentName, currentMessage)) == NULL)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: PnP_CreateTelemetryMessageHandle failed.");
    }
    else if ((result = IoTHubDeviceClient_SendEventAsync(EnvironmentalSensor->DeviceClient, messageHandle,
            SampleEnvironmentalSensor_TelemetryCallback, EnvironmentalSensor)) != IOTHUB_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: IoTHubDeviceClient_SendEventAsync failed, error=%d", result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return result;
}
