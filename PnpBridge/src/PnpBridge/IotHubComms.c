// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// DeviceAggregator.cpp : Defines the entry point for the console application.

#include "PnpBridgeCommon.h"
#include "ConfigurationParser.h"
#include "DiscoveryManager.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

#include "PnpBridgeh.h"

// Enable DPS connection to IoT Central
#define ENABLE_IOT_CENTRAL
#define USE_DPS_AUTH_SYMM_KEY

#ifdef ENABLE_IOT_CENTRAL
// IoT Central requires DPS.  Include required header and constants
#include "azure_prov_client/iothub_security_factory.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/prov_security_factory.h"

// State of DPS registration process.  We cannot proceed with DPS until we get into the state APP_DPS_REGISTRATION_SUCCEEDED.
typedef enum APP_DPS_REGISTRATION_STATUS_TAG
{
    APP_DPS_REGISTRATION_PENDING,
    APP_DPS_REGISTRATION_SUCCEEDED,
    APP_DPS_REGISTRATION_FAILED
} APP_DPS_REGISTRATION_STATUS;

#ifdef USE_DPS_AUTH_SYMM_KEY
const SECURE_DEVICE_TYPE secureDeviceTypeForProvisioning = SECURE_DEVICE_TYPE_SYMMETRIC_KEY;
const IOTHUB_SECURITY_TYPE secureDeviceTypeForIotHub = IOTHUB_SECURITY_TYPE_SYMMETRIC_KEY;

#else // USE_DPS_AUTH_SYMM_KEY
const SECURE_DEVICE_TYPE secureDeviceTypeForProvisioning = SECURE_DEVICE_TYPE_X509;
const IOTHUB_SECURITY_TYPE secureDeviceTypeForIotHub = IOTHUB_SECURITY_TYPE_X509;


#endif // USE_DPS_AUTH_SYMM_KEY

// Amount to sleep between querying state from DPS registration loop
static const int pnpSampleDevice_dpsRegistrationPollSleep = 1000;

// Maximum amount of times we'll poll for DPS registration being ready.
static const int pnpSampleDevice_dpsRegistrationMaxPolls = 60;

static const char* pnpSample_CustomProvisioningData = "{ \
                                                          \"__iot:interfaces\": \
                                                          { \
                                                              \"CapabilityModelUri\": \"%s\", \
                                                              \"ModelRepositoryUri\": \"%s\" \
                                                          } \
                                                       }";
#endif // ENABLE_IOT_CENTRAL

#ifdef ENABLE_IOT_CENTRAL
char* pnpSample_provisioning_IoTHubUri;
char* pnpSample_provisioning_DeviceId;

static void provisioningRegisterCallback(PROV_DEVICE_RESULT register_result, const char* iothub_uri, const char* device_id, void* user_context)
{
    APP_DPS_REGISTRATION_STATUS* appDpsRegistrationStatus = (APP_DPS_REGISTRATION_STATUS*)user_context;

    if (register_result != PROV_DEVICE_RESULT_OK)
    {
        LogError("DPS Provisioning callback called with error state %d", register_result);
        *appDpsRegistrationStatus = APP_DPS_REGISTRATION_FAILED;
    }
    else
    {
        if ((mallocAndStrcpy_s(&pnpSample_provisioning_IoTHubUri, iothub_uri) != 0) ||
            (mallocAndStrcpy_s(&pnpSample_provisioning_DeviceId, device_id) != 0))
        {
            LogError("Unable to copy provisioning information");
            *appDpsRegistrationStatus = APP_DPS_REGISTRATION_FAILED;
        }
        else
        {
            LogInfo("Provisioning callback indicates success.  iothubUri=%s, deviceId=%s", pnpSample_provisioning_IoTHubUri, pnpSample_provisioning_DeviceId);
            *appDpsRegistrationStatus = APP_DPS_REGISTRATION_SUCCEEDED;
        }
    }
}

