// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Header file for sample for manipulating DigitalTwin Interface for device info.
#pragma once

#ifndef DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR
#define DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR

#include "environmental_sensor_pnpbridge.h"

// PnP helper utilities.
#include "pnp_device_client_helpers.h"
#include "pnp_protocol_helpers.h"

//
// Application state associated with the particular interface.  In particular it contains 
// the DIGITALTWIN_INTERFACE_CLIENT_HANDLE used for responses in callbacks along with properties set
// and representations of the property update and command callbacks invoked on given interface
//
typedef struct ENVIRONMENTAL_SENSOR_STATE_TAG
{
    char componentName[PNP_MAXIMUM_COMPONENT_LENGTH + 1];
    int brightness;
    char* customerName;
    int numTimesBlinkCommandCalled;
} ENVIRONMENTAL_SENSOR_STATE, * PENVIRONMENTAL_SENSOR_STATE;

typedef struct _ENVIRONMENT_SENSOR {
    THREAD_HANDLE WorkerHandle;
    volatile bool ShuttingDown;
    PENVIRONMENTAL_SENSOR_STATE SensorState;
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient;
} ENVIRONMENT_SENSOR, * PENVIRONMENT_SENSOR;

#ifdef __cplusplus
extern "C"
{
#endif

    // Sends DigitalTwin telemetry messages about current environment
    IOTHUB_CLIENT_RESULT DigitalTwinSampleEnvironmentalSensor_SendTelemetryMessagesAsync(
        PENVIRONMENT_SENSOR EnvironmentalSensor);
    IOTHUB_CLIENT_RESULT DigitalTwinSampleEnvironmentalSensor_ReportDeviceStateAsync(
        IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
        const char * ComponentName);
    void DigitalTwinSampleEnvironmentalSensor_ProcessPropertyUpdate(
        PENVIRONMENT_SENSOR EnvironmentalSensor,
        IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version);
    int DigitalTwinSample_ProcessCommandUpdate(
        PENVIRONMENT_SENSOR EnvironmentalSensor,
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

#ifdef __cplusplus
}
#endif


#endif // DIGITALTWIN_SAMPLE_ENVIRONMENTAL_SENSOR