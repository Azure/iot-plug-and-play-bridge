// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "PnpBridgeCommon.h"
#include "ConfigurationParser.h"
#include "DiscoveryManager.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"
#include "IotHubComms.h"

#include "PnpBridgeh.h"

//////////////////////////// GLOBALS ///////////////////////////////////
PPNP_BRIDGE g_PnpBridge = NULL;
bool g_Shutdown = false;
////////////////////////////////////////////////////////////////////////

typedef struct _PNPBRIDGE_INTERFACE_TAG {
    char* InterfaceName;
    PPNPADAPTER_INTERFACE pnpAdapterInterface;
} PNPBRIDGE_INTERFACE_TAG, *PPNPBRIDGE_INTERFACE_TAG;

PNPBRIDGE_RESULT PnpBridge_Initialize(JSON_Value* config) {
    const char* connectionString;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    connectionString = Configuration_GetConnectionString(config);

    TRY
    {
        if (NULL == connectionString) {
            LogError("Connection string not specified in the config\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        g_PnpBridge = (PPNP_BRIDGE)calloc(1, sizeof(PNP_BRIDGE));

        if (NULL == g_PnpBridge) {
            LogError("Failed to allocate memory for PnpBridge global");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        g_PnpBridge->configuration = config;

        g_PnpBridge->dispatchLock = Lock_Init();
        if (NULL == g_PnpBridge->dispatchLock) {
            LogError("Failed to init PnpBridge lock");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        g_Shutdown = false;

        // Connect to Iot Hub and create a PnP device client handle
        if ((g_PnpBridge->deviceHandle = InitializeIotHubDeviceHandle(connectionString)) == NULL)
        {
            LogError("Could not allocate IoTHub Device handle\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }
        else if ((g_PnpBridge->pnpDeviceClientHandle = PnP_DeviceClient_CreateFromDeviceHandle(g_PnpBridge->deviceHandle)) == NULL)
        {
            LogError("PnP_DeviceClient_CreateFromDeviceHandle failed\n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }
    }
    FINALLY
    {
        if (PNPBRIDGE_OK != result) {
            PnpBridge_Release(g_PnpBridge);
            g_PnpBridge = NULL;
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
        Lock(g_PnpBridge->dispatchLock);
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

    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    // Publish persistent Pnp Interfaces
    result = DiscoveryAdapterManager_PublishAlwaysInterfaces(g_PnpBridge->discoveryMgr, g_PnpBridge->configuration);
    if (!PNPBRIDGE_SUCCESS(result)) {
        goto exit;
    }

    // Start Device Discovery
    result = DiscoveryAdapterManager_Start(g_PnpBridge->discoveryMgr, g_PnpBridge->configuration);
    if (PNPBRIDGE_OK != result) {
        goto exit;
    }

    // TODO: Change this to an event
    while (true) 
    {
        ThreadAPI_Sleep(1 * 1000);
        if (g_Shutdown) {
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
    PNPBRIDGE_RESULT result;
    bool containsFilter = false;
    int key = 0;

    Lock(g_PnpBridge->dispatchLock);

    if (g_Shutdown) {
        LogInfo("PnpBridge is shutting down. Dropping the change notification");
        goto end;
    }

    result = PnpBridge_ValidateDeviceChangePayload(DeviceChangePayload);
    if (PNPBRIDGE_OK != result) {
        LogInfo("Change payload is invalid");
        goto end;
    }

    JSON_Value* jmsg;
    JSON_Object* jobj;
    JSON_Object* jdevice;

    jmsg = json_parse_string(DeviceChangePayload->Message);
    if (NULL == jmsg) {
        goto end;
    }

    jobj = json_value_get_object(jmsg);
    if (Configuration_IsDeviceConfigured(g_PnpBridge->configuration, jobj, &jdevice) < 0) {
        LogInfo("Device is not configured. Dropping the change notification");
        goto end;
    }

    result = PnpAdapterManager_SupportsIdentity(g_PnpBridge->adapterMgr, jdevice, &containsFilter, &key);
    if (PNPBRIDGE_OK != result) {
        goto end;
    }

    if (containsFilter) {
        // Create an Pnp interface
        char* interfaceId = (char*) json_object_get_string(jobj, "InterfaceId");

        if (interfaceId != NULL) {
            int idSize = (int) strlen(interfaceId) + 1;
            char* id = malloc(idSize*sizeof(char));
            strcpy_s(id, idSize, interfaceId);
            id[idSize - 1] = '\0';

            if (PnpAdapterManager_IsInterfaceIdPublished(g_PnpBridge->adapterMgr, id)) {
                LogError("PnP Interface has already been published. Dropping the change notification. \n");
                result = PNPBRIDGE_FAILED;
                goto end;
            }

            result = PnpAdapterManager_CreatePnpInterface(g_PnpBridge->adapterMgr, g_PnpBridge->pnpDeviceClientHandle, key, jdevice, DeviceChangePayload);
            if (PNPBRIDGE_OK != result) {
                goto end;
            }

            // Query all the pnp interface clients and publish them
            {
                PNP_INTERFACE_CLIENT_HANDLE* interfaces = NULL;
                int count = 0;
                PnpAdapterManager_GetAllInterfaces(g_PnpBridge->adapterMgr, &interfaces, &count);

                LogInfo("Publishing Azure Pnp Interface %s\n", interfaceId);
                AppRegisterPnPInterfacesAndWait(g_PnpBridge->pnpDeviceClientHandle, interfaces, count);
                free(interfaces);
            }

            goto end;
        }
        else {
            LogError("Dropping device change callback\n");
        }
    }

end:
    Unlock(g_PnpBridge->dispatchLock);

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpBridge_Main() {
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    JSON_Value* config = NULL;

    TRY
    {
        LogInfo("Starting Azure PnpBridge\n");

        // Read the config file from a file
        result = PnpBridgeConfig_ReadConfigurationFromFile("config.json", &config);
        if (PNPBRIDGE_OK != result) {
            LogError("Failed to parse configuration. Check if config file is present and ensure its formatted correctly.\n");
            LEAVE;
        }

        // Check if config file has mandatory parameters
        result = PnpBridgeConfig_ValidateConfiguration(config);
        if (PNPBRIDGE_OK != result) {
            LogError("Config file is invalid\n");
            LEAVE;
        }

        result = PnpBridge_Initialize(config);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpBridge_Initialize failed: %d\n", result);
            LEAVE;
        }

        LogInfo("Connected to Azure IoT Hub\n");

        // Load all the adapters in interface manifest that implement Azure IoT PnP Interface
        // PnpBridge will call into corresponding adapter when a device is reported by 
        // DiscoveryExtension
        result = PnpAdapterManager_Create(&g_PnpBridge->adapterMgr, g_PnpBridge->configuration);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpAdapterManager_Create failed: %d\n", result);
            LEAVE;
        }

        // Load all the extensions that are capable of discovering devices
        // and reporting back to PnpBridge
        result = DiscoveryAdapterManager_Create(&g_PnpBridge->discoveryMgr);
        if (PNPBRIDGE_OK != result) {
            LogError("DiscoveryAdapterManager_Create failed: %d\n", result);
            LEAVE;
        }

        THREAD_HANDLE workerThreadHandle = NULL;
        if (THREADAPI_OK != ThreadAPI_Create(&workerThreadHandle, PnpBridge_Worker_Thread, NULL)) {
            LogError("Failed to create PnpBridge_Worker_Thread\n");
            workerThreadHandle = NULL;
        }
        else {
            ThreadAPI_Join(workerThreadHandle, NULL);
        }
    }
    FINALLY
    {
        PnpBridge_Release(g_PnpBridge);
    }

    return result;
}

void PnpBridge_Stop() {
    Lock(g_PnpBridge->dispatchLock);
    g_Shutdown = true;
    Unlock(g_PnpBridge->dispatchLock);
}

#include <windows.h>

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
    // Handle the CTRL-C signal. 
    case CTRL_C_EVENT:
        PnpBridge_Stop();
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

