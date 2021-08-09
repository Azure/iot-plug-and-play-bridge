// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iothub.h"
#include "iothub_device_client.h"
#include "iothub_module_client.h"
#include "iothub_client_options.h"
#include "iothubtransportmqtt.h"
#include "pnp_device_client.h"
#include "pnp_dps.h"


#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"

#ifdef SET_TRUSTED_CERT
// For devices that do not have (or want) an OS level trusted certificate store,
// but instead bring in default trusted certificates from the Azure IoT C SDK.
#include "azure_c_shared_utility/shared_util_options.h"
#include "certs.h"
#endif // SET_TRUSTED_CERT

//
// AllocateDeviceClientHandle does the actual createHandle call, depending on the security type
//
static IOTHUB_DEVICE_CLIENT_HANDLE AllocateDeviceClientHandle(const PNP_DEVICE_CONFIGURATION* pnpDeviceConfiguration)
{
    IOTHUB_DEVICE_CLIENT_HANDLE deviceHandle = NULL;

    if (pnpDeviceConfiguration->securityType == PNP_CONNECTION_SECURITY_TYPE_CONNECTION_STRING)
    {
        if ((deviceHandle = IoTHubDeviceClient_CreateFromConnectionString(pnpDeviceConfiguration->u.connectionString, MQTT_Protocol)) == NULL)
        {
            LogError("Failure creating IotHub client.  Hint: Check your connection string");
        }
    }
    else if ((deviceHandle = PnP_CreateDeviceClientHandle_ViaDps(pnpDeviceConfiguration)) == NULL)
    {
        LogError("Cannot retrieve IoT Hub connection information from DPS client");
    }

    return deviceHandle;
}

