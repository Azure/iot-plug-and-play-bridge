// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <pnpbridge.h>

#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

#include "parson.h"

typedef struct _ENVIRONMENTSENSOR_DISCOVERY {
    PNPMEMORY DeviceArgs;
    PNPMEMORY AdapterArgs;
    THREAD_HANDLE WorkerHandle;
} ENVIRONMENTSENSOR_DISCOVERY;

ENVIRONMENTSENSOR_DISCOVERY EnvironmentSensor_Context = { 0 };

// Worker method that is responsible for discovering an environmental sensor and reporting
// it to PnPBridge. In this method we will create a notification for a dummy environmental sensor.
// When the Bridge processes it, we should see a 'environment-sensor-sample-pnp-adapter' adapter
// being called.
int EnvironmentSensor_DiscoveryWorker(void* context) {
    ENVIRONMENTSENSOR_DISCOVERY* envContext = context;
    PDEVICE_ADAPTER_PARMAETERS deviceArgs;
    char* str;
    JSON_Value* jdeviceArgsValue;

    if (NULL == envContext->DeviceArgs) {
        LogInfo("EnvironmentSensor_DiscoveryWorker: No device discovery parameters found in configuration.");
        return DIGITALTWIN_CLIENT_OK;
    }

    // The discovery adapter will parse the discovery parameters from the Bridge's
    //  configuration and report discovered devices to the bridge using
    // DiscoveryAdapter_ReportDevice
    deviceArgs = PnpMemory_GetBuffer(envContext->DeviceArgs, NULL);

    LogInfo("EnvironmentSensor_DiscoveryWorker: Found configuration parameters for %d devices", deviceArgs->Count);

    // This discovery adapter examines the parameters reported for the first device at AdapterParameters[0]
    // It obtains the "sensor_id" parameter and reports a fake device to the bridge using
    // DiscoveryAdapter_ReportDevice
    str = deviceArgs->AdapterParameters[0];

    jdeviceArgsValue = json_parse_string(str);
    if (NULL == jdeviceArgsValue) {
        LogError("EnvironmentSensor_DiscoveryWorker: json_parse_string failed. Invalid discovery parameters");
        return -1;
    }

    JSON_Object* jDeviceArgObject = json_value_get_object(jdeviceArgsValue);
    if (NULL == jDeviceArgObject) {
        LogError("EnvironmentSensor_DiscoveryWorker: json_value_get_object failed.  Invalid discovery parameters");
        return -1;
    }

    // Get sensor id
    const char* sensorId = json_object_dotget_string(jDeviceArgObject, "sensor_id");

    // Device change format
    const char* deviceChangeMessageformat = "{ \
                                                \"identity\": \"environment-sensor-sample-discovery-adapter\", \
                                                \"match_parameters\": { \
                                                \"sensor_id\": \"%s\" \
                                                } \
                                            }";

    // Create the device change PnP notification message
    PNPMESSAGE msg = NULL;

    PnpMessage_CreateMessage(&msg);

    size_t length = strlen(deviceChangeMessageformat) + strlen(sensorId) + 1;
    char* deviceChangeBuff = (char*)malloc(length * sizeof(char));

    sprintf_s(deviceChangeBuff, length * sizeof(char), deviceChangeMessageformat, sensorId);
    PnpMessage_SetMessage(msg, deviceChangeBuff);

    // Notify the pnpbridge of device discovery
    LogInfo("EnvironmentSensor_DiscoveryWorker: Reporting sensor ID %s to bridge", sensorId);
    DiscoveryAdapter_ReportDevice(msg);

    free(deviceChangeBuff);

    // Drop reference on the PNPMESSAGE
    PnpMemory_ReleaseReference(msg);

    return DIGITALTWIN_CLIENT_OK;
}

// .StartDiscovery callback. This is the first method called by the PnpBridge for a discovery adapter.
// The discovery adapter is expected to do initialization and save the DeviceChangeCallback. When it
// discovers a device, it needs to invoke the DeviceChangeCallback.
//
// StartDiscovery should not block
// Don't invoke DeviceChangeCallback callback from the StartDiscovery
// On returning failure, the .StopDiscovery won't be invoked
// 
int EnvironmentSensor_StartDiscovery(PNPMEMORY deviceArgs, PNPMEMORY adapterArgs) {
    int error = DIGITALTWIN_CLIENT_OK;

    // Add reference to deviceArgs and adapterArgs
    EnvironmentSensor_Context.DeviceArgs = deviceArgs;
    EnvironmentSensor_Context.AdapterArgs = adapterArgs;

    // Take reference on deviceArgs
    if (deviceArgs) {
        PnpMemory_AddReference(deviceArgs);
    }

    // Take reference on adapterArgs
    if (adapterArgs) {
        PnpMemory_AddReference(adapterArgs);
    }

    // Create a thread to discover devices
    if (ThreadAPI_Create(&EnvironmentSensor_Context.WorkerHandle, EnvironmentSensor_DiscoveryWorker, &EnvironmentSensor_Context) != THREADAPI_OK) {
        LogError("ThreadAPI_Create failed");
        error = -1;
        goto exit;
    }

exit:
    if (error != DIGITALTWIN_CLIENT_OK) {
        // Release deviceArgs reference on failure 
        if (deviceArgs) {
            PnpMemory_ReleaseReference(deviceArgs);
        }

        // Release adapterArgs reference on failure 
        if (adapterArgs) {
            PnpMemory_ReleaseReference(adapterArgs);
        }

        EnvironmentSensor_Context.DeviceArgs = NULL;
        EnvironmentSensor_Context.AdapterArgs = NULL;
    }

    return error;
}

// .StopDiscovery callback. Cleanup any discovery related resources
// created as part of .StartDiscovery method
int EnvironmentSensor_StopDiscovery() {
    return DIGITALTWIN_CLIENT_OK;
}

// PnpBridge discovery adapter callbacks. This needs to be added to the global 
// DISCOVERY_ADAPTER_MANIFEST array as shown in adapter_manifest.c
DISCOVERY_ADAPTER EnvironmentSensorDiscovery = {
    .Identity = "environment-sensor-sample-discovery-adapter",
    .StartDiscovery = EnvironmentSensor_StartDiscovery,
    .StopDiscovery = EnvironmentSensor_StopDiscovery
};


int main()
{
    PnpBridge_Main();
}