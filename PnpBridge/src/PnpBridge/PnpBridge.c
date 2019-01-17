// DeviceAggregator.cpp : Defines the entry point for the console application.
//

#include "common.h"

#include "PnpBridgeCommon.h"

#include "ConfigurationParser.h"

#include "DiscoveryManager.h"

#include "PnpAdapterInterface.h"
#include "PnpAdapterManager.h"

#include "PnpBridge.h"

// Things to implement
//
// 1. Allow a module to intercept traffic anf execute commands
// 2. Clean up all warnings
// 3. Restart mechanism

//////////////////////////// GLOBALS ///////////////////////////////////
PPNP_BRIDGE g_PnpBridge = NULL;
////////////////////////////////////////////////////////////////////////

//const char* connectionString = "HostName=iot-pnp-hub1.azure-devices.net;DeviceId=win-gateway;SharedAccessKey=GfbYy7e2PikTf2qHyabvEDBaJB5S4T+H+b9TbLsXfns=";
const char* connectionString = "HostName=saas-iothub-1529564b-8f58-4871-b721-fe9459308cb1.azure-devices.net;DeviceId=956da476-8b3c-41ce-b405-d2d32bcf5e79;SharedAccessKey=sQcfPeDCZGEJWPI3M3SyB8pD60TNdOw10oFKuv5FBio=";

// InitializeIotHubDeviceHandle initializes underlying IoTHub client, creates a device handle with the specified connection string,
// and sets some options on this handle prior to beginning.
IOTHUB_DEVICE_HANDLE InitializeIotHubDeviceHandle()
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

PNPBRIDGE_RESULT PnpBridge_Initialize() {
	g_PnpBridge = (PPNP_BRIDGE) calloc(1, sizeof(PNP_BRIDGE));

    if (!g_PnpBridge) {
        LogError("Failed to allocate memory for PnpBridge global");
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    }

	g_PnpBridge->publishedInterfaces = singlylinkedlist_create();
	g_PnpBridge->publishedInterfaceCount = 0;

	g_PnpBridge->dispatchLock = Lock_Init();

	g_PnpBridge->shuttingDown = false;

    // Connect to Iot Hub and create a PnP device client handle
    if ((g_PnpBridge->deviceHandle = InitializeIotHubDeviceHandle()) == NULL)
    {
        LogError("Could not allocate IoTHub Device handle\n");
        return PNPBRIDGE_FAILED;
    }
    else if ((g_PnpBridge->pnpDeviceClientHandle = PnP_DeviceClient_CreateFromDeviceHandle(g_PnpBridge->deviceHandle)) == NULL)
    {
        LogError("PnP_DeviceClient_CreateFromDeviceHandle failed\n");
        return PNPBRIDGE_FAILED;
    }

    return PNPBRIDGE_OK;
}

void PnpBridge_Release() {

	LogInfo("Cleaning DeviceAggregator resources");

	// Stop Device Disovery Modules
	if (g_PnpBridge->discoveryMgr) {
		DiscoveryAdapterManager_Stop(g_PnpBridge->discoveryMgr);
		g_PnpBridge->discoveryMgr = NULL;
	}

	if (g_PnpBridge->interfaceMgr) {
		PnpAdapterManager_Release(g_PnpBridge->interfaceMgr);
		g_PnpBridge->interfaceMgr = NULL;
	}

	if (g_PnpBridge) {
		free(g_PnpBridge);
	}
}

static PNPBRIDGE_RESULT PnpBridge_Worker_Thread(void* threadArgument)
{
	PNPBRIDGE_RESULT result;

	// Start Device Discovery
	result = DiscoveryAdapterManager_Start(g_PnpBridge->discoveryMgr);
	if (PNPBRIDGE_OK != result) {
		return result;
	}

    // TODO: Change this to an event
	while (true) 
    {
		ThreadAPI_Sleep(1 * 1000);
        if (g_PnpBridge->shuttingDown) {
            return PNPBRIDGE_OK;
        }
	}

	return PNPBRIDGE_OK;
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

static void reportedStateCallback(int status_code, void* userContextCallback)
{
	LogError("Device Twin reported properties update completed with result: %d\r\n", status_code);
}

typedef struct _DAG_PNP_INTERFACE_TAG {
	PNP_INTERFACE_CLIENT_HANDLE Interface;
	char* InterfaceName;
} PNPBRIDGE_INTERFACE_TAG, *PPNPBRIDGE_INTERFACE_TAG;

// Invokes PnP_DeviceClient_RegisterInterfacesAsync, which indicates to Azure IoT which PnP interfaces this device supports.
// The PnP Handle *is not valid* until this operation has completed (as indicated by the callback appPnpInterfacesRegistered being invoked).
// In this sample, we block indefinitely but production code should include a timeout.
int AppRegisterPnPInterfacesAndWait(PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle)
{
	APP_PNP_REGISTRATION_STATUS appPnpRegistrationStatus = APP_PNP_REGISTRATION_PENDING;
	int result;

	int i = 0;

	PPNPBRIDGE_INTERFACE_TAG* interfaceTags = malloc(sizeof(PPNPBRIDGE_INTERFACE_TAG)*g_PnpBridge->publishedInterfaceCount);
	PNP_INTERFACE_CLIENT_HANDLE* interfaceClients = malloc(sizeof(PNP_INTERFACE_CLIENT_HANDLE)*g_PnpBridge->publishedInterfaceCount);
	LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_PnpBridge->publishedInterfaces);
	while (interfaceItem != NULL) {
		PNP_INTERFACE_CLIENT_HANDLE interface = ((PPNPBRIDGE_INTERFACE_TAG) singlylinkedlist_item_get_value(interfaceItem))->Interface;
		interfaceClients[i++] = interface;
		interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
	}

	PnP_DeviceClient_RegisterInterfacesAsync(pnpDeviceClientHandle, interfaceClients, g_PnpBridge->publishedInterfaceCount, appPnpInterfacesRegistered, &appPnpRegistrationStatus);

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

	free(interfaceClients);
	free(interfaceTags);

	return result;
}

