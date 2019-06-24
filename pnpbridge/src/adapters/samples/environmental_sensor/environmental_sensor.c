// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Implements a sample DigitalTwin interface that integrates with an environmental sensor (for reporting 
// temperature and humidity over time).  It also has basic commands and properties it can process,
// such as setting brightness of a light and blinking LEDs.  Because this sample is designed
// to be run anywhere, all of the sameple data and command processing is expressed simply with 
// random numbers and LogInfo() calls.

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "environmental_sensor.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"



// DigitalTwin interface name from service perspective.
static const char DigitalTwinSampleEnvironmentalSensor_InterfaceName[] = "http://contoso.com/environmentalsensor/1.0.0";


//  
//  Telemetry names for this interface.
//
static const char* DigitalTwinSampleEnvironmentalSensor_TemperatureTelemetry = "temp";
static const char* DigitalTwinSampleEnvironmentalSensor_HumidityTelemetry = "humid";


//
//  Property names and data for DigitalTwin read-only properties for this interface.
//
// digitaltwinSample_DeviceState* represents the environmental sensor's read-only property, whether its online or not.
static const char digitaltwinSample_DeviceStateProperty[] = "state";
static const char digitaltwinSample_DeviceStateData[] = "true";
static const int digitaltwinSample_DeviceStateDataLen = sizeof(digitaltwinSample_DeviceStateData) - 1;

//
//  Callback function declarations and DigitalTwin synchronous command names for this interface.
//
static void DigitalTwinSampleEnvironmentalSensor_BlinkCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback);
static void DigitalTwinSampleEnvironmentalSensor_TurnOnLightCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback);
static void DigitalTwinSampleEnvironmentalSensor_TurnOffLightCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback);

#define digitaltwinSample_EnvironmentalSensorCommandBlink "blink"
#define digitaltwinSample_EnvironmentalSensorCommandTurnOn "turnon"
#define digitaltwinSample_EnvironmentalSensorCommandTurnOff "turnoff"

static const char* digitaltwinSample_EnvironmentalSensorCommandNames[] = {
    digitaltwinSample_EnvironmentalSensorCommandBlink,
    digitaltwinSample_EnvironmentalSensorCommandTurnOn,
    digitaltwinSample_EnvironmentalSensorCommandTurnOff
};

static const DIGITALTWIN_COMMAND_EXECUTE_CALLBACK digitaltwinSample_EnvironmentalSensorCommandCallbacks[] = {
    DigitalTwinSampleEnvironmentalSensor_BlinkCallback,
    DigitalTwinSampleEnvironmentalSensor_TurnOnLightCallback,
    DigitalTwinSampleEnvironmentalSensor_TurnOffLightCallback
};

//
//  Callback function declarations and DigitalTwin asynchronous command names for this interface.
//
static void DigitalTwinSampleEnvironmentalSensor_RunDiagnosticsCallback(const DIGITALTWIN_CLIENT_ASYNC_COMMAND_REQUEST* dtClientAsyncCommandContext, DIGITALTWIN_CLIENT_ASYNC_COMMAND_RESPONSE* dtClientAsyncCommandResponseContext, void* userContextCallback);

#define digitaltwinSample_EnvironmentalSensorCommandRunDiagnostics "rundiagnostics"

static const char* digitaltwinSample_EnvironmentalSensorAsyncCommandNames[] = {
    digitaltwinSample_EnvironmentalSensorCommandRunDiagnostics
};

static const DIGITALTWIN_ASYNC_COMMAND_EXECUTE_CALLBACK digitaltwinSample_EnvironmentalSensorAsyncCommandCallbacks[] = {
    DigitalTwinSampleEnvironmentalSensor_RunDiagnosticsCallback
};



// What we respond to various commands with.  Must be valid JSON.
static const char digitaltwinSample_EnviromentalSensor_BlinkResponse[] = "{ \"status\": 12, \"description\": \"leds blinking\" }";
static const char digitaltwinSample_EnviromentalSensor_TurnOnLightResponse[] = "{ \"status\": 1, \"description\": \"light on\" }";
static const char digitaltwinSample_EnviromentalSensor_TurnOffLightResponse[] = "{ \"status\": 1, \"description\": \"light off\" }";
static const char digitaltwinSample_EnviromentalSensor_EmptyBody[] = "\" \"";


