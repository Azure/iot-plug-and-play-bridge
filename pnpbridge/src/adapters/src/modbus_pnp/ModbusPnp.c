#include <PnpBridge.h>

#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"

#include "parson.h"

#include "ModbusPnp.h"
#include "ModbusCapability.h"
#include "ModbusConnection/ModbusConnection.h"


const char* modbusDeviceChangeMessageformat = "{ \
                                                 \"identity\": \"modbus-pnp-discovery\" \
                                               }";

#pragma region PnpDiscovery
#pragma endregion

int ModbusPnp_GetListCount(SINGLYLINKEDLIST_HANDLE list) {
	if (NULL == list) {
		return 0;
	}

	LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(list);
	int count = 0;
	while (NULL != item) {
		count++;
		item = singlylinkedlist_get_next_item(item);
	}
	return count;
}

#pragma region Parser

ModbusDataType ToModbusDataTypeEnum(const char* dataType)
{
	if (stricmp(dataType, "hexstring") == 0 || stricmp(dataType, "HEXSTRING") == 0) {
		return HEXSTRING;
	}
	else if (stricmp(dataType, "string") == 0 || stricmp(dataType, "STRING") == 0) {
		return STRING;
	}
	else if (stricmp(dataType, "decimal") == 0 || stricmp(dataType, "DECIMAL") == 0
		|| stricmp(dataType, "float") == 0 || stricmp(dataType, "FLOAT") == 0
		|| stricmp(dataType, "double") == 0 || stricmp(dataType, "DOUBLE") == 0
		|| stricmp(dataType, "int") == 0 || stricmp(dataType, "INT") == 0
		|| stricmp(dataType, "integer") == 0 || stricmp(dataType, "INTEGER") == 0)
	{
		return NUMERIC;
	}
	else if (stricmp(dataType, "flag") == 0 || stricmp(dataType, "FLAG") == 0
		|| stricmp(dataType, "bool") == 0 || stricmp(dataType, "BOOL") == 0
		|| stricmp(dataType, "boolean") == 0 || stricmp(dataType, "BOOLEAN") == 0)
	{
		return FLAG;
	}
	return INVALID;
}

