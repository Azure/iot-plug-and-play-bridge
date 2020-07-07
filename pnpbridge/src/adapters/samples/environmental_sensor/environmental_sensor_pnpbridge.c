// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <pnpbridge.h>

#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/const_defines.h"


#include "parson.h"

#include "environmental_sensor_pnpbridge.h"
#include "environmental_sensor.h"

typedef struct _ENVIRONMENT_SENSOR {
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpinterfaceHandle;
    THREAD_HANDLE WorkerHandle;
    volatile bool ShuttingDown;
} ENVIRONMENT_SENSOR, * PENVIRONMENT_SENSOR;


int EnvironmentSensor_TelemetryWorker(
    void* context)
{
    PENVIRONMENT_SENSOR device = (PENVIRONMENT_SENSOR)context;

    // Report telemetry every 5 minutes till we are asked to stop
    while (true) {
        if (device->ShuttingDown) {
            return DIGITALTWIN_CLIENT_OK;
        }

        DigitalTwinSampleEnvironmentalSensor_SendTelemetryMessagesAsync(device->pnpinterfaceHandle);

        // Sleep for 5 sec
        ThreadAPI_Sleep(5000);
    }

    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT EnvironmentSensor_StartPnpInterface(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    PENVIRONMENT_SENSOR device = PnpInterfaceHandleGetContext(PnpInterfaceHandle);

    // Create a thread to periodically publish telemetry
    if (ThreadAPI_Create(&device->WorkerHandle, EnvironmentSensor_TelemetryWorker, device) != THREADAPI_OK) {
        LogError("ThreadAPI_Create failed");
        return DIGITALTWIN_CLIENT_ERROR;
    }
    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT EnvironmentSensor_StopPnpInterface(
    PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle)
{
    PENVIRONMENT_SENSOR device = PnpInterfaceHandleGetContext(PnpInterfaceHandle);

    if (device) {
        device->ShuttingDown = true;
        ThreadAPI_Join(device->WorkerHandle, NULL);
    }
    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT EnvironmentSensor_DestroyPnpInterface(
    PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle)
{
    PENVIRONMENT_SENSOR device = PnpInterfaceHandleGetContext(PnpInterfaceHandle);
    if (device)
    {
        DigitalTwinSampleEnvironmentalSensor_Close(device->pnpinterfaceHandle);
        free(device);
        PnpInterfaceHandleSetContext(PnpInterfaceHandle, NULL);
    }

    return DIGITALTWIN_CLIENT_OK;
}



DIGITALTWIN_CLIENT_RESULT
EnvironmentSensor_CreatePnpInterface(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName, 
    const JSON_Object* AdapterInterfaceConfig,
    PNPBRIDGE_INTERFACE_HANDLE BridgeInterfaceHandle,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* PnpInterfaceClient)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterInterfaceConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    PENVIRONMENT_SENSOR device = NULL;

    device = calloc(1, sizeof(ENVIRONMENT_SENSOR));
        if (NULL == device) {
            result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
            goto exit;
        }
        device->ShuttingDown = false;

    pnpInterfaceClient = DigitalTwinSampleEnvironmentalSensor_CreateInterface(ComponentName);
    if (NULL == pnpInterfaceClient) {
        result = DIGITALTWIN_CLIENT_ERROR;
        goto exit;
    }

    device->pnpinterfaceHandle = pnpInterfaceClient;

    PnpInterfaceHandleSetContext(BridgeInterfaceHandle, device);
    *PnpInterfaceClient = pnpInterfaceClient;

exit:
    return result;
}


DIGITALTWIN_CLIENT_RESULT EnvironmentSensor_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterGlobalConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    return DIGITALTWIN_CLIENT_OK;
}

DIGITALTWIN_CLIENT_RESULT EnvironmentSensor_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    return DIGITALTWIN_CLIENT_OK;
}

PNP_ADAPTER EnvironmentSensorInterface = {
    .identity = "environment-sensor-sample-pnp-adapter",
    .createAdapter = EnvironmentSensor_CreatePnpAdapter,
    .createPnpInterface = EnvironmentSensor_CreatePnpInterface,
    .startPnpInterface = EnvironmentSensor_StartPnpInterface,
    .stopPnpInterface = EnvironmentSensor_StopPnpInterface,
    .destroyPnpInterface = EnvironmentSensor_DestroyPnpInterface,
    .destroyAdapter = EnvironmentSensor_DestroyPnpAdapter,
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