// DIGITALTWIN_CLIENT_COMMAND_CALLBACK_TABLE provides the environmental sensor with all callbacks
// required to process commands.  Note that unlike IoTHub API's - where various callbacks may
// be added and removed dynamically over a handle's lifetime - DIGITALTWIN_INTERFACE_CLIENT_HANDLE's require all callbacks
// for the lifetime of the object be specified at DigitalTwin_InterfaceClient_Create() time.
static const DIGITALTWIN_CLIENT_COMMAND_CALLBACK_TABLE digitaltwinSample_EnvironmentalSensorCommandTable =
{
    DIGITALTWIN_CLIENT_COMMAND_CALLBACK_VERSION_1, // version of structure
    sizeof(digitaltwinSample_EnvironmentalSensorCommandNames) / sizeof(digitaltwinSample_EnvironmentalSensorCommandNames[0]),
    (const char**)digitaltwinSample_EnvironmentalSensorCommandNames,
    (const DIGITALTWIN_COMMAND_EXECUTE_CALLBACK*)digitaltwinSample_EnvironmentalSensorCommandCallbacks,
    sizeof(digitaltwinSample_EnvironmentalSensorAsyncCommandNames) / sizeof(digitaltwinSample_EnvironmentalSensorAsyncCommandNames[0]),
    digitaltwinSample_EnvironmentalSensorAsyncCommandNames,
    digitaltwinSample_EnvironmentalSensorAsyncCommandCallbacks,
};

//
// Callback function declarations and DigitalTwin updateable (from service side) properties for this interface
//
static void DigitalTwinSampleEnvironmentalSensor_BrightnessCallback(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback);
static void DigitalTwinSampleEnvironmentalSensor_CustomerNameCallback(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback);

#define digitaltwinSample_EnvironmentalSensorPropertyCustomerName "name"
#define digitaltwinSample_EnvironmentalSensorPropertyBrightness "brightness"

static const char* digitaltwinSample_EnvironmentalSensorPropertyNames[] = {
    digitaltwinSample_EnvironmentalSensorPropertyCustomerName,
    digitaltwinSample_EnvironmentalSensorPropertyBrightness
};

static const DIGITALTWIN_PROPERTY_UPDATE_CALLBACK digitaltwinSample_EnvironmentalSensorPropertyCallbacks[] = {
    DigitalTwinSampleEnvironmentalSensor_CustomerNameCallback,
    DigitalTwinSampleEnvironmentalSensor_BrightnessCallback
};

// DIGITALTWIN_CLIENT_PROPERTY_UPDATED_CALLBACK_TABLE provides the environmental sensor with all
// property callbacks required to process commands.  Like DIGITALTWIN_CLIENT_COMMAND_CALLBACK_TABLE, all
// these callbacks must be specified during handle creation time.
static const DIGITALTWIN_CLIENT_PROPERTY_UPDATED_CALLBACK_TABLE digitaltwinSample_propertyTable =
{
    DIGITALTWIN_CLIENT_PROPERTY_UPDATE_VERSION_1, // version of structure
    sizeof(digitaltwinSample_EnvironmentalSensorPropertyNames) / sizeof(digitaltwinSample_EnvironmentalSensorPropertyNames[0]), // number of properties and callbacks to map to
    (const char**)digitaltwinSample_EnvironmentalSensorPropertyNames,
    (const DIGITALTWIN_PROPERTY_UPDATE_CALLBACK*)digitaltwinSample_EnvironmentalSensorPropertyCallbacks
};

// State of simulated diagnostic run.
typedef enum DIGITALTWIN_SAMPLE_DIAGNOSTIC_STATE_TAG
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_INACTIVE,
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE1,
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE2
} DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE;

//
// Application state associated with the particular interface.  In particular it contains 
// the DIGITALTWIN_INTERFACE_CLIENT_HANDLE used for responses in callbacks along with properties set
// and representations of the property update and command callbacks invoked on given interface
//
typedef struct DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE_TAG
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceClientHandle;
    int brightness;
    char* customerName;
    int numTimesBlinkCommandCalled;
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE diagnosticState;
    char* requestId;
} DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE;

