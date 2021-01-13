#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "impinj_reader_r700.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

#include "azure_c_shared_utility/const_defines.h"

// Telemetry names for this interface
//
static const char* impinjReader_telemetry_eventData = "eventData";

//
// Read-only property, device state indiciating whether its online or not
//
static const char impinjReader_property_deviceStatus[] = "deviceStatus";
static const unsigned char impinjReader_property_deviceStatus_exampleData[] = "{\"status\":\"idle\",\"time\":\"2021-01-12T22:19:10.293609888Z\",\"serialNumber\":\"37020340091\",\"mqttBrokerConnectionStatus\":\"disconnected\",\"mqttTlsAuthentication\":\"none\",\"kafkaClusterConnectionStatus\":\"connected\"}";
static const int impinjReader_property_deviceStatus_DataLen = sizeof(impinjReader_property_deviceStatus_exampleData) - 1;

//
// Callback command names for this interface.
//vb
static const char impinjReader_command_startPreset[] = "startPreset";
static const char impinjReader_command_stopPreset[] = "stopPreset";

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
// static const unsigned char sampleEnviromentalSensor_BlinkResponse[] = "{ \"status\": 12, \"description\": \"leds blinking\" }";
// static const unsigned char sampleEnviromentalSensor_TurnOnLightResponse[] = "{ \"status\": 1, \"description\": \"light on\" }";
// static const unsigned char sampleEnviromentalSensor_TurnOffLightResponse[] = "{ \"status\": 1, \"description\": \"light off\" }";
static const unsigned char impinjReader_EmptyBody[] = "\" \"";

static const unsigned char impinjReader_OutOfMemory[] = "\"Out of memory\"";
static const unsigned char impinjReader_NotImplemented[] = "\"Requested command not implemented on this interface\"";

//
// Property names that are updatable from the server application/operator
//
static const char impinjReader_property_testPropertyString[] = "testPropertyString";

//
// Response description is an optional, human readable message including more information
// about the setting of the property
//
static const char g_ImpinjReaderPropertyResponseDescription[] = "success";

// Format of the body when responding to a targetTemperature 
// static const char g_environmentalSensorBrightnessResponseFormat[] = "%.2d";


