#include "azure_c_shared_utility/xlogging.h"

#include "parson.h"

#include "ModbusPnp.h"
#include "ModbusCapability.h"
#include "ModbusConnection\ModbusConnection.h"

static bool ModbusPnP_ContinueReadTasks = false;
static CONDITION_VARIABLE StopPolling;

#pragma region Commands

static void ModbusPnp_SetCommandResponse(DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, const char* responseData, int status)
{
	if (NULL == responseData)
	{
		return;
	}

	size_t responseLen = strlen(responseData);
	memset(pnpClientCommandResponseContext, 0, sizeof(*pnpClientCommandResponseContext));
	pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;

	// Allocate a copy of the response data to return to the invoker.  The PnP layer that invoked PnPSampleEnvironmentalSensor_Blink
	// takes responsibility for freeing this data.
	if ((pnpClientCommandResponseContext->responseData = malloc(responseLen + 1)) == NULL)
	{
		LogError("ModbusPnp: Unable to allocate response data");
		pnpClientCommandResponseContext->status = 500;
	}
	else
	{
		pnpClientCommandResponseContext->responseData = (unsigned char*)responseData;
		//strcpy_s((char*)pnpClientCommandResponseContext->responseData, responseData);
		pnpClientCommandResponseContext->responseDataLen = responseLen;
		pnpClientCommandResponseContext->status = status;
	}
}