PNPBRIDGE_RESULT PnpBridge_DeviceChangeCallback(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload) {
    PNPBRIDGE_RESULT result;
    bool containsFilter = false;
    int key = 0;

    Lock(g_PnpBridge->dispatchLock);

    if (g_PnpBridge->shuttingDown) {
        LogInfo("PnpBridge is shutting down. Dropping the change notification");
        result = PNPBRIDGE_OK;
        goto end;
    }

    if (Configuration_IsDeviceConfigured(DeviceChangePayload->Message) < 0) {
        LogInfo("Device is not configured. Dropping the change notification");
        result = PNPBRIDGE_OK;
        goto end;
    }

    result = PnpAdapterManager_SupportsIdentity(g_PnpBridge->interfaceMgr, DeviceChangePayload->Message, &containsFilter, &key);
    if (PNPBRIDGE_OK != result) {
        goto end;
    }

	if (containsFilter) {
		// Get the interface ID
		// PnpAdapterMangager_GetInterfaceId(key, Message, &interfaceId);

		// Create an Azure IoT PNP interface
		PPNPBRIDGE_INTERFACE_TAG pInt = malloc(sizeof(PNPBRIDGE_INTERFACE_TAG));
		char* interfaceId = (char*) json_object_get_string(DeviceChangePayload->Message, "InterfaceId");

		if (interfaceId != NULL) {
			// check if interface is already published
			PPNPBRIDGE_INTERFACE_TAG* interfaceTags = malloc(sizeof(PPNPBRIDGE_INTERFACE_TAG)*g_PnpBridge->publishedInterfaceCount);
			LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(g_PnpBridge->publishedInterfaces);
			while (interfaceItem != NULL) {
				PPNPBRIDGE_INTERFACE_TAG interface = (PPNPBRIDGE_INTERFACE_TAG) singlylinkedlist_item_get_value(interfaceItem);
				if (strcmp(interface->InterfaceName, interfaceId) == 0) {
				    LogError("PnP Interface has already been published \n");
                    result = PNPBRIDGE_FAILED;
                    goto end;
				}
				interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
			}

			pInt->InterfaceName = interfaceId;

			PnpAdapterManager_CreatePnpInterface(g_PnpBridge->interfaceMgr, g_PnpBridge->pnpDeviceClientHandle, key, &pInt->Interface, DeviceChangePayload);

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
    PNPBRIDGE_RESULT result;
    LogInfo("Starting Azure PnpBridge\n");

    result = PnpBridge_Initialize();
    if (PNPBRIDGE_OK != result) {
        LogError("PnpBridge_Initialize failed: %d\n", result);
        return result;
    }

    LogInfo("Connected to Azure IoT Hub\n");

    //#define CONFIG_FILE "c:\\data\\test\\dag\\config.json"
    result = PnpBridgeConfig_ReadConfigurationFromFile("config.json");
    if (PNPBRIDGE_OK != result) {
        LogError("Failed to parse configuration. Check if config file is present and ensure its formatted correctly.\n");
        return result;
    }

    // Load all the adapters in interface manifest that implement Azure IoT PnP Interface
    // PnpBridge will call into corresponding adapter when a device is reported by 
    // DiscoveryExtension
    result = PnpAdapterManager_Create(&g_PnpBridge->interfaceMgr);
    if (PNPBRIDGE_OK != result) {
        LogError("PnpAdapterManager_Create failed: %d\n", result);
        return result;
    }

    // Load all the extensions that are capable of discovering devices
    // and reporting back to PnpBridge
    result = DiscoveryAdapterManager_Create(&g_PnpBridge->discoveryMgr);
    if (PNPBRIDGE_OK != result) {
        LogError("DiscoveryAdapterManager_Create failed: %d\n", result);
        return result;
    }

    THREAD_HANDLE workerThreadHandle = NULL;
    if (THREADAPI_OK != ThreadAPI_Create(&workerThreadHandle, PnpBridge_Worker_Thread, NULL)) {
        workerThreadHandle = NULL;
    }
    else {
        ThreadAPI_Join(workerThreadHandle, NULL);
    }

    PnpBridge_Release();

    return PNPBRIDGE_OK;
}

void PnpBridge_Stop() {
    Lock(g_PnpBridge->dispatchLock);
    g_PnpBridge->shuttingDown = true;
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
        printf("\n -- Press Ctrl+C to stop PnpBridge");
    }

    PnpBridge_Main();
}

