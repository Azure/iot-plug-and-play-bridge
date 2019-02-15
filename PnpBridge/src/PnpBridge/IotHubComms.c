// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// DeviceAggregator.cpp : Defines the entry point for the console application.

#include "PnpBridgeCommon.h"
#include "ConfigurationParser.h"
#include "DiscoveryManager.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

#include "PnpBridgeh.h"

// State of PnP registration process.  We cannot proceed with PnP until we get into the state APP_PNP_REGISTRATION_SUCCEEDED.
typedef enum APP_PNP_REGISTRATION_STATUS_TAG
{
    APP_PNP_REGISTRATION_PENDING,
    APP_PNP_REGISTRATION_SUCCEEDED,
    APP_PNP_REGISTRATION_FAILED
} APP_PNP_REGISTRATION_STATUS;

// appPnpInterfacesRegistered is invoked when the interfaces have been registered or failed.
void appPnpInterfacesRegistered(PNP_REPORTED_INTERFACES_STATUS pnpInterfaceStatus, void *userContextCallback)
{
    APP_PNP_REGISTRATION_STATUS* appPnpRegistrationStatus = (APP_PNP_REGISTRATION_STATUS*)userContextCallback;
    *appPnpRegistrationStatus = (pnpInterfaceStatus == PNP_REPORTED_INTERFACES_OK) ? APP_PNP_REGISTRATION_SUCCEEDED : APP_PNP_REGISTRATION_FAILED;
}

// Invokes PnP_DeviceClient_RegisterInterfacesAsync, which indicates to Azure IoT which PnP interfaces this device supports.
// The PnP Handle *is not valid* until this operation has completed (as indicated by the callback appPnpInterfacesRegistered being invoked).
// In this sample, we block indefinitely but production code should include a timeout.
int AppRegisterPnPInterfacesAndWait(PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, PNP_INTERFACE_CLIENT_HANDLE* interfaces, int count)
{
    APP_PNP_REGISTRATION_STATUS appPnpRegistrationStatus = APP_PNP_REGISTRATION_PENDING;
    PNPBRIDGE_RESULT result;
    PNP_CLIENT_RESULT pnpResult;

    pnpResult = PnP_DeviceClient_RegisterInterfacesAsync(pnpDeviceClientHandle, interfaces, count, appPnpInterfacesRegistered, &appPnpRegistrationStatus);
    if (PNP_CLIENT_OK != pnpResult) {
        result = PNPBRIDGE_FAILED;
        goto end;
    }

    while (appPnpRegistrationStatus == APP_PNP_REGISTRATION_PENDING) {
        ThreadAPI_Sleep(100);
    }

    if (appPnpRegistrationStatus != APP_PNP_REGISTRATION_SUCCEEDED) {
        LogError("PnP has failed to register.\n");
        result = __FAILURE__;
    }
    else {
        result = 0;
    }

end:

    return result;
}

// InitializeIotHubDeviceHandle initializes underlying IoTHub client, creates a device handle with the specified connection string,
// and sets some options on this handle prior to beginning.
IOTHUB_DEVICE_HANDLE InitializeIotHubDeviceHandle(const char* connectionString)
{
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;
    IOTHUB_CLIENT_RESULT iothubClientResult;
    bool traceOn = false;
    bool urlEncodeOn = true;

    // TODO: PnP SDK should auto-set OPTION_AUTO_URL_ENCODE_DECODE for MQTT as its strictly required.  Need way for IoTHub handle to communicate this back.
    if (IoTHub_Init() != 0)
    {
        LogError("IoTHub_Init failed\n");
    }
    else
    {
        // Get connection string from config
        if ((deviceHandle = IoTHubDeviceClient_CreateFromConnectionString(connectionString, MQTT_Protocol)) == NULL)
        {
            LogError("Failed to create device handle\n");
        }
        else if ((iothubClientResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_LOG_TRACE, &traceOn)) != IOTHUB_CLIENT_OK)
        {
            LogError("Failed to set option %s, error=%d\n", OPTION_LOG_TRACE, iothubClientResult);
            IoTHubDeviceClient_Destroy(deviceHandle);
            deviceHandle = NULL;
        }
        else if ((iothubClientResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_AUTO_URL_ENCODE_DECODE, &urlEncodeOn)) != IOTHUB_CLIENT_OK)
        {
            LogError("Failed to set option %s, error=%d\n", OPTION_AUTO_URL_ENCODE_DECODE, iothubClientResult);
            IoTHubDeviceClient_Destroy(deviceHandle);
            deviceHandle = NULL;
        }

        if (deviceHandle == NULL)
        {
            IoTHub_Deinit();
        }
    }

    return deviceHandle;
}