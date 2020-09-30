// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "azure_c_shared_utility/xlogging.h"

#include "parson.h"

#include "ModbusPnp.h"
#include "ModbusCapability.h"
#include "ModbusConnection/ModbusConnection.h"
#include "azure_c_shared_utility/condition.h"

static bool ModbusPnP_ContinueReadTasks = false;
// This condition variable is used to signal when to stop polling
// and only serves to establish the event notification mechanism using 
// azure_c's shared utility library. The lock passed into the Condition_Wait
// function does not protect against any shared resource, and is instead created
// to comply with the cross-platform library's function signature

static COND_HANDLE StopPolling;

#pragma region Commands

const ModbusCommand* ModbusPnp_LookupCommand(
    PModbusInterfaceConfig interfaceDefinitions,
    const char* commandName)
{
    const  ModbusInterfaceConfig* interfaceDef = (PModbusInterfaceConfig)(interfaceDefinitions);

    SINGLYLINKEDLIST_HANDLE commandList= interfaceDef->Commands;
    LIST_ITEM_HANDLE commandItemHandle = singlylinkedlist_get_head_item(commandList);
    const ModbusCommand* command = NULL;
    while (commandItemHandle != NULL) {
        command = singlylinkedlist_item_get_value(commandItemHandle);
        if (strcmp(command->Name, commandName) == 0) {
            return command;
        }
        commandItemHandle = singlylinkedlist_get_next_item(commandItemHandle);
    }
    return NULL;
}

int 
ModbusPnp_CommandHandler(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    PMODBUS_DEVICE_CONTEXT modbusDevice = PnpComponentHandleGetContext(PnpComponentHandle);

    const ModbusCommand* command = ModbusPnp_LookupCommand(modbusDevice->InterfaceConfig, CommandName);

    if (command == NULL)
    {
        return PNP_STATUS_NOT_FOUND;
    }

    uint8_t resultedData[MODBUS_RESPONSE_MAX_LENGTH];
    memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

    CapabilityContext* capContext = calloc(1, sizeof(CapabilityContext));
    if (NULL == capContext)
    {
        LogError("Modbus Adapter: Could not allocate memory for modbus capability context.");
        return PNP_STATUS_INTERNAL_ERROR;
    }

    capContext->capability = (ModbusCommand*)command;
    capContext->hDevice = modbusDevice->hDevice;
    capContext->connectionType = modbusDevice->DeviceConfig->ConnectionType;
    capContext->hLock = modbusDevice->hConnectionLock;
    capContext->componentName = modbusDevice->ComponentName;

    char * CommandValueString = (char*) json_value_get_string(CommandValue);
    if ( NULL == CommandValueString)
    {
        LogError("Modbus Adapter: Command Value String is invalid.");
        return PNP_STATUS_INTERNAL_ERROR;
    }

    int resultLength = ModbusPnp_WriteToCapability(capContext, Command, CommandValueString, resultedData);

    if (0 < resultLength) 
    {
        *CommandResponseSize = strlen((char*)resultedData);
        memset(CommandResponse, 0, sizeof(*CommandResponse));

        // Allocate a copy of the response data to return to the invoker. Caller will free this.
        if (mallocAndStrcpy_s((char**)CommandResponse, (char*)resultedData) != 0)
        {
            LogError("Modbus Adapter: Unable to allocate response data");
            free(capContext);
            return PNP_STATUS_INTERNAL_ERROR;
        }
    }

    free(capContext);
    return PNP_STATUS_SUCCESS;
}
#pragma endregion

#pragma region ReadWriteProperty

const ModbusProperty* ModbusPnp_LookupProperty(
    PModbusInterfaceConfig interfaceDefinitions,
    const char* propertyName)
{
    const ModbusInterfaceConfig* interfaceDef = (PModbusInterfaceConfig)(interfaceDefinitions);
    SINGLYLINKEDLIST_HANDLE propertyList = interfaceDef->Properties;
    LIST_ITEM_HANDLE propertyItemHandle = singlylinkedlist_get_head_item(propertyList);
    const ModbusProperty* property;
    while (propertyItemHandle != NULL) {
        property = singlylinkedlist_item_get_value(propertyItemHandle);
        if (strcmp(property->Name, propertyName) == 0) {
            return property;
        }
        propertyItemHandle = singlylinkedlist_get_next_item(propertyItemHandle);
    }

    return NULL;
}

