// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Header file for manipulating component for device info.
#pragma once

#ifndef SAMPLE_ENVIRONMENTAL_SENSOR
#define SAMPLE_ENVIRONMENTAL_SENSOR

#include "environmental_sensor_pnpbridge.h"

// PnP utility headers
#include "pnp_device_client.h"
#include "pnp_dps.h"
#include "pnp_protocol.h"

//
// Application state associated with the particular component. In particular it contains 
// the resources used for responses in callbacks along with properties set
// and representations of the property update and command callbacks invoked on given component
//
typedef struct ENVIRONMENTAL_SENSOR_STATE_TAG
{
    char * componentName;
    int brightness;
    char* customerName;
    int numTimesBlinkCommandCalled;
    int blinkInterval;
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

    // Sends  telemetry messages about current environment
    IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_SendTelemetryMessagesAsync(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);
    IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_ReportDeviceStateAsync(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char * ComponentName);
    IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_RouteReportedState(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const unsigned char * ReportedState,
        size_t Size,
        IOTHUB_CLIENT_REPORTED_STATE_CALLBACK ReportedStateCallback,
        void * UserContextCallback);
    IOTHUB_CLIENT_RESULT SampleEnvironmentalSensor_RouteSendEventAsync(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        IOTHUB_MESSAGE_HANDLE EventMessageHandle,
        IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK EventConfirmationCallback,
        void * UserContextCallback
        );
    void SampleEnvironmentalSensor_ProcessPropertyUpdate(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version);
    int SampleEnvironmentalSensor_ProcessCommandUpdate(
        PENVIRONMENT_SENSOR EnvironmentalSensor,
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

#ifdef __cplusplus
}
#endif


#endif // SAMPLE_ENVIRONMENTAL_SENSOR