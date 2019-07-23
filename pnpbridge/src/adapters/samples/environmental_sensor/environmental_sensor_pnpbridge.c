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
} ENVIRONMENT_SENSOR, *PENVIRONMENT_SENSOR;

int EnvironmentSensor_TelemetryWorker(void* context) {
    PENVIRONMENT_SENSOR device = (PENVIRONMENT_SENSOR) context;

    // Report telemetry every 5 minutes till we are asked to stop
    while (true) {
        if (device->ShuttingDown) {
            return 0;
        }

        DigitalTwinSampleEnvironmentalSensor_SendTelemetryMessagesAsync(device->pnpinterfaceHandle);

        // Sleep for 5 sec
        ThreadAPI_Sleep(5000);
    }

    return 0;
}

int EnvironmentSensor_StartInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface) {
    PENVIRONMENT_SENSOR device = PnpAdapterInterface_GetContext(pnpInterface);

    // Create a thread to periodically publish telemetry
    if (ThreadAPI_Create(&device->WorkerHandle, EnvironmentSensor_TelemetryWorker, device) != THREADAPI_OK) {
        LogError("ThreadAPI_Create failed");
        return -1;
    }

    return 0;
}

int EnvironmentSensor_ReleaseInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface) {
    PENVIRONMENT_SENSOR device = PnpAdapterInterface_GetContext(pnpInterface);

    if (device) {
        device->ShuttingDown = true;
        ThreadAPI_Join(device->WorkerHandle, NULL);
        free(device);
    }

    return 0;
}

int 
EnvironmentSensor_CreatePnpInterface(
    PNPADAPTER_CONTEXT AdapterHandle,
    PNPMESSAGE Message
    )
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    PENVIRONMENT_SENSOR device = NULL;
    const char* interfaceId = NULL;
    PNPADPATER_INTERFACE_PARAMS interfaceParams = { 0 };
    PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;
    int res = 0;
    PNPMESSAGE_PROPERTIES* props = NULL;

    interfaceId = PnpMessage_GetInterfaceId(Message);
    props = PnpMessage_AccessProperties(Message);

    device = calloc(1, sizeof(ENVIRONMENT_SENSOR));
    if (NULL == device) {
        res = -1;
        goto end;
    }

    device->ShuttingDown = false;

    // Create PNP interface using PNP device SDK
    pnpInterfaceClient = DigitalTwinSampleEnvironmentalSensor_CreateInterface((char*)interfaceId, props->ComponentName);
    if (NULL == pnpInterfaceClient) {
        res = -1;
        goto end;
    }

    device->pnpinterfaceHandle = pnpInterfaceClient;

    // Create PnpBridge-PnpAdapter Interface
    PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, AdapterHandle, pnpInterfaceClient);

    interfaceParams.StartInterface = EnvironmentSensor_StartInterface;
    interfaceParams.ReleaseInterface = EnvironmentSensor_ReleaseInterface;
    interfaceParams.InterfaceId = (char*)interfaceId;

    res = PnpAdapterInterface_Create(&interfaceParams, &pnpAdapterInterface);
    if (res < 0) {
        goto end;
    }

    PnpAdapterInterface_SetContext(pnpAdapterInterface, device);

    // Don't bind the interface for publishMode always. There will be a device
    // arrived notification from the discovery adapter.
    /*if (NULL != publishMode && stricmp(publishMode, "always") == 0) {
        goto end;
    }*/

end:
    return res;
}

int EnvironmentSensor_Initialize(const char* adapterArgs) {
    AZURE_UNREFERENCED_PARAMETER(adapterArgs);
    return 0;
}

int EnvironmentSensor_Shutdown() {
    return 0;
}

PNP_ADAPTER EnvironmentSensorInterface = {
    .identity = "environment-sensor-sample-pnp-adapter",
    .initialize = EnvironmentSensor_Initialize,
    .shutdown = EnvironmentSensor_Shutdown,
    .createPnpInterface = EnvironmentSensor_CreatePnpInterface,
};