void ModbusPnp_PropertyHandler(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    void* userContextCallback)
{
    UNREFERENCED_PARAMETER(version);
    PMODBUS_DEVICE_CONTEXT modbusDevice = PnpComponentHandleGetContext(PnpComponentHandle);
    IOTHUB_DEVICE_CLIENT_HANDLE deviceClient = (IOTHUB_DEVICE_CLIENT_HANDLE)userContextCallback;

    const ModbusProperty* property = ModbusPnp_LookupProperty(modbusDevice->InterfaceConfig, PropertyName);

    if (property == NULL) {
        return;
    }

    char * PropertyValueString = (char*) json_value_get_string(PropertyValue);

    uint8_t resultedData[MODBUS_RESPONSE_MAX_LENGTH];
    memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

    CapabilityContext* capContext = calloc(1, sizeof(CapabilityContext));
    if (!capContext)
    {
        LogError("Could not allocate memory for capability context in property handler.");
        return;
    }
    capContext->capability = (ModbusProperty*) property;
    capContext->hDevice = modbusDevice->hDevice;
    capContext->connectionType = modbusDevice->DeviceConfig->ConnectionType;
    capContext->hLock= modbusDevice->hConnectionLock;
    capContext->deviceClient = deviceClient;
    capContext->componentName = modbusDevice->ComponentName;

    if (PropertyValueString)
    {
        ModbusPnp_WriteToCapability(capContext, Property, PropertyValueString, resultedData);
    }

    free(capContext);
}
#pragma endregion

#pragma region ReadOnlyProperty

void ModbusPnp_ReportPropertyUpdatedCallback(
    int pnpReportedStatus,
    void* userContextCallback)
{
    LogInfo("ModbusPnp_ReportPropertyUpdatedCallback called, result=%d, property name=%s", pnpReportedStatus, (const char*) userContextCallback);
}

IOTHUB_CLIENT_RESULT 
ModbusPnp_ReportReadOnlyProperty(
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
    const char * ComponentName,
    const char* PropertyName,
    const char* PropertyValue)
{
    IOTHUB_CLIENT_RESULT iothubClientResult = IOTHUB_CLIENT_OK;

    if (DeviceClient == NULL) {
        return iothubClientResult;
    }

    STRING_HANDLE jsonToSend = NULL;

    if ((jsonToSend = PnP_CreateReportedProperty(ComponentName, PropertyName, PropertyValue)) == NULL)
    {
        LogError("Unable to build reported property response for propertyName=%s, propertyValue=%s", PropertyName, PropertyValue);
    }
    else
    {
        const char* jsonToSendStr = STRING_c_str(jsonToSend);
        size_t jsonToSendStrLen = strlen(jsonToSendStr);

        if ((iothubClientResult = IoTHubDeviceClient_SendReportedState(DeviceClient, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
            ModbusPnp_ReportPropertyUpdatedCallback, (void*)PropertyName)) != IOTHUB_CLIENT_OK)
        {
            LogError("Modbus Adapter: Unable to send reported state for property=%s, error=%d",
                                PropertyName, iothubClientResult);
        }
        else
        {
            LogInfo("Modbus Adapter: Sending device information property to IoTHub. propertyName=%s, propertyValue=%s",
                        PropertyName, PropertyValue);
        }

        STRING_delete(jsonToSend);
    }

    return iothubClientResult;
}

