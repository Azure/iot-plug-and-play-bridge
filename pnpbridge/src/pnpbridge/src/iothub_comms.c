// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// DeviceAggregator.cpp : Defines the entry point for the console application.

#include "pnpbridge_common.h"

// Enable DPS connection to IoT Central
#define ENABLE_IOT_CENTRAL

#include "iothub_comms.h"

#ifdef ENABLE_IOT_CENTRAL
// IoT Central requires DPS.  Include required header and constants
#include "azure_prov_client/iothub_security_factory.h"
#include "azure_prov_client/prov_device_client.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/prov_security_factory.h"

// State of DPS registration process.  We cannot proceed with DPS until we get into the state APP_DPS_REGISTRATION_SUCCEEDED.
typedef enum APP_DPS_REGISTRATION_STATUS_TAG
{
    APP_DPS_REGISTRATION_PENDING,
    APP_DPS_REGISTRATION_SUCCEEDED,
    APP_DPS_REGISTRATION_FAILED
} APP_DPS_REGISTRATION_STATUS;

// Amount to sleep between querying state from DPS registration loop
static const int pnpSampleDevice_dpsRegistrationPollSleep = 1000;

// Maximum amount of times we'll poll for DPS registration being ready.
static const int pnpSampleDevice_dpsRegistrationMaxPolls = 60;

static const char* pnpSample_CustomProvisioningData = "{ \
                                                          \"__iot:interfaces\": \
                                                          { \
                                                              \"CapabilityModelId\": \"%s\" \
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