// State for interface.  For simplicity we set this as a global and set during DigitalTwin_InterfaceClient_Create, but  
// callbacks of this interface don't reference it directly but instead use userContextCallback passed to them.
static DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE digitaltwinSample_EnvironmentalSensorState;

static const int commandStatusSuccess = 200;
static const int commandStatusFailure = 500;

// DigitalTwinSampleEnvironmentalSensor_SetCommandResponse is a helper that fills out a DIGITALTWIN_CLIENT_COMMAND_RESPONSE
static void DigitalTwinSampleEnvironmentalSensor_SetCommandResponse(DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, const char* responseData, int status)
{
    size_t responseLen = strlen(responseData);
    memset(dtClientCommandResponseContext, 0, sizeof(*dtClientCommandResponseContext));
    dtClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;

    // Allocate a copy of the response data to return to the invoker.  The DigitalTwin layer that invoked the application callback
    // takes responsibility for freeing this data.
    if (mallocAndStrcpy_s((char**)&dtClientCommandResponseContext->responseData, responseData) != 0)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate response data");
        dtClientCommandResponseContext->status = commandStatusFailure;
    }
    else
    {
        dtClientCommandResponseContext->responseDataLen = responseLen;
        dtClientCommandResponseContext->status = status;
    }
}

// Implement the callback to process the command "blink".  Information pertaining to the request is specified in DIGITALTWIN_CLIENT_COMMAND_REQUEST,
// and the callback fills out data it wishes to return to the caller on the service in DIGITALTWIN_CLIENT_COMMAND_RESPONSE.
static void DigitalTwinSampleEnvironmentalSensor_BlinkCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE* sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Blink command invoked.  It has been invoked %d times previously", sensorState->numTimesBlinkCommandCalled);
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Blink data=<%.*s>", (int)dtClientCommandContext->requestDataLen, dtClientCommandContext->requestData);

    sensorState->numTimesBlinkCommandCalled++;

    DigitalTwinSampleEnvironmentalSensor_SetCommandResponse(dtClientCommandResponseContext, digitaltwinSample_EnviromentalSensor_BlinkResponse, commandStatusSuccess);
}

// Implement the callback to process the command "turnon".
static void DigitalTwinSampleEnvironmentalSensor_TurnOnLightCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE* sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;
    (void)sensorState; // Sensor state not used in this sample

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn on light command invoked");
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn on light data=<%.*s>", (int)dtClientCommandContext->requestDataLen, dtClientCommandContext->requestData);

    DigitalTwinSampleEnvironmentalSensor_SetCommandResponse(dtClientCommandResponseContext, digitaltwinSample_EnviromentalSensor_TurnOnLightResponse, commandStatusSuccess);
}

// Implement the callback to process the command "turnoff".
static void DigitalTwinSampleEnvironmentalSensor_TurnOffLightCallback(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE* sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;
    (void)sensorState; // Sensor state not used in this sample

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn off light command invoked");
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Turn off light data=<%.*s>", (int)dtClientCommandContext->requestDataLen, dtClientCommandContext->requestData);

    DigitalTwinSampleEnvironmentalSensor_SetCommandResponse(dtClientCommandResponseContext, digitaltwinSample_EnviromentalSensor_TurnOffLightResponse, commandStatusSuccess);
}

// Fills in DIGITALTWIN_CLIENT_ASYNC_COMMAND_RESPONSE with appropriate status code.  Note that we do NOT return payload body at this time, but
// instead (on success cases at least) rely on subsequent calls to DigitalTwin_InterfaceClient_UpdateAsyncCommandStatus to send data as modeled.
static void DigitalTwinSampleEnvironmentalSensor_SetAsyncCommandResponse(DIGITALTWIN_CLIENT_ASYNC_COMMAND_RESPONSE* dtClientAsyncCommandResponseContext, int status)
{
    memset(dtClientAsyncCommandResponseContext, 0, sizeof(*dtClientAsyncCommandResponseContext));
    dtClientAsyncCommandResponseContext->version = DIGITALTWIN_CLIENT_ASYNC_COMMAND_RESPONSE_VERSION_1;

    // Allocate a copy of the response data to return to the invoker.  The DigitalTwin layer that invoked the application callback
    // takes responsibility for freeing this data.
    if (mallocAndStrcpy_s((char**)&dtClientAsyncCommandResponseContext->responseData, digitaltwinSample_EnviromentalSensor_EmptyBody) != 0)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate response data");
        dtClientAsyncCommandResponseContext->status = 500;
    }
    else
    {
        dtClientAsyncCommandResponseContext->responseDataLen = strlen(digitaltwinSample_EnviromentalSensor_EmptyBody);
        dtClientAsyncCommandResponseContext->status = status;
    }
}