int
ModbusPnp_PollingSingleProperty(
    void *param)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    CapabilityContext* context = param;
    ModbusProperty* property = context->capability;
    LogInfo("Start polling task for property \"%s\".", property->Name);
    uint8_t resultedData[MODBUS_RESPONSE_MAX_LENGTH];
    memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

    LOCK_HANDLE lock;
    lock = Lock_Init();
    Lock(lock);

    while (ModbusPnP_ContinueReadTasks)
    {
        int resultLen = ModbusPnp_ReadCapability(context, Property, resultedData);
        if (resultLen > 0)
        {
            result = ModbusPnp_ReportReadOnlyProperty(context->deviceClient, context->componentName, property->Name, (const char*) resultedData);
        }

        Condition_Wait(StopPolling, lock, property->DefaultFrequency);
    }

    Unlock(lock);
    Lock_Deinit(lock);
    LogInfo("Stopped polling task for property \"%s\".", property->Name);
    free(context);
    ThreadAPI_Exit(THREADAPI_OK);
    return result;
}

#pragma endregion

#pragma region SendTelemetry

void ModbusPnp_ReportTelemetryCallback(
    IOTHUB_CLIENT_CONFIRMATION_RESULT pnpSendEventStatus,
    void* userContextCallback)
{
    LogInfo("ModbusPnp_ReportTelemetryCallback called, result=%d, telemetry name=%s", pnpSendEventStatus, (const char*) userContextCallback);
}

IOTHUB_CLIENT_RESULT
ModbusPnp_ReportTelemetry(
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient,
    const char * ComponentName,
    const char* TelemetryName,
    const char* TelemetryValue)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

    if (DeviceClient == NULL) {
        return result;
    }

    char telemetryMessageData[512] = {0};
    sprintf(telemetryMessageData, "{\"%s\":%s}", TelemetryName, TelemetryValue);

    if ((messageHandle = PnP_CreateTelemetryMessageHandle(ComponentName, (const char*) telemetryMessageData)) == NULL)
    {
        LogError("Modbus Adapter: PnP_CreateTelemetryMessageHandle failed.");
    }
    else if ((result = IoTHubDeviceClient_SendEventAsync(DeviceClient, messageHandle,
            ModbusPnp_ReportTelemetryCallback, (void*) TelemetryName)) != IOTHUB_CLIENT_OK)
    {
        LogError("Modbus Adapter: IoTHubDeviceClient_SendEventAsync failed, error=%d", result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return result;
}

int ModbusPnp_PollingSingleTelemetry(
    void *param)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    CapabilityContext* context = param;
    ModbusTelemetry* telemetry = context->capability;
    LogInfo("Start polling task for telemetry \"%s\".", telemetry->Name);
    uint8_t resultedData[MODBUS_RESPONSE_MAX_LENGTH];

    LOCK_HANDLE lock;
    lock = Lock_Init();
    Lock(lock);
    while (ModbusPnP_ContinueReadTasks)
    {
        memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);
        int resultLen = ModbusPnp_ReadCapability(context, Telemetry, resultedData);

        if (resultLen > 0) {
            result = ModbusPnp_ReportTelemetry(context->deviceClient, (const char*) context->componentName,
                (const char*) telemetry->Name, (const char*) resultedData);
        }

        Condition_Wait(StopPolling, lock, telemetry->DefaultFrequency);
    }
    Unlock(lock);
    Lock_Deinit(lock);

    LogInfo("Stopped polling task for telemetry \"%s\".", telemetry->Name);
    free(context);
    ThreadAPI_Exit(THREADAPI_OK);
    return result;
}

#pragma endregion

void StopPollingTasks()
{
    ModbusPnP_ContinueReadTasks = false;
    if (Condition_Post(StopPolling) != COND_OK)
    {
        LogError("Condition variable could not be signalled.");
    }
}