int ModbusPnp_ParseInterfaceConfig(MODBUS_DEVICE_CONTEXT *deviceContext, JSON_Object* configObj)
{
	ModbusDeviceConfig* deviceConfig = deviceContext->DeviceConfig;
	SINGLYLINKEDLIST_HANDLE interfaceDefinitions = deviceContext->InterfaceDefinitions;
	const char* interfaceId = NULL;

	//Get interface ID
	interfaceId = (const char*)json_object_dotget_string(configObj, "interfaceId");
	if (NULL == interfaceId) {
		LogError("interfaceId parameter is missing in configuration");
		return -1;
	}

	// inline interface
	ModbusInterfaceConfig* interfaceConfig = calloc(1, sizeof(ModbusInterfaceConfig));
	interfaceConfig->Events = singlylinkedlist_create();
	interfaceConfig->Properties = singlylinkedlist_create();
	interfaceConfig->Commands = singlylinkedlist_create();
	interfaceConfig->Id = interfaceId;

	JSON_Object* telemetryList = json_object_dotget_object(configObj, "telemetry");
	for (size_t i = 0; i < json_object_get_count(telemetryList); i++)
	{
		const char* name = json_object_get_name(telemetryList, i);
		JSON_Object* telemetryArgs = json_object_dotget_object(telemetryList, name);

		if (NULL == telemetryArgs)
		{
			LogError("ERROR: Telemetry \"%s\" definition is empty.", name);
			return -1;
		}

		ModbusTelemetry* telemetry = calloc(1, sizeof(ModbusTelemetry));
		if (NULL == telemetry)
		{
			LogError("Failed to allocation memory for telemetry configuration: \"%s\": 0x%x ", name, GetLastError());
			return -1;
		}

		telemetry->Name = name;

		telemetry->StartAddress = json_object_dotget_string(telemetryArgs, "startAddress");
		if (0 == telemetry->StartAddress)
		{
			LogError("\"startAddress\" of telemetry \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}
		telemetry->Length = (byte)json_object_dotget_number(telemetryArgs, "length");
		if (0 == telemetry->Length)
		{
			LogError("\"length\" of telemetry \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		const char* dataTypeStr = (const char*)json_object_dotget_string(telemetryArgs, "dataType");
		if (NULL == dataTypeStr)
		{
			LogError("\"dataType\" of telemetry \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}
		telemetry->DataType = ToModbusDataTypeEnum(dataTypeStr);
		if (telemetry->DataType == INVALID)
		{
			LogError("\"dataType\" of telemetry \"%s\" is in valid.", name);
			return -1;
		}

		telemetry->DefaultFrequency = (int)json_object_dotget_number(telemetryArgs, "defaultFrequency");
		if (0 == telemetry->DefaultFrequency)
		{
			LogError("\"defaultFrequency\" of telemetry \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		telemetry->ConversionCoefficient = json_object_dotget_number(telemetryArgs, "conversionCoefficient");
		if (0 == telemetry->ConversionCoefficient)
		{
			LogError("\"conversionCoefficient\" of telemetry \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		if (0 != ModbusPnp_SetReadRequest(deviceConfig, Telemetry, telemetry))
		{
			LogError("Failed to create read request for telemetry \"%s\": 0x%x ", name, GetLastError());
			return -1;
		}

		singlylinkedlist_add(interfaceConfig->Events, telemetry);
	}

	JSON_Object* propertyList = json_object_dotget_object(configObj, "properties");
	for (size_t i = 0; i < json_object_get_count(propertyList); i++)
	{
		const char* name = json_object_get_name(propertyList, i);
		JSON_Object* propertyArgs = json_object_dotget_object(propertyList, name);

		if (NULL == propertyArgs)
		{
			LogError("ERROR: Property \"%s\" definition is empty.", name);
			return -1;
		}

		ModbusProperty* property = calloc(1, sizeof(ModbusProperty));
		if (NULL == property)
		{
			LogError("Failed to allocation memory for property configuration: \"%s\": 0x%x", name, GetLastError());
			return -1;
		}

		property->Name = name;

		property->StartAddress = json_object_dotget_string(propertyArgs, "startAddress");
		if (0 == property->StartAddress)
		{
			LogError("\"startAddress\" of property \"%s\" is in valid: 0x%x", name, GetLastError());
			return -1;
		}

		property->Length = (byte)json_object_dotget_number(propertyArgs, "length");
		if (0 == property->Length)
		{
			LogError("\"length\" of property \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		const char* dataTypeStr = (const char*)json_object_dotget_string(propertyArgs, "dataType");
		if (NULL == dataTypeStr)
		{
			LogError("\"dataType\" of property \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}
		property->DataType = ToModbusDataTypeEnum(dataTypeStr);
		if (property->DataType == INVALID)
		{
			LogError("\"dataType\" of telemetry \"%s\" is in valid.", name);
			return -1;
		}

		property->DefaultFrequency = (int)json_object_dotget_number(propertyArgs, "defaultFrequency");
		if (0 == property->DefaultFrequency)
		{
			LogError("\"defaultFrequency\" of property \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		property->ConversionCoefficient = json_object_dotget_number(propertyArgs, "conversionCoefficient");
		if (0 == property->ConversionCoefficient)
		{
			LogError("\"conversionCoefficient\" of property \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		property->Access = (int)json_object_dotget_number(propertyArgs, "access");
		if (0 == property->Access)
		{
			LogError("\"conversionCoefficient\" of property \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		switch (deviceConfig->ConnectionType)
		{
			case TCP:
				property->WriteRequest.TcpRequest.MBAP.UnitID = deviceContext->DeviceConfig->UnitId;
				break;
			case RTU:
				property->WriteRequest.RtuRequest.UnitID = deviceContext->DeviceConfig->UnitId;
				break;
			default:
				break;
		}

		if (0 != ModbusPnp_SetReadRequest(deviceConfig, Property, property))
		{
			LogError("Failed to create read request for property \"%s\": 0x%x ", name, GetLastError());
			return -1;
		}

		singlylinkedlist_add(interfaceConfig->Properties, property);
	}

	JSON_Object* commandList = json_object_dotget_object(configObj, "commands");
	for (size_t i = 0; i < json_object_get_count(commandList); i++)
	{
		const char* name = json_object_get_name(commandList, i);
		JSON_Object* commandArgs = json_object_dotget_object(commandList, name);

		if (NULL == commandArgs)
		{
			LogError("ERROR: Command \"%s\" definition is empty.", name);
			return -1;
		}

		ModbusCommand* command = calloc(1, sizeof(ModbusCommand));

		if (NULL == command)
		{
			LogError("Failed to allocation memory for command configuration: \"%s\": 0x%x ", name, GetLastError());
			return -1;
		}

		command->Name = name;

		command->StartAddress = json_object_dotget_string(commandArgs, "startAddress");
		if (0 == command->StartAddress)
		{
			LogError("\"startAddress\" of command \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		command->Length = (byte)json_object_dotget_number(commandArgs, "length");
		if (0 == command->Length)
		{
			LogError("\"length\" of command \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		const char* dataTypeStr = (const char*)json_object_dotget_string(commandArgs, "dataType");
		if (NULL == dataTypeStr)
		{
			LogError("\"dataType\" of command \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}
		command->DataType = ToModbusDataTypeEnum(dataTypeStr);
		if (command->DataType == INVALID)
		{
			LogError("\"dataType\" of telemetry \"%s\" is in valid.", name);
			return -1;
		}

		command->ConversionCoefficient = json_object_dotget_number(commandArgs, "conversionCoefficient");
		if (0 == command->ConversionCoefficient)
		{
			LogError("\"conversionCoefficient\" of command \"%s\" is in valid: 0x%x ", name, GetLastError());
			return -1;
		}

		switch (deviceConfig->ConnectionType)
		{
			case TCP:
				command->WriteRequest.TcpRequest.MBAP.UnitID = deviceContext->DeviceConfig->UnitId;
				break;
			case RTU:
				command->WriteRequest.RtuRequest.UnitID = deviceContext->DeviceConfig->UnitId;
				break;
			default:
				break;

		}

		singlylinkedlist_add(interfaceConfig->Commands, command);
	}


	singlylinkedlist_add(interfaceDefinitions, interfaceConfig);

	return 0;
}

int ModbusPnp_ParseRtuSettings(ModbusDeviceConfig* deviceConfig, JSON_Object* configObj)
{
	//Get serial port name
	deviceConfig->ConnectionConfig.RtuConfig.Port = (char*)json_object_dotget_string(configObj, "port");
	if (NULL == deviceConfig->ConnectionConfig.RtuConfig.Port) {
		LogError("ComPort parameter is missing in RTU configuration");
		return -1;
	}

	//Get serial baudRate
	const char* baudRateStr = (const char*)json_object_dotget_string(configObj, "baudRate");
	if (NULL == baudRateStr) {
		LogError("BaudRate parameter is missing in RTU configuration");
		return -1;
	}
	deviceConfig->ConnectionConfig.RtuConfig.BaudRate = atoi(baudRateStr);

	//Get serial dataBits
	deviceConfig->ConnectionConfig.RtuConfig.DataBits = (BYTE)json_object_dotget_number(configObj, "dataBits");
	if (7 != deviceConfig->ConnectionConfig.RtuConfig.DataBits && 8 != deviceConfig->ConnectionConfig.RtuConfig.DataBits) {
		LogError("data Bits parameter is missing or invalid in RTU configuration.  Valid Input: {7, 8}");
		return -1;
	}

	//Get serial stopBits
	const char* stopBitStr = (const char*)json_object_dotget_string(configObj, "stopBits");
	if (NULL != stopBitStr) {
		if (stricmp(stopBitStr, "ONE") == 0 || stricmp(stopBitStr, "1") == 0) {
			deviceConfig->ConnectionConfig.RtuConfig.StopBits = ONESTOPBIT;
		}
		else if (stricmp(stopBitStr, "TWO") == 0 || stricmp(stopBitStr, "2") == 0) {
			deviceConfig->ConnectionConfig.RtuConfig.StopBits = TWOSTOPBITS;
		}
		else if (stricmp(stopBitStr, "OnePointFive") == 0 || stricmp(stopBitStr, "3") == 0) {
			deviceConfig->ConnectionConfig.RtuConfig.StopBits = ONE5STOPBITS;
		}
		else {
			LogError("Invalide Stop Bit parameter in RTU configuration. Valid Input: {\"ONE\", \"TWO\", \"OnePointFive\"}");
			return -1;
		}
	}
	else {

		BYTE stopBitInput = (BYTE)json_object_dotget_number(configObj, "stopBits");
		if (0 == stopBitInput) {
			LogError("Stop Bit parameter is missing in RTU configuration.");
			return -1;
		}

		switch (stopBitInput) {
		case 1:
			deviceConfig->ConnectionConfig.RtuConfig.StopBits = ONESTOPBIT;
			break;
		case 2:
			deviceConfig->ConnectionConfig.RtuConfig.StopBits = TWOSTOPBITS;
			break;
		case 3:
			deviceConfig->ConnectionConfig.RtuConfig.StopBits = ONE5STOPBITS;
			break;
		default:
			LogError("Invalide Stop Bit parameter in RTU configuration. Valid Input: {\"ONE\", \"TWO\", \"OnePointFive\", 1, 2, 3}");
			return -1;
			break;
		}
	}

	//Get serial port partiy
	const char* parityStr = (const char*)json_object_dotget_string(configObj, "parity");
	if (NULL == parityStr) {
		LogError("Parity parameter is missing in RTU configuration.");
		return -1;
	}

	if (stricmp(parityStr, "NONE") == 0) {
		deviceConfig->ConnectionConfig.RtuConfig.Parity = NOPARITY;
	}
	else if (stricmp(parityStr, "EVEN") == 0) {
		deviceConfig->ConnectionConfig.RtuConfig.Parity = EVENPARITY;
	}
	else if (stricmp(parityStr, "ODD") == 0) {
		deviceConfig->ConnectionConfig.RtuConfig.Parity = ODDPARITY;
	}
	else if (stricmp(parityStr, "MARK") == 0) {
		deviceConfig->ConnectionConfig.RtuConfig.Parity = MARKPARITY;
	}
	else if (stricmp(parityStr, "SPACE") == 0) {
		deviceConfig->ConnectionConfig.RtuConfig.Parity = SPACEPARITY;
	}
	else {
		LogError("Invalide Stop Bit parameter in RTU configuration. Valid Input: {\"NONE\", \"EVEN\", \"ODD\", \"MARK\", \"SPACE\"}");
		return -1;
	}

	deviceConfig->ConnectionType = RTU;
	return 0;
}

int ModbusPnP_ParseTcpSettings(ModbusDeviceConfig* deviceConfig, JSON_Object* configObj)
{
	//Get serial port name
	deviceConfig->ConnectionConfig.TcpConfig.Host = (char*)json_object_dotget_string(configObj, "host");
	if (NULL == deviceConfig->ConnectionConfig.TcpConfig.Host) {
		LogError("\"Host\" parameter is missing in TCP configuration");
		return -1;
	}

	//Get serial dataBits
	deviceConfig->ConnectionConfig.TcpConfig.Port = (UINT16)json_object_dotget_number(configObj, "port");
	if (0 == deviceConfig->ConnectionConfig.TcpConfig.Port) {
		LogError("\"Port\" is invalide in TCP configuration: Cannot be 0.");
		return -1;
	}

	deviceConfig->ConnectionType = TCP;

	return 0;
}
#pragma endregion

#pragma region SerialConnection

typedef struct _SERIAL_DEVICE {
	char* InterfaceName;
} SERIAL_DEVICE, *PSERIAL_DEVICE;

SINGLYLINKEDLIST_HANDLE ModbusDeviceList = NULL;
int ModbusDeviceCount = 0;

int ModbusPnp_FindSerialDevices()
{
	CONFIGRET cmResult = CR_SUCCESS;
	char* deviceInterfaceList;
	ULONG bufferSize = 0;

	ModbusDeviceList = singlylinkedlist_create();

	cmResult = CM_Get_Device_Interface_List_Size(
		&bufferSize,
		(LPGUID)&GUID_DEVINTERFACE_COMPORT,
		NULL,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (CR_SUCCESS != cmResult) {
		return -1;
	}

	deviceInterfaceList = malloc(bufferSize * sizeof(char));

	cmResult = CM_Get_Device_Interface_ListA(
		(LPGUID)&GUID_DEVINTERFACE_COMPORT,
		NULL,
		deviceInterfaceList,
		bufferSize,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (CR_SUCCESS != cmResult) {
		return -1;
	}

	for (PCHAR currentDeviceInterface = deviceInterfaceList;
		*currentDeviceInterface != '\0';
		currentDeviceInterface += strlen(currentDeviceInterface) + 1)
	{
		PSERIAL_DEVICE serialDevs = malloc(sizeof(SERIAL_DEVICE));
		serialDevs->InterfaceName = malloc((strlen(currentDeviceInterface) + 1) * sizeof(char));
		strcpy_s(serialDevs->InterfaceName, strlen(currentDeviceInterface) + 1, currentDeviceInterface);
		singlylinkedlist_add(ModbusDeviceList, serialDevs);
		ModbusDeviceCount++;
	}

	return 0;
}

int ModbusPnp_OpenSerial(MODBUS_RTU_CONFIG* rtuConfig, HANDLE *serialHandle) {

	*serialHandle = CreateFileA(rtuConfig->Port,
		GENERIC_READ | GENERIC_WRITE,
		0, // must be opened with exclusive-access
		0, //NULL, no security attributes
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);

	if (*serialHandle == INVALID_HANDLE_VALUE) {
		// Handle the error
		int error = GetLastError();
		LogError("Failed to open com port %s, %x", rtuConfig->Port, error);
		if (error == ERROR_FILE_NOT_FOUND) {
			return -1;
		}
		return -1;
	}

	//  Initialize the DCB structure.
	DCB dcbSerialParams;
	SecureZeroMemory(&dcbSerialParams, sizeof(DCB));
	dcbSerialParams.DCBlength = sizeof(DCB);
	if (!GetCommState(*serialHandle, &dcbSerialParams)) {
		LogError("Failed to open com port %s, %x", rtuConfig->Port, GetLastError());
		return -1;
	}
	dcbSerialParams.BaudRate = rtuConfig->BaudRate;
	dcbSerialParams.ByteSize = rtuConfig->DataBits;
	dcbSerialParams.StopBits = rtuConfig->StopBits;
	dcbSerialParams.Parity = rtuConfig->Parity;
	if (!SetCommState(*serialHandle, &dcbSerialParams)) {
		//error setting serial port state
		LogError("Failed to open com port %s, %x", rtuConfig->Port, GetLastError());
		return -1;
	}

	LogInfo("Opened com port %s", rtuConfig->Port);

	COMMTIMEOUTS timeouts;
	timeouts.ReadIntervalTimeout = 3 * 1000;
	timeouts.ReadTotalTimeoutMultiplier = 1;
	timeouts.ReadTotalTimeoutConstant = 1;
	timeouts.WriteTotalTimeoutMultiplier = 1;
	timeouts.WriteTotalTimeoutConstant = 1;
	if (!SetCommTimeouts(*serialHandle, &timeouts))
	{
		int error = GetLastError();
		printf("Set timeout failed on com port %s, %x", rtuConfig->Port, error);
		return -1;
	}
	return 0;
}

int ModbusPnp_OpenSocket(MODBUS_TCP_CONFIG* tcpConfig, SOCKET *socketHandle)
{
	WSADATA wsaData = { 0 };
	int result = 0;
	
	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		LogError("WSAStartup failed: %d\n", result);
		return -1;
	}

	// Create socket
	*socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*socketHandle == INVALID_SOCKET) {
		LogError("Failed to create socket with error = %d.", WSAGetLastError());
		goto ExitOnError;
	}

	// Connect to device
	struct sockaddr_in deviceAddress;
	deviceAddress.sin_family = AF_INET;
	deviceAddress.sin_addr.s_addr = inet_addr(tcpConfig->Host);
	deviceAddress.sin_port = htons(tcpConfig->Port);

	result = connect(*socketHandle, (SOCKADDR *)& deviceAddress, sizeof(deviceAddress));
	if (result == SOCKET_ERROR) {
		LogError("Failed to connect with socker \"%s:%d\" with error: %ld.", tcpConfig->Host, tcpConfig->Port, WSAGetLastError());
		result = closesocket(*socketHandle);
		if (result == SOCKET_ERROR)
			LogError("Failed to close socket with error: %ld.", WSAGetLastError());
		goto ExitOnError;
	}
	LogInfo("Connected to device at \"%s:%d\".", tcpConfig->Host, tcpConfig->Port);
	return 0;

ExitOnError:
	WSACleanup();
	return -1;
}

#pragma endregion

int ModbusPnp_OpenDeviceWorker(void* context)
{
	PMODBUS_DEVICE_CONTEXT deviceContext = context;

	JSON_Value* json = json_parse_string(modbusDeviceChangeMessageformat);
	JSON_Object* jsonObject = json_value_get_object(json);

	LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	while (interfaceItem != NULL) {
		const ModbusInterfaceConfig* def = (const ModbusInterfaceConfig*)singlylinkedlist_item_get_value(interfaceItem);
        PNPMESSAGE payload = NULL;
        PNPMESSAGE_PROPERTIES* pnpMsgProps = NULL;

        PnpMessage_CreateMessage(&payload);

        PnpMessage_SetMessage(payload, json_serialize_to_string(json_object_get_wrapping_value(jsonObject)));
        PnpMessage_SetInterfaceId(payload, def->Id);

        pnpMsgProps = PnpMessage_AccessProperties(payload);
        pnpMsgProps->Context = deviceContext;
		
        // Notify the pnpbridge of device discovery
        deviceContext->ModbusDeviceChangeCallback(payload);
		interfaceItem = singlylinkedlist_get_next_item(interfaceItem);

        // Drop reference on the PNPMESSAGE
        PnpMemory_ReleaseReference(payload);
	}

	//Start polling all telemetry
	ModbusPnp_StartPollingAllTelemetryProperty(deviceContext);

	return 0;
}

int ModbusPnp_OpenDevice(ModbusDeviceConfig* deviceConfig, JSON_Object* interfaceConfig) {

	PMODBUS_DEVICE_CONTEXT deviceContext = calloc(1, sizeof(MODBUS_DEVICE_CONTEXT));

	if (deviceConfig->ConnectionType == RTU)
	{
		if (0 != ModbusPnp_OpenSerial(&(deviceConfig->ConnectionConfig.RtuConfig), &(deviceContext->hDevice)))
		{
			LogError("Failed to open serial connection to \"%s\".", deviceConfig->ConnectionConfig.RtuConfig.Port);
			goto cleanup;
		}
	}
	else if (deviceConfig->ConnectionType == TCP)
	{
		//TOOD
		if (0 != ModbusPnp_OpenSocket(&(deviceConfig->ConnectionConfig.TcpConfig), (SOCKET*)&(deviceContext->hDevice)))
		{
			LogError("Failed to open socket connection to \"%s:%d\".", deviceConfig->ConnectionConfig.TcpConfig.Host, deviceConfig->ConnectionConfig.TcpConfig.Port);
			goto cleanup;
		}
	}
	else
	{
		goto cleanup;
	}

	deviceContext->pnpAdapterInterface = NULL;
	deviceContext->ModbusDeviceChangeCallback = DiscoveryAdapter_ReportDevice;
	deviceContext->DeviceConfig = deviceConfig;
	deviceContext->InterfaceDefinitions = singlylinkedlist_create();
	deviceContext->hConnectionLock = CreateMutex(NULL, false, NULL);
	if (NULL == deviceContext->hConnectionLock)
	{
		LogError("Failed to create mutex lock for device connection with error: %d.", GetLastError());
		goto cleanup;
	}

	if (ModbusPnp_ParseInterfaceConfig(deviceContext, interfaceConfig) != 0) {
		LogError("Failed to parse Modbus interface configuration.");
		goto cleanup;
	}

	if (ThreadAPI_Create(&deviceContext->ModbusDeviceWorker, ModbusPnp_OpenDeviceWorker, deviceContext) != THREADAPI_OK) {
		LogError("ThreadAPI_Create failed");
		goto cleanup;
	}
	return 0;

cleanup:
	free(deviceContext);
	return -1;
}

#pragma region PnpDiscovery

int 
ModbusPnp_StartDiscovery(
    _In_ PNPMEMORY DeviceArgs,
    _In_ PNPMEMORY AdapterArgs
    )
{
	if (DeviceArgs == NULL) {
		LogInfo("ModbusPnp_StartDiscovery: No device discovery parameters found in configuration.");
		return 0;
	}
	
	UNREFERENCED_PARAMETER(AdapterArgs);

    LogInfo("Starting modbus discovery adapter");

	// Parse Modbus DeviceConfig
	ModbusDeviceConfig* deviceConfig = calloc(1, sizeof(ModbusDeviceConfig));
	deviceConfig->ConnectionType = UNKOWN;

    PDEVICE_ADAPTER_PARMAETERS deviceParams = (PDEVICE_ADAPTER_PARMAETERS) PnpMemory_GetBuffer(DeviceArgs, NULL);

	JSON_Value* jvalue = json_parse_string(deviceParams->AdapterParameters[0]);
	JSON_Object* args = json_value_get_object(jvalue);
	JSON_Object* deviceConfigObj = json_object_dotget_object(args, "deviceConfig");

	//BYTE unitId = (BYTE)json_object_dotget_number(deviceConfigObj, "unitId");
	deviceConfig->UnitId = (BYTE)json_object_dotget_number(deviceConfigObj, "unitId");

	JSON_Object* rtuArgs = json_object_dotget_object(deviceConfigObj, "rtu");
	if (NULL != rtuArgs && ModbusPnp_ParseRtuSettings(deviceConfig, rtuArgs) != 0) {
		LogError("Failed to parse RTU connection settings.");
		return -1;
	}

	JSON_Object* tcpArgs = json_object_dotget_object(deviceConfigObj, "tcp");
	if (NULL != tcpArgs && ModbusPnP_ParseTcpSettings(deviceConfig, tcpArgs) != 0) {
		LogError("Failed to parse TCP connection settings.");
		return -1;
	}

	if (UNKOWN == deviceConfig->ConnectionType)	{
		LogError("Missing Modbus connection settings.");
		return -1;
	}

	JSON_Object* interfaceConfigObj = json_object_dotget_object(args, "interfaceConfig");

	LogInfo("Opening device %s", deviceConfig->ConnectionConfig.RtuConfig.Port);
	ModbusPnp_OpenDevice(deviceConfig, interfaceConfigObj);

	return 0;
}

int ModbusPnp_StopDiscovery() {
	return 0;
}

#pragma endregion


#pragma region PnpInterface
void ModbusPnp_FreeTelemetryList(SINGLYLINKEDLIST_HANDLE telemetryList) {
	if (NULL == telemetryList) {
		return;
	}

	LIST_ITEM_HANDLE telemetryItem = singlylinkedlist_get_head_item(telemetryList);
	while (NULL != telemetryItem) {
		ModbusTelemetry* telemetry = (ModbusTelemetry*)singlylinkedlist_item_get_value(telemetryItem);
		free(telemetry);
		telemetryItem = singlylinkedlist_get_next_item(telemetryItem);
	}
}

void ModbusPnp_FreeCommandList(SINGLYLINKEDLIST_HANDLE commandList) {
	if (NULL == commandList) {
		return;
	}

	LIST_ITEM_HANDLE commandItem = singlylinkedlist_get_head_item(commandList);
	while (NULL != commandItem) {
		ModbusCommand* command = (ModbusCommand*)singlylinkedlist_item_get_value(commandItem);
		free(command);
		commandItem = singlylinkedlist_get_next_item(commandItem);
	}
}

void ModbusPnp_FreePropertyList(SINGLYLINKEDLIST_HANDLE propertyList) {
	if (NULL == propertyList) {
		return;
	}

	LIST_ITEM_HANDLE propertyItem = singlylinkedlist_get_head_item(propertyList);
	while (NULL != propertyItem) {
		ModbusProperty* property = (ModbusProperty*)singlylinkedlist_item_get_value(propertyItem);
		free(property);
		propertyItem = singlylinkedlist_get_next_item(propertyItem);
	}
}

void ModbusPnP_CleanupPollingTasks(PMODBUS_DEVICE_CONTEXT deviceContext)
{
	if (NULL != deviceContext->PollingTasks)
	{
		StopPollingTasks();

		LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
		const ModbusInterfaceConfig* interfaceConfig = singlylinkedlist_item_get_value(interfaceDefHandle);
		int telemetryCount = ModbusPnp_GetListCount(interfaceConfig->Events);
		int propertyCount = ModbusPnp_GetListCount(interfaceConfig->Properties);
		int threadCount = telemetryCount + propertyCount;

		for (int i = 0; i < threadCount; i++)
		{
			int res = 0;
			int result = ThreadAPI_Join(deviceContext->PollingTasks[i], &res);
			if (result != THREADAPI_OK)
			{
				LogError("Failed to stop thread. error: %02X.", result);
			}
			//TODO: To be able to stop sleeping thread
		}
		free(deviceContext->PollingTasks);
	}
}

int
ModbusPnp_StartPnpInterface(
    _In_ PNPADAPTER_INTERFACE_HANDLE PnpInterface
    )
{
    AZURE_UNREFERENCED_PARAMETER(PnpInterface);
    return 0;
}

int ModbusPnp_ReleasePnpInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface) {
	PMODBUS_DEVICE_CONTEXT deviceContext = PnpAdapterInterface_GetContext(pnpInterface);

	if (NULL == deviceContext) {
		return 0;
	}

	ModbusPnP_CleanupPollingTasks(deviceContext);

	if (NULL != deviceContext->hDevice) {
		ModbusPnp_CloseDevice(deviceContext->DeviceConfig->ConnectionType, deviceContext->hDevice, deviceContext->hConnectionLock);
	}

	if (NULL != deviceContext->hConnectionLock) {
		CloseHandle(deviceContext->hConnectionLock);
	}

	if (deviceContext->InterfaceDefinitions) {
		LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
		while (interfaceItem != NULL) {
			ModbusInterfaceConfig* def = (ModbusInterfaceConfig*)singlylinkedlist_item_get_value(interfaceItem);
			ModbusPnp_FreeTelemetryList(def->Events);
			ModbusPnp_FreeCommandList(def->Commands);
			ModbusPnp_FreePropertyList(def->Properties);

			free(def);
			interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
		}
		singlylinkedlist_destroy(deviceContext->InterfaceDefinitions);
	}

	free(deviceContext->DeviceConfig);
	free(deviceContext->InterfaceDefinitions);
	free(deviceContext);

	return 0;
}

int 
ModbusPnp_CreatePnpInterface(
    _In_ PNPADAPTER_CONTEXT AdapterHandle,
    _In_ PNPMESSAGE Message
    )
{
    PNPMESSAGE_PROPERTIES* pnpMsgProps = NULL;
    PMODBUS_DEVICE_CONTEXT deviceContext = NULL;
    const char* interfaceId = NULL;
    int result = 0;

    pnpMsgProps = PnpMessage_AccessProperties(Message);
    deviceContext = (PMODBUS_DEVICE_CONTEXT)pnpMsgProps->Context;
    interfaceId = PnpMessage_GetInterfaceId(Message);

	// Create an Azure Pnp interface for each interface in the SerialPnp descriptor
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	while (interfaceDefHandle != NULL) {
		PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface = NULL;
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient = NULL;
		char** propertyNames = NULL;
		int propertyCount = 0;
        DIGITALTWIN_PROPERTY_UPDATE_CALLBACK* propertyUpdateTable = NULL;
		char** commandNames = NULL;
		int commandCount = 0;
        DIGITALTWIN_COMMAND_EXECUTE_CALLBACK* commandUpdateTable = NULL;
		const ModbusInterfaceConfig* interfaceConfig = singlylinkedlist_item_get_value(interfaceDefHandle);
        DIGITALTWIN_CLIENT_RESULT dtRes;

		// Construct property table
		{
			SINGLYLINKEDLIST_HANDLE propertyList = interfaceConfig->Properties;
			propertyCount = ModbusPnp_GetListCount(interfaceConfig->Properties);

			int readWritePropertyCount = 0;
			LIST_ITEM_HANDLE propertyItemHandle = NULL;
			SINGLYLINKEDLIST_HANDLE readWritePropertyList = singlylinkedlist_create();

			for (int i = 0; i < propertyCount ; i++) 
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

				if (READ_WRITE == property->Access)
				{
					readWritePropertyCount++;
					singlylinkedlist_add(readWritePropertyList, property);
				}
			}

			if (readWritePropertyCount > 0)
			{

				propertyNames = calloc(1, sizeof(char*)*readWritePropertyCount);
				if (NULL == propertyNames) {
					result = -1;
					goto exit;
				}

				propertyUpdateTable = calloc(1, sizeof(DIGITALTWIN_PROPERTY_UPDATE_CALLBACK*)*readWritePropertyCount);
				if (NULL == propertyUpdateTable) {
					result = -1;
					goto exit;
				}

				propertyItemHandle = NULL;
				for (int i = 0; i < readWritePropertyCount; i++)
				{
					if (NULL == propertyItemHandle)
					{
						propertyItemHandle = singlylinkedlist_get_head_item(readWritePropertyList);
					}
					else
					{
						propertyItemHandle = singlylinkedlist_get_next_item(propertyItemHandle);
					}

					const ModbusProperty* property = singlylinkedlist_item_get_value(propertyItemHandle);

					propertyNames[i] = (char*) property->Name;
					propertyUpdateTable[i] = ModbusPnp_PropertyHandler;
				}

			/*	modbusPropertyTable.numCallbacks = readWritePropertyCount;
				modbusPropertyTable.propertyNames = propertyNames;
				modbusPropertyTable.callbacks = propertyUpdateTable;
				modbusPropertyTable.version = 1;*/
			}
		}

		// Construct command table
		{
			SINGLYLINKEDLIST_HANDLE commandList = interfaceConfig->Commands;
			commandCount = ModbusPnp_GetListCount(commandList);

			if (commandCount > 0)
			{
				commandNames = calloc(1, sizeof(char*)*commandCount);
				if (NULL == commandNames) {
					result = -1;
					goto exit;
				}

				commandUpdateTable = calloc(1, sizeof(DIGITALTWIN_COMMAND_EXECUTE_CALLBACK*)*commandCount);
				if (NULL == commandUpdateTable) {
					result = -1;
					goto exit;
				}

				LIST_ITEM_HANDLE commandItemHandle = NULL;
				for (int i = 0; i < commandCount; i++)
				{
					if (NULL == commandItemHandle)
					{
						commandItemHandle = singlylinkedlist_get_head_item(commandList);
					}
					else
					{
						commandItemHandle = singlylinkedlist_get_next_item(commandItemHandle);
					}

					if (NULL != commandItemHandle) {
						const ModbusCommand* command = singlylinkedlist_item_get_value(commandItemHandle);
						commandNames[i] = (char*) command->Name;
						commandUpdateTable[i] = ModbusPnp_CommandHandler;
					}
				}

				/*modbusCommandTable.numCommandCallbacks = commandCount;
				modbusCommandTable.commandNames = commandNames;
				modbusCommandTable.commandCallbacks = commandUpdateTable;
				modbusCommandTable.version = 1;*/
			}
		}

        dtRes = DigitalTwin_InterfaceClient_Create(interfaceId,
                    "modbus",
			        NULL,
			        deviceContext, 
                    &pnpInterfaceClient);
        if (DIGITALTWIN_CLIENT_OK != dtRes) {
            result = -1;
            goto exit;
        }

        if (propertyCount > 0) {
            dtRes = DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback(pnpInterfaceClient, ModbusPnp_PropertyHandler);
            if (DIGITALTWIN_CLIENT_OK != dtRes) {
                result = -1;
                goto exit;
            }
        }

        if (commandCount > 0) {
            dtRes = DigitalTwin_InterfaceClient_SetCommandsCallback(pnpInterfaceClient, ModbusPnp_CommandHandler);
            if (DIGITALTWIN_CLIENT_OK != dtRes) {
                result = -1;
                goto exit;
            }
        }

		// Create PnpAdapter Interface
		{
			PNPADPATER_INTERFACE_PARAMS interfaceParams = { 0 };
            PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, AdapterHandle, pnpInterfaceClient);
            interfaceParams.InterfaceId = (char*)interfaceId;
			interfaceParams.ReleaseInterface = ModbusPnp_ReleasePnpInterface;
            interfaceParams.StartInterface = ModbusPnp_StartPnpInterface;

			result = PnpAdapterInterface_Create(&interfaceParams, &pnpAdapterInterface);
			if (result < 0) {
				goto exit;
			}
		}

		if (NULL != propertyUpdateTable) {
			free(propertyUpdateTable);
		}
		if (NULL != propertyNames) {
			free(propertyNames);
		}

		if (NULL != commandUpdateTable) {
			free(commandUpdateTable);
		}
		if (NULL != commandNames) {
			free(commandNames);
		}

		// Save the PnpAdapterInterface in device context
		deviceContext->pnpAdapterInterface = pnpAdapterInterface;


		// Update telemetry's interface client handle
		{
			SINGLYLINKEDLIST_HANDLE telemetryList = interfaceConfig->Events;
			int telemetryCount = ModbusPnp_GetListCount(telemetryList);

			if (telemetryCount > 0)
			{
				LIST_ITEM_HANDLE telemetryItemHandle = NULL;
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

					if (NULL != telemetryItemHandle) {
						ModbusTelemetry* telemetry = (ModbusTelemetry*) singlylinkedlist_item_get_value(telemetryItemHandle);
						telemetry->InterfaceClient = pnpAdapterInterface;
					}
				}
			}
		}

		// Update property's interface client handle
		{
			SINGLYLINKEDLIST_HANDLE propertyList = interfaceConfig->Properties;
			//int propertyCount = ModbusPnp_GetListCount(propertyList);

			if (propertyCount > 0)
			{
				LIST_ITEM_HANDLE propertyItemHandle = NULL;
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

					if (NULL != propertyItemHandle) {
						ModbusProperty* property = (ModbusProperty*) singlylinkedlist_item_get_value(propertyItemHandle);
						property->InterfaceClient = pnpAdapterInterface;
					}
				}
			}
		}

		// Update command's interface client handle
		{
			SINGLYLINKEDLIST_HANDLE commandList = interfaceConfig->Commands;
			commandCount = ModbusPnp_GetListCount(commandList);

			if (commandCount > 0)
			{
				LIST_ITEM_HANDLE commandItemHandle = NULL;
				for (int i = 0; i < commandCount; i++)
				{
					if (NULL == commandItemHandle)
					{
						commandItemHandle = singlylinkedlist_get_head_item(commandList);
					}
					else
					{
						commandItemHandle = singlylinkedlist_get_next_item(commandItemHandle);
					}

					if (NULL != commandItemHandle) {
						ModbusCommand* command = (ModbusCommand*) singlylinkedlist_item_get_value(commandItemHandle);
						command->InterfaceClient = pnpAdapterInterface;
					}
				}
			}
		}

		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
exit:

	// Cleanup incase of failure
	if (result < 0) {
		//Destroy PnpInterfaceClient
		/*if (NULL != pnpInterfaceClient) {
		    PnP_InterfaceClient_Destroy(pnpInterfaceClient);
		}
		*/
	}

	return 0;
}

int ModbusPnp_Initialize(const char* adapterArgs) {
	UNREFERENCED_PARAMETER(adapterArgs);
	return 0;
}

int 
ModbusPnp_Shutdown() {
	return 0;
}

#pragma endregion

DISCOVERY_ADAPTER ModbusPnpDeviceDiscovery = {
	.Identity = "modbus-pnp-discovery",
	.StartDiscovery = ModbusPnp_StartDiscovery,
	.StopDiscovery = ModbusPnp_StopDiscovery
};

PNP_ADAPTER ModbusPnpInterface = {
	.identity = "modbus-pnp-interface",
	.initialize = ModbusPnp_Initialize,
	.shutdown = ModbusPnp_Shutdown,
	.createPnpInterface = ModbusPnp_CreatePnpInterface
};