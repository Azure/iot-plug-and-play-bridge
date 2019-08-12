// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"
#include "configuration_parser.h"
#include "discoveryadapter_manager.h"
#include "pnpadapter_api.h"
#include "pnpadapter_manager.h"
#include "iothub_comms.h"

#include "pnpbridgeh.h"
#include <iothub_client.h>

//TODO:
// 2. Try interface teardown for incremental publishing
// 3. Update the adapters
// 4. Take the DCM name from config
// 5. Add sequence diagram
// 6. Take config file as exe input parameter

// Globals PNP bridge instance
PPNP_BRIDGE g_PnpBridge = NULL;
PNP_BRIDGE_STATE g_PnpBridgeState = PNP_BRIDGE_UNINITIALIZED;

PNPBRIDGE_RESULT 
PnpBridge_Initialize(
    PPNP_BRIDGE* PnpBridge
    ) 
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    PPNP_BRIDGE pbridge = NULL;
    JSON_Value* config = NULL;

    TRY {
        g_PnpBridgeState = PNP_BRIDGE_UNINITIALIZED;

        // Allocate memory for the PNP_BRIDGE structure
        pbridge = (PPNP_BRIDGE) calloc(1, sizeof(PNP_BRIDGE));
        if (NULL == pbridge) {
            LogError("Failed to allocate memory for PnpBridge global");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        pbridge->ExitCondition = Condition_Init();
        if (NULL == pbridge->ExitCondition) {
            LogError("Failed to init ExitCondition");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        pbridge->ExitLock = Lock_Init();
        if (NULL == pbridge->ExitLock) {
            LogError("Failed to init ExitLock lock");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }
        Lock(pbridge->ExitLock);

        // Get the JSON VALUE of configuration file
        result = PnpBridgeConfig_GetJsonValueFromConfigFile("config.json", &config);
        if (PNPBRIDGE_OK != result) {
            LogError("Failed to retrieve the configuration.");
            LEAVE;
        }

        // Check if config file has REQUIRED parameters
        result = PnpBridgeConfig_RetrieveConfiguration(config, &pbridge->Configuration);
        if (PNPBRIDGE_OK != result) {
            LogError("Config file is invalid");
            LEAVE;
        }

        // Connect to Iot Hub and create a PnP device client handle
        {
            result = IotComms_InitializeIotHandle(&pbridge->IotHandle, pbridge->Configuration.TraceOn, pbridge->Configuration.ConnParams);
            if (PNPBRIDGE_OK != result) {
                LogError("IotComms_InitializeIotHandle failed\n");
                result = PNPBRIDGE_FAILED;
                LEAVE;
            }
        }

        PnpMessageQueue_Create(&pbridge->MessageQueue);

        *PnpBridge = pbridge;

        g_PnpBridgeState = PNP_BRIDGE_INITIALIZED;
    }
    FINALLY
    {
        if (PNPBRIDGE_OK != result) {
            PnpBridge_Release(pbridge);
            *PnpBridge = NULL;
        }
    }
   
    return result;
}

void 
PnpBridge_Release(
    _In_ PPNP_BRIDGE pnpBridge
    )
{
    LogInfo("Cleaning DeviceAggregator resources");

    assert((PNP_BRIDGE_UNINITIALIZED == g_PnpBridgeState) ||
           (PNP_BRIDGE_INITIALIZED == g_PnpBridgeState) ||
           (PNP_BRIDGE_TEARING_DOWN == g_PnpBridgeState));

    g_PnpBridgeState = PNP_BRIDGE_DESTROYED;

    // The order of resource release is important here
    // 1. First release discovery adapters so that no new PNPMESSAGE is sent
    // 2. Drain the message queue and release it
    // 3. Release the pnp adapter resources

    // Stop Disovery Modules
    if (pnpBridge->DiscoveryMgr) {
        DiscoveryAdapterManager_Release(pnpBridge->DiscoveryMgr);
        pnpBridge->DiscoveryMgr = NULL;
    }

    // Release PNPMESSAGE queue
    if (pnpBridge->MessageQueue) {
        PnpMessageQueue_Release(pnpBridge->MessageQueue);
        pnpBridge->MessageQueue = NULL;
    }

    // Stop Pnp Modules
    if (pnpBridge->PnpMgr) {
        PnpAdapterManager_Release(pnpBridge->PnpMgr);
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
PnpBridge_Worker(
    _In_ void* ThreadArgument
    )
{
    AZURE_UNREFERENCED_PARAMETER(ThreadArgument);

    PPNP_BRIDGE pnpBridge = (PPNP_BRIDGE)ThreadArgument;

    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    // Publish persistent Pnp Interfaces
    /* result = DiscoveryAdapterManager_PublishAlwaysInterfaces(pnpBridge->discoveryMgr, pnpBridge->Configuration.JsonConfig);
    if (!PNPBRIDGE_SUCCESS(result)) {
        goto exit;
    }*/

    // Start Device Discovery
    result = DiscoveryAdapterManager_Start(pnpBridge->DiscoveryMgr, &pnpBridge->Configuration);
    if (PNPBRIDGE_OK != result) {
        goto exit;
    }

exit:
    return result;
}

PNPBRIDGE_RESULT 
PnpBridge_ValidateDeviceChangePayload(
    PPNPBRIDGE_CHANGE_PAYLOAD DeviceChangePayload
    )
{
    if (NULL == DeviceChangePayload->Message) {
        LogError("Device change payload message is NULL");
        return PNPBRIDGE_INVALID_ARGS;
    }

    if (0 == DeviceChangePayload->MessageLength) {
        LogError("Device change payload MessageLength is zero");
        return PNPBRIDGE_INVALID_ARGS;
    }

    return PNPBRIDGE_OK;
}

int 
PnpBridge_ProcessPnpMessage(
    PNPMESSAGE PnpMessage
    )
{
    PPNP_BRIDGE pnpBridge = g_PnpBridge;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    bool containsFilter = false;
    int key = 0;
    JSON_Value* jmsg = NULL;
    PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;

    msg = (PPNPBRIDGE_CHANGE_PAYLOAD) PnpMemory_GetBuffer(PnpMessage, NULL);

    TRY {
        result = PnpBridge_ValidateDeviceChangePayload(msg);
        if (PNPBRIDGE_OK != result) {
            LogInfo("Change payload is invalid");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        JSON_Object* jobj;
        JSON_Object* jdevice;

        jmsg = json_parse_string(msg->Message);
        if (NULL == jmsg) {
            LogError("PNPMESSAGE payload is not a valid json");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        jobj = json_value_get_object(jmsg);
        if (NULL == jobj) {
            LogError("PNPMESSAGE payload is not a valid json");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        if (PNPBRIDGE_OK != Configuration_IsDeviceConfigured(pnpBridge->Configuration.JsonConfig, jobj, &jdevice)) {
            LogInfo("Device is not configured. Dropping the change notification");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        result = PnpAdapterManager_SupportsIdentity(pnpBridge->PnpMgr, jdevice, &containsFilter, &key);
        if (PNPBRIDGE_OK != result) {
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        if (!containsFilter) {
            LogError("Dropping device change callback");
            result = PNPBRIDGE_INVALID_ARGS;
            LEAVE;
        }

        // Create an Pnp interface
        char* interfaceId = NULL;
        char* componentName = NULL;
        char* selfDescribing = (char*)json_object_get_string(jdevice, PNP_CONFIG_SELF_DESCRIBING);
        if (NULL != selfDescribing && 0 == strcmp(selfDescribing, "true")) {
            interfaceId = msg->InterfaceId;
            if (NULL == interfaceId) {
                LogError("Interface id is missing for self descrbing device");
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }
        }
        else {
            interfaceId = (char*)json_object_get_string(jdevice, PNP_CONFIG_INTERFACE_ID);
            if (NULL == interfaceId) {
                LogError("Interface Id is missing in config for the device");
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            componentName = (char*)json_object_get_string(jdevice, PNP_CONFIG_COMPONENT_NAME);
            if (NULL == interfaceId) {
                LogError("Interface Id is missing in config for the device");
                result = PNPBRIDGE_INVALID_ARGS;
                LEAVE;
            }

            // Add interfaceId to the message
            PnpMessage_SetInterfaceId(PnpMessage, interfaceId);

            // Add the component name
            mallocAndStrcpy_s(&PnpMessage_AccessProperties(PnpMessage)->ComponentName, componentName);
        }

        if (PnpAdapterManager_IsInterfaceIdPublished(pnpBridge->PnpMgr, interfaceId)) {
            LogError("PnP Interface has already been published. Dropping the change notification. \n");
            result = PNPBRIDGE_FAILED;
            LEAVE;
        }

        result = PnpAdapterManager_CreatePnpInterface(pnpBridge->PnpMgr, &pnpBridge->IotHandle, key, jdevice, PnpMessage);
        if (!PNPBRIDGE_SUCCESS(result)) {
            LEAVE;
        }

        //// Query all the pnp interface clients and publish them
        //DiscoveryAdapter_PublishInterfaces();

    } FINALLY {
        if (NULL != jmsg) {
            json_value_free(jmsg);
        }
    }

    return result;
}

int
DiscoveryAdapter_PublishInterfaces() {
    // Query all the pnp interface clients and publish them
    PPNP_BRIDGE pnpBridge = g_PnpBridge;
    int interfaceCount = 0;
    PNPBRIDGE_RESULT result;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* interfaces = NULL;
    PnpAdapterManager_GetAllInterfaces(pnpBridge->PnpMgr, &interfaces, &interfaceCount);

    LogInfo("Publishing Azure Pnp Interface, count %d", interfaceCount);

    result = IotComms_RegisterPnPInterfaces(&pnpBridge->IotHandle,
                                            pnpBridge->Configuration.ConnParams->DeviceCapabilityModelUri,
                                            interfaces,
                                            interfaceCount,
                                            pnpBridge->Configuration.TraceOn,
                                            pnpBridge->Configuration.ConnParams);

    if (result != PNPBRIDGE_OK) {
        goto end;
    }

    // Notify interfaces of successful publish
    PnpAdapterManager_InvokeStartInterface(pnpBridge->PnpMgr);

end:
    free(interfaces);
    return result;
}

int
PnpBridge_ReinitPnpAdapters()
{
    PPNP_BRIDGE pnpBridge = g_PnpBridge;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    if (pnpBridge->PnpMgr) {
        PnpAdapterManager_Release(pnpBridge->PnpMgr);
        pnpBridge->PnpMgr = NULL;
    }

    // Load all the adapters in interface manifest that implement Azure IoT PnP Interface
    // PnpBridge will call into corresponding adapter when a device is reported by 
    // DiscoveryExtension
    result = PnpAdapterManager_Create(&pnpBridge->PnpMgr, pnpBridge->Configuration.JsonConfig);
    if (PNPBRIDGE_OK != result) {
        LogError("PnpAdapterManager_Create failed: %d", result);
    }

    return 0;
}

int 
DiscoveryAdapter_ReportDevice(
    _In_ PNPMESSAGE PnpMessage
    )
{
    // Ensure that PnpBridge is initialized
    if (PNP_BRIDGE_INITIALIZED != g_PnpBridgeState) {
        LogInfo("PnpBridge is shutting down. Dropping the change notification");
        return -1;
    }

    PMESSAGE_QUEUE queue = g_PnpBridge->MessageQueue;

    // Add it to PnpMessagesList
    PnpMesssageQueue_Add(queue, PnpMessage);

    // Notify the worker
    Lock(queue->WaitConditionLock);
    Condition_Post(queue->WaitCondition);
    Unlock(queue->WaitConditionLock);

    return 0;
}

int
PnpBridge_Main()
{
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;
    PPNP_BRIDGE pnpBridge = NULL;

    TRY {
        LogInfo("Starting Azure PnpBridge");

        result = PnpBridge_Initialize(&pnpBridge);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpBridge_Initialize failed: %d", result);
            LEAVE;
        }

        g_PnpBridge = pnpBridge;

        LogInfo("Connected to Azure IoT Hub");

        // Load all the adapters in interface manifest that implement Azure IoT PnP Interface
        // PnpBridge will call into corresponding adapter when a device is reported by 
        // DiscoveryExtension
        result = PnpAdapterManager_Create(&pnpBridge->PnpMgr, pnpBridge->Configuration.JsonConfig);
        if (PNPBRIDGE_OK != result) {
            LogError("PnpAdapterManager_Create failed: %d", result);
            LEAVE;
        }

        // Load all the extensions that are capable of discovering devices
        // and reporting back to PnpBridge
        result = DiscoveryAdapterManager_Create(&pnpBridge->DiscoveryMgr);
        if (PNPBRIDGE_OK != result) {
            LogError("DiscoveryAdapterManager_Create failed: %d", result);
            LEAVE;
        }

        PnpBridge_Worker(pnpBridge);

        // Prevent main thread from returning by waiting for the
        // exit condition to be set. This condition will be set when
        // the bridge has received a stop signal
        // ExitLock was taken in call to PnpBridge_Initialize so does not need to be reacquired.
        Condition_Wait(pnpBridge->ExitCondition, pnpBridge->ExitLock, 0);
        Unlock(pnpBridge->ExitLock);
    } FINALLY  {
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
    assert(g_PnpBridge != NULL);
    assert(g_PnpBridgeState == PNP_BRIDGE_INITIALIZED);

    if (PNP_BRIDGE_TEARING_DOWN != g_PnpBridgeState) {
        g_PnpBridgeState = PNP_BRIDGE_TEARING_DOWN;
        Lock(g_PnpBridge->ExitLock);
        Condition_Post(g_PnpBridge->ExitCondition);
        Unlock(g_PnpBridge->ExitLock);
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
        return PNPBRIDGE_FAILED;
    }

    handle = g_PnpBridge->IotHandle.IsModule ? g_PnpBridge->IotHandle.u1.IotModule.moduleHandle :
                                               g_PnpBridge->IotHandle.u1.IotDevice.deviceHandle;

    if (NULL == pszDestination || (NULL == pbData && cbData > 0) ||
        (NULL != pbData && cbData == 0) ||
        NULL == iotHubClientFileUploadCallback)
    {
        return PNPBRIDGE_INVALID_ARGS;
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
        return PNPBRIDGE_OK;
        break;
    case IOTHUB_CLIENT_INVALID_ARG:
    case IOTHUB_CLIENT_INVALID_SIZE:
        return PNPBRIDGE_INVALID_ARGS;
        break;
    case IOTHUB_CLIENT_INDEFINITE_TIME:
    case IOTHUB_CLIENT_ERROR:
    default:
        return PNPBRIDGE_FAILED;
        break;
    }
}