// Implement the callback to process the command "rundiagnostic".  Note that this is an asyncronous command, so all we do in this 
// stage is to do some rudimentary checks and to store off the fact we're async for later.
static void DigitalTwinSampleEnvironmentalSensor_RunDiagnosticsCallback(const DIGITALTWIN_CLIENT_ASYNC_COMMAND_REQUEST* dtClientAsyncCommandContext, DIGITALTWIN_CLIENT_ASYNC_COMMAND_RESPONSE* dtClientAsyncCommandResponseContext, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE *sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Run diagnostics command invoked");
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Diagnostics data=<%.*s>, requestId=<%s>", (int)dtClientAsyncCommandContext->requestDataLen, dtClientAsyncCommandContext->requestData, dtClientAsyncCommandContext->requestId);

    if (sensorState->diagnosticState != DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_INACTIVE)
    {
        // If the diagnostic is already in progress, do not allow simultaneous requests.
        // Note that the requirement for only one simultaneous asynchronous command at a time *is for simplifying the sample only*.
        // The underlying DigitalTwin protocol will allow multiple simultaneous requests to be sent to the client; whether the
        // device allows this or not is a decision for the interface & device implementors.
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Run diagnostics already active.  Cannot support multiple in parallel.");
        DigitalTwinSampleEnvironmentalSensor_SetAsyncCommandResponse(dtClientAsyncCommandResponseContext, 500);
    }
    else
    {
        // At this point we need to save the requestId.  This is what the server uses to correlate subsequent responses from this operation.
        if (mallocAndStrcpy_s(&sensorState->requestId, dtClientAsyncCommandContext->requestId) != 0)
        {
            LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Cannot allocate requestId.");
            DigitalTwinSampleEnvironmentalSensor_SetAsyncCommandResponse(dtClientAsyncCommandResponseContext, 500);
        }
        else
        {
            LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Successfully set sensorState to run diagnostics.  Will run later.");
            DigitalTwinSampleEnvironmentalSensor_SetAsyncCommandResponse(dtClientAsyncCommandResponseContext, 202);
            // Moving us into DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE1 will mean our periodic wakeup will process this.
            sensorState->diagnosticState = DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE1;
        }
    }
}

static void DigitalTwinSampleEnvironmentalSensor_SetAsyncUpdateState(DIGITALTWIN_CLIENT_ASYNC_COMMAND_UPDATE *asyncCommandUpdate, const char* propertyData, int status)
{
    memset(asyncCommandUpdate, 0, sizeof(*asyncCommandUpdate));
    asyncCommandUpdate->version = DIGITALTWIN_CLIENT_ASYNC_COMMAND_UPDATE_VERSION_1;
    asyncCommandUpdate->commandName = digitaltwinSample_EnvironmentalSensorCommandRunDiagnostics;
    asyncCommandUpdate->requestId = digitaltwinSample_EnvironmentalSensorState.requestId;
    asyncCommandUpdate->propertyData = propertyData;
    asyncCommandUpdate->statusCode = status;
}

