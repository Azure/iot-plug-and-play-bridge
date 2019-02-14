// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "PnpBridgeCommon.h"
#include "ConfigurationParser.h"
#include "DiscoveryManager.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"
#include "IotHubComms.h"

#include "PnpBridgeh.h"

// Global instance of PnpBridge
PPNP_BRIDGE g_PnpBridge = NULL;

PNPBRIDGE_RESULT PnpBridge_Initialize(PPNP_BRIDGE* pnpBridge, JSON_Value* config) {
    const char* connectionString;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    PPNP_BRIDGE pbridge = NULL;

    connectionString = Configuration_GetConnectionString(config);

    TRY
    {
        if (NULL == connectionString) {
            LogError("Connection string not specified in the config\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        pbridge = (PPNP_BRIDGE)calloc(1, sizeof(PNP_BRIDGE));

        if (NULL == pbridge) {
            LogError("Failed to allocate memory for PnpBridge global");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        pbridge->configuration = config;

        pbridge->dispatchLock = Lock_Init();
        if (NULL == pbridge->dispatchLock) {
            LogError("Failed to init PnpBridge lock");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        pbridge->shutdown = false;

        // Connect to Iot Hub and create a PnP device client handle
        if ((pbridge->deviceHandle = InitializeIotHubDeviceHandle(connectionString)) == NULL)
        {
            LogError("Could not allocate IoTHub Device handle\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }
        else if ((pbridge->pnpDeviceClientHandle = PnP_DeviceClient_CreateFromDeviceHandle(pbridge->deviceHandle)) == NULL)
        {
            LogError("PnP_DeviceClient_CreateFromDeviceHandle failed\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        *pnpBridge = pbridge;
    }
    FINALLY
    {
        if (PNPBRIDGE_OK != result) {
            PnpBridge_Release(pbridge);
            *pnpBridge = NULL;
        }
    }
   
    return result;
}

void PnpBridge_Release(PPNP_BRIDGE pnpBridge) {
    if (NULL == pnpBridge) {
        return;
    }

    LogInfo("Cleaning DeviceAggregator resources");

    if (pnpBridge->dispatchLock) {
        Lock(pnpBridge->dispatchLock);
    }

    // Stop Disovery Modules
    if (pnpBridge->discoveryMgr) {
        DiscoveryAdapterManager_Stop(pnpBridge->discoveryMgr);
        pnpBridge->discoveryMgr = NULL;
    }

    // Stop Pnp Modules
    if (pnpBridge->adapterMgr) {
        PnpAdapterManager_Release(pnpBridge->adapterMgr);
        pnpBridge->adapterMgr = NULL;
    }

    if (pnpBridge->dispatchLock) {
        Unlock(pnpBridge->dispatchLock);
        Lock_Deinit(pnpBridge->dispatchLock);
    }

    if (pnpBridge) {
        free(pnpBridge);
    }
}

int PnpBridge_Worker_Thread(void* threadArgument)
{
    AZURE_UNREFERENCED_PARAMETER(threadArgument);

    PPNP_BRIDGE pnpBridge = (PPNP_BRIDGE)threadArgument;

    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    // Publish persistent Pnp Interfaces
    result = DiscoveryAdapterManager_PublishAlwaysInterfaces(pnpBridge->discoveryMgr, pnpBridge->configuration);
    if (!PNPBRIDGE_SUCCESS(result)) {
        goto exit;
    }

    // Start Device Discovery
    result = DiscoveryAdapterManager_Start(pnpBridge->discoveryMgr, pnpBridge->configuration);
    if (PNPBRIDGE_OK != result) {
        goto exit;
    }

    // TODO: Change this to an event
    while (true) 
    {
        ThreadAPI_Sleep(1 * 1000);
        if (pnpBridge->shutdown) {
            goto exit;
        }
    }

exit:
    return result;
}

PNPBRIDGE_RESULT PnpBridge_ValidateDeviceChangePayload(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
    if (NULL == DeviceChangePayload) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    if (NULL == DeviceChangePayload->Message) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    if (0 == DeviceChangePayload->MessageLength) {
        return PNPBRIDGE_INVALID_ARGS;
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpBridge_DeviceChangeCallback(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
    PPNP_BRIDGE pnpBridge = g_PnpBridge;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    bool containsFilter = false;
    int key = 0;
    JSON_Value* jmsg = NULL;

    TRY
    {
        Lock(pnpBridge->dispatchLock);

        if (pnpBridge->shutdown) {
            LogInfo("PnpBridge is shutting down. Dropping the change notification");
            LEAVE;
        }

        result = PnpBridge_ValidateDeviceChangePayload(DeviceChangePayload);
        if (PNPBRIDGE_OK != result) {
            LogInfo("Change payload is invalid");
            LEAVE;
        }

        JSON_Object* jobj;
        JSON_Object* jdevice;

        jmsg = json_parse_string(DeviceChangePayload->Message);
        if (NULL == jmsg) {
            LEAVE;
        }

        jobj = json_value_get_object(jmsg);
        if (Configuration_IsDeviceConfigured(pnpBridge->configuration, jobj, &jdevice) < 0) {
            LogInfo("Device is not configured. Dropping the change notification");
            LEAVE;
        }

        result = PnpAdapterManager_SupportsIdentity(pnpBridge->adapterMgr, jdevice, &containsFilter, &key);
        if (PNPBRIDGE_OK != result) {
            LEAVE;
        }

        if (!containsFilter) {
            LogError("Dropping device change callback");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        // Create an Pnp interface
        char* interfaceId = NULL;
        char* selfDescribing = (char*)json_object_get_string(jdevice, PNP_CONFIG_NAME_SELF_DESCRIBING);;
        if (NULL != selfDescribing && 0 == stricmp(selfDescribing, "true")) {
            interfaceId = (char*)json_object_get_string(jobj, "InterfaceId");
            if (NULL == interfaceId) {
                LogError("Interface id is missing for self descrbing device");
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }
        }
        else {
            interfaceId = (char*)json_object_get_string(jdevice, "InterfaceId");
            if (NULL == interfaceId) {
                LogError("Interface Id is missing in config for the device");
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            // Add interfaceId to the message
            json_object_set_string(jobj, PNP_CONFIG_NAME_INTERFACE_ID, interfaceId);
        }

        if (PnpAdapterManager_IsInterfaceIdPublished(pnpBridge->adapterMgr, interfaceId)) {
            LogError("PnP Interface has already been published. Dropping the change notification. \n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        result = PnpAdapterManager_CreatePnpInterface(pnpBridge->adapterMgr, pnpBridge->pnpDeviceClientHandle, key, jdevice, DeviceChangePayload);
        if (!PNPBRIDGE_SUCCESS(result)) {
            LEAVE;
        }

        // Query all the pnp interface clients and publish them
        {
            PNP_INTERFACE_CLIENT_HANDLE* interfaces = NULL;
            int count = 0;
            PnpAdapterManager_GetAllInterfaces(pnpBridge->adapterMgr, &interfaces, &count);

            LogInfo("Publishing Azure Pnp Interface %s", interfaceId);
            AppRegisterPnPInterfacesAndWait(pnpBridge->pnpDeviceClientHandle, interfaces, count);
            free(interfaces);
            }
    }
    FINALLY
    {
        Unlock(pnpBridge->dispatchLock);
        if (NULL != jmsg) {
            json_value_free(jmsg);
        }
    }

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpBridge_Main() {
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    JSON_Value* config = NULL;
    PPNP_BRIDGE pnpBridge = NULL;

    TRY
    {
        LogInfo("Starting Azure PnpBridge");

        // Read the config file from a file
        result = PnpBridgeConfig_ReadConfigurationFromFile("config.json", &config);
        if (PNPBRIDGE_OK != result) {
            LogError("Failed to parse configuration. Check if config file is present and ensure its formatted correctly");
            LEAVE;
        }

        // Check if config file has mandatory parameters
        result = PnpBridgeConfig_ValidateConfiguration(config);
        if (PNPBRIDGE_OK != result) {
            LogError("Config file is invalid");
            LEAVE;
        }

        result = PnpBridge_Initialize(&pnpBridge, config);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpBridge_Initialize failed: %d", result);
            LEAVE;
        }

        g_PnpBridge = pnpBridge;

        LogInfo("Connected to Azure IoT Hub");

        // Load all the adapters in interface manifest that implement Azure IoT PnP Interface
        // PnpBridge will call into corresponding adapter when a device is reported by 
        // DiscoveryExtension
        result = PnpAdapterManager_Create(&pnpBridge->adapterMgr, pnpBridge->configuration);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpAdapterManager_Create failed: %d", result);
            LEAVE;
        }

        // Load all the extensions that are capable of discovering devices
        // and reporting back to PnpBridge
        result = DiscoveryAdapterManager_Create(&pnpBridge->discoveryMgr);
        if (PNPBRIDGE_OK != result) {
            LogError("DiscoveryAdapterManager_Create failed: %d", result);
            LEAVE;
        }

        THREAD_HANDLE workerThreadHandle = NULL;
        if (THREADAPI_OK != ThreadAPI_Create(&workerThreadHandle, PnpBridge_Worker_Thread, pnpBridge)) {
            LogError("Failed to create PnpBridge_Worker_Thread");
            workerThreadHandle = NULL;
        }
        else {
            ThreadAPI_Join(workerThreadHandle, NULL);
        }
    }
    FINALLY
    {
        g_PnpBridge = NULL;

        PnpBridge_Release(pnpBridge);

        if (NULL != config) {
            json_value_free(config);
        }
    }

    return result;
}

void PnpBridge_Stop(PPNP_BRIDGE pnpBridge) {
    Lock(pnpBridge->dispatchLock);
    pnpBridge->shutdown = true;
    Unlock(pnpBridge->dispatchLock);
}

#include <windows.h>

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
    // Handle the CTRL-C signal. 
    case CTRL_C_EVENT:
        PnpBridge_Stop(g_PnpBridge);
        return TRUE;

    default:
        return FALSE;
    }
}

int main()
{
    if (SetConsoleCtrlHandler(CtrlHandler, TRUE))
    {
        printf("\n -- Press Ctrl+C to stop PnpBridge\n");
    }

    PnpBridge_Main();
}