IOTHUB_CLIENT_RESULT ModbusPnp_StartPollingAllTelemetryProperty(
    void* context)
{
    PMODBUS_DEVICE_CONTEXT deviceContext = (PMODBUS_DEVICE_CONTEXT)context;
    const ModbusInterfaceConfig* interfaceConfig = deviceContext->InterfaceConfig;
    SINGLYLINKEDLIST_HANDLE telemetryList = interfaceConfig->Events;
    int telemetryCount = ModbusPnp_GetListCount(interfaceConfig->Events);
    LIST_ITEM_HANDLE telemetryItemHandle = NULL;

    SINGLYLINKEDLIST_HANDLE propertyList = interfaceConfig->Properties;
    int propertyCount = ModbusPnp_GetListCount(interfaceConfig->Properties);
    LIST_ITEM_HANDLE propertyItemHandle = NULL;


    // Initialize the polling tasks for telemetry and properties
    deviceContext->PollingTasks = NULL;
    if (telemetryCount > 0 || propertyCount > 0)
    {
        deviceContext->PollingTasks = calloc((telemetryCount + propertyCount), sizeof(THREAD_HANDLE));
        if (NULL == deviceContext->PollingTasks) {
            return IOTHUB_CLIENT_ERROR;
        }
    }

    StopPolling = Condition_Init();
    ModbusPnP_ContinueReadTasks = true;

    for (int i = 0; i < telemetryCount; i++)
    {
        if (NULL == telemetryItemHandle)
        {
            telemetryItemHandle = singlylinkedlist_get_head_item(telemetryList);
        }
        else
        {
            telemetryItemHandle = singlylinkedlist_get_next_item(telemetryItemHandle);
        }

        const ModbusTelemetry* telemetry = singlylinkedlist_item_get_value(telemetryItemHandle);
        CapabilityContext* pollingPayload = calloc(1, sizeof(CapabilityContext));
        if (!pollingPayload)
        {
            LogError("Could not allocate memory for telemetry polling capability context.");
            continue;
        }
        pollingPayload->hDevice = deviceContext->hDevice;
        pollingPayload->capability = (void*)telemetry;
        pollingPayload->hLock = deviceContext->hConnectionLock;
        pollingPayload->connectionType = deviceContext->DeviceConfig->ConnectionType;
        pollingPayload->deviceClient = deviceContext->DeviceClient;
        pollingPayload->componentName = deviceContext->ComponentName;

        if (ThreadAPI_Create(&(deviceContext->PollingTasks[i]), ModbusPnp_PollingSingleTelemetry, (void*)pollingPayload) != THREADAPI_OK)
        {
#ifdef WIN32
            LogError("Failed to create worker thread for telemetry \"%s\", 0x%x", telemetry->Name, GetLastError());
#else
            LogError("Failed to create worker thread for telemetry \"%s\".", telemetry->Name);
#endif
        }
    }

    for (int i = 0; i < propertyCount; i++)
    {
        if (NULL == propertyItemHandle)
        {
            propertyItemHandle = singlylinkedlist_get_head_item(propertyList);
        }
        else
        {
            propertyItemHandle = singlylinkedlist_get_next_item(propertyItemHandle);
        }

        const ModbusProperty* property = singlylinkedlist_item_get_value(propertyItemHandle);
        CapabilityContext* pollingPayload = calloc(1, sizeof(CapabilityContext));
        if (!pollingPayload)
        {
            LogError("Could not allocate memory for property polling capability context.");
            continue;
        }
        pollingPayload->hDevice = deviceContext->hDevice;
        pollingPayload->capability = (void*)property;
        pollingPayload->hLock = deviceContext->hConnectionLock;
        pollingPayload->connectionType = deviceContext->DeviceConfig->ConnectionType;
        pollingPayload->deviceClient = deviceContext->DeviceClient;
        pollingPayload->componentName = deviceContext->ComponentName;

        if (ThreadAPI_Create(&(deviceContext->PollingTasks[telemetryCount + i]), ModbusPnp_PollingSingleProperty, (void*)pollingPayload) != THREADAPI_OK)
        {
#ifdef WIN32
            LogError("Failed to create worker thread for property \"%s\", 0x%x", property->Name, GetLastError());
#else
            LogError("Failed to create worker thread for property \"%s\".", property->Name);
#endif
        }
    }

    return IOTHUB_CLIENT_OK;
}
