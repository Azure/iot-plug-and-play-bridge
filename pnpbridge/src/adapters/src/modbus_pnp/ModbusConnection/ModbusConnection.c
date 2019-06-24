
#include "ModbusConnection.h"

int ModbusPnp_GetHeaderSize(MODBUS_CONNECTION_TYPE connectionType)
{
	switch (connectionType)
	{
	case TCP:
		return ModbusTcp_GetHeaderSize();;
	case RTU:
		return ModbusRtu_GetHeaderSize();
	default:
		break;
	}
	return 0;
}

bool ValidateModbusResponse(MODBUS_CONNECTION_TYPE connectionType, byte* response, byte* request)
{
	if (NULL == response)
	{
		return false;
	}

	const int HEADER_SIZE = ModbusPnp_GetHeaderSize(connectionType);

	if (response[HEADER_SIZE] == request[HEADER_SIZE])
	{
		return true;
	}
	else if (response[HEADER_SIZE] == request[HEADER_SIZE] + MODBUS_EXCEPTION_CODE)
	{
		LogError("Modbus exception code: 0x%x", response[HEADER_SIZE + 1]);
		return false;
	}
	else
	{
		LogError("Error in parsing response. ");
		return false;
	}
}

int ProcessModbusResponse(MODBUS_CONNECTION_TYPE connectionType, CapabilityType capabilityType, void* capability, byte* response, size_t responseLength, byte* result)
{
	int stepSize = 0;
	int dataOffset = ModbusPnp_GetHeaderSize(connectionType);
	int resultLength = -1;

	// Get excpectd number of bits or registers in the payload
	switch (response[dataOffset])//function code
	{
	case ReadCoils:
	case ReadInputs:
		stepSize = 1;
		dataOffset += 2;    // Header + Function Code (1 byte) + Data Length (1 byte)
		break;
	case ReadHoldingRegisters:
	case ReadInputRegisters:
		stepSize = 2;
		dataOffset += 2;    // Header + Function Code (1 byte) + Data Length (1 byte)
		break;
	case WriteCoil:
		stepSize = 1;
		dataOffset += 3;    // Header + Function Code (1 byte) + Address (2 byte)
		break;
	case WriteHoldingRegister:
		stepSize = 2;
		dataOffset += 3;    // Header + Function Code (1 byte) + Address (2 byte)
		break;
	}

	ModbusDataType dataType = INVALID;
	int dataLength = 0;
	double conversionCoefficient = 1.0;
	switch (capabilityType) {
	case Telemetry:
	{
		ModbusTelemetry* telemetry = (ModbusTelemetry*)capability;
		dataType = telemetry->DataType;
		dataLength = telemetry->Length;
		conversionCoefficient = telemetry->ConversionCoefficient;
		break;
	}
	case Property:
	{
		ModbusProperty* property = (ModbusProperty*)capability;
		dataType = property->DataType;
		dataLength = property->Length;
		conversionCoefficient = property->ConversionCoefficient;
		break;
	}
	case Command:
	{
		ModbusCommand* command = (ModbusCommand*)capability;
		dataType = command->DataType;
		dataLength = command->Length;
		conversionCoefficient = command->ConversionCoefficient;
		break;
	}
	}

	//Check Endian-ness

	if (stepSize == 1)
	{
		//Read bits (1 bit)
		byte bitVal = (byte)(response[dataOffset] & 0b1);
		const char* data = (bitVal == (byte)1) ? "true" : "false";
		resultLength = sprintf_s((char*)result, MODBUS_RESPONSE_MAX_LENGTH, "%s", data);
	}
	else if (stepSize == 2)
	{
		double value = 0.0;
		switch (dataType)
		{
		case HEXSTRING:
		{
			byte *strBuffer = calloc((dataLength * 2) + 1, sizeof(byte));
			if (NULL == strBuffer)
			{
				LogError("Failed to convert the response to string: Memory allocation failed.");
				return -1;
			}
			//reverse data array
			for (int i = 0; i < dataLength * 2; i++)
			{
				strBuffer[(dataLength * 2) - i - 1] = response[dataOffset + i];
			}

			byte temp = 0x00;
			for (int i = 0; i < dataLength; i++)
			{
				temp = strBuffer[i * 2];
				strBuffer[i * 2] = strBuffer[(i * 2) + 1];
				strBuffer[(i * 2) + 1] = temp;
			}

			//Convert Hex string
			char* strPtr = (char*)result;
			*(strPtr++) = '\"';
			resultLength = 1;
			for (int i = 0; i < (dataLength * 2); i++)
			{
				int length = 0;
				length = sprintf_s(strPtr, MODBUS_RESPONSE_MAX_LENGTH - resultLength - 2, "%02X", (byte)strBuffer[i]);
				strPtr += length;
				resultLength += length;
			}

			if (resultLength < (MODBUS_RESPONSE_MAX_LENGTH - 2))
			{
				result[resultLength++] = '\"';
				result[resultLength++] = '\0';
			}
			free(strBuffer);
			return resultLength;
		}
		case STRING:
		{
			byte *strBuffer = calloc((dataLength * 2) + 1, sizeof(byte));
			if (NULL == strBuffer)
			{
				LogError("Failed to convert the response to string: Memory allocation failed.");
				return -1;
			}
			//reverse data array
			for (int i = 0; i < dataLength * 2; i++)
			{
				strBuffer[(dataLength * 2) - i - 1] = response[dataOffset + i];
			}

			char temp = 0x00;
			for (int i = 0; i < dataLength; i++)
			{
				temp = strBuffer[i * 2];
				strBuffer[i * 2] = strBuffer[(i * 2) + 1];
				strBuffer[(i * 2) + 1] = temp;
			}

			//Convert non-alpha-numberic char to Hex char
			char* strPtr = (char*)result;
			*(strPtr++) = '\"';
			resultLength = 1;
			for (int i = 0; i < (dataLength * 2); i++)
			{
				int length = 0;
				if (iscsym(strBuffer[i]) != 0)
				{
					length = sprintf_s(strPtr, MODBUS_RESPONSE_MAX_LENGTH - resultLength - 2, "%c", (char)strBuffer[i]);
				}
				else
				{
					length = sprintf_s(strPtr, MODBUS_RESPONSE_MAX_LENGTH - resultLength - 2, "%02X", (byte)strBuffer[i]);
				}

				strPtr += length;
				resultLength += length;
			}

			if (resultLength < (MODBUS_RESPONSE_MAX_LENGTH - 2))
			{
				result[resultLength++] = '\"';
				result[resultLength++] = '\0';
			}
			free(strBuffer);
			return resultLength;
		}
		case NUMERIC:
		{
			int n = 1;
			if (dataLength > 1 && *(char *)&n == 1)
			{
				//Is little Endian, reverse response array
				byte temp = 0x00;
				for (size_t i = dataOffset, j = responseLength - 1; i < ((responseLength - dataOffset) / 2); i++, j--)
				{
					temp = response[i];
					response[i] = response[j];
					response[j] = temp;
				}
			}
			switch (dataLength)
			{
			case 1:
			{
				UINT16 rawValue = ((response[dataOffset] << 8) + response[dataOffset + 1]);
				value = rawValue * conversionCoefficient;
				break;
			}
			case 2:
			{
				UINT32 rawValue = 0;
				for (int i = 0; i < dataLength; i++)
				{
					rawValue <<= 16;
					rawValue += ((response[dataOffset + i * 2] << 8) + response[dataOffset + 1]);
				}
				value = rawValue * conversionCoefficient;
				break;
			}
			case 4:
			{
				UINT64 rawValue = 0;
				for (int i = 0; i < dataLength; i++)
				{
					rawValue <<= 16;
					rawValue += ((response[dataOffset + i * 2] << 8) + response[dataOffset + 1]);
				}
				value = rawValue * conversionCoefficient;
				break;
			}
			}
			break;
		}
		default:
			LogError("Unsupported datatype.");
			return -1;
		}
		resultLength = sprintf_s((char*)result, MODBUS_RESPONSE_MAX_LENGTH, "%g", value);
	}
	return resultLength;
}
bool ModbusPnp_CloseDevice(MODBUS_CONNECTION_TYPE connectionType, HANDLE *hDevice, HANDLE hLock)
{
	bool result = false;
	switch (connectionType)
	{
	case TCP:
		result = ModbusTcp_CloseDevice((SOCKET)hDevice, hLock);
		break;
	case RTU:
		result = ModbusRtu_CloseDevice(hDevice, hLock);
		break;
	default:
		break;
	}
	return result;
}

