// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "configuration_parser.h"
#include "pnpadapter_api.h"
#include "pnpadapter_manager.h"
#include "iothub_comms.h"

#include <iothub_client.h>

// Globals PNP bridge instance
PPNP_BRIDGE g_PnpBridge = NULL;
PNP_BRIDGE_STATE g_PnpBridgeState = PNP_BRIDGE_UNINITIALIZED;
bool g_PnpBridgeShutdown = false;

IOTHUB_CLIENT_RESULT 
PnpBridge_Initialize(
    PPNP_BRIDGE* PnpBridge,
    const char * ConfigFilePath
    ) 
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PPNP_BRIDGE pbridge = NULL;
    JSON_Value* config = NULL;
    bool lockAcquired = false;

    {
        g_PnpBridgeState = PNP_BRIDGE_UNINITIALIZED;

        // Allocate memory for the PNP_BRIDGE structure
        pbridge = (PPNP_BRIDGE) calloc(1, sizeof(PNP_BRIDGE));
        if (NULL == pbridge) {
            LogError("Failed to allocate memory for PnpBridge global");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        pbridge->ExitCondition = Condition_Init();
        if (NULL == pbridge->ExitCondition) {
            LogError("Failed to init ExitCondition");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        pbridge->ExitLock = Lock_Init();
        if (NULL == pbridge->ExitLock) {
            LogError("Failed to init ExitLock lock");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }
        Lock(pbridge->ExitLock);
        lockAcquired = true;

        // Get the JSON VALUE of configuration file
        result = PnpBridgeConfig_GetJsonValueFromConfigFile(ConfigFilePath, &config);
        if (IOTHUB_CLIENT_OK != result) {
            LogError("Failed to retrieve the bridge configuration from specified file location.");
            goto exit;
        }

        // Check if config file has REQUIRED parameters
        result = PnpBridgeConfig_RetrieveConfiguration(config, &pbridge->Configuration);
        if (IOTHUB_CLIENT_OK != result) {
            LogError("Config file is invalid");
            goto exit;
        }

        *PnpBridge = pbridge;

        g_PnpBridgeState = PNP_BRIDGE_INITIALIZED;
    }
exit:
    {
        if (IOTHUB_CLIENT_OK != result) {
            if (lockAcquired) {
                Unlock(pbridge->ExitLock);
            }
            PnpBridge_Release(pbridge);
            *PnpBridge = NULL;
        }
    }
   
    return result;
}

IOTHUB_CLIENT_RESULT 
PnpBridge_RegisterIoTHubHandle()
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    result = IotComms_InitializeIotHandle(&g_PnpBridge->IotHandle,
        g_PnpBridge->Configuration.TraceOn, g_PnpBridge->Configuration.ConnParams);
    if (IOTHUB_CLIENT_OK != result) {
        LogError("IotComms_InitializeIotHandle failed.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

exit:
    return result;
}

IOTHUB_CLIENT_RESULT 
PnpBridge_UnregisterIoTHubHandle()
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    result = IotComms_DeinitializeIotHandle(&g_PnpBridge->IotHandle, g_PnpBridge->Configuration.ConnParams);
    if (IOTHUB_CLIENT_OK != result) {
        LogError("IotComms_DeinitializeIotHandle failed.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

exit:
    return result;
}

void 
PnpBridge_Release(
    PPNP_BRIDGE pnpBridge
    )
{
    LogInfo("Cleaning Pnp Bridge resources");

    assert((PNP_BRIDGE_UNINITIALIZED == g_PnpBridgeState) ||
           (PNP_BRIDGE_INITIALIZED == g_PnpBridgeState) ||
           (PNP_BRIDGE_TEARING_DOWN == g_PnpBridgeState));

    g_PnpBridgeState = PNP_BRIDGE_DESTROYED;

    if (pnpBridge->PnpMgr)
    {
        // Free resources used by components
        PnpAdapterManager_DestroyComponents(pnpBridge->PnpMgr);
        // Free resources used by adapters and adapter manager
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

int
PnpBridge_Main(const char * ConfigurationFilePath)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PPNP_BRIDGE pnpBridge = NULL;

    {
            LogInfo("Starting Azure PnpBridge");

            result = PnpBridge_Initialize(&pnpBridge, ConfigurationFilePath);
            if (IOTHUB_CLIENT_OK != result) {
                LogError("PnpBridge_Initialize failed: %d", result);
                goto exit;
            }

            g_PnpBridge = pnpBridge;

            LogInfo("Building Pnp Bridge Adapter Manager & Adapters");

            // Create all the adapters that are required by configured devices
            result = PnpAdapterManager_CreateManager(&pnpBridge->PnpMgr, pnpBridge->Configuration.JsonConfig);
            if (IOTHUB_CLIENT_OK != result) {
                LogError("PnpAdapterManager_CreateManager failed: %d", result);
                goto exit;
            }

            result = PnpAdapterManager_CreateComponents(pnpBridge->PnpMgr, pnpBridge->Configuration.JsonConfig);
            if (IOTHUB_CLIENT_OK != result) {
                LogError("PnpAdapterManager_CreateComponents failed: %d", result);
                goto exit;
            }

            result = PnpAdapterManager_BuildComponentsInModel(pnpBridge->PnpMgr);
            if (IOTHUB_CLIENT_OK != result) {
                LogError("PnpAdapterManager_BuildComponentsInModel failed: %d", result);
                goto exit;
            }

            result = PnpBridge_RegisterIoTHubHandle();
            if (IOTHUB_CLIENT_OK != result) {
                LogError("PnpBridge_RegisterIoTHubHandle failed: %d", result);
                goto exit;
            }

            LogInfo("Connected to Azure IoT Hub");


            result = PnpAdapterManager_StartComponents(pnpBridge->PnpMgr);
            if (IOTHUB_CLIENT_OK != result) {
                LogError("PnpAdapterManager_StartComponents failed: %d", result);
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
        if (pnpBridge)
        {
            PnpAdapterManager_StopComponents(pnpBridge->PnpMgr);
            PnpBridge_UnregisterIoTHubHandle();
            PnpBridge_Release(pnpBridge);
        }
        g_PnpBridge = NULL;
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
        return IOTHUB_CLIENT_ERROR;
    }

    handle = g_PnpBridge->IotHandle.IsModule ? g_PnpBridge->IotHandle.u1.IotModule.moduleHandle :
                                               g_PnpBridge->IotHandle.u1.IotDevice.deviceHandle;

    if (NULL == pszDestination || (NULL == pbData && cbData > 0) ||
        (NULL != pbData && cbData == 0) ||
        NULL == iotHubClientFileUploadCallback)
    {
        return IOTHUB_CLIENT_INVALID_ARG;
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
        return IOTHUB_CLIENT_OK;
        break;
    case IOTHUB_CLIENT_INVALID_ARG:
    case IOTHUB_CLIENT_INVALID_SIZE:
        return IOTHUB_CLIENT_INVALID_ARG;
        break;
    case IOTHUB_CLIENT_INDEFINITE_TIME:
    case IOTHUB_CLIENT_ERROR:
    default:
        return IOTHUB_CLIENT_ERROR;
        break;
    }
}