// DigitalTwinSampleEnvironmentalSensor_ProcessDiagnosticIfNecessary is periodically invoked in this sample by the main()
// thread.  It will evaluate whether the service has requested diagnostics to be run, which is an async operation.  If so,
// it will send the server an update status message depending on the state that we're at.
//
// Threading note: When this interface is invoked on the convenience layer (../digitaltwin_sample_device), this operation can
// run on any thread - while processing a callback, on the main() thread itself, or on a new thread spun up by the process.
// When running on the _LL_ layer (../digitaltwin_sample_ll_device) it *must* run on the main() thread because the the _LL_ is not
// thread safe by design.
DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleEnvironmentalSensor_ProcessDiagnosticIfNecessary(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    (void)interfaceHandle;

    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_ERROR;

    if (digitaltwinSample_EnvironmentalSensorState.diagnosticState == DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_INACTIVE)
    {
        // No pending commands to process, so this is a no-op
        result = DIGITALTWIN_CLIENT_OK;
    }
    else if (digitaltwinSample_EnvironmentalSensorState.diagnosticState == DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE1)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: In phase1 of running diagnostics.  Will alert server that we are still in progress and transition to phase2");

        // In phase1 of the diagnostic, we *only* report that the diagnostic is in progress but not yet complete.  We also transition to the next stage.
        DIGITALTWIN_CLIENT_ASYNC_COMMAND_UPDATE asyncCommandUpdate;
        DigitalTwinSampleEnvironmentalSensor_SetAsyncUpdateState(&asyncCommandUpdate, "\"Diagnostic still in progress\"", 202);

        if ((result = DigitalTwin_InterfaceClient_UpdateAsyncCommandStatus(digitaltwinSample_EnvironmentalSensorState.interfaceClientHandle, &asyncCommandUpdate)) != DIGITALTWIN_CLIENT_OK)
        {
            LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_UpdateAsyncCommandStatus failed, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        }
        else
        {
            result = DIGITALTWIN_CLIENT_OK;
        }

        digitaltwinSample_EnvironmentalSensorState.diagnosticState = DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE2;
    }
    else if (digitaltwinSample_EnvironmentalSensorState.diagnosticState == DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_PHASE2)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: In phase2 of running diagnostics.  Will alert server that we are complete.");

        // In phase2 of the diagnostic, we're complete.  Indicate to service, free resources, and move us to inactive phase so subsequent commands can arrive.
        DIGITALTWIN_CLIENT_ASYNC_COMMAND_UPDATE asyncCommandUpdate;
        DigitalTwinSampleEnvironmentalSensor_SetAsyncUpdateState(&asyncCommandUpdate, "\"Successfully run diagnostics\"", 200);

        if ((result = DigitalTwin_InterfaceClient_UpdateAsyncCommandStatus(digitaltwinSample_EnvironmentalSensorState.interfaceClientHandle, &asyncCommandUpdate)) != DIGITALTWIN_CLIENT_OK)
        {
            LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_UpdateAsyncCommandStatus failed, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        }
        else
        {
            result = DIGITALTWIN_CLIENT_OK;
        }

        free(digitaltwinSample_EnvironmentalSensorState.requestId);
        digitaltwinSample_EnvironmentalSensorState.requestId = NULL;
        digitaltwinSample_EnvironmentalSensorState.diagnosticState = DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_INACTIVE;
    }

    return result;
}



// DigitalTwinSampleEnvironmentalSensor_PropertyCallback is invoked when a property is updated (or failed) going to server.
// In this sample, we route ALL property callbacks to this function and just have the userContextCallback set
// to the propertyName.  Product code will potentially have context stored in this userContextCallback.
static void DigitalTwinSampleEnvironmentalSensor_PropertyCallback(DIGITALTWIN_CLIENT_RESULT dtReportedStatus, void* userContextCallback)
{
    if (dtReportedStatus == DIGITALTWIN_CLIENT_OK)
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Updating property=<%s> succeeded", (const char*)userContextCallback);
    }
    else
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Updating property property=<%s> failed, error=<%s>", (const char*)userContextCallback, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtReportedStatus));
    }
}

