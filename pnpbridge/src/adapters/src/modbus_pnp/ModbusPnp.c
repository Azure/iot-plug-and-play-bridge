// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpadapter_api.h"

#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"

#include "parson.h"
#include "ModbusPnp.h"
#include "ModbusCapability.h"
#include "ModbusConnection/ModbusConnection.h"

#ifndef WIN32
    #include <strings.h>
    #define strcmpcasei strcasecmp
#else
    #define strcmpcasei stricmp
#endif

int ModbusPnp_GetListCount(
    SINGLYLINKEDLIST_HANDLE list)
{
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

ModbusDataType ToModbusDataTypeEnum(
    const char* dataType)
{
    if (strcmpcasei(dataType, "hexstring") == 0) {
        return HEXSTRING;
    }
    else if (strcmpcasei(dataType, "string") == 0) {
        return STRING;
    }
    else if (strcmpcasei(dataType, "decimal") == 0
        || strcmpcasei(dataType, "float") == 0
        || strcmpcasei(dataType, "double") == 0
        || strcmpcasei(dataType, "int") == 0
        || strcmpcasei(dataType, "integer") == 0)
    {
        return NUMERIC;
    }
    else if (strcmpcasei(dataType, "flag") == 0
        || strcmpcasei(dataType, "bool") == 0
        || strcmpcasei(dataType, "boolean") == 0)
    {
        return FLAG;
    }
    return INVALID;
}

IOTHUB_CLIENT_RESULT ModbusPnp_ParseInterfaceConfig(
    PModbusInterfaceConfig * ModbusInterfaceConfig,
    JSON_Object* ConfigObj)
{
    if (NULL == *ModbusInterfaceConfig)
    {
        LogError("Cannot populate interface config for modbus adapter.");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    (*ModbusInterfaceConfig)->Events = singlylinkedlist_create();
    (*ModbusInterfaceConfig)->Properties = singlylinkedlist_create();
    (*ModbusInterfaceConfig)->Commands = singlylinkedlist_create();

    JSON_Object* telemetryList = json_object_dotget_object(ConfigObj, "telemetry");
    for (size_t i = 0; i < json_object_get_count(telemetryList); i++)
    {
        const char* name = json_object_get_name(telemetryList, i);
        JSON_Object* telemetryArgs = json_object_dotget_object(telemetryList, name);

        if (NULL == telemetryArgs)
        {
            LogError("ERROR: Telemetry \"%s\" definition is empty.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        ModbusTelemetry* telemetry = calloc(1, sizeof(ModbusTelemetry));
        if (NULL == telemetry)
        {
            LogError("Failed to allocation memory for telemetry configuration: \"%s\". ", name);
            return IOTHUB_CLIENT_ERROR;
        }

        mallocAndStrcpy_s(&telemetry->Name, name);

        telemetry->StartAddress = json_object_dotget_string(telemetryArgs, "startAddress");
        if (0 == telemetry->StartAddress)
        {
            LogError("\"startAddress\" of telemetry \"%s\" is in valid. ", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }
        telemetry->Length = (uint8_t)json_object_dotget_number(telemetryArgs, "length");
        if (0 == telemetry->Length)
        {
            LogError("\"length\" of telemetry \"%s\" is in valid. ", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        const char* dataTypeStr = (const char*)json_object_dotget_string(telemetryArgs, "dataType");
        if (NULL == dataTypeStr)
        {
            LogError("\"dataType\" of telemetry \"%s\" is in valid. ", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }
        telemetry->DataType = ToModbusDataTypeEnum(dataTypeStr);
        if (telemetry->DataType == INVALID)
        {
            LogError("\"dataType\" of telemetry \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        telemetry->DefaultFrequency = (int)json_object_dotget_number(telemetryArgs, "defaultFrequency");
        if (0 == telemetry->DefaultFrequency)
        {
            LogError("\"defaultFrequency\" of telemetry \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        telemetry->ConversionCoefficient = json_object_dotget_number(telemetryArgs, "conversionCoefficient");
        if (0 == telemetry->ConversionCoefficient)
        {
            LogError("\"conversionCoefficient\" of telemetry \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        singlylinkedlist_add((*ModbusInterfaceConfig)->Events, telemetry);
    }

    JSON_Object* propertyList = json_object_dotget_object(ConfigObj, "properties");
    for (size_t i = 0; i < json_object_get_count(propertyList); i++)
    {
        const char* name = json_object_get_name(propertyList, i);
        JSON_Object* propertyArgs = json_object_dotget_object(propertyList, name);

        if (NULL == propertyArgs)
        {
            LogError("ERROR: Property \"%s\" definition is empty.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        ModbusProperty* property = calloc(1, sizeof(ModbusProperty));
        if (NULL == property)
        {
            LogError("Failed to allocation memory for property configuration: \"%s\".", name);
            return IOTHUB_CLIENT_ERROR;
        }

        mallocAndStrcpy_s(&property->Name, name);

        property->StartAddress = json_object_dotget_string(propertyArgs, "startAddress");
        if (0 == property->StartAddress)
        {
            LogError("\"startAddress\" of property \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        property->Length = (uint8_t)json_object_dotget_number(propertyArgs, "length");
        if (0 == property->Length)
        {
            LogError("\"length\" of property \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        const char* dataTypeStr = (const char*)json_object_dotget_string(propertyArgs, "dataType");
        if (NULL == dataTypeStr)
        {
            LogError("\"dataType\" of property \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }
        property->DataType = ToModbusDataTypeEnum(dataTypeStr);
        if (property->DataType == INVALID)
        {
            LogError("\"dataType\" of telemetry \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        property->DefaultFrequency = (int)json_object_dotget_number(propertyArgs, "defaultFrequency");
        if (0 == property->DefaultFrequency)
        {
            LogError("\"defaultFrequency\" of property \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        property->ConversionCoefficient = json_object_dotget_number(propertyArgs, "conversionCoefficient");
        if (0 == property->ConversionCoefficient)
        {
            LogError("\"conversionCoefficient\" of property \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        property->Access = (int)json_object_dotget_number(propertyArgs, "access");
        if (0 == property->Access)
        {
            LogError("\"conversionCoefficient\" of property \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        singlylinkedlist_add((*ModbusInterfaceConfig)->Properties, property);
    }

    JSON_Object* commandList = json_object_dotget_object(ConfigObj, "commands");
    for (size_t i = 0; i < json_object_get_count(commandList); i++)
    {
        const char* name = json_object_get_name(commandList, i);
        JSON_Object* commandArgs = json_object_dotget_object(commandList, name);

        if (NULL == commandArgs)
        {
            LogError("ERROR: Command \"%s\" definition is empty.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        ModbusCommand* command = calloc(1, sizeof(ModbusCommand));

        if (NULL == command)
        {
            LogError("Failed to allocation memory for command configuration: \"%s\".", name);
            return IOTHUB_CLIENT_ERROR;
        }

        mallocAndStrcpy_s(&command->Name, name);

        command->StartAddress = json_object_dotget_string(commandArgs, "startAddress");
        if (0 == command->StartAddress)
        {
            LogError("\"startAddress\" of command \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        command->Length = (uint8_t)json_object_dotget_number(commandArgs, "length");
        if (0 == command->Length)
        {
            LogError("\"length\" of command \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        const char* dataTypeStr = (const char*)json_object_dotget_string(commandArgs, "dataType");
        if (NULL == dataTypeStr)
        {
            LogError("\"dataType\" of command \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }
        command->DataType = ToModbusDataTypeEnum(dataTypeStr);
        if (command->DataType == INVALID)
        {
            LogError("\"dataType\" of telemetry \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        command->ConversionCoefficient = json_object_dotget_number(commandArgs, "conversionCoefficient");
        if (0 == command->ConversionCoefficient)
        {
            LogError("\"conversionCoefficient\" of command \"%s\" is in valid.", name);
            return IOTHUB_CLIENT_INVALID_ARG;
        }

        singlylinkedlist_add((*ModbusInterfaceConfig)->Commands, command);
    }

    return IOTHUB_CLIENT_OK;
}

int ModbusPnp_ParseRtuSettings(
    ModbusDeviceConfig* deviceConfig,
    JSON_Object* configObj)
{
    // Get serial port name
    deviceConfig->ConnectionConfig.RtuConfig.Port = (char*)json_object_dotget_string(configObj, "port");
    if (NULL == deviceConfig->ConnectionConfig.RtuConfig.Port) {
        LogError("ComPort parameter is missing in RTU configuration");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    // Get serial baudRate
    const char* baudRateStr = (const char*)json_object_dotget_string(configObj, "baudRate");
    if (NULL == baudRateStr) {
        LogError("BaudRate parameter is missing in RTU configuration");
        return IOTHUB_CLIENT_INVALID_ARG;
    }
    deviceConfig->ConnectionConfig.RtuConfig.BaudRate = atoi(baudRateStr);

    // Get serial dataBits
    deviceConfig->ConnectionConfig.RtuConfig.DataBits = (uint8_t)json_object_dotget_number(configObj, "dataBits");
    if (7 != deviceConfig->ConnectionConfig.RtuConfig.DataBits && 8 != deviceConfig->ConnectionConfig.RtuConfig.DataBits) {
        LogError("data Bits parameter is missing or invalid in RTU configuration.  Valid Input: {7, 8}");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    // Get serial stopBits
    const char* stopBitStr = (const char*)json_object_dotget_string(configObj, "stopBits");
    if (NULL != stopBitStr) {
        if (strcmpcasei(stopBitStr, "ONE") == 0 || strcmpcasei(stopBitStr, "1") == 0) {
            deviceConfig->ConnectionConfig.RtuConfig.StopBits = ONESTOPBIT;
        }
        else if (strcmpcasei(stopBitStr, "TWO") == 0 || strcmpcasei(stopBitStr, "2") == 0) {
            deviceConfig->ConnectionConfig.RtuConfig.StopBits = TWOSTOPBITS;
        }
        else if (strcmpcasei(stopBitStr, "OnePointFive") == 0 || strcmpcasei(stopBitStr, "3") == 0) {
            deviceConfig->ConnectionConfig.RtuConfig.StopBits = ONE5STOPBITS;
        }
        else {
            LogError("Invalide Stop Bit parameter in RTU configuration. Valid Input: {\"ONE\", \"TWO\", \"OnePointFive\"}");
            return IOTHUB_CLIENT_INVALID_ARG;
        }
    }
    else {

        uint8_t stopBitInput = (uint8_t)json_object_dotget_number(configObj, "stopBits");
        if (0 == stopBitInput) {
            LogError("Stop Bit parameter is missing in RTU configuration.");
            return IOTHUB_CLIENT_INVALID_ARG;
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
            return IOTHUB_CLIENT_INVALID_ARG;
            break;
        }
    }

    // Get serial port partiy
    const char* parityStr = (const char*)json_object_dotget_string(configObj, "parity");
    if (NULL == parityStr) {
        LogError("Parity parameter is missing in RTU configuration.");
        return -1;
    }

    if (strcmpcasei(parityStr, "NONE") == 0) {
        deviceConfig->ConnectionConfig.RtuConfig.Parity = NOPARITY;
    }
    else if (strcmpcasei(parityStr, "EVEN") == 0) {
        deviceConfig->ConnectionConfig.RtuConfig.Parity = EVENPARITY;
    }
    else if (strcmpcasei(parityStr, "ODD") == 0) {
        deviceConfig->ConnectionConfig.RtuConfig.Parity = ODDPARITY;
    }
    else if (strcmpcasei(parityStr, "MARK") == 0) {
        deviceConfig->ConnectionConfig.RtuConfig.Parity = MARKPARITY;
    }
    else if (strcmpcasei(parityStr, "SPACE") == 0) {
        deviceConfig->ConnectionConfig.RtuConfig.Parity = SPACEPARITY;
    }
    else {
        LogError("Invalide Stop Bit parameter in RTU configuration. Valid Input: {\"NONE\", \"EVEN\", \"ODD\", \"MARK\", \"SPACE\"}");
        return -1;
    }

    deviceConfig->ConnectionType = RTU;
    return IOTHUB_CLIENT_OK;
}

int ModbusPnP_ParseTcpSettings(
    ModbusDeviceConfig* deviceConfig,
    JSON_Object* configObj)
{
    // Get serial port name
    deviceConfig->ConnectionConfig.TcpConfig.Host = (char*)json_object_dotget_string(configObj, "host");
    if (NULL == deviceConfig->ConnectionConfig.TcpConfig.Host) {
        LogError("\"Host\" parameter is missing in TCP configuration");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    // Get serial dataBits
    deviceConfig->ConnectionConfig.TcpConfig.Port = (uint16_t)json_object_dotget_number(configObj, "port");
    if (0 == deviceConfig->ConnectionConfig.TcpConfig.Port) {
        LogError("\"Port\" is invalide in TCP configuration: Cannot be 0.");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    deviceConfig->ConnectionType = TCP;

    return IOTHUB_CLIENT_OK;
}
#pragma endregion

#pragma region SerialConnection

SINGLYLINKEDLIST_HANDLE ModbusDeviceList = NULL;
int ModbusDeviceCount = 0;

int ModbusPnp_OpenSerial(
    MODBUS_RTU_CONFIG* rtuConfig,
    HANDLE *serialHandle)
{

#ifdef WIN32
    *serialHandle = CreateFileA(rtuConfig->Port,
        GENERIC_READ | GENERIC_WRITE,
        0, // must be opened with exclusive-access
        0, //NULL, no security attributes
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0);
#else
    *serialHandle = open(rtuConfig->Port, O_RDWR | O_NOCTTY);
#endif

    if (*serialHandle == INVALID_HANDLE_VALUE) {
        // Handle the error
        LogError("Failed to open com port %s", rtuConfig->Port);
        return IOTHUB_CLIENT_INVALID_ARG;
    }

#ifdef WIN32
    //  Initialize the DCB structure.
    DCB dcbSerialParams;
    SecureZeroMemory(&dcbSerialParams, sizeof(DCB));
    dcbSerialParams.DCBlength = sizeof(DCB);
    if (!GetCommState(*serialHandle, &dcbSerialParams)) {
        LogError("Failed to open com port %s, %x", rtuConfig->Port, GetLastError());
        return IOTHUB_CLIENT_INVALID_ARG;
    }
    dcbSerialParams.BaudRate = rtuConfig->BaudRate;
    dcbSerialParams.ByteSize = rtuConfig->DataBits;
    dcbSerialParams.StopBits = rtuConfig->StopBits;
    dcbSerialParams.Parity = rtuConfig->Parity;
    if (!SetCommState(*serialHandle, &dcbSerialParams)) {
        // Error setting serial port state
        LogError("Failed to open com port %s, %x", rtuConfig->Port, GetLastError());
        return IOTHUB_CLIENT_INVALID_ARG;
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
        return IOTHUB_CLIENT_INVALID_ARG;
    }
#else
    struct termios settings;
    tcgetattr(*serialHandle, &settings);
    cfsetospeed(&settings, rtuConfig->BaudRate);
    if (rtuConfig->Parity == NOPARITY)
    {
        settings.c_cflag &= ~PARENB;
    }
    else
    {
        settings.c_cflag |= PARENB;
    }
    
    if (rtuConfig->Parity == ODDPARITY)
    {
        settings.c_cflag |= PARODD;
    }
    else if (rtuConfig->Parity == EVENPARITY)
    {
        settings.c_cflag &= ~PARODD;
    }

    if (rtuConfig->StopBits == ONESTOPBIT)
    {
        settings.c_cflag &= ~CSTOPB;
    }
    else if (rtuConfig->StopBits == TWOSTOPBITS)
    {
        settings.c_cflag |= CSTOPB;
    }

    tcsetattr(*serialHandle, TCSANOW, &settings);
    tcflush(*serialHandle, TCOFLUSH);
#endif
    return IOTHUB_CLIENT_OK;
}

int ModbusPnp_OpenSocket(
    MODBUS_TCP_CONFIG* tcpConfig,
    SOCKET *socketHandle)
{
    int result = 0;
#ifdef WIN32
    WSADATA wsaData = { 0 };
    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LogError("WSAStartup failed: %d\n", result);
        return IOTHUB_CLIENT_INVALID_ARG;
    }
#endif

    // Create socket
    *socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*socketHandle == INVALID_SOCKET) {
#ifdef WIN32
        LogError("Failed to create socket with error = %d.", WSAGetLastError());
#else
        LogError("Failed to create socket.");
#endif
        goto ExitOnError;
    }

    // Connect to device
    struct sockaddr_in deviceAddress;
    deviceAddress.sin_family = AF_INET;
    deviceAddress.sin_addr.s_addr = inet_addr(tcpConfig->Host);
    deviceAddress.sin_port = htons(tcpConfig->Port);

    result = connect(*socketHandle, (struct sockaddr *)& deviceAddress, sizeof(deviceAddress));
    if (result == SOCKET_ERROR) {
#ifdef WIN32
        LogError("Failed to connect with socket \"%s:%d\" with error: %ld.", tcpConfig->Host, tcpConfig->Port, WSAGetLastError());
#else
        LogError("Failed to connect with socket \"%s:%d\".", tcpConfig->Host, tcpConfig->Port);
#endif

#ifdef WIN32
        if (SOCKET_ERROR == closesocket(*socketHandle))
        {
            LogError("Failed to close socket with error: %ld.", WSAGetLastError());
        }
#else
        if (!(close(*socketHandle)))
        {
            LogError("Failed to close socket");
        }

#endif
        goto ExitOnError;
    }
    LogInfo("Connected to device at \"%s:%d\".", tcpConfig->Host, tcpConfig->Port);
    return IOTHUB_CLIENT_OK;

ExitOnError:
#ifdef WIN32
    WSACleanup();
#endif
    return IOTHUB_CLIENT_INVALID_ARG;
}

#pragma endregion


#pragma region PnpInterface
void ModbusPnp_FreeTelemetryList(
    SINGLYLINKEDLIST_HANDLE telemetryList)
{
    if (NULL == telemetryList) {
        return;
    }

    LIST_ITEM_HANDLE telemetryItem = singlylinkedlist_get_head_item(telemetryList);
    while (NULL != telemetryItem) {
        ModbusTelemetry* telemetry = (ModbusTelemetry*)singlylinkedlist_item_get_value(telemetryItem);
        if (telemetry->Name)
        {
            free(telemetry->Name);
        }
        free(telemetry);
        telemetryItem = singlylinkedlist_get_next_item(telemetryItem);
    }
}

void ModbusPnp_FreeCommandList(
    SINGLYLINKEDLIST_HANDLE commandList)
{
    if (NULL == commandList) {
        return;
    }

    LIST_ITEM_HANDLE commandItem = singlylinkedlist_get_head_item(commandList);
    while (NULL != commandItem) {
        ModbusCommand* command = (ModbusCommand*)singlylinkedlist_item_get_value(commandItem);
        if (command->Name)
        {
            free(command->Name);
        }
        free(command);
        commandItem = singlylinkedlist_get_next_item(commandItem);
    }
}

void ModbusPnp_FreePropertyList(
    SINGLYLINKEDLIST_HANDLE propertyList)
{
    if (NULL == propertyList) {
        return;
    }

    LIST_ITEM_HANDLE propertyItem = singlylinkedlist_get_head_item(propertyList);
    while (NULL != propertyItem) {
        ModbusProperty* property = (ModbusProperty*)singlylinkedlist_item_get_value(propertyItem);
        if (property->Name)
        {
            free(property->Name);
        }
        free(property);
        propertyItem = singlylinkedlist_get_next_item(propertyItem);
    }
}

void Modbus_CleanupPollingTasks(
    PMODBUS_DEVICE_CONTEXT deviceContext)
{
    if (NULL != deviceContext->PollingTasks)
    {
        StopPollingTasks();
        const ModbusInterfaceConfig* interfaceConfig = (PModbusInterfaceConfig)(deviceContext->InterfaceConfig);
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

IOTHUB_CLIENT_RESULT
Modbus_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    PMODBUS_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);
    if (deviceContext == NULL)
    {
        LogError("Modbus_StartPnpComponent: Device context is null, cannot start interface.");
        return IOTHUB_CLIENT_ERROR;
    }

    // Assign client handle
    if (deviceContext->ClientType == PNP_BRIDGE_IOT_TYPE_DEVICE)
    {
        deviceContext->DeviceClient = PnpComponentHandleGetIotHubDeviceClient(PnpComponentHandle);
    }
    else
    {
        deviceContext->ModuleClient = PnpComponentHandleGetIotHubModuleClient(PnpComponentHandle);
    }

    PnpComponentHandleSetContext(PnpComponentHandle, deviceContext);

    // Start polling all telemetry
    return ModbusPnp_StartPollingAllTelemetryProperty(deviceContext);
}

IOTHUB_CLIENT_RESULT Modbus_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PMODBUS_ADAPTER_CONTEXT adapterContext = PnpAdapterHandleGetContext(AdapterHandle);
    if (adapterContext == NULL)
    {
        return result;
    }

    if (adapterContext->InterfaceDefinitions)
    {
        LIST_ITEM_HANDLE interfaceDef = singlylinkedlist_get_head_item(adapterContext->InterfaceDefinitions);
        while (interfaceDef != NULL) {
            ModbusInterfaceConfig* def = (ModbusInterfaceConfig*)singlylinkedlist_item_get_value(interfaceDef);
            ModbusPnp_FreeTelemetryList(def->Events);
            ModbusPnp_FreeCommandList(def->Commands);
            ModbusPnp_FreePropertyList(def->Properties);

            free(def);
            interfaceDef = singlylinkedlist_get_next_item(interfaceDef);
        }
        singlylinkedlist_destroy(adapterContext->InterfaceDefinitions);
    }
    free(adapterContext);
    return result;
}

IOTHUB_CLIENT_RESULT Modbus_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PMODBUS_ADAPTER_CONTEXT adapterContext = calloc(1, sizeof(MODBUS_ADAPTER_CONTEXT));
    if (!adapterContext)
    {
        LogError("Could not allocate memory for adapter context.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    adapterContext->InterfaceDefinitions = singlylinkedlist_create();

    if (AdapterGlobalConfig == NULL)
    {
        LogError("Modbus adapter requires associated global parameters in config");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    // Build list of interface configs supported by the adapter
    for (int i = 0; i < (int)json_object_get_count(AdapterGlobalConfig); i++) {
        const char* modbusConfigIdentity = json_object_get_name(AdapterGlobalConfig, i);
        ModbusInterfaceConfig* interfaceConfig = calloc(1, sizeof(ModbusInterfaceConfig));
        if (!interfaceConfig)
        {
            LogError("Could not allocate memory for interface configuration.");
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        if (0 != mallocAndStrcpy_s((char**)(&interfaceConfig->Id), modbusConfigIdentity))
        {
            LogError("Could not allocate memory for interface configuration's identity");
            result = IOTHUB_CLIENT_ERROR;
            free(interfaceConfig);
            goto exit;
        }

        JSON_Object* interfaceConfigJson = json_object_get_object(AdapterGlobalConfig, modbusConfigIdentity);

        if (IOTHUB_CLIENT_OK != ModbusPnp_ParseInterfaceConfig(&interfaceConfig, interfaceConfigJson))
        {
            LogError("Could not parse interface config definition for %s", (char*)interfaceConfig->Id);
            result = IOTHUB_CLIENT_INVALID_ARG;
            goto exit;
        }

        singlylinkedlist_add(adapterContext->InterfaceDefinitions, interfaceConfig);

    }

    PnpAdapterHandleSetContext(AdapterHandle, (void*)adapterContext);

exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        result = Modbus_DestroyPnpAdapter(AdapterHandle);
    }
    return result;
}

IOTHUB_CLIENT_RESULT Modbus_RetrieveMatchingInterfaceConfig(
    const char* ModbusId,
    SINGLYLINKEDLIST_HANDLE InterfaceDefinitions,
    PMODBUS_DEVICE_CONTEXT DeviceContext)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_INVALID_ARG;
    if (ModbusId == NULL || InterfaceDefinitions == NULL || DeviceContext == NULL)
    {
        LogError("Modbus_RetrieveMatchingInterfaceConfig: Invalid arguments.");
        return result;
    }

    LIST_ITEM_HANDLE interfaceDefinition = singlylinkedlist_get_head_item(InterfaceDefinitions);

    while (NULL != interfaceDefinition) {

        PModbusInterfaceConfig interfaceConfig = (PModbusInterfaceConfig)singlylinkedlist_item_get_value(interfaceDefinition);
        if (strcmp(interfaceConfig->Id, ModbusId) == 0)
        {
            DeviceContext->InterfaceConfig = interfaceConfig;
            result = IOTHUB_CLIENT_OK;
        }
        interfaceDefinition = singlylinkedlist_get_next_item(interfaceDefinition);
    }

    return result;
}

IOTHUB_CLIENT_RESULT Modbus_ParseDeviceConfig(
    const JSON_Object* AdapterComponentConfig,
    PModbusDeviceConfig DeviceConfig)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    if (AdapterComponentConfig == NULL || DeviceConfig == NULL)
    {
        LogError("Modbus_ParseDeviceConfig: Invalid arguments.");
        return result;
    }

    DeviceConfig->UnitId = (uint8_t)json_object_get_number(AdapterComponentConfig, PNP_CONFIG_ADAPTER_INTERFACE_UNITID);
    JSON_Object* rtuArgs = json_object_get_object(AdapterComponentConfig, PNP_CONFIG_ADAPTER_INTERFACE_RTU);
    if (NULL != rtuArgs && ModbusPnp_ParseRtuSettings(DeviceConfig, rtuArgs) != IOTHUB_CLIENT_OK) {
        LogError("Failed to parse RTU connection settings.");
        result =  IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    JSON_Object* tcpArgs = json_object_get_object(AdapterComponentConfig, PNP_CONFIG_ADAPTER_INTERFACE_TCP);
    if (NULL != tcpArgs && ModbusPnP_ParseTcpSettings(DeviceConfig, tcpArgs) != IOTHUB_CLIENT_OK) {
        LogError("Failed to parse RTU connection settings.");
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    if (UNKOWN == DeviceConfig->ConnectionType) {
        LogError("Missing Modbus connection settings.");
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }
exit:
    return result;
}

IOTHUB_CLIENT_RESULT Modbus_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PMODBUS_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);

    if (NULL == deviceContext) {
        return IOTHUB_CLIENT_OK;
    }

    if (NULL != deviceContext->DeviceConfig)
    {
        free(deviceContext->DeviceConfig);
    }

    if (NULL != deviceContext->ComponentName)
    {
        free(deviceContext->ComponentName);
    }

    if (NULL != deviceContext)
    {
        free(deviceContext);
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT
Modbus_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PMODBUS_DEVICE_CONTEXT deviceContext = calloc(1, sizeof(MODBUS_DEVICE_CONTEXT));
    if (NULL == deviceContext)
    {
        LogError("Could not allocate memory for device context.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    // Assign client handle
    if (PnpComponentHandleGetIoTType(BridgeComponentHandle) == PNP_BRIDGE_IOT_TYPE_DEVICE)
    {
        deviceContext->ClientType = PNP_BRIDGE_IOT_TYPE_DEVICE;
    }
    else
    {
        deviceContext->ClientType = PNP_BRIDGE_IOT_TYPE_RUNTIME_MODULE;
    }

    // Allocate and copy component name into device context
    mallocAndStrcpy_s((char**)&deviceContext->ComponentName, ComponentName);

    // Populate interface config from adapter's supported interface definitions

    PMODBUS_ADAPTER_CONTEXT adapterContext = PnpAdapterHandleGetContext(AdapterHandle);
    const char* modbusIdentity = json_object_dotget_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_MODBUS_IDENTITY);
    result = Modbus_RetrieveMatchingInterfaceConfig(modbusIdentity, adapterContext->InterfaceDefinitions, deviceContext);
    if (IOTHUB_CLIENT_OK != result)
    {
        LogError("Could not find matching modbus interface configuration for this interface");
        goto exit;
    }

    // Initialize interface's connection lock

    deviceContext->hConnectionLock = Lock_Init();
    if (NULL == deviceContext->hConnectionLock)
    {
        result = IOTHUB_CLIENT_ERROR;
        LogError("Failed to create a valid lock handle for device connection.");
        goto exit;
    }

    // Parse interface specific device config
    ModbusDeviceConfig* deviceConfig = calloc(1, sizeof(ModbusDeviceConfig));
    if (!deviceConfig)
    {
        LogError("Could not allocate memory for device config.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }
    deviceConfig->ConnectionType = UNKOWN;
    result = Modbus_ParseDeviceConfig(AdapterComponentConfig, deviceConfig);
    if (result != IOTHUB_CLIENT_OK)
    {
        LogError("Could not parse device configuration for this modbus interface.");
        goto exit;
    }

    // Open the device
    if (deviceConfig->ConnectionType == RTU)
    {
        result = ModbusPnp_OpenSerial(&(deviceConfig->ConnectionConfig.RtuConfig), 
                    &(deviceContext->hDevice));
        if (IOTHUB_CLIENT_OK != result)
        {
            LogError("Failed to open serial connection to \"%s\".", 
                        deviceConfig->ConnectionConfig.RtuConfig.Port);
            goto exit;
        }
    }
    else if (deviceConfig->ConnectionType == TCP)
    {
        result = ModbusPnp_OpenSocket(&(deviceConfig->ConnectionConfig.TcpConfig), 
                    (SOCKET*)&(deviceContext->hDevice));
        if (IOTHUB_CLIENT_OK != result)
        {
            LogError("Failed to open socket connection to \"%s:%d\".", 
                deviceConfig->ConnectionConfig.TcpConfig.Host, 
                deviceConfig->ConnectionConfig.TcpConfig.Port);
            goto exit;
        }
    }
    else
    {
        goto exit;
    }

    deviceContext->DeviceConfig = deviceConfig;

    // Set read requests for telemetry
    if (NULL != deviceContext->InterfaceConfig->Events)
    {
        LIST_ITEM_HANDLE telemetryHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceConfig->Events);

        while (NULL != telemetryHandle) {
            PModbusTelemetry telemetry = (PModbusTelemetry)singlylinkedlist_item_get_value(telemetryHandle);
            result = ModbusPnp_SetReadRequest(deviceConfig, Telemetry, telemetry);
            if (IOTHUB_CLIENT_OK != result)
            {
                LogError("Failed to create read request for telemetry \"%s\".", telemetry->Name);
                return IOTHUB_CLIENT_INVALID_ARG;
            }
            telemetryHandle = singlylinkedlist_get_next_item(telemetryHandle);
        }
    }

    // Set read requests for property
    if (NULL != deviceContext->InterfaceConfig->Properties)
    {
        LIST_ITEM_HANDLE propertyhandle = singlylinkedlist_get_head_item(deviceContext->InterfaceConfig->Properties);

        while (NULL != propertyhandle) {
            PModbusProperty property = (PModbusProperty)singlylinkedlist_item_get_value(propertyhandle);
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

            result = ModbusPnp_SetReadRequest(deviceConfig, Property, property);
            if (IOTHUB_CLIENT_OK != result)
            {
                LogError("Failed to create read request for telemetry \"%s\".", property->Name);
                return IOTHUB_CLIENT_INVALID_ARG;
            }
            propertyhandle = singlylinkedlist_get_next_item(propertyhandle);
        }
    }

    // Set read requests for commands
    if (NULL != deviceContext->InterfaceConfig->Commands)
    {
        LIST_ITEM_HANDLE commandHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceConfig->Commands);

        while (NULL != commandHandle) {
            PModbusCommand command = (PModbusCommand)singlylinkedlist_item_get_value(commandHandle);
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
            commandHandle = singlylinkedlist_get_next_item(commandHandle);
        }
    }

    int propertyCount = 0;
    int commandCount = 0;

    // Construct a property table
    if (NULL != deviceContext->InterfaceConfig->Properties)
    {
        MODBUS_PROPERTY_UPDATE_CALLBACK* propertyUpdateTable = NULL;
        char** propertyNames = NULL;
        SINGLYLINKEDLIST_HANDLE propertyList = deviceContext->InterfaceConfig->Properties;
        propertyCount = ModbusPnp_GetListCount(deviceContext->InterfaceConfig->Properties);

        int readWritePropertyCount = 0;
        LIST_ITEM_HANDLE propertyItemHandle = NULL;
        SINGLYLINKEDLIST_HANDLE readWritePropertyList = singlylinkedlist_create();

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

            if (READ_WRITE == property->Access)
            {
                readWritePropertyCount++;
                singlylinkedlist_add(readWritePropertyList, property);
            }
        }

        if (readWritePropertyCount > 0)
        {
            propertyNames = calloc(1, sizeof(char*) * readWritePropertyCount);
            if (NULL == propertyNames) {
                result = IOTHUB_CLIENT_ERROR;
                goto exit;
            }

            propertyUpdateTable = calloc(1, sizeof(MODBUS_PROPERTY_UPDATE_CALLBACK*) * readWritePropertyCount);
            if (NULL == propertyUpdateTable) {
                result = IOTHUB_CLIENT_ERROR;
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

                propertyNames[i] = (char*)property->Name;
                propertyUpdateTable[i] = ModbusPnp_PropertyHandler;
            }
        }
    }

    // Construct command table
    if (NULL != deviceContext->InterfaceConfig->Commands)
    {
        char** commandNames = NULL;
        MODBUS_COMMAND_EXECUTE_CALLBACK* commandUpdateTable = NULL;
        SINGLYLINKEDLIST_HANDLE commandList = deviceContext->InterfaceConfig->Commands;
        commandCount = ModbusPnp_GetListCount(commandList);

        if (commandCount > 0)
        {
            commandNames = calloc(1, sizeof(char*) * commandCount);
            if (NULL == commandNames) {
                result = IOTHUB_CLIENT_ERROR;
                goto exit;
            }

            commandUpdateTable = calloc(1, sizeof(MODBUS_COMMAND_EXECUTE_CALLBACK*) * commandCount);
            if (NULL == commandUpdateTable) {
                result = IOTHUB_CLIENT_ERROR;
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
                    commandNames[i] = (char*)command->Name;
                    commandUpdateTable[i] = ModbusPnp_CommandHandler;
                }
            }
        }
    }

    PnpComponentHandleSetContext(BridgeComponentHandle, deviceContext);
    PnpComponentHandleSetPropertyUpdateCallback(BridgeComponentHandle, ModbusPnp_PropertyHandler);
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, ModbusPnp_CommandHandler);

exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        Modbus_DestroyPnpComponent(BridgeComponentHandle);
    }

    return result;
}

IOTHUB_CLIENT_RESULT Modbus_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PMODBUS_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);
    if (NULL == deviceContext) {
        return IOTHUB_CLIENT_OK;
    }

    Modbus_CleanupPollingTasks(deviceContext);

    if (INVALID_FILE != deviceContext->hDevice) {

        ModbusPnp_CloseDevice(deviceContext->DeviceConfig->ConnectionType, deviceContext->hDevice, 
            deviceContext->hConnectionLock);
    }

    if (NULL != deviceContext->hConnectionLock) {
        Lock_Deinit(deviceContext->hConnectionLock);
    }

    return IOTHUB_CLIENT_OK;
}

#pragma endregion

PNP_ADAPTER ModbusPnpInterface = {
    .identity = "modbus-pnp-interface",
    .createAdapter = Modbus_CreatePnpAdapter,
    .createPnpComponent = Modbus_CreatePnpComponent,
    .startPnpComponent = Modbus_StartPnpComponent,
    .stopPnpComponent = Modbus_StopPnpComponent,
    .destroyPnpComponent = Modbus_DestroyPnpComponent,
    .destroyAdapter = Modbus_DestroyPnpAdapter
};