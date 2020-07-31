// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "environmental_sensor_pnpbridge.h"

int EnvironmentSensor_TelemetryWorker(
    void* context)
{
    PENVIRONMENT_SENSOR device = (PENVIRONMENT_SENSOR)context;

    // Report telemetry every 5 minutes till we are asked to stop
    while (true) {
        if (device->ShuttingDown) {
            return IOTHUB_CLIENT_OK;
        }

        SampleEnvironmentalSensor_SendTelemetryMessagesAsync(device);

        // Sleep for 5 sec
        ThreadAPI_Sleep(5000);
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT EnvironmentSensor_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    PENVIRONMENT_SENSOR device = PnpComponentHandleGetContext(PnpComponentHandle);
    IOTHUB_DEVICE_CLIENT_HANDLE deviceHandle = PnpComponentHandleGetIotHubDeviceClient(PnpComponentHandle);
    device->DeviceClient = deviceHandle;

    // Report Device State Async
    result = SampleEnvironmentalSensor_ReportDeviceStateAsync(deviceHandle, device->SensorState->componentName);
    device->ShuttingDown = false;

    // Create a thread to periodically publish telemetry
    if (ThreadAPI_Create(&device->WorkerHandle, EnvironmentSensor_TelemetryWorker, device) != THREADAPI_OK) {
        LogError("ThreadAPI_Create failed");
        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT EnvironmentSensor_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PENVIRONMENT_SENSOR device = PnpComponentHandleGetContext(PnpComponentHandle);

    if (device) {
        device->ShuttingDown = true;
        ThreadAPI_Join(device->WorkerHandle, NULL);
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT EnvironmentSensor_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PENVIRONMENT_SENSOR device = PnpComponentHandleGetContext(PnpComponentHandle);
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


IOTHUB_CLIENT_RESULT EnvironmentSensor_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterGlobalConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT EnvironmentSensor_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    return IOTHUB_CLIENT_OK;
}

void EnvironmentSensor_ProcessPropertyUpdate(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    void* userContextCallback
)
{
    PENVIRONMENT_SENSOR device = PnpComponentHandleGetContext(PnpComponentHandle);
    IOTHUB_DEVICE_CLIENT_HANDLE deviceClient = (IOTHUB_DEVICE_CLIENT_HANDLE)userContextCallback;
    SampleEnvironmentalSensor_ProcessPropertyUpdate(device, deviceClient, PropertyName, PropertyValue, version);
}

int EnvironmentalSensor_ProcessCommand(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize
)
{
    PENVIRONMENT_SENSOR device = PnpComponentHandleGetContext(PnpComponentHandle);
    return SampleEnvironmentalSensor_ProcessCommandUpdate(device, CommandName, CommandValue, CommandResponse, CommandResponseSize);
}

IOTHUB_CLIENT_RESULT
EnvironmentSensor_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName, 
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterComponentConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PENVIRONMENT_SENSOR device = NULL;

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        BridgeComponentHandle = NULL;
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    device = calloc(1, sizeof(ENVIRONMENT_SENSOR));
    if (NULL == device) {
        LogError("Unable to allocate memory for environmental sensor.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    device->SensorState = calloc(1, sizeof(ENVIRONMENTAL_SENSOR_STATE));
    if (NULL == device) {
        LogError("Unable to allocate memory for environmental sensor state.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    memset(&device->SensorState, 0, sizeof(device->SensorState));
    strcpy(device->SensorState->componentName, ComponentName);

    PnpComponentHandleSetContext(BridgeComponentHandle, device);
    PnpComponentHandleSetPropertyUpdateCallback(BridgeComponentHandle, EnvironmentSensor_ProcessPropertyUpdate);
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, EnvironmentalSensor_ProcessCommand);

exit:
    return result;
}

PNP_ADAPTER EnvironmentSensorInterface = {
    .identity = "environment-sensor-sample-pnp-adapter",
    .createAdapter = EnvironmentSensor_CreatePnpAdapter,
    .createPnpComponent = EnvironmentSensor_CreatePnpComponent,
    .startPnpComponent = EnvironmentSensor_StartPnpComponent,
    .stopPnpComponent = EnvironmentSensor_StopPnpComponent,
    .destroyPnpComponent = EnvironmentSensor_DestroyPnpComponent,
    .destroyAdapter = EnvironmentSensor_DestroyPnpAdapter
};

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        LogInfo("Using configuration from specified file path: %s", argv[1]);
        PnpBridge_Main((const char*)argv[1]);
    }
    else
    {
        LogInfo("Using default configuration location");
        PnpBridge_Main((const char*)"config.json");
    }
    
    return 0;
}
