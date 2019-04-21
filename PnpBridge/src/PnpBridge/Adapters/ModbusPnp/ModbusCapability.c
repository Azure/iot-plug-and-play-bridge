#include "azure_c_shared_utility/xlogging.h"

#include "parson.h"

#include "Adapters\ModbusPnp\ModbusPnp.h"
#include "Adapters\ModbusPnp\ModbusCapability.h"
#include "Adapters\ModbusPnp\ModbusConnection\ModbusConnection.h"

static bool ModbusPnP_ContinueReadTasks = false;
static CONDITION_VARIABLE StopPolling;

#pragma region Commands

static void ModbusPnp_SetCommandResponse(PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, const char* responseData, int status)
{
	if (NULL == responseData)
	{
		return;
	}

	size_t responseLen = strlen(responseData);
	memset(pnpClientCommandResponseContext, 0, sizeof(*pnpClientCommandResponseContext));
	pnpClientCommandResponseContext->version = PNP_CLIENT_COMMAND_RESPONSE_VERSION_1;

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

int ModbusPnp_CommandHandler(PMODBUS_DEVICE_CONTEXT modbusDevice, const char* commandName, char* data, char** response)
{
	const ModbusCommand* command = ModbusPnp_LookupCommand(modbusDevice->InterfaceDefinitions, commandName, 0);
	byte* input = (byte*)data;

	if (command == NULL)
	{
		return -1;
	}

	byte resultedData[MODBUS_RESPONSE_MAX_LENGTH];
	memset(resultedData, 0x00, MODBUS_RESPONSE_MAX_LENGTH);

	CapabilityContext* capContext = calloc(1, sizeof(CapabilityContext));
	capContext->capability = (ModbusCommand*)command;
	capContext->hDevice = modbusDevice->hDevice;
	capContext->connectionType = modbusDevice->DeviceConfig->ConnectionType;
	capContext->hLock = modbusDevice->hConnectionLock;

	int resultLength = ModbusPnp_WriteToCapability(capContext, Command, data, resultedData);

	if (0 < resultLength) 
	{
		*response = calloc(resultLength + 1, sizeof(char));
		memcpy_s(*response, resultLength, resultedData, resultLength);
	}

	free(capContext);
	return resultLength;
}

void ModbusPnp_CommandUpdateHandler(const char* commandName, const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext, PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, void* userContextCallback) {
	PMODBUS_DEVICE_CONTEXT deviceContext;

	// LogInfo("Processed command frequency property updated.  propertyUpdated = %.*s", (int)propertyDataUpdatedLen, propertyDataUpdated);

	deviceContext = (PMODBUS_DEVICE_CONTEXT)userContextCallback;

	char* response = NULL;
	char* requestData = calloc(pnpClientCommandContext->requestDataLen + 1, sizeof(char));
	
	if (NULL != requestData)
	{
		memcpy_s(requestData, pnpClientCommandContext->requestDataLen, pnpClientCommandContext->requestData, pnpClientCommandContext->requestDataLen);

		if (0 < ModbusPnp_CommandHandler(deviceContext, commandName, requestData, &response)) 
		{
			ModbusPnp_SetCommandResponse(pnpClientCommandResponseContext, response, 200);
		}
		free(requestData);
	}
}

void ModbusPnp_CommandUpdateHandlerRedirect(int index,
	const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
	PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
	void* userContextCallback)
{
	PMODBUS_DEVICE_CONTEXT deviceContext = userContextCallback;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	const ModbusInterfaceConfig* interfaceDef;

	for (int i = 0; i < 0 - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE commandList = interfaceDef->Commands;
	LIST_ITEM_HANDLE commandItemHandle = singlylinkedlist_get_head_item(commandList);
	const ModbusCommand* command = NULL;
	for (int i = 0; i < index + 1; i++) {
		command = singlylinkedlist_item_get_value(commandItemHandle);
		commandItemHandle = singlylinkedlist_get_next_item(commandItemHandle);
	}
	if (NULL != command) {
		ModbusPnp_CommandUpdateHandler(command->Name, pnpClientCommandContext, pnpClientCommandResponseContext, userContextCallback);
	}
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

int ModbusPnp_PropertyHandler(PMODBUS_DEVICE_CONTEXT modbusDevice, const char* propertyName, char* data, char** response)
{
	const ModbusProperty* property = ModbusPnp_LookupProperty(modbusDevice->InterfaceDefinitions, propertyName, 0);

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

	resultLength = ModbusPnp_WriteToCapability(capContext, Property, data, resultedData);

	if (resultLength > 0) {
		*response = calloc(resultLength + 1, sizeof(char));		//free in ModbusPnp_PropertyUpdateHandler
		memcpy_s(*response, resultLength, resultedData, resultLength);
	}

	free(capContext);
	return resultLength;
}

static void ModbusPnp_PropertyUpdateHandler(const char* propertyName, unsigned const char* propertyInitial, size_t propertyInitialLen, unsigned const char* propertyDataUpdated, size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback)
{
	PNP_CLIENT_RESULT pnpClientResult;
	PNP_CLIENT_READWRITE_PROPERTY_RESPONSE propertyResponse;
	PMODBUS_DEVICE_CONTEXT deviceContext;

	UNREFERENCED_PARAMETER(propertyInitial);
	UNREFERENCED_PARAMETER(propertyInitialLen);

	LogInfo("Processed property.  propertyUpdated = %.*s", (int)propertyDataUpdatedLen, propertyDataUpdated);

	deviceContext = (PMODBUS_DEVICE_CONTEXT)userContextCallback;

	char* response = NULL;
	int responseLength = ModbusPnp_PropertyHandler(deviceContext, propertyName, (char*)propertyDataUpdated, &response);

	// TODO: Send the correct response code below
	propertyResponse.version = 1;
	if (responseLength < 0)
	{
		propertyResponse.propertyData = propertyDataUpdated;
		propertyResponse.propertyDataLen = propertyDataUpdatedLen;
		propertyResponse.responseVersion = desiredVersion;
		propertyResponse.statusCode = 400;
		propertyResponse.statusDescription = "Property Updated Failed";
	}
	else
	{
		propertyResponse.propertyData = response;
		propertyResponse.propertyDataLen = responseLength;
		propertyResponse.responseVersion = desiredVersion;
		propertyResponse.statusCode = 200;
		propertyResponse.statusDescription = "Property Updated Successfully";
	}

	pnpClientResult = PnP_InterfaceClient_ReportReadWritePropertyStatusAsync(PnpAdapterInterface_GetPnpInterfaceClient(deviceContext->pnpAdapterInterface), propertyName, &propertyResponse, NULL, NULL);

	if (NULL != response)
	{
		free(response);
	}
}

void ModbusPnp_PropertyUpdateHandlerRedirect(int index, unsigned const char* propertyInitial,
	size_t propertyInitialLen, unsigned const char* propertyDataUpdated,
	size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback)
{

	PMODBUS_DEVICE_CONTEXT deviceContext = userContextCallback;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	const ModbusInterfaceConfig* interfaceConfig;

	for (int i = 0; i < 0 - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceConfig = singlylinkedlist_item_get_value(interfaceDefHandle);

	//Get ReadWrite Properties
	SINGLYLINKEDLIST_HANDLE propertyList = interfaceConfig->Properties;
	int propertyCount = ModbusPnp_GetListCount(interfaceConfig->Properties);
	LIST_ITEM_HANDLE propertyItemHandle = singlylinkedlist_get_head_item(propertyList);
	const ModbusProperty* property = NULL;

	int readWriteIndex = 0;

	for (int i = 0; i < propertyCount; i++) {
		property = singlylinkedlist_item_get_value(propertyItemHandle);
		if (READ_WRITE == property->Access)
		{
			if (readWriteIndex == index)
			{

				break;
			}
			else 
			{
				readWriteIndex++;
			}
		}
		propertyItemHandle = singlylinkedlist_get_next_item(propertyItemHandle);
	}
	if (NULL != property) {
		ModbusPnp_PropertyUpdateHandler(property->Name, propertyInitial, propertyInitialLen, propertyDataUpdated, propertyDataUpdatedLen,
			desiredVersion, userContextCallback);
	}
}
#pragma endregion

#pragma region ReadOnlyProperty

void ModbusPnP_ReportPropertyUpdatedCallback(PNP_REPORTED_PROPERTY_STATUS pnpReportedStatus, void* userContextCallback)
{
	LogInfo("ModbusPnP_ReportPropertyUpdatedCallback called, result=%d, userContextCallback=%p", pnpReportedStatus, userContextCallback);
}

int ModbusPnP_ReportReadOnlyProperty(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* propertyName, char* data)
{
	int result;
	PNP_CLIENT_RESULT pnpClientResult;

	if (pnpInterface == NULL) {
		return 0;
	}

	if ((pnpClientResult = PnP_InterfaceClient_ReportReadOnlyPropertyStatusAsync(pnpInterface, propertyName, (const unsigned char*)data, strlen(data), ModbusPnP_ReportPropertyUpdatedCallback, (void*)propertyName)) != PNP_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_ReportReadOnlyPropertyStatusAsync failed, result=%d\n", pnpClientResult);
		result = __FAILURE__;
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
			ModbusPnP_ReportReadOnlyProperty(PnpAdapterInterface_GetPnpInterfaceClient(property->InterfaceClient), (char*)property->Name, resultedData);
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

void ModbusPnp_ReportTelemetryCallback(PNP_SEND_TELEMETRY_STATUS pnpSendEventStatus, void* userContextCallback)
{
	LogInfo("ModbusPnp_ReportTelemetryCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int ModbusPnp_ReportTelemetry(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data)
{
	int result;
	PNP_CLIENT_RESULT pnpClientResult;

	if (pnpInterface == NULL) {
		return 0;
	}

	if ((pnpClientResult = PnP_InterfaceClient_SendTelemetryAsync(pnpInterface, eventName, (const unsigned char*)data, strlen(data), ModbusPnp_ReportTelemetryCallback, NULL)) != PNP_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_SendTelemetryAsync failed, result=%d\n", pnpClientResult);
		result = __FAILURE__;
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
			ModbusPnp_ReportTelemetry(PnpAdapterInterface_GetPnpInterfaceClient(telemetry->InterfaceClient), (char*)telemetry->Name, resultedData);
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
	int result = 0;
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