IOTHUB_DEVICE_CLIENT_HANDLE PnP_CreateDeviceClientHandle(const PNP_DEVICE_CONFIGURATION* pnpDeviceConfiguration)
{
    IOTHUB_DEVICE_CLIENT_HANDLE deviceHandle = NULL;
    IOTHUB_CLIENT_RESULT iothubResult;
    bool urlAutoEncodeDecode = true;
    int iothubInitResult;
    bool result;
    int keepalivePeriodSeconds = 30;

    // Before invoking ANY IoT Hub or DPS functionality, IoTHub_Init must be invoked.
    if ((iothubInitResult = IoTHub_Init()) != 0)
    {
        LogError("Failure to initialize client, error=%d", iothubInitResult);
        result = false;
    }
    else if ((deviceHandle = AllocateDeviceClientHandle(pnpDeviceConfiguration)) == NULL)
    {
        LogError("Unable to allocate deviceHandle");
        result = false;
    }
    // Sets verbosity level
    else if ((iothubResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_LOG_TRACE, &pnpDeviceConfiguration->enableTracing)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set logging option, error=%d", iothubResult);
        result = false;
    }
    // Sets the name of ModelId for this PnP device.
    // This *MUST* be set before the client is connected to IoTHub.  We do not automatically connect when the 
    // handle is created, but will implicitly connect to subscribe for device method and device twin callbacks below.
    else if ((iothubResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_MODEL_ID, pnpDeviceConfiguration->modelId)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set the ModelID, error=%d", iothubResult);
        result = false;
    }
    // Optionally, set the callback function that processes incoming device methods, which is the channel PnP Commands are transferred over
    else if ((pnpDeviceConfiguration->deviceMethodCallback != NULL) && (iothubResult = IoTHubDeviceClient_SetDeviceMethodCallback(deviceHandle, pnpDeviceConfiguration->deviceMethodCallback, NULL)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set device method callback, error=%d", iothubResult);
        result = false;
    }
    // Optionall, set the callback function that processes device twin changes from the IoTHub, which is the channel that PnP Properties are 
    // transferred over. This will also automatically retrieve the full twin for the application on startup.
    else if ((pnpDeviceConfiguration->deviceTwinCallback != NULL) && (iothubResult = IoTHubDeviceClient_SetDeviceTwinCallback(deviceHandle, pnpDeviceConfiguration->deviceTwinCallback, (void*)deviceHandle)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set device twin callback, error=%d", iothubResult);
        result = false;
    }
    // Enabling auto url encode will have the underlying SDK perform URL encoding operations automatically.
    else if ((iothubResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_AUTO_URL_ENCODE_DECODE, &urlAutoEncodeDecode)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set auto Url encode option, error=%d", iothubResult);
        result = false;
    }

    // Set MQTT keepalive interval (seconds)
    else if ((iothubResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_KEEP_ALIVE, &keepalivePeriodSeconds)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set keep alive option, error=%d", iothubResult);
        result = false;
    }

#ifdef SET_TRUSTED_CERT
    // Setting the Trusted Certificate.  This is only necessary on systems without built in certificate stores.
    else if ((iothubResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_TRUSTED_CERT, certificates)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set the trusted cert, error=%d", iothubResult);
        result = false;
    }
#endif // SET_TRUSTED_CERT
    else if ((iothubResult = IoTHubDeviceClient_SetOption(deviceHandle, OPTION_PRODUCT_INFO, pnpDeviceConfiguration->UserAgentString)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set product info string option, error=%d", iothubResult);
        result = false;
    }
    else
    {
        result = true;
    }

    if ((result == false) && (deviceHandle != NULL))
    {
        IoTHubDeviceClient_Destroy(deviceHandle);
        deviceHandle = NULL;
    }

    if ((result == false) &&  (iothubInitResult == 0))
    {
        IoTHub_Deinit();
    }

    return deviceHandle;
}


//
// AllocateModuleClientHandle does the actual createHandle call, depending on the security type
//
static IOTHUB_MODULE_CLIENT_HANDLE AllocateModuleClientHandle(const PNP_DEVICE_CONFIGURATION* pnpModuleConfiguration)
{
    IOTHUB_MODULE_CLIENT_HANDLE moduleClientHandle = NULL;

    if (pnpModuleConfiguration->securityType == PNP_CONNECTION_SECURITY_TYPE_CONNECTION_STRING)
    {
        if ((moduleClientHandle = IoTHubModuleClient_CreateFromEnvironment(MQTT_Protocol)) == NULL)
        {
            LogError("Failure creating IotHub module client from environment info.");
        }
    }

    return moduleClientHandle;
}

IOTHUB_MODULE_CLIENT_HANDLE PnP_CreateModuleClientHandle(const PNP_DEVICE_CONFIGURATION* pnpModuleConfiguration)
{
    IOTHUB_MODULE_CLIENT_HANDLE moduleClientHandle = NULL;
    IOTHUB_CLIENT_RESULT iothubResult;
    bool urlAutoEncodeDecode = true;
    int iothubInitResult;
    bool result;

    // Before invoking ANY IoT Hub or DPS functionality, IoTHub_Init must be invoked.
    if ((iothubInitResult = IoTHub_Init()) != 0)
    {
        LogError("Failure to initialize module client, error=%d", iothubInitResult);
        result = false;
    }
    else if ((moduleClientHandle = AllocateModuleClientHandle(pnpModuleConfiguration)) == NULL)
    {
        LogError("Unable to allocate moduleClientHandle");
        result = false;
    }
    // Sets verbosity level
    else if ((iothubResult = IoTHubModuleClient_SetOption(moduleClientHandle, OPTION_LOG_TRACE, &pnpModuleConfiguration->enableTracing)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set logging option for module client, error=%d", iothubResult);
        result = false;
    }
    // Sets the name of ModelId for this PnP device.
    // This *MUST* be set before the client is connected to IoTHub.  We do not automatically connect when the 
    // handle is created, but will implicitly connect to subscribe for device method and device twin callbacks below.
    else if ((iothubResult = IoTHubModuleClient_SetOption(moduleClientHandle, OPTION_MODEL_ID, pnpModuleConfiguration->modelId)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set the ModelID for module client, error=%d", iothubResult);
        result = false;
    }
    // Optionally, set the callback function that processes incoming device methods, which is the channel PnP Commands are transferred over
    else if ((pnpModuleConfiguration->deviceMethodCallback != NULL) && (iothubResult = IoTHubModuleClient_SetModuleMethodCallback(moduleClientHandle, pnpModuleConfiguration->deviceMethodCallback, NULL)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set device method callback for module client, error=%d", iothubResult);
        result = false;
    }
    // Optionall, set the callback function that processes device twin changes from the IoTHub, which is the channel that PnP Properties are 
    // transferred over.  This will also automatically retrieve the full twin for the application on startup. 
    else if ((pnpModuleConfiguration->deviceTwinCallback != NULL) && (iothubResult = IoTHubModuleClient_SetModuleTwinCallback(moduleClientHandle, pnpModuleConfiguration->deviceTwinCallback, (void*)moduleClientHandle)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set device twin callback for module client, error=%d", iothubResult);
        result = false;
    }
    // Enabling auto url encode will have the underlying SDK perform URL encoding operations automatically.
    else if ((iothubResult = IoTHubModuleClient_SetOption(moduleClientHandle, OPTION_AUTO_URL_ENCODE_DECODE, &urlAutoEncodeDecode)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set auto Url encode option for module client, error=%d", iothubResult);
        result = false;
    }
#ifdef SET_TRUSTED_CERT
    // Setting the Trusted Certificate.  This is only necessary on systems without built in certificate stores.
    else if ((iothubResult = IoTHubModuleClient_SetOption(moduleClientHandle, OPTION_TRUSTED_CERT, certificates)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set the trusted cert for module client, error=%d", iothubResult);
        result = false;
    }
#endif // SET_TRUSTED_CERT
    else if ((iothubResult = IoTHubModuleClient_SetOption(moduleClientHandle, OPTION_PRODUCT_INFO, pnpModuleConfiguration->UserAgentString)) != IOTHUB_CLIENT_OK)
    {
        LogError("Unable to set product info string option for module client, error=%d", iothubResult);
        result = false;
    }
    else
    {
        result = true;
    }

    if ((result == false) && (moduleClientHandle != NULL))
    {
        IoTHubModuleClient_Destroy(moduleClientHandle);
        moduleClientHandle = NULL;
    }

    if ((result == false) &&  (iothubInitResult == 0))
    {
        IoTHub_Deinit();
    }

    return moduleClientHandle;

}