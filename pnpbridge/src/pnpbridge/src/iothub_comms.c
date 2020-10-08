// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"

#include "iothub_comms.h"

// IoT Central requires DPS. Include required header and constants
#include "azure_prov_client/iothub_security_factory.h"
#include "azure_prov_client/prov_device_client.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/prov_security_factory.h"

IOTHUB_CLIENT_RESULT
IotComms_InitializeIotDeviceHandle(
    MX_IOT_HANDLE_TAG* IotHandle,
    PCONNECTION_PARAMETERS ConnectionParams
)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    {
        // Mark this as device handle
        IotHandle->IsModule = false;

        // Connect to Iot Hub Device
        IotHandle->u1.IotDevice.deviceHandle = PnP_CreateDeviceClientHandle(&ConnectionParams->PnpDeviceConfiguration);
        if (NULL == IotHandle->u1.IotDevice.deviceHandle) {
            LogError("PnP_CreateDeviceClientHandle failed\n");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

    // Completed initializing the pnp client
    IotHandle->ClientHandleInitialized = true;

    }
exit:
    return result;
}

IOTHUB_CLIENT_RESULT
IotComms_InitializeIotModuleHandle(
    MX_IOT_HANDLE_TAG* IotHandle,
    PCONNECTION_PARAMETERS ConnectionParams
)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    {
        // Mark this as device handle
        IotHandle->IsModule = true;

        // Connect to Iot Hub Device
        IotHandle->u1.IotModule.moduleHandle = PnP_CreateModuleClientHandle(&ConnectionParams->PnpDeviceConfiguration);
        if (NULL == IotHandle->u1.IotModule.moduleHandle) {
            LogError("PnP_CreateModuleClientHandle failed\n");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

    // Completed initializing the pnp client handle
    IotHandle->ClientHandleInitialized = true;

    }
exit:
    return result;
}


IOTHUB_CLIENT_RESULT IotComms_InitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, PCONNECTION_PARAMETERS ConnectionParams)
{
    if (ConnectionParams->ConnectionType == CONNECTION_TYPE_EDGE_MODULE) {
        return IotComms_InitializeIotModuleHandle(IotHandle, ConnectionParams);
    }
    else {
        return IotComms_InitializeIotDeviceHandle(IotHandle, ConnectionParams);
    }
}

IOTHUB_CLIENT_RESULT IotComms_DeinitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, PCONNECTION_PARAMETERS ConnectionParams)
{
    if (ConnectionParams->ConnectionType == CONNECTION_TYPE_EDGE_MODULE)
    {
        if (IotHandle->u1.IotModule.moduleHandle != NULL)
        {
            IoTHubModuleClient_Destroy(IotHandle->u1.IotModule.moduleHandle);
            IoTHub_Deinit();
            IotHandle->u1.IotModule.moduleHandle = NULL;
            IotHandle->ClientHandleInitialized = false;
        }
    }
    else
    {
        if (IotHandle->u1.IotDevice.deviceHandle != NULL)
        {
            IoTHubDeviceClient_Destroy(IotHandle->u1.IotDevice.deviceHandle);
            IoTHub_Deinit();
            IotHandle->u1.IotDevice.deviceHandle = NULL;
            IotHandle->ClientHandleInitialized = false;
            if (ConnectionParams->PnpDeviceConfiguration.u.connectionString != NULL)
            {
                free(ConnectionParams->PnpDeviceConfiguration.u.connectionString);
            }
        }
    }
    return IOTHUB_CLIENT_OK;
}