int ModbusPnp_SetReadRequest(ModbusDeviceConfig* deviceConfig, CapabilityType capabilityType, void* capability)
{
	int result = -1;
	switch (deviceConfig->ConnectionType)
	{
		case TCP:
			result = ModbusTcp_SetReadRequest(capabilityType, capability, deviceConfig->UnitId);
			break;
		case RTU:
			result = ModbusRtu_SetReadRequest(capabilityType, capability, deviceConfig->UnitId);
			break;
		default:
			break;
	}
	return result;
}

int ModbusPnp_SetWriteRequest(MODBUS_CONNECTION_TYPE connectionType, CapabilityType capabilityType, void* capability, char* valueStr)
{
	int result = -1;
	switch (connectionType)
	{
		case TCP:
			result = ModbusTcp_SetWriteRequest(capabilityType, capability, valueStr);
			break;
		case RTU:
			result = ModbusRtu_SetWriteRequest(capabilityType, capability, valueStr);
			break;
		default:
			break;

	}
	return result;
}

int ModbusPnp_ReadResponse(MODBUS_CONNECTION_TYPE connectionType, void *handler, byte *responseArr, DWORD arrLen)
{
	int result = -1;
	switch (connectionType)
	{
		case TCP:
			result = ModbusTcp_ReadResponse((SOCKET)handler, responseArr, arrLen);
			break;
		case RTU:
			result = ModbusRtu_ReadResponse(handler, responseArr, arrLen);
			break;
		default:
			break;
	}
	return result;
}

