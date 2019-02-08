// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// DeviceAggregator.cpp : Defines the entry point for the console application.

#include "PnpBridgeCommon.h"
#include "ConfigurationParser.h"
#include "DiscoveryManager.h"
#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

#include "PnpBridgeh.h"

//////////////////////////// GLOBALS ///////////////////////////////////
PPNP_BRIDGE g_PnpBridge = NULL;
bool g_Shutdown = false;
////////////////////////////////////////////////////////////////////////

//const char* connectionString = "HostName=iot-pnp-hub1.azure-devices.net;DeviceId=win-gateway;SharedAccessKey=GfbYy7e2PikTf2qHyabvEDBaJB5S4T+H+b9TbLsXfns=";
//const char* connectionString = "HostName=saas-iothub-1529564b-8f58-4871-b721-fe9459308cb1.azure-devices.net;DeviceId=956da476-8b3c-41ce-b405-d2d32bcf5e79;SharedAccessKey=sQcfPeDCZGEJWPI3M3SyB8pD60TNdOw10oFKuv5FBio=";

typedef struct _PNPBRIDGE_INTERFACE_TAG {
    PNP_INTERFACE_CLIENT_HANDLE Interface;
    char* InterfaceName;
} PNPBRIDGE_INTERFACE_TAG, *PPNPBRIDGE_INTERFACE_TAG;

// InitializeIotHubDeviceHandle initializes underlying IoTHub client, creates a device handle with the specified connection string,
// and sets some options on this handle prior to beginning.
IOTHUB_DEVICE_HANDLE InitializeIotHubDeviceHandle(const char* connectionString)
{
    IOTHUB_DEVICE_HANDLE deviceHandle = NULL;
    IOTHUB_CLIENT_RESULT iothubClientResult;
    bool traceOn = true;
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

void PnpBridge_CoreCleanup() {
    if (g_PnpBridge->publishedInterfaces) {
        LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_PnpBridge->publishedInterfaces);
        while (interfaceItem != NULL) {
            PPNPBRIDGE_INTERFACE_TAG interface = (PPNPBRIDGE_INTERFACE_TAG)singlylinkedlist_item_get_value(interfaceItem);
            free(interface->Interface);
            interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
        }
        singlylinkedlist_destroy(g_PnpBridge->publishedInterfaces);
    }
    if (g_PnpBridge->dispatchLock) {
        Lock_Deinit(g_PnpBridge->dispatchLock);
    }
}

