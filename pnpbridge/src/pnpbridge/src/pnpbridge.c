// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "configuration_parser.h"
#include "pnpadapter_api.h"
#include "pnpadapter_manager.h"
#include "iothub_comms.h"

#include "pnpbridgeh.h"
#include <iothub_client.h>

// Globals PNP bridge instance
PPNP_BRIDGE g_PnpBridge = NULL;
PNP_BRIDGE_STATE g_PnpBridgeState = PNP_BRIDGE_UNINITIALIZED;
bool g_PnpBridgeShutdown = false;

DIGITALTWIN_CLIENT_RESULT 
PnpBridge_Initialize(
    PPNP_BRIDGE* PnpBridge,
    const char * ConfigFilePath
    ) 
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    PPNP_BRIDGE pbridge = NULL;
    JSON_Value* config = NULL;
    bool lockAcquired = false;

    {
        g_PnpBridgeState = PNP_BRIDGE_UNINITIALIZED;

        // Allocate memory for the PNP_BRIDGE structure
        pbridge = (PPNP_BRIDGE) calloc(1, sizeof(PNP_BRIDGE));
        if (NULL == pbridge) {
            LogError("Failed to allocate memory for PnpBridge global");
            result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
            goto exit;
        }

        pbridge->ExitCondition = Condition_Init();
        if (NULL == pbridge->ExitCondition) {
            LogError("Failed to init ExitCondition");
            result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
            goto exit;
        }

        pbridge->ExitLock = Lock_Init();
        if (NULL == pbridge->ExitLock) {
            LogError("Failed to init ExitLock lock");
            result = DIGITALTWIN_CLIENT_ERROR_OUT_OF_MEMORY;
            goto exit;
        }
        Lock(pbridge->ExitLock);
        lockAcquired = true;

        // Get the JSON VALUE of configuration file
        result = PnpBridgeConfig_GetJsonValueFromConfigFile(ConfigFilePath, &config);
        if (DIGITALTWIN_CLIENT_OK != result) {
            LogError("Failed to retrieve the bridge configuration from specified file location.");
            goto exit;
        }

        // Check if config file has REQUIRED parameters
        result = PnpBridgeConfig_RetrieveConfiguration(config, &pbridge->Configuration);
        if (DIGITALTWIN_CLIENT_OK != result) {
            LogError("Config file is invalid");
            goto exit;
        }

        // Connect to Iot Hub and create a PnP device client handle
        {
            result = IotComms_InitializeIotHandle(&pbridge->IotHandle, pbridge->Configuration.TraceOn, pbridge->Configuration.ConnParams);
            if (DIGITALTWIN_CLIENT_OK != result) {
                LogError("IotComms_InitializeIotHandle failed\n");
                result = DIGITALTWIN_CLIENT_ERROR;
                goto exit;
            }
        }

        *PnpBridge = pbridge;

        g_PnpBridgeState = PNP_BRIDGE_INITIALIZED;
    }
exit:
    {
        if (DIGITALTWIN_CLIENT_OK != result) {
            if (lockAcquired) {
                Unlock(pbridge->ExitLock);
            }
            PnpBridge_Release(pbridge);
            *PnpBridge = NULL;
        }
    }
   
    return result;
}

void 
PnpBridge_Release(
    PPNP_BRIDGE pnpBridge
    )
{
    LogInfo("Cleaning DeviceAggregator resources");

    assert((PNP_BRIDGE_UNINITIALIZED == g_PnpBridgeState) ||
           (PNP_BRIDGE_INITIALIZED == g_PnpBridgeState) ||
           (PNP_BRIDGE_TEARING_DOWN == g_PnpBridgeState));

    g_PnpBridgeState = PNP_BRIDGE_DESTROYED;

    if (pnpBridge->PnpMgr) {
        PnpAdapterManager_ReleaseManager(pnpBridge->PnpMgr);
        pnpBridge->PnpMgr = NULL;
    }

    if (NULL != pnpBridge->ExitCondition) {
        Condition_Deinit(pnpBridge->ExitCondition);
    }

    if (NULL != pnpBridge->ExitLock) {
        Lock_Deinit(pnpBridge->ExitLock);
    }

    if (pnpBridge) {
        free(pnpBridge);
    }
}

DIGITALTWIN_CLIENT_RESULT
PnpBridge_RegisterInterfaces(
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* interfaces,
    unsigned int interfaceCount
)
{
    DIGITALTWIN_CLIENT_RESULT result;
    PPNP_BRIDGE pnpBridge = g_PnpBridge;
    result = IotComms_RegisterPnPInterfaces(&pnpBridge->IotHandle,
        pnpBridge->Configuration.ConnParams->DeviceCapabilityModelUri,
        interfaces,
        interfaceCount);

    return result;
}

