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

void
IotComms_DigitalTwinClient_Destroy(
    MX_IOT_HANDLE_TAG* IotHandle
);

PNPBRIDGE_RESULT
IotComms_DigitalTwinClient_Initalize(
    MX_IOT_HANDLE_TAG* IotHandle
    );

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

    TRY {

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
            LEAVE;
        }

        size_t customProvDataLength = strlen(pnpSample_CustomProvisioningData) + strlen(dcmModelId) + 1;
        customProvisioningData = calloc(1, customProvDataLength * sizeof(char));
        if (NULL == customProvisioningData) {
            LogError("Failed to allocate memory fo the customProvisiongData.");
            LEAVE;
        }

        if (-1 == sprintf_s(customProvisioningData, customProvDataLength, pnpSample_CustomProvisioningData, dcmModelId)) {
            LogError("Failed to create the customProvisiongData. DcmModelId is too long.");
        }
        else if (IoTHub_Init() != 0) {
            LogError("IoTHub_Init failed");
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

    } FINALLY {

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

// State of PnP registration process.  We cannot proceed with PnP until we get into the state APP_PNP_REGISTRATION_SUCCEEDED.
typedef enum APP_PNP_REGISTRATION_STATUS_TAG
{
    APP_PNP_REGISTRATION_PENDING,
    APP_PNP_REGISTRATION_SUCCEEDED,
    APP_PNP_REGISTRATION_FAILED
} APP_PNP_REGISTRATION_STATUS;

typedef struct PNP_REGISTRATION_CONTEXT {
    COND_HANDLE Condition;
    LOCK_HANDLE Lock;
    APP_PNP_REGISTRATION_STATUS RegistrationStatus;
} PNP_REGISTRATION_CONTEXT, *PPNP_REGISTRATION_CONTEXT;

// appPnpInterfacesRegistered is invoked when the interfaces have been registered or failed.
void 
IotComms_PnPInterfaceRegisteredCallback(
    DIGITALTWIN_CLIENT_RESULT pnpInterfaceStatus,
    void *userContextCallback
    )
{
    PPNP_REGISTRATION_CONTEXT registrationContext = (PPNP_REGISTRATION_CONTEXT)userContextCallback;
    registrationContext->RegistrationStatus = (pnpInterfaceStatus == DIGITALTWIN_CLIENT_OK) ? APP_PNP_REGISTRATION_SUCCEEDED : APP_PNP_REGISTRATION_FAILED;
    Lock(registrationContext->Lock);
    Condition_Post(registrationContext->Condition);
    Unlock(registrationContext->Lock);
}

// Invokes PnP_DeviceClient_RegisterInterfacesAsync, which indicates to Azure IoT which PnP interfaces this device supports.
// The PnP Handle *is not valid* until this operation has completed (as indicated by the callback appPnpInterfacesRegistered being invoked).
// In this sample, we block indefinitely but production code should include a timeout.
int 
IotComms_RegisterPnPInterfaces(
    MX_IOT_HANDLE_TAG* IotHandle,
    const char* ModelRepoId,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* Interfaces,
    int InterfaceCount,
    bool traceOn,
    PCONNECTION_PARAMETERS connectionParams
    )
{
    PNPBRIDGE_RESULT result;
    DIGITALTWIN_CLIENT_RESULT pnpResult;
    PNP_REGISTRATION_CONTEXT callbackContext = { 0 };
    callbackContext.RegistrationStatus = APP_PNP_REGISTRATION_PENDING;
    callbackContext.Condition = Condition_Init();
    callbackContext.Lock = Lock_Init();

    // DigitalTwinClient doesn't support incremental publishing of PnP Interface
    // Inorder to workaround this we will destroy the DigitalTwinClient and recreate it
    IotComms_DigitalTwinClient_Destroy(IotHandle);
    IoTHub_Deinit();
    result = IotComms_InitializeIotHandle(IotHandle, traceOn, connectionParams);
    if (PNPBRIDGE_OK != result) {
        LogError("IotComms_InitializeIotHandle failed\n");
        goto end;
    }
    IotComms_DigitalTwinClient_Initalize(IotHandle);

    if (!IotHandle->IsModule) {
        pnpResult = DigitalTwin_DeviceClient_RegisterInterfacesAsync(
                        IotHandle->u1.IotDevice.PnpDeviceClientHandle, ModelRepoId, Interfaces,
                        InterfaceCount, IotComms_PnPInterfaceRegisteredCallback, &callbackContext);
    }
    else {
        LogError("Module support is not present in public preview");
        pnpResult = PNPBRIDGE_NOT_SUPPORTED;
    }

    if (DIGITALTWIN_CLIENT_OK != pnpResult) {
        result = PNPBRIDGE_FAILED;
        goto end;
    }

    Lock(callbackContext.Lock);
    Condition_Wait(callbackContext.Condition, callbackContext.Lock, 0);
    Unlock(callbackContext.Lock);

    if (callbackContext.RegistrationStatus != APP_PNP_REGISTRATION_SUCCEEDED) {
        LogError("PnP has failed to register.\n");
        result = -1;
    }
    else {
        result = 0;
    }

end:

    return result;
}

// InitializeIotHubDeviceHandle initializes underlying IoTHub client, creates a device handle with the specified connection string,
// and sets some options on this handle prior to beginning.
IOTHUB_DEVICE_HANDLE IotComms_InitializeIotHubDeviceHandle(bool TraceOn, const char* ConnectionString)
{
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;
    IOTHUB_CLIENT_RESULT iothubClientResult;

    // TODO: PnP SDK should auto-set OPTION_AUTO_URL_ENCODE_DECODE for MQTT as its strictly required.  Need way for IoTHub handle to communicate this back.
    if (IoTHub_Init() != 0) {
        LogError("IoTHub_Init failed\n");
    } else {
        // Get connection string from config
        if ((deviceHandle = IoTHubDeviceClient_CreateFromConnectionString(ConnectionString, MQTT_Protocol)) == NULL)
        {
            LogError("Failed to create device handle\n");
        }
        else if ((iothubClientResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_LOG_TRACE, &TraceOn)) != IOTHUB_CLIENT_OK)
        {
            LogError("Failed to set option %s, error=%d\n", OPTION_LOG_TRACE, iothubClientResult);
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
                connectionString = (char*) ConnectionParams->u1.ConnectionString;
            }
            else {
                format = "%s;SharedAccessKey=%s";
                connectionString = (char*) malloc(strlen(ConnectionParams->AuthParameters.u1.DeviceKey) +
                                                strlen(ConnectionParams->u1.ConnectionString) + strlen(format) + 1);
                sprintf(connectionString, format, ConnectionParams->u1.ConnectionString, ConnectionParams->AuthParameters.u1.DeviceKey);
            }

            handle = IotComms_InitializeIotHubDeviceHandle(TraceOn, connectionString);

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

PNPBRIDGE_RESULT 
IotComms_InitializeIotDeviceHandle(
    MX_IOT_HANDLE_TAG* IotHandle, 
    bool TraceOn, 
    PCONNECTION_PARAMETERS ConnectionParams
    ) 
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    TRY {
        // Mark this as device handle
        IotHandle->IsModule = false;
       
        // Connect to Iot Hub Device
        IotHandle->u1.IotDevice.deviceHandle = IotComms_InitializeIotDevice(TraceOn, ConnectionParams);
        if (NULL == IotHandle->u1.IotDevice.deviceHandle) {
            LogError("IotComms_InitializeIotDevice failed\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        // We have completed initializing the pnp client
        IotHandle->DeviceClientInitialized = true;
    } FINALLY {

    }

    return result;
}

PNPBRIDGE_RESULT
IotComms_DigitalTwinClient_Initalize(
    MX_IOT_HANDLE_TAG* IotHandle
    )
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    // Create PnpDeviceHandle
    if (!IotHandle->IsModule) {
        if (DIGITALTWIN_CLIENT_OK != DigitalTwin_DeviceClient_CreateFromDeviceHandle(
            IotHandle->u1.IotDevice.deviceHandle, &IotHandle->u1.IotDevice.PnpDeviceClientHandle))
        {
            LogError("DigitalTwin_DeviceClient_CreateFromDeviceHandle failed\n");
            result = PNPBRIDGE_FAILED;
        }
    }
    else {
        result = PNPBRIDGE_NOT_SUPPORTED;
    }

    IotHandle->DigitalTwinClientInitialized = (PNPBRIDGE_OK == result);

    return result;
}

void
IotComms_DigitalTwinClient_Destroy(
    MX_IOT_HANDLE_TAG* IotHandle
    )
{
    if (NULL != IotHandle->u1.IotDevice.PnpDeviceClientHandle) {
        if (!IotHandle->IsModule) {
            DigitalTwin_DeviceClient_Destroy(IotHandle->u1.IotDevice.PnpDeviceClientHandle);
        }
        else {
            LogError("Module support is not present in public preview");
        }
    }

    IotHandle->DigitalTwinClientInitialized = false;
}

PNPBRIDGE_RESULT IotComms_InitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams)
{
    if (ConnectionParams->ConnectionType == CONNECTION_TYPE_EDGE_MODULE) {
        return PNPBRIDGE_NOT_SUPPORTED;
    }
    else {
        return IotComms_InitializeIotDeviceHandle(IotHandle, TraceOn, ConnectionParams);
    }
}