int ModbusPnp_SendRequest(MODBUS_CONNECTION_TYPE connectionType, void *handler, byte *requestArr, DWORD arrLen)
{
	int result = -1;
	switch (connectionType)
	{
	case TCP:
		result = ModbusTcp_SendRequest((SOCKET)handler, requestArr, arrLen);
		break;
	case RTU:
		result = ModbusRtu_SendRequest(handler, requestArr, arrLen);
		break;
	default:
		break;
	}
	return result;
}

int ModbusPnp_ReadCapability(CapabilityContext* capabilityContext, CapabilityType capabilityType, byte* resultedData)
{
	byte response[MODBUS_RESPONSE_MAX_LENGTH];
	memset(response, 0x00, MODBUS_RESPONSE_MAX_LENGTH);
	size_t responseLength = 0;

	int resultLength = -1;

	const char* capabilityName = NULL;
	byte* requestArr = NULL;
	int requestArrSize = 0;

	switch (capabilityType)
	{
		case Telemetry:
		{
			ModbusTelemetry* telemetry = (ModbusTelemetry*) (capabilityContext->capability);
			capabilityName = telemetry->Name;

			switch (capabilityContext->connectionType)
			{
				case TCP:
					requestArr = telemetry->ReadRequest.TcpArr;
					requestArrSize = sizeof(telemetry->ReadRequest.TcpArr);
					break;
				case RTU:
					requestArr = telemetry->ReadRequest.RtuArr;
					requestArrSize = sizeof(telemetry->ReadRequest.RtuArr);
					break;
				default:
					break;
			}
			break;
		}
		case Property:
		{
			ModbusProperty* property = (ModbusProperty*) (capabilityContext->capability);
			capabilityName = property->Name;
			switch (capabilityContext->connectionType)
			{
				case TCP:
					requestArr = property->ReadRequest.TcpArr;
					requestArrSize = sizeof(property->ReadRequest.TcpArr);
					break;
				case RTU:
					requestArr = property->ReadRequest.RtuArr;
					requestArrSize = sizeof(property->ReadRequest.RtuArr);
					break;
				default:
					break;
			}
			break;
		}
	default:
		LogError("Modbus read is not supported for the capability \"%s\".", capabilityName);
		resultLength = -1;
		goto exit;
	}

	DWORD waitResult = WaitForSingleObject(capabilityContext->hLock, INFINITE);
	if (WAIT_ABANDONED == waitResult)
	{
		LogError("Device communicate lock is abandoned: %d", GetLastError());
		resultLength = -1;
		goto exit;
	}

	if (requestArrSize != ModbusPnp_SendRequest(capabilityContext->connectionType, capabilityContext->hDevice, requestArr, requestArrSize))
	{
		LogError("Failed to send read request for capability \"%s\": %x", capabilityName, GetLastError());
		resultLength = -1;
		goto exit;
	}

	responseLength = ModbusPnp_ReadResponse(capabilityContext->connectionType, capabilityContext->hDevice, response, MODBUS_RESPONSE_MAX_LENGTH);
	if (responseLength < 0)
	{
		LogError("Failed to get read response for capability \"%s\": %x", capabilityName, GetLastError());
		resultLength = -1;
		goto exit;
	}

	if (ValidateModbusResponse(capabilityContext->connectionType, response, requestArr))
	{
		resultLength = ProcessModbusResponse(capabilityContext->connectionType, capabilityType, capabilityContext->capability, response, responseLength, resultedData);

		if (resultLength < 0)
		{
			LogError("Failed to parse response for reading capability \"%s\": %x", capabilityName, GetLastError());
			resultLength = -1;
			goto exit;
		}
	}
	else
	{
		LogError("Invalid response for reading capability \"%s\": %x", capabilityName, GetLastError());
		resultLength = -1;
		goto exit;

	}

exit:
	ReleaseMutex(capabilityContext->hLock);
	return resultLength;
}