IOTHUB_DEVICE_HANDLE PnPSampleDevice_InitializeIotHubViaProvisioning(bool traceOn, const char* globalProvUri, const char* idScope, const char* deviceId, const char* deviceKey, const char* dcmUri, const char* modelUri)
{
    PROV_DEVICE_RESULT provDeviceResult;
    PROV_DEVICE_LL_HANDLE provDeviceLLHandle = NULL;
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;
    char customProvisioningData[PNP_BRIDGE_STRING_MAX_SIZE] = { 0 };

    APP_DPS_REGISTRATION_STATUS appDpsRegistrationStatus = APP_DPS_REGISTRATION_PENDING;

    if (-1 == sprintf_s(customProvisioningData, PNP_BRIDGE_STRING_MAX_SIZE, pnpSample_CustomProvisioningData, dcmUri, modelUri)) {
        LogError("Failed to create the customProvisiongData. URI's are too long.");
    }
    else if (IoTHub_Init() != 0)
    {
        LogError("IoTHub_Init failed");
    }
#ifdef USE_DPS_AUTH_SYMM_KEY
    else if (prov_dev_set_symmetric_key_info(deviceId, deviceKey) != 0)
    {
        LogError("prov_dev_set_symmetric_key_info failed.");
    }
#endif
    else if (prov_dev_security_init(secureDeviceTypeForProvisioning) != 0)
    {
        LogError("prov_dev_security_init failed");
    }
    else if ((provDeviceLLHandle = Prov_Device_LL_Create(globalProvUri, idScope, Prov_Device_MQTT_Protocol)) == NULL)
    {
        LogError("failed calling Prov_Device_Create");
    }
    else if ((provDeviceResult = Prov_Device_LL_SetOption(provDeviceLLHandle, PROV_OPTION_LOG_TRACE, &traceOn)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Setting provisioning tracing on failed, error=%d", provDeviceResult);
    }
    else if ((provDeviceResult = Prov_Device_LL_SetProvisioningData(provDeviceLLHandle, customProvisioningData)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Failed setting provisioning data, error=%d", provDeviceResult);
    }
    else if ((provDeviceResult = Prov_Device_LL_Register_Device(provDeviceLLHandle, provisioningRegisterCallback, &appDpsRegistrationStatus, NULL, NULL)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Prov_Device_Register_Device failed, error=%d", provDeviceResult);
    }
    else
    {
        for (int i = 0; (i < pnpSampleDevice_dpsRegistrationMaxPolls) && (appDpsRegistrationStatus == APP_DPS_REGISTRATION_PENDING); i++)
        {
            ThreadAPI_Sleep(pnpSampleDevice_dpsRegistrationPollSleep);
            Prov_Device_LL_DoWork(provDeviceLLHandle);
        }

        if (appDpsRegistrationStatus == APP_DPS_REGISTRATION_SUCCEEDED)
        {
            LogInfo("DPS successfully registered.  Continuing on to creation of IoTHub device client handle.");
        }
        else if (appDpsRegistrationStatus == APP_DPS_REGISTRATION_PENDING)
        {
            LogError("Timed out attempting to register DPS device");
        }
        else
        {
            LogError("Error registering device for DPS");
        }
    }

    if (appDpsRegistrationStatus == APP_DPS_REGISTRATION_SUCCEEDED)
    {
        // The initial provisioning stage has succeeded.  Now use this material to connect to IoTHub.
        IOTHUB_CLIENT_RESULT iothubClientResult;
        bool urlEncodeOn = true;

        if (iothub_security_init(secureDeviceTypeForIotHub) != 0)
        {
            LogError("iothub_security_init failed");
        }
        else if ((deviceHandle = IoTHubDeviceClient_CreateFromDeviceAuth(pnpSample_provisioning_IoTHubUri, pnpSample_provisioning_DeviceId, MQTT_Protocol)) == NULL)
        {
            LogError("IoTHubDeviceClient_CreateFromDeviceAuth failed");
        }
        else if ((iothubClientResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_LOG_TRACE, &traceOn)) != IOTHUB_CLIENT_OK)
        {
            LogError("Failed to set option %s, error=%d", OPTION_LOG_TRACE, iothubClientResult);
            IoTHubDeviceClient_Destroy(deviceHandle);
            deviceHandle = NULL;
        }
        // Setting OPTION_AUTO_URL_ENCODE_DECODE is required by callers only for private-preview.
        // https://github.com/Azure/azure-iot-sdk-c-pnp/issues/2 tracks making underlying PnP set this automatically.
        else if ((iothubClientResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_AUTO_URL_ENCODE_DECODE, &urlEncodeOn)) != IOTHUB_CLIENT_OK)
        {
            LogError("Failed to set option %s, error=%d", OPTION_AUTO_URL_ENCODE_DECODE, iothubClientResult);
            IoTHubDeviceClient_Destroy(deviceHandle);
            deviceHandle = NULL;
        }
        else
        {
            // Never log the complete connection string , as this contains information
            // that could compromise security of the device.
            LogInfo("***** Successfully created device device=<%s> via provisioning *****", pnpSample_provisioning_DeviceId);
        }
    }

    if (provDeviceLLHandle != NULL)
    {
        Prov_Device_LL_Destroy(provDeviceLLHandle);
    }

    if (deviceHandle == NULL)
    {
        IoTHub_Deinit();
    }

    return deviceHandle;
}
#endif // ENABLE_IOT_CENTRAL

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
IOTHUB_DEVICE_HANDLE InitializeIotHubDeviceHandle(bool traceOn, const char* connectionString)
{
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;
    IOTHUB_CLIENT_RESULT iothubClientResult;
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