// Processes a property update, which the server initiated, for customer name.
static void DigitalTwinSampleEnvironmentalSensor_CustomerNameCallback(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE *sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;
    DIGITALTWIN_CLIENT_RESULT result;

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: CustomerName property invoked...");
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: CustomerName data=<%.*s>", (int)dtClientPropertyUpdate->propertyDesiredLen, dtClientPropertyUpdate->propertyDesired);

    DIGITALTWIN_CLIENT_PROPERTY_RESPONSE propertyResponse;

    // Version of this structure for C SDK.
    propertyResponse.version = DIGITALTWIN_CLIENT_PROPERTY_RESPONSE_VERSION_1;
    propertyResponse.responseVersion = dtClientPropertyUpdate->desiredVersion;

    free(sensorState->customerName);
    if ((sensorState->customerName = (char*)malloc(dtClientPropertyUpdate->propertyDesiredLen + 1)) == NULL)
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Out of memory updating CustomerName...");

        // Indicates failure
        propertyResponse.statusCode = 500;
        // Optional additional human readable information about status.
        propertyResponse.statusDescription = "Out of memory";
    }
    else
    {
        strncpy(sensorState->customerName, (char*)dtClientPropertyUpdate->propertyDesired, dtClientPropertyUpdate->propertyDesiredLen);
        sensorState->customerName[dtClientPropertyUpdate->propertyDesiredLen] = 0;
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: CustomerName sucessfully updated...");

        // Indicates success
        propertyResponse.statusCode = 200;
        // Optional additional human readable information about status.
        propertyResponse.statusDescription = "Property Updated Successfully";
    }

    //
    // DigitalTwin_InterfaceClient_ReportPropertyAsync takes the DIGITALTWIN_CLIENT_PROPERTY_RESPONSE and returns information back to service.
    //
    result = DigitalTwin_InterfaceClient_ReportPropertyAsync(sensorState->interfaceClientHandle, digitaltwinSample_EnvironmentalSensorPropertyCustomerName, (const char*)dtClientPropertyUpdate->propertyDesired,
        &propertyResponse, DigitalTwinSampleEnvironmentalSensor_PropertyCallback, digitaltwinSample_EnvironmentalSensorPropertyCustomerName);
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_ReportPropertyAsync for CustomerName failed, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Successfully queued Property update for CustomerName");
    }
}

// Processes a property update, which the server initiated, for brightness.
static int DigitalTwinSampleEnvironmentalSensor_ParseBrightness(const char* propertyDesired, int *brightness)
{
    int result;

    char* next;
    *brightness = (int)strtol(propertyDesired, &next, 0);
    if ((propertyDesired == next) || ((((*brightness) == INT_MAX) || ((*brightness) == INT_MIN)) && (errno != 0)))
    {
        LogError("Could not parse data=<%s> specified", propertyDesired);
        result = MU_FAILURE;
    }
    else
    {
        result = 0;
    }

    return result;
}

// Process a property update for bright level.
static void DigitalTwinSampleEnvironmentalSensor_BrightnessCallback(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE *sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;
    DIGITALTWIN_CLIENT_RESULT result;

    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Brightness property invoked...");
    LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Brightness data=<%.*s>", (int)dtClientPropertyUpdate->propertyDesiredLen, dtClientPropertyUpdate->propertyDesired);

    DIGITALTWIN_CLIENT_PROPERTY_RESPONSE propertyResponse;
    int brightness;

    // Version of this structure for C SDK.
    propertyResponse.version = DIGITALTWIN_CLIENT_PROPERTY_RESPONSE_VERSION_1;
    propertyResponse.responseVersion = dtClientPropertyUpdate->desiredVersion;

    if (DigitalTwinSampleEnvironmentalSensor_ParseBrightness((const char*)dtClientPropertyUpdate->propertyDesired, &brightness) != 0)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Invalid brightness data=<%.*s> specified", (int)dtClientPropertyUpdate->propertyDesiredLen, dtClientPropertyUpdate->propertyDesired);

        // Indicates failure
        propertyResponse.statusCode = 500;
        // Optional additional human readable information about status.
        propertyResponse.statusDescription = "Invalid brightness setting";
    }
    else
    {
        sensorState->brightness = brightness;
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Brightness successfully updated to %d...", sensorState->brightness);

        // Indicates success
        propertyResponse.statusCode = 200;
        // Optional additional human readable information about status.
        propertyResponse.statusDescription = "Brightness updated";
    }

    //
    // DigitalTwin_InterfaceClient_ReportPropertyAsync takes the DIGITALTWIN_CLIENT_PROPERTY_RESPONSE and returns information back to service.
    //
    result = DigitalTwin_InterfaceClient_ReportPropertyAsync(sensorState->interfaceClientHandle, digitaltwinSample_EnvironmentalSensorPropertyBrightness, (const char*)dtClientPropertyUpdate->propertyDesired, &propertyResponse,
        DigitalTwinSampleEnvironmentalSensor_PropertyCallback, digitaltwinSample_EnvironmentalSensorPropertyBrightness);
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_ReportPropertyAsync for Brightness failed, error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Successfully queued Property update for Brightness");
    }
}