int
PnpBridge_Main(const char * ConfigurationFilePath)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_OK;
    PPNP_BRIDGE pnpBridge = NULL;

    {
            LogInfo("Starting Azure PnpBridge");

            if (IoTHub_Init() != 0) {
                LogError("IoTHub_Init failed\n");
                result = DIGITALTWIN_CLIENT_ERROR;
                goto exit;
            }

            result = PnpBridge_Initialize(&pnpBridge, ConfigurationFilePath);
            if (DIGITALTWIN_CLIENT_OK != result) {
                LogError("PnpBridge_Initialize failed: %d", result);
                goto exit;
            }

            g_PnpBridge = pnpBridge;

            LogInfo("Connected to Azure IoT Hub");

            // Create all the adapters that are required by configured devices
            result = PnpAdapterManager_CreateManager(&pnpBridge->PnpMgr, pnpBridge->Configuration.JsonConfig);
            if (DIGITALTWIN_CLIENT_OK != result) {
                LogError("PnpAdapterManager_Create failed: %d", result);
                goto exit;
            }

            result = PnpAdapterManager_CreateInterfaces(pnpBridge->PnpMgr, pnpBridge->Configuration.JsonConfig);
            if (DIGITALTWIN_CLIENT_OK != result) {
                LogError("PnpAdapterManager_CreateInterfaces failed: %d", result);
                goto exit;
            }

            result = PnPAdapterManager_RegisterInterfaces(pnpBridge->PnpMgr);
            if (DIGITALTWIN_CLIENT_OK != result) {
                LogError("PnPAdapterManager_RegisterInterfaces failed: %d", result);
                goto exit;
            }

            result = PnpAdapterManager_StartInterfaces(pnpBridge->PnpMgr);
            if (DIGITALTWIN_CLIENT_OK != result) {
                LogError("PnpAdapterManager_StartInterfaces failed: %d", result);
                goto exit;
            }

            // Prevent main thread from returning by waiting for the
            // exit condition to be set. This condition will be set when
            // the bridge has received a stop signal
            // ExitLock was taken in call to PnpBridge_Initialize so does not need to be reacquired.
            Condition_Wait(pnpBridge->ExitCondition, pnpBridge->ExitLock, 0);
            Unlock(pnpBridge->ExitLock);

    } 
exit:
    {
        // Destroy the DigitalTwinClient and recreate it
        IotComms_DigitalTwinClient_Destroy(&pnpBridge->IotHandle);
        g_PnpBridge = NULL;

        if (pnpBridge) {
            PnpBridge_Release(pnpBridge);
        }
    }

    return result;
}

void 
PnpBridge_Stop()
{
    if (!g_PnpBridgeShutdown)
    {
        g_PnpBridgeShutdown = true;
        assert(g_PnpBridge != NULL);
        assert(g_PnpBridgeState == PNP_BRIDGE_INITIALIZED);

        if (PNP_BRIDGE_TEARING_DOWN != g_PnpBridgeState) {
            g_PnpBridgeState = PNP_BRIDGE_TEARING_DOWN;
            Lock(g_PnpBridge->ExitLock);
            Condition_Post(g_PnpBridge->ExitCondition);
            Unlock(g_PnpBridge->ExitLock);
        }

        IoTHub_Deinit();
    }
}

// Note: PnpBridge_UploadToBlobAsync method is not synchronized 
// with the g_PnpBridge cleanup path

int
PnpBridge_UploadToBlobAsync(
    const char* pszDestination,
    const unsigned char* pbData,
    size_t cbData,
    IOTHUB_CLIENT_FILE_UPLOAD_CALLBACK iotHubClientFileUploadCallback,
    void* context
    )
{
    IOTHUB_CLIENT_RESULT    iotResult = IOTHUB_CLIENT_OK;
    IOTHUB_CLIENT_HANDLE handle = NULL;

    if (!g_PnpBridge->IotHandle.DeviceClientInitialized)
    {
        return DIGITALTWIN_CLIENT_ERROR;
    }

    handle = g_PnpBridge->IotHandle.IsModule ? g_PnpBridge->IotHandle.u1.IotModule.moduleHandle :
                                               g_PnpBridge->IotHandle.u1.IotDevice.deviceHandle;

    if (NULL == pszDestination || (NULL == pbData && cbData > 0) ||
        (NULL != pbData && cbData == 0) ||
        NULL == iotHubClientFileUploadCallback)
    {
        return DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
    }

    iotResult = IoTHubClient_UploadToBlobAsync(handle,
        pszDestination,
        pbData,
        cbData,
        iotHubClientFileUploadCallback,
        context);
    switch (iotResult)
    {
    case IOTHUB_CLIENT_OK:
        return DIGITALTWIN_CLIENT_OK;
        break;
    case IOTHUB_CLIENT_INVALID_ARG:
    case IOTHUB_CLIENT_INVALID_SIZE:
        return DIGITALTWIN_CLIENT_ERROR_INVALID_ARG;
        break;
    case IOTHUB_CLIENT_INDEFINITE_TIME:
    case IOTHUB_CLIENT_ERROR:
    default:
        return DIGITALTWIN_CLIENT_ERROR;
        break;
    }
}