// ImpinjReader_SetCommandResponse is a helper that fills out a command response
static int ImpinjReader_SetCommandResponse(
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



int ImpinjReader_TelemetryWorker(
    void* context)
{
    PNPBRIDGE_COMPONENT_HANDLE componentHandle = (PNPBRIDGE_COMPONENT_HANDLE) context;
    PIMPINJ_READER device = PnpComponentHandleGetContext(componentHandle);


    // Report telemetry every 5 seconds till we are asked to stop
    while (true)
    {
        if (device->ShuttingDown)
        {
            return IOTHUB_CLIENT_OK;
        }

        // ImpinjReader_SendTelemetryMessagesAsync(componentHandle);
        LogInfo("Telemetry Worker: Ping");  // Placeholder for actual data
        // Sleep for 5 seconds
        ThreadAPI_Sleep(5000);
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterGlobalConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);

    curl_global_init(CURL_GLOBAL_DEFAULT);  // initialize cURL globally

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT
ImpinjReader_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName, 
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterComponentConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PIMPINJ_READER device = NULL;

    /* print component creation message */
    char compCreateStr[] = "Creating Impinj Reader component: ";
    const char* compHostname;
    compHostname = json_object_dotget_string(AdapterComponentConfig, "hostname");
    strcat(compCreateStr, ComponentName);
    strcat(compCreateStr, "\n       Hostname: ");
    strcat(compCreateStr, compHostname);

    LogInfo(compCreateStr);

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        BridgeComponentHandle = NULL;
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    /* initialize cURL handles */
    CURL *static_handle;
    CURL *stream_handle;
    CURLM *multi_handle;
    static_handle = curl_easy_init();
    stream_handle = curl_easy_init();   

    /* initialize base HTTP strings */
    char str_http[] = "http://";
    char str_basepath[] = "/api/v1/";
    char str_endpoint_status[] = "status";
    char str_endpoint_stream[] = "data/stream";

    char str_url_always[100] = "";
    strcat(str_url_always, str_http);
    strcat(str_url_always, compHostname);
    strcat(str_url_always, str_basepath);

    char str_url_status[100] = "";
    strcat(str_url_status, str_url_always);
    strcat(str_url_status, str_endpoint_status);

    char str_url_stream[100] = "";
    strcat(str_url_stream, str_url_always);
    strcat(str_url_stream, str_endpoint_stream);

    char http_user[] = "root";
    char http_pass[] = "impinj";
    char http_login[50] = "";
    strcat(http_login, http_user);
    strcat(http_login, ":");
    strcat(http_login, http_pass);

    LogInfo("Status Endpoint: %s", str_url_status);
    LogInfo("Stream Endpoint: %s", str_url_stream);
    LogInfo("Reader Login: %s", http_login);

    curl_easy_setopt(static_handle, CURLOPT_URL, str_url_status);
    curl_easy_setopt(static_handle, CURLOPT_USERPWD, http_login);
    curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, DataReadCallback);

    curl_easy_setopt(stream_handle, CURLOPT_URL, str_url_stream);
    curl_easy_setopt(stream_handle, CURLOPT_USERPWD, http_login);
    curl_easy_setopt(stream_handle, CURLOPT_WRITEFUNCTION, DataReadCallback);


    device = calloc(1, sizeof(IMPINJ_READER));
    if (NULL == device) {

        LogError("Unable to allocate memory for Impinj Reader component.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    device->SensorState = calloc(1, sizeof(IMPINJ_READER_STATE));
    if (NULL == device) {
        LogError("Unable to allocate memory for Impinj Reader component state.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    mallocAndStrcpy_s(&device->SensorState->componentName, ComponentName);

    PnpComponentHandleSetContext(BridgeComponentHandle, device);
    PnpComponentHandleSetPropertyUpdateCallback(BridgeComponentHandle, ImpinjReader_OnPropertyCallback); 
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, ImpinjReader_OnCommandCallback); 

exit:
    return result;
}

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

    // Report Device State Async
    // result = ImpinjReader_ReportDeviceStateAsync(PnpComponentHandle, device->SensorState->componentName);

    // Create a thread to periodically publish telemetry
    if (ThreadAPI_Create(&device->WorkerHandle, ImpinjReader_TelemetryWorker, PnpComponentHandle) != THREADAPI_OK) {
        LogError("ThreadAPI_Create failed");
        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    if (device) {
        device->ShuttingDown = true;
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

    curl_global_cleanup();  // cleanup cURL globally

    return IOTHUB_CLIENT_OK;
}


void ImpinjReader_ProcessProperty(void* ClientHandle, const char* PropertyName, JSON_Value* PropertyValue, int version, PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    LogInfo("Proccesing Property Update: %s", PropertyName);
    LogInfo("   New Property Value: %s", json_value_get_string(PropertyValue));
}

void ImpinjReader_OnPropertyCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    void* ClientHandle
)
// {
//     SampleEnvironmentalSensor_ProcessPropertyUpdate(userContextCallback, PropertyName, PropertyValue, version, PnpComponentHandle);
// }

// void SampleEnvironmentalSensor_ProcessPropertyUpdate(
//     void * ClientHandle,
//     const char* PropertyName,
//     JSON_Value* PropertyValue,
//     int version,
//     PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    
    LogInfo("Processing Property: %s", PropertyName);
    if (strcmp(PropertyName, impinjReader_property_testPropertyString) == 0)
    {
        ImpinjReader_ProcessProperty(ClientHandle, PropertyName, PropertyValue, version, PnpComponentHandle);
    }
    // if (strcmp(PropertyName, sampleEnvironmentalSensorPropertyCustomerName) == 0)
    // {
    //     SampleEnvironmentalSensor_CustomerNameCallback(ClientHandle, PropertyName, PropertyValue, version, PnpComponentHandle);
    // }
    // else if (strcmp(PropertyName, sampleEnvironmentalSensorPropertyBrightness) == 0)
    // {
    //     SampleEnvironmentalSensor_BrightnessCallback(ClientHandle, PropertyName, PropertyValue, version, PnpComponentHandle);
    // }
    // else if (strcmp(PropertyName, sampleDeviceStateProperty) == 0)
    // {
    //     const char * PropertyValueString = json_value_get_string(PropertyValue);
    //     size_t PropertyValueLen = strlen(PropertyValueString);

    //     LogInfo("Environmental Sensor Adapter:: Property name <%s>, last reported value=<%.*s>",
    //         PropertyName, (int)PropertyValueLen, PropertyValueString);
    // }
    else
    {
        // If the property is not implemented by this interface, presently we only record a log message but do not have a mechanism to report back to the service
        LogError("Impinj Reader Adapter:: Property name <%s> is not associated with this interface", PropertyName);
    }
}

int ImpinjReader_ProcessCommand(
    PIMPINJ_READER ImpinjReader,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    if (strcmp(CommandName, "startPreset") == 0)
    {
        // return SampleEnvironmentalSensor_BlinkCallback(EnvironmentalSensor, CommandValue, CommandResponse, CommandResponseSize);
        LogInfo("Stub: Execute Start Preset command with value = %s", json_value_get_string(CommandValue));
    }
    else if (strcmp(CommandName, "stopPreset") == 0)
    {
        // return SampleEnvironmentalSensor_TurnOnLightCallback(EnvironmentalSensor, CommandValue, CommandResponse, CommandResponseSize);
        LogInfo("Stub: Execute Stop Preset command.");
    }
    else
    {
        // If the command is not implemented by this interface, by convention we return a 404 error to server.
        LogError("Impinj Reader Adapter:: Command name <%s> is not associated with this interface", CommandName);
        return 0; // SampleEnvironmentalSensor_SetCommandResponse(CommandResponse, CommandResponseSize, sampleEnviromentalSensor_NotImplemented);
    }
}

int ImpinjReader_OnCommandCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize
)
{
    LogInfo("Processing Command: %s", CommandName);
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    return ImpinjReader_ProcessCommand(device, CommandName, CommandValue, CommandResponse, CommandResponseSize);
}

PNP_ADAPTER ImpinjReaderR700 = {
    .identity = "impinj-reader-r700",
    .createAdapter = ImpinjReader_CreatePnpAdapter,
    .createPnpComponent = ImpinjReader_CreatePnpComponent,
    .startPnpComponent = ImpinjReader_StartPnpComponent,
    .stopPnpComponent = ImpinjReader_StopPnpComponent,
    .destroyPnpComponent = ImpinjReader_DestroyPnpComponent,
    .destroyAdapter = ImpinjReader_DestroyPnpAdapter
};