PNPBRIDGE_RESULT PnpBridge_Initialize(JSON_Value* config) {
    const char* connectionString;

    connectionString = Configuration_GetConnectionString(config);

    if (NULL == connectionString) {
        LogError("Connection string not specified in the config\n");
        return PNPBRIDGE_FAILED;
    }

    g_PnpBridge = (PPNP_BRIDGE) calloc(1, sizeof(PNP_BRIDGE));

    if (!g_PnpBridge) {
        LogError("Failed to allocate memory for PnpBridge global");
        PnpBridge_CoreCleanup();
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

    g_PnpBridge->configuration = config;

    g_PnpBridge->publishedInterfaces = singlylinkedlist_create();
    if (NULL == g_PnpBridge->publishedInterfaces) {
        LogError("Failed to allocate memory publish interface list");
        PnpBridge_CoreCleanup();
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

    g_PnpBridge->publishedInterfaceCount = 0;

    g_PnpBridge->dispatchLock = Lock_Init();
    if (NULL == g_PnpBridge->dispatchLock) {
        LogError("Failed to init PnpBridge lock");
        PnpBridge_CoreCleanup();
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

    g_Shutdown = false;

    // Connect to Iot Hub and create a PnP device client handle
    if ((g_PnpBridge->deviceHandle = InitializeIotHubDeviceHandle(connectionString)) == NULL)
    {
        LogError("Could not allocate IoTHub Device handle\n");
        PnpBridge_CoreCleanup();
        return PNPBRIDGE_FAILED;
    }
    else if ((g_PnpBridge->pnpDeviceClientHandle = PnP_DeviceClient_CreateFromDeviceHandle(g_PnpBridge->deviceHandle)) == NULL)
    {
        LogError("PnP_DeviceClient_CreateFromDeviceHandle failed\n");
        PnpBridge_CoreCleanup();
        return PNPBRIDGE_FAILED;
    }

    return PNPBRIDGE_OK;
}

void PnpBridge_Release() {

    LogInfo("Cleaning DeviceAggregator resources");

    Lock(g_PnpBridge->dispatchLock);

    // Stop Disovery Modules
    if (g_PnpBridge->discoveryMgr) {
        DiscoveryAdapterManager_Stop(g_PnpBridge->discoveryMgr);
        g_PnpBridge->discoveryMgr = NULL;
    }

    // Stop Pnp Modules
    if (g_PnpBridge->interfaceMgr) {
        PnpAdapterManager_Release(g_PnpBridge->interfaceMgr);
        g_PnpBridge->interfaceMgr = NULL;
    }

    Unlock(g_PnpBridge->dispatchLock);

    PnpBridge_CoreCleanup();

    if (g_PnpBridge) {
        free(g_PnpBridge);
    }
}

int PnpBridge_Worker_Thread(void* threadArgument)
{
    AZURE_UNREFERENCED_PARAMETER(threadArgument);
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

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
int AppRegisterPnPInterfacesAndWait(PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle)
{
    APP_PNP_REGISTRATION_STATUS appPnpRegistrationStatus = APP_PNP_REGISTRATION_PENDING;
    PNPBRIDGE_RESULT result;
    PNP_CLIENT_RESULT pnpResult;
    PPNPBRIDGE_INTERFACE_TAG* interfaceTags = malloc(sizeof(PPNPBRIDGE_INTERFACE_TAG)*g_PnpBridge->publishedInterfaceCount);
    PNP_INTERFACE_CLIENT_HANDLE* interfaceClients = malloc(sizeof(PNP_INTERFACE_CLIENT_HANDLE)*g_PnpBridge->publishedInterfaceCount);
    LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_PnpBridge->publishedInterfaces);
    int i = 0;

    while (interfaceItem != NULL) {
        PNP_INTERFACE_CLIENT_HANDLE interface = ((PPNPBRIDGE_INTERFACE_TAG) singlylinkedlist_item_get_value(interfaceItem))->Interface;
        interfaceClients[i++] = interface;
        interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
    }

    pnpResult = PnP_DeviceClient_RegisterInterfacesAsync(pnpDeviceClientHandle, interfaceClients, g_PnpBridge->publishedInterfaceCount, appPnpInterfacesRegistered, &appPnpRegistrationStatus);
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
    free(interfaceClients);
    free(interfaceTags);

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

    //if (PNPBRIDGE_INTERFACE_CHANGE_INVALID == DeviceChangePayload->ChangeType) {
    //    return PNPBRIDGE_INVALID_ARGS;
    //}

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
    jobj = json_value_get_object(jmsg);
    if (Configuration_IsDeviceConfigured(g_PnpBridge->configuration, jobj, &jdevice) < 0) {
        LogInfo("Device is not configured. Dropping the change notification");
        goto end;
    }

    result = PnpAdapterManager_SupportsIdentity(g_PnpBridge->interfaceMgr, jdevice, &containsFilter, &key);
    if (PNPBRIDGE_OK != result) {
        goto end;
    }

    if (containsFilter) {
        // Create an Azure IoT PNP interface
        PPNPBRIDGE_INTERFACE_TAG pInt = malloc(sizeof(PNPBRIDGE_INTERFACE_TAG));
        char* interfaceId = (char*) json_object_get_string(jobj, "InterfaceId");

        if (interfaceId != NULL) {
            int idSize = (int) strlen(interfaceId) + 1;
            char* id = malloc(idSize*sizeof(char));
            strcpy_s(id, idSize, interfaceId);
            id[idSize - 1] = '\0';

            // check if interface is already published
            //PPNPBRIDGE_INTERFACE_TAG* interfaceTags = malloc(sizeof(PPNPBRIDGE_INTERFACE_TAG)*g_PnpBridge->publishedInterfaceCount);
            LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_PnpBridge->publishedInterfaces);
            while (interfaceItem != NULL) {
                PPNPBRIDGE_INTERFACE_TAG interface = (PPNPBRIDGE_INTERFACE_TAG) singlylinkedlist_item_get_value(interfaceItem);
                if (strcmp(interface->InterfaceName, id) == 0) {
                    LogError("PnP Interface has already been published. Dropping the change notification. \n");
                    result = PNPBRIDGE_FAILED;
                    goto end;
                }
                interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
            }

            pInt->InterfaceName = id;

            result = PnpAdapterManager_CreatePnpInterface(g_PnpBridge->interfaceMgr, g_PnpBridge->pnpDeviceClientHandle, key, &pInt->Interface, DeviceChangePayload);
            if (PNPBRIDGE_OK != result) {
                goto end;
            }

            g_PnpBridge->publishedInterfaceCount++;
            singlylinkedlist_add(g_PnpBridge->publishedInterfaces, pInt);

            LogInfo("Publishing Azure Pnp Interface %s\n", interfaceId);
            AppRegisterPnPInterfacesAndWait(g_PnpBridge->pnpDeviceClientHandle);

            goto end;
        }
        else {
            LogError("Interface id not set for the device change callback\n");
        }
    }

end:
    Unlock(g_PnpBridge->dispatchLock);

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT PnpBridge_Main() {
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    JSON_Value* config = NULL;

    LogInfo("Starting Azure PnpBridge\n");

    //#define CONFIG_FILE "c:\\data\\test\\dag\\config.json"
    result = PnpBridgeConfig_ReadConfigurationFromFile("config.json", &config);
    if (PNPBRIDGE_OK != result) {
        LogError("Failed to parse configuration. Check if config file is present and ensure its formatted correctly.\n");
        goto exit;
    }

    result = PnpBridge_Initialize(config);
    if (PNPBRIDGE_OK != result) {
        LogError("PnpBridge_Initialize failed: %d\n", result);
        goto exit;
    }

    LogInfo("Connected to Azure IoT Hub\n");

    // Load all the adapters in interface manifest that implement Azure IoT PnP Interface
    // PnpBridge will call into corresponding adapter when a device is reported by 
    // DiscoveryExtension
    result = PnpAdapterManager_Create(&g_PnpBridge->interfaceMgr, g_PnpBridge->configuration);
    if (PNPBRIDGE_OK != result) {
        LogError("PnpAdapterManager_Create failed: %d\n", result);
        goto exit;
    }

    // Load all the extensions that are capable of discovering devices
    // and reporting back to PnpBridge
    result = DiscoveryAdapterManager_Create(&g_PnpBridge->discoveryMgr);
    if (PNPBRIDGE_OK != result) {
        LogError("DiscoveryAdapterManager_Create failed: %d\n", result);
        goto exit;
    }

    THREAD_HANDLE workerThreadHandle = NULL;
    if (THREADAPI_OK != ThreadAPI_Create(&workerThreadHandle, PnpBridge_Worker_Thread, NULL)) {
        LogError("Failed to create PnpBridge_Worker_Thread \n");
        workerThreadHandle = NULL;
    }
    else {
        ThreadAPI_Join(workerThreadHandle, NULL);
    }

exit:
    PnpBridge_Release();

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