// Sends a reported property for device state of this simulated device.
static DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleEnvironmentalSensor_ReportDeviceState(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    DIGITALTWIN_CLIENT_RESULT result;

    result = DigitalTwin_InterfaceClient_ReportPropertyAsync(interfaceHandle, digitaltwinSample_DeviceStateProperty, digitaltwinSample_DeviceStateData,
        NULL, DigitalTwinSampleEnvironmentalSensor_PropertyCallback, (void*)digitaltwinSample_DeviceStateProperty);

    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Reporting property=<%s> failed, error=<%s>", digitaltwinSample_DeviceStateProperty, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Queued async report read only property for %s", digitaltwinSample_DeviceStateProperty);
    }

    return result;
}

// DigitalTwinSampleEnvironmentalSensor_InterfaceRegisteredCallback is invoked when this interface
// is successfully or unsuccessfully registered with the service, and also when the interface is deleted.
static void DigitalTwinSampleEnvironmentalSensor_InterfaceRegisteredCallback(DIGITALTWIN_CLIENT_RESULT dtInterfaceStatus, void* userContextCallback)
{
    DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE *sensorState = (DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_STATE*)userContextCallback;
    if (dtInterfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        // Once the interface is registered, send our reported properties to the service.  
        // It *IS* safe to invoke most DigitalTwin API calls from a callback thread like this, though it 
        // is NOT safe to create/destroy/register interfaces now.
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Interface successfully registered.");
        DigitalTwinSampleEnvironmentalSensor_ReportDeviceState(sensorState->interfaceClientHandle);
    }
    else if (dtInterfaceStatus == DIGITALTWIN_CLIENT_ERROR_INTERFACE_UNREGISTERING)
    {
        // Once an interface is marked as unregistered, it cannot be used for any DigitalTwin SDK calls.
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Interface received unregistering callback.");
    }
    else
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Interface received failed, status=<%s>.", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtInterfaceStatus));
    }
}