IOTHUB_DEVICE_HANDLE IotComms_InitializeIotHubViaProvisioning(bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams)
{
    PROV_DEVICE_RESULT provDeviceResult;
    PROV_DEVICE_HANDLE provDeviceHandle = NULL;
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;
    char* customProvisioningData = NULL;
    APP_DPS_REGISTRATION_STATUS appDpsRegistrationStatus = APP_DPS_REGISTRATION_PENDING;

    const char* globalProvUri = ConnectionParams->u1.Dps.GlobalProvUri;
    const char* idScope = ConnectionParams->u1.Dps.IdScope;
    const char* deviceId = ConnectionParams->u1.Dps.DeviceId;
    const char* deviceKey = ConnectionParams->AuthParameters.u1.DeviceKey;
    const char* dcmModelId = ConnectionParams->u1.Dps.DcmModelId;

    SECURE_DEVICE_TYPE secureDeviceTypeForProvisioning;
    IOTHUB_SECURITY_TYPE secureDeviceTypeForIotHub;

    {

        if (AUTH_TYPE_SYMMETRIC_KEY == ConnectionParams->AuthParameters.AuthType) {
            secureDeviceTypeForProvisioning = SECURE_DEVICE_TYPE_SYMMETRIC_KEY;
            secureDeviceTypeForIotHub = IOTHUB_SECURITY_TYPE_SYMMETRIC_KEY;
        }
        else if (AUTH_TYPE_X509 == ConnectionParams->AuthParameters.AuthType) {
            secureDeviceTypeForProvisioning = SECURE_DEVICE_TYPE_X509;
            secureDeviceTypeForIotHub = IOTHUB_SECURITY_TYPE_X509;
        }
        else {
            LogError("IotComms_InitializeIotHubViaProvisioning: Unknown AuthType");
            goto exit;
        }

        size_t customProvDataLength = strlen(pnpSample_CustomProvisioningData) + strlen(dcmModelId) + 1;
        customProvisioningData = calloc(1, customProvDataLength * sizeof(char));
        if (NULL == customProvisioningData) {
            LogError("Failed to allocate memory fo the customProvisiongData.");
            goto exit;
        }

        if (-1 == sprintf_s(customProvisioningData, customProvDataLength, pnpSample_CustomProvisioningData, dcmModelId)) {
            LogError("Failed to create the customProvisiongData. DcmModelId is too long.");
        }
        else if ((AUTH_TYPE_SYMMETRIC_KEY == ConnectionParams->AuthParameters.AuthType)
                && (prov_dev_set_symmetric_key_info(deviceId, deviceKey) != 0)) {
            LogError("prov_dev_set_symmetric_key_info failed.");
        }
        else if (prov_dev_security_init(secureDeviceTypeForProvisioning) != 0)
        {
            LogError("prov_dev_security_init failed");
        }
        else if ((provDeviceHandle = Prov_Device_Create(globalProvUri, idScope, Prov_Device_MQTT_Protocol)) == NULL)
        {
            LogError("failed calling Prov_Device_Create");
        }
        else if ((provDeviceResult = Prov_Device_SetOption(provDeviceHandle, PROV_OPTION_LOG_TRACE, &TraceOn)) != PROV_DEVICE_RESULT_OK)
        {
            LogError("Setting provisioning tracing on failed, error=%d", provDeviceResult);
        }
        else if ((provDeviceResult = Prov_Device_Set_Provisioning_Payload(provDeviceHandle, customProvisioningData)) != PROV_DEVICE_RESULT_OK)
        {
            LogError("Failed setting provisioning data, error=%d", provDeviceResult);
        }
        else if ((provDeviceResult = Prov_Device_Register_Device(provDeviceHandle, provisioningRegisterCallback, &appDpsRegistrationStatus, NULL, NULL)) != PROV_DEVICE_RESULT_OK)
        {
            LogError("Prov_Device_Register_Device failed, error=%d", provDeviceResult);
        }
        else
        {
            for (int i = 0; (i < pnpSampleDevice_dpsRegistrationMaxPolls) && (appDpsRegistrationStatus == APP_DPS_REGISTRATION_PENDING); i++)
            {
                ThreadAPI_Sleep(pnpSampleDevice_dpsRegistrationPollSleep);
                //Prov_Device_LL_DoWork(provDeviceLLHandle);
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

            if (iothub_security_init(secureDeviceTypeForIotHub) != 0)
            {
                LogError("iothub_security_init failed");
            }
            else if ((deviceHandle = IoTHubDeviceClient_CreateFromDeviceAuth(pnpSample_provisioning_IoTHubUri, pnpSample_provisioning_DeviceId, MQTT_Protocol)) == NULL)
            {
                LogError("IoTHubDeviceClient_CreateFromDeviceAuth failed");
            }
            else if ((iothubClientResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_LOG_TRACE, &TraceOn)) != IOTHUB_CLIENT_OK)
            {
                LogError("Failed to set option %s, error=%d", OPTION_LOG_TRACE, iothubClientResult);
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

    } 
exit:
    {

        if (NULL != customProvisioningData) {
            free(customProvisioningData);
        }

        if (provDeviceHandle != NULL) {
            Prov_Device_Destroy(provDeviceHandle);
        }

        if (deviceHandle == NULL) {
            IoTHub_Deinit();
        }
    }

    return deviceHandle;
}
#endif // ENABLE_IOT_CENTRAL


// InitializeIotHubDeviceHandle initializes underlying IoTHub client, creates a device handle with the specified connection string,
// and sets some options on this handle prior to beginning.
IOTHUB_DEVICE_HANDLE IotComms_InitializeIotHubDeviceHandle(bool TraceOn, const char* ConnectionString, const char* ModelId)
{
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;

    // Get connection string from config
    if ((deviceHandle = PnPHelper_CreateDeviceClientHandle(ConnectionString, ModelId, TraceOn,
            (IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC) PnpAdapterManager_DeviceMethodCallback,
            (IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK) PnpAdapterManager_DeviceTwinCallback)) == NULL)
    {
        LogError("Failed to create IoT hub device handle\n");
        IoTHub_Deinit();
    }

    return deviceHandle;
}


IOTHUB_DEVICE_HANDLE IotComms_InitializeIotDevice(bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams)
{
    if (ConnectionParams->ConnectionType == CONNECTION_TYPE_CONNECTION_STRING) {
        if (ConnectionParams->AuthParameters.AuthType == AUTH_TYPE_SYMMETRIC_KEY) {
            IOTHUB_DEVICE_HANDLE handle = NULL;
            char* connectionString;
            char* format = NULL;

            if (NULL != strstr(ConnectionParams->u1.ConnectionString, "SharedAccessKey=")) {
                LogInfo("WARNING: SharedAccessKey is included in connection string. Ignoring "
                    PNP_CONFIG_CONNECTION_AUTH_TYPE_DEVICE_SYMM_KEY " in config file.");
                connectionString = (char*)ConnectionParams->u1.ConnectionString;
            }
            else {
                format = "%s;SharedAccessKey=%s";
                connectionString = (char*)malloc(strlen(ConnectionParams->AuthParameters.u1.DeviceKey) +
                    strlen(ConnectionParams->u1.ConnectionString) + strlen(format) + 1);
                sprintf(connectionString, format, ConnectionParams->u1.ConnectionString, ConnectionParams->AuthParameters.u1.DeviceKey);
            }

            handle = IotComms_InitializeIotHubDeviceHandle(TraceOn, connectionString, ConnectionParams->RootInterfaceModelId);

            if (NULL != format) {
                free(connectionString);
            }

            return handle;
        }
        else {
            LogError("Auth type (%d) is not supported for symmetric key", ConnectionParams->AuthParameters.AuthType);
        }
    }
    else if (ConnectionParams->ConnectionType == CONNECTION_TYPE_DPS) {
        if ((AUTH_TYPE_SYMMETRIC_KEY == ConnectionParams->AuthParameters.AuthType) ||
            (AUTH_TYPE_X509 == ConnectionParams->AuthParameters.AuthType)) {
            return IotComms_InitializeIotHubViaProvisioning(TraceOn, ConnectionParams);
        }
        else {
            LogError("Auth type (%d) is not supported for DPS", ConnectionParams->AuthParameters.AuthType);
        }
    }
    else {
        LogError("Connection type (%d) is not supported", ConnectionParams->ConnectionType);
    }
    return NULL;
}

IOTHUB_CLIENT_RESULT
IotComms_InitializeIotDeviceHandle(
    MX_IOT_HANDLE_TAG* IotHandle,
    bool TraceOn,
    PCONNECTION_PARAMETERS ConnectionParams
)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    {
        // Mark this as device handle
        IotHandle->IsModule = false;

        // Connect to Iot Hub Device
        IotHandle->u1.IotDevice.deviceHandle = IotComms_InitializeIotDevice(TraceOn, ConnectionParams);
        if (NULL == IotHandle->u1.IotDevice.deviceHandle) {
            LogError("IotComms_InitializeIotDevice failed\n");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
    }

    // We have completed initializing the pnp client
    IotHandle->DeviceClientInitialized = true;
    }
exit:
    return result;
}


IOTHUB_CLIENT_RESULT IotComms_InitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams)
{
    if (ConnectionParams->ConnectionType == CONNECTION_TYPE_EDGE_MODULE) {
        return IOTHUB_CLIENT_ERROR;
    }
    else {
        return IotComms_InitializeIotDeviceHandle(IotHandle, TraceOn, ConnectionParams);
    }
}