int ModbusPnp_WriteToCapability(CapabilityContext* capabilityContext, CapabilityType capabilityType, char* requestStr, byte* resultedData)
{
	byte response[MODBUS_RESPONSE_MAX_LENGTH];
	memset(response, 0x00, MODBUS_RESPONSE_MAX_LENGTH);
	size_t responseLength = 0;
	int resultLength = -1;

	const char* capabilityName = NULL;
	byte* requestArr = NULL;
	int requestArrSize = 0;

	switch (capabilityType)
	{
		case Command:
		{
			ModbusCommand* command = (ModbusCommand*)(capabilityContext->capability);
			capabilityName = command->Name;
			if (0 != ModbusPnp_SetWriteRequest(capabilityContext->connectionType, Command, command, requestStr))
			{
				LogError("Failed to create write request for command \"%s\": 0x%x ", capabilityName, GetLastError());
				return -1;
			}

			switch (capabilityContext->connectionType)
			{
				case TCP:
					requestArr = command->WriteRequest.TcpArr;
					requestArrSize = sizeof(command->WriteRequest.TcpArr);
					break;
				case RTU:
					requestArr = command->WriteRequest.RtuArr;
					requestArrSize = sizeof(command->WriteRequest.RtuArr);
					break;
				default:
					break;
			}
			break;
		}
		case Property:
		{
			ModbusProperty* property = (ModbusProperty*)(capabilityContext->capability);
			capabilityName = property->Name;
			if (0 != ModbusPnp_SetWriteRequest(capabilityContext->connectionType, Property, property, requestStr))
			{
				LogError("Failed to create write request for writable property \"%s\": 0x%x ", capabilityName, GetLastError());
				return -1;
			}

			switch (capabilityContext->connectionType)
			{
				case TCP:
					requestArr = property->WriteRequest.TcpArr;
					requestArrSize = sizeof(property->WriteRequest.TcpArr);
					break;
				case RTU:
					requestArr = property->WriteRequest.RtuArr;
					requestArrSize = sizeof(property->WriteRequest.RtuArr);
					break;
				default:
					break;
			}
			break;
		}
		default:
			LogError("Modbus write is not supported for the capability.");
			resultLength = -1;
			goto exit;
	}

	DWORD waitResult = WaitForSingleObject(capabilityContext->hLock, INFINITE);
	if (WAIT_ABANDONED == waitResult)
	{
		LogError("Device communicate lock is abandoned: %d", GetLastError());
		resultLength = -1;
		goto exit;
	}

	if (requestArrSize != ModbusPnp_SendRequest(capabilityContext->connectionType, capabilityContext->hDevice, requestArr, requestArrSize))
	{
		LogError("Failed to send write request for capability \"%s\": %x", capabilityName, GetLastError());
		resultLength = -1;
		goto exit;
	}

	responseLength = ModbusPnp_ReadResponse(capabilityContext->connectionType, capabilityContext->hDevice, response, MODBUS_RESPONSE_MAX_LENGTH);
	if (responseLength < 0)
	{
		LogError("Failed to get write response for capability \"%s\": %x", capabilityName, GetLastError());
		resultLength = -1;
		goto exit;
	}

	if (ValidateModbusResponse(capabilityContext->connectionType, response, requestArr))
	{
		resultLength = ProcessModbusResponse(capabilityContext->connectionType, capabilityType, capabilityContext->capability, response, responseLength, resultedData);
		if (resultLength < 0)
		{
			LogError("Failed to parse response for command \"%s\": %x", capabilityName, GetLastError());
			resultLength = -1;
			goto exit;
		}
	}
	else
	{
		LogError("Invalid response for command \"%s\": %x", capabilityName, GetLastError());
		resultLength = -1;
		goto exit;
	}

exit:
	ReleaseMutex(capabilityContext->hLock);
	return resultLength;
}