const ModbusCommand* ModbusPnp_LookupCommand(SINGLYLINKEDLIST_HANDLE interfaceDefinitions, const char* commandName, int InterfaceId)
{
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);
	const  ModbusInterfaceConfig* interfaceDef;

	for (int i = 0; i < InterfaceId - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

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

void 
ModbusPnp_CommandHandler(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext,
    void* userContextCallback
    )
{
    PMODBUS_DEVICE_CONTEXT modbusDevice = (PMODBUS_DEVICE_CONTEXT) userContextCallback;
	const ModbusCommand* command = ModbusPnp_LookupCommand(modbusDevice->InterfaceDefinitions, dtClientCommandContext->commandName, 0);
	//byte* input = (byte*)data;

	if (command == NULL)
	{
        return; // -1;
	}

	byte resultedData[MODBUS_RESPONSE_MAX_LENGTH];
	memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

	CapabilityContext* capContext = calloc(1, sizeof(CapabilityContext));
	capContext->capability = (ModbusCommand*)command;
	capContext->hDevice = modbusDevice->hDevice;
	capContext->connectionType = modbusDevice->DeviceConfig->ConnectionType;
	capContext->hLock = modbusDevice->hConnectionLock;

	int resultLength = ModbusPnp_WriteToCapability(capContext, Command, (char*)dtClientCommandContext->requestData, resultedData);

	if (0 < resultLength) 
	{
        dtClientCommandResponseContext->responseData = calloc(resultLength + 1, sizeof(char));
		memcpy_s((void*)dtClientCommandResponseContext->responseData, resultLength, resultedData, resultLength);
        dtClientCommandResponseContext->responseDataLen = resultLength;
	}

	free(capContext);
    return; // resultLength;
}
#pragma endregion

#pragma region ReadWriteProperty

const ModbusProperty* ModbusPnp_LookupProperty(SINGLYLINKEDLIST_HANDLE interfaceDefinitions, const char* propertyName, int InterfaceId)
{
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);
	const ModbusInterfaceConfig* interfaceDef;

	for (int i = 0; i < InterfaceId - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

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

int ModbusPnp_PropertyHandler(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback)
    //PMODBUS_DEVICE_CONTEXT modbusDevice, const char* propertyName, char* data, char** response)
{
    PMODBUS_DEVICE_CONTEXT modbusDevice = (PMODBUS_DEVICE_CONTEXT)userContextCallback;
	const ModbusProperty* property = ModbusPnp_LookupProperty(modbusDevice->InterfaceDefinitions, dtClientPropertyUpdate->propertyName, 0);

	if (property == NULL) {
		return -1;
	}

	int resultLength = -1;
	byte resultedData[MODBUS_RESPONSE_MAX_LENGTH];
	memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

	CapabilityContext* capContext = calloc(1, sizeof(CapabilityContext));
	capContext->capability = (ModbusProperty*) property;
	capContext->hDevice = modbusDevice->hDevice;
	capContext->connectionType = modbusDevice->DeviceConfig->ConnectionType;
	capContext->hLock= modbusDevice->hConnectionLock;

	resultLength = ModbusPnp_WriteToCapability(capContext, Property, (char*)dtClientPropertyUpdate->propertyDesired, resultedData);

	if (resultLength > 0) {
		//*response = calloc(resultLength + 1, sizeof(char));		//free in ModbusPnp_PropertyUpdateHandler
		//memcpy_s(*response, resultLength, resultedData, resultLength);
	}

	free(capContext);
	return resultLength;
}
#pragma endregion

#pragma region ReadOnlyProperty

void ModbusPnP_ReportPropertyUpdatedCallback(DIGITALTWIN_CLIENT_RESULT pnpReportedStatus, void* userContextCallback)
{
	LogInfo("ModbusPnP_ReportPropertyUpdatedCallback called, result=%d, userContextCallback=%p", pnpReportedStatus, userContextCallback);
}

int ModbusPnP_ReportReadOnlyProperty(DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterface, char* propertyName, char* data)
{
	int result;
    DIGITALTWIN_CLIENT_RESULT pnpClientResult;

	if (pnpInterface == NULL) {
		return 0;
	}

	if ((pnpClientResult = DigitalTwin_InterfaceClient_ReportPropertyAsync(pnpInterface, (const char*) propertyName, (const char*)data, NULL, ModbusPnP_ReportPropertyUpdatedCallback, (void*)propertyName)) != DIGITALTWIN_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_ReportReadOnlyPropertyStatusAsync failed, result=%d\n", pnpClientResult);
        result = -1;// __FAILURE__;
	}
	else
	{
		result = 0;
	}

	return result;
}

int ModbusPnp_PollingSingleProperty(CapabilityContext *context)
{
	ModbusProperty* property = context->capability;
	LogInfo("Start polling task for property \"%s\".", property->Name);
	byte resultedData[MODBUS_RESPONSE_MAX_LENGTH];
	memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

	SRWLOCK lock;
	AcquireSRWLockShared(&lock);
	
	while (ModbusPnP_ContinueReadTasks)
	{
		int resultLen = ModbusPnp_ReadCapability(context, Property, resultedData);
		if (resultLen > 0) {
			ModbusPnP_ReportReadOnlyProperty(PnpAdapterInterface_GetPnpInterfaceClient(property->InterfaceClient), (char*)property->Name, (char*)resultedData);
		}

		SleepConditionVariableSRW(&StopPolling, &lock, property->DefaultFrequency, CONDITION_VARIABLE_LOCKMODE_SHARED);
	}

	ReleaseSRWLockShared(&lock);

	LogInfo("Stopped polling task for property \"%s\".", property->Name);
	free(context);
	ThreadAPI_Exit(THREADAPI_OK);
	return 0;
}

#pragma endregion

#pragma region SendTelemetry

void ModbusPnp_ReportTelemetryCallback(DIGITALTWIN_CLIENT_RESULT pnpSendEventStatus, void* userContextCallback)
{
	LogInfo("ModbusPnp_ReportTelemetryCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int ModbusPnp_ReportTelemetry(DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data)
{
	int result;
    DIGITALTWIN_CLIENT_RESULT pnpClientResult;

	if (pnpInterface == NULL) {
		return 0;
	}

	if ((pnpClientResult = DigitalTwin_InterfaceClient_SendTelemetryAsync(pnpInterface, eventName, (const char*)data, ModbusPnp_ReportTelemetryCallback, NULL)) != DIGITALTWIN_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_SendTelemetryAsync failed, result=%d\n", pnpClientResult);
        result = -1;// __FAILURE__;
	}
	else
	{
		result = 0;
	}

	return result;
}

int ModbusPnp_PollingSingleTelemetry(CapabilityContext *context)
{
	ModbusTelemetry* telemetry = context->capability;
	LogInfo("Start polling task for telemetry \"%s\".", telemetry->Name);
	byte resultedData[MODBUS_RESPONSE_MAX_LENGTH];

	SRWLOCK lock;
	AcquireSRWLockShared(&lock);
	while (ModbusPnP_ContinueReadTasks)
	{
		memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);
		int resultLen = ModbusPnp_ReadCapability(context, Telemetry, resultedData);
		if (resultLen > 0) {
			ModbusPnp_ReportTelemetry(PnpAdapterInterface_GetPnpInterfaceClient(telemetry->InterfaceClient), (char*)telemetry->Name, (char*)resultedData);
		}

		SleepConditionVariableSRW(&StopPolling, &lock, telemetry->DefaultFrequency, CONDITION_VARIABLE_LOCKMODE_SHARED);
	}
	ReleaseSRWLockShared(&lock);

	LogInfo("Stopped polling task for telemetry \"%s\".", telemetry->Name);
	free(context);
	ThreadAPI_Exit(THREADAPI_OK);
	return 0;
}

#pragma endregion

void StopPollingTasks()
{
	ModbusPnP_ContinueReadTasks = false;
	WakeAllConditionVariable(&StopPolling);;
}

int ModbusPnp_StartPollingAllTelemetryProperty(void* context)
{
	PMODBUS_DEVICE_CONTEXT deviceContext = (PMODBUS_DEVICE_CONTEXT)context;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	const ModbusInterfaceConfig* interfaceConfig = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE telemetryList = interfaceConfig->Events;
	int telemetryCount = ModbusPnp_GetListCount(interfaceConfig->Events);
	LIST_ITEM_HANDLE telemetryItemHandle = NULL;

	SINGLYLINKEDLIST_HANDLE propertyList = interfaceConfig->Properties;
	int propertyCount = ModbusPnp_GetListCount(interfaceConfig->Properties);
	LIST_ITEM_HANDLE propertyItemHandle = NULL;


	//Initialize the polling tasks for telemetry and properties
	deviceContext->PollingTasks = NULL;
	if (telemetryCount > 0 || propertyCount > 0)
	{
		deviceContext->PollingTasks = calloc(telemetryCount + propertyCount, sizeof(THREAD_HANDLE));
		if (NULL == deviceContext->PollingTasks) {
			return -1;
		}
	}

	InitializeConditionVariable(&StopPolling);
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
		pollingPayload->hDevice = deviceContext->hDevice;
		pollingPayload->capability = (void*)telemetry;
		pollingPayload->hLock = deviceContext->hConnectionLock;
		pollingPayload->connectionType = deviceContext->DeviceConfig->ConnectionType;

		if (ThreadAPI_Create(&(deviceContext->PollingTasks[i]), ModbusPnp_PollingSingleTelemetry, (void*)pollingPayload) != THREADAPI_OK)
		{
			LogError("Failed to create worker thread for telemetry \"%s\", 0x%x", telemetry->Name, GetLastError());
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
		pollingPayload->hDevice = deviceContext->hDevice;
		pollingPayload->capability = (void*)property;
		pollingPayload->hLock = deviceContext->hConnectionLock;
		pollingPayload->connectionType = deviceContext->DeviceConfig->ConnectionType;

		if (ThreadAPI_Create(&(deviceContext->PollingTasks[telemetryCount + i]), ModbusPnp_PollingSingleProperty, (void*)pollingPayload) != THREADAPI_OK)
		{
			LogError("Failed to create worker thread for proerty \"%s\", 0x%x", property->Name, GetLastError());
		}
	}

	return 0;
}