//
// DigitalTwinSampleEnvironmentalSensor_CreateInterface is the initial entry point into the DigitalTwin Sample Environmental Sensor interface.
// It simply creates a DIGITALTWIN_INTERFACE_CLIENT_HANDLE that is mapped to the environmental sensor interface name.
// This call is synchronous, as simply creating an interface only performs initial allocations.
//
// NOTE: The actual registration of this interface is left to the caller, which may register 
// multiple interfaces on one DIGITALTWIN_DEVICE_CLIENT_HANDLE.
//
DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinSampleEnvironmentalSensor_CreateInterface(char* interfaceId)
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle;
    DIGITALTWIN_CLIENT_RESULT result;

    memset(&digitaltwinSample_EnvironmentalSensorState, 0, sizeof(digitaltwinSample_EnvironmentalSensorState));
    digitaltwinSample_EnvironmentalSensorState.diagnosticState = DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR_DIAGNOSTIC_STATE_INACTIVE;

    if ((result = DigitalTwin_InterfaceClient_Create(interfaceId, DigitalTwinSampleEnvironmentalSensor_InterfaceRegisteredCallback, (void*)&digitaltwinSample_EnvironmentalSensorState, &interfaceHandle)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate interface client handle for <%s>, error=<%s>", DigitalTwinSampleEnvironmentalSensor_InterfaceName, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        interfaceHandle = NULL;
    }
    else if ((result = DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallbacks(interfaceHandle, &digitaltwinSample_propertyTable)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallbacks failed. error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        DigitalTwinSampleEnvironmentalSensor_Close(interfaceHandle);
        interfaceHandle = NULL;
    }
    else if ((result = DigitalTwin_InterfaceClient_SetCommandsCallbacks(interfaceHandle, &digitaltwinSample_EnvironmentalSensorCommandTable)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SetCommandsCallbacks failed. error=<%s>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        DigitalTwinSampleEnvironmentalSensor_Close(interfaceHandle);
        interfaceHandle = NULL;
    }
    else
    {
        LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: Created DIGITALTWIN_INTERFACE_CLIENT_HANDLE.  InterfaceName=<%s>, handle=<%p>", DigitalTwinSampleEnvironmentalSensor_InterfaceName, interfaceHandle);
        digitaltwinSample_EnvironmentalSensorState.interfaceClientHandle = interfaceHandle;
    }

    return interfaceHandle;
}


// DigitalTwinSampleEnvironmentalSensor_TelemetryCallback is invoked when a DigitalTwin telemetry message
// is either successfully delivered to the service or else fails.  For this sample, the userContextCallback
// is simply a string pointing to the name of the message sent.  More complex scenarios may include
// more detailed state information as part of this callback.
static void DigitalTwinSampleEnvironmentalSensor_TelemetryCallback(DIGITALTWIN_CLIENT_RESULT dtTelemetryStatus, void* userContextCallback)
{
    if (dtTelemetryStatus == DIGITALTWIN_CLIENT_OK)
    {
        // This tends to overwhelm the logging on output based on how frequently this function is invoked, so removing by default.
        // LogInfo("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin successfully delivered telemetry message for <%s>", (const char*)userContextCallback);
    }
    else
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin failed delivered telemetry message for <%s>, error=<%s>", (const char*)userContextCallback, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtTelemetryStatus));
    }
}

//
// DigitalTwinSampleEnvironmentalSensor_SendTelemetryMessages is periodically invoked by the caller to
// send telemetry containing the current temperature and humidity (in both cases random numbers
// so this sample will work on platforms without these sensors).
//
DIGITALTWIN_CLIENT_RESULT DigitalTwinSampleEnvironmentalSensor_SendTelemetryMessages(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    DIGITALTWIN_CLIENT_RESULT result;

    float currentTemperature = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
    float currentHumidity = 60.0f + ((float)rand() / RAND_MAX) * 20.0f;

    char currentTemperatureMessage[128];
    char currentHumidityMessage[128];

    sprintf(currentTemperatureMessage, "%.3f", currentTemperature);
    sprintf(currentHumidityMessage, "%.3f", currentHumidity);

    if ((result = DigitalTwin_InterfaceClient_SendTelemetryAsync(interfaceHandle, DigitalTwinSampleEnvironmentalSensor_TemperatureTelemetry,
        currentTemperatureMessage, DigitalTwinSampleEnvironmentalSensor_TelemetryCallback,
        (void*)DigitalTwinSampleEnvironmentalSensor_TemperatureTelemetry)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SendTelemetryAsync failed for sending %s", DigitalTwinSampleEnvironmentalSensor_TemperatureTelemetry);
    }
    else if ((result = DigitalTwin_InterfaceClient_SendTelemetryAsync(interfaceHandle, DigitalTwinSampleEnvironmentalSensor_HumidityTelemetry,
        currentHumidityMessage, DigitalTwinSampleEnvironmentalSensor_TelemetryCallback,
        (void*)DigitalTwinSampleEnvironmentalSensor_HumidityTelemetry)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: DigitalTwin_InterfaceClient_SendTelemetryAsync failed for sending %s", DigitalTwinSampleEnvironmentalSensor_HumidityTelemetry);
    }

    return result;
}

//
// DigitalTwinSampleEnvironmentalSensor_Close is invoked when the sample device is shutting down.
//
void DigitalTwinSampleEnvironmentalSensor_Close(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    // On shutdown, in general the first call made should be to DigitalTwin_InterfaceClient_Destroy.
    // This will block if there are any active callbacks in this interface, and then
    // mark the underlying handle such that no future callbacks shall come to it.
    DigitalTwin_InterfaceClient_Destroy(interfaceHandle);

    // After DigitalTwin_InterfaceClient_Destroy returns, it is safe to assume
    // no more callbacks shall arrive for this interface and it is OK to free
    // resources callbacks otherwise may have needed.
    free(digitaltwinSample_EnvironmentalSensorState.customerName);
    digitaltwinSample_EnvironmentalSensorState.customerName = NULL;

    free(digitaltwinSample_EnvironmentalSensorState.requestId);
    digitaltwinSample_EnvironmentalSensorState.requestId = NULL;
}

