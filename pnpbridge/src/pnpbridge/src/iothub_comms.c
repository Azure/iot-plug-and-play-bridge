// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// DeviceAggregator.cpp : Defines the entry point for the console application.

#include "pnpbridge_common.h"

#include "iothub_comms.h"

// IoT Central requires DPS. Include required header and constants
#include "azure_prov_client/iothub_security_factory.h"
#include "azure_prov_client/prov_device_client.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/prov_security_factory.h"

static const char* ConnectionParameterUserAgentString = "PnpBridgeUserAgentString";

IOTHUB_DEVICE_HANDLE IotComms_CreateIotDeviceHandle(bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams)
{
    IOTHUB_DEVICE_HANDLE iotHubDeviceHandle = NULL;
    ConnectionParams->PnpDeviceConfiguration.deviceMethodCallback = (IOTHUB_CLIENT_DEVICE_METHOD_CALLBACK_ASYNC) PnpAdapterManager_DeviceMethodCallback;
    ConnectionParams->PnpDeviceConfiguration.deviceTwinCallback = (IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK) PnpAdapterManager_DeviceTwinCallback;
    ConnectionParams->PnpDeviceConfiguration.enableTracing = TraceOn;
    ConnectionParams->PnpDeviceConfiguration.modelId = ConnectionParams->RootInterfaceModelId;
    ConnectionParams->PnpDeviceConfiguration.UserAgentString = ConnectionParameterUserAgentString;

    if (ConnectionParams != NULL)
    {
        if (ConnectionParams->ConnectionType == CONNECTION_TYPE_CONNECTION_STRING)
        {
            ConnectionParams->PnpDeviceConfiguration.securityType = PNP_CONNECTION_SECURITY_TYPE_CONNECTION_STRING;
            if (AUTH_TYPE_SYMMETRIC_KEY == ConnectionParams->AuthParameters.AuthType)
            {
                char* format = NULL;

                if (NULL != strstr(ConnectionParams->u1.ConnectionString, "SharedAccessKey="))
                {
                    LogInfo("WARNING: SharedAccessKey is included in connection string. Ignoring "
                        PNP_CONFIG_CONNECTION_AUTH_TYPE_DEVICE_SYMM_KEY " in config file.");
                    ConnectionParams->PnpDeviceConfiguration.u.connectionString = (char*)ConnectionParams->u1.ConnectionString;
                }
                else
                {
                    format = "%s;SharedAccessKey=%s";
                    ConnectionParams->PnpDeviceConfiguration.u.connectionString = (char*)malloc(strlen(ConnectionParams->AuthParameters.u1.DeviceKey) +
                        strlen(ConnectionParams->u1.ConnectionString) + strlen(format) + 1);
                    sprintf(ConnectionParams->PnpDeviceConfiguration.u.connectionString, format, ConnectionParams->u1.ConnectionString,
                        ConnectionParams->AuthParameters.u1.DeviceKey);
                }
            }
            else
            {
                LogError("Auth type (%d) is not supported for symmetric key", ConnectionParams->AuthParameters.AuthType);
                goto exit;
            }
        }
        else if (ConnectionParams->ConnectionType == CONNECTION_TYPE_DPS)
        {
            ConnectionParams->PnpDeviceConfiguration.securityType = PNP_CONNECTION_SECURITY_TYPE_DPS;
            if (AUTH_TYPE_SYMMETRIC_KEY == ConnectionParams->AuthParameters.AuthType)
            {
                ConnectionParams->PnpDeviceConfiguration.u.dpsConnectionAuth.endpoint =  ConnectionParams->u1.Dps.GlobalProvUri;
                ConnectionParams->PnpDeviceConfiguration.u.dpsConnectionAuth.idScope = ConnectionParams->u1.Dps.IdScope;
                ConnectionParams->PnpDeviceConfiguration.u.dpsConnectionAuth.deviceId = ConnectionParams->u1.Dps.DeviceId;
                ConnectionParams->PnpDeviceConfiguration.u.dpsConnectionAuth.deviceKey = ConnectionParams->AuthParameters.u1.DeviceKey;
            }
            else
            {
                LogError("Auth type (%d) is not supported for DPS", ConnectionParams->AuthParameters.AuthType);
                goto exit;
            }
        }
        else
        {
            LogError("Connection type (%d) is not supported", ConnectionParams->ConnectionType);
            goto exit;
        }
        // Use Pnp Device utilities to create device client handle
        iotHubDeviceHandle = PnP_CreateDeviceClientHandle(&ConnectionParams->PnpDeviceConfiguration);
    }
exit:
    return iotHubDeviceHandle;
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
        IotHandle->u1.IotDevice.deviceHandle = IotComms_CreateIotDeviceHandle(TraceOn, ConnectionParams);
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

IOTHUB_CLIENT_RESULT IotComms_DeinitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, PCONNECTION_PARAMETERS ConnectionParams)
{
    if (ConnectionParams->ConnectionType == CONNECTION_TYPE_EDGE_MODULE) {
        return IOTHUB_CLIENT_ERROR;
    }
    else
    {
        if (IotHandle->u1.IotDevice.deviceHandle != NULL)
        {
            IoTHubDeviceClient_Destroy(IotHandle->u1.IotDevice.deviceHandle);
            IoTHub_Deinit();
            IotHandle->u1.IotDevice.deviceHandle = NULL;
            IotHandle->DeviceClientInitialized = false;
            if (ConnectionParams->PnpDeviceConfiguration.u.connectionString != NULL)
            {
                free(ConnectionParams->PnpDeviceConfiguration.u.connectionString);
            }
        }
    }
    return IOTHUB_CLIENT_OK;
}