#include "ModbusTCPConnection.h"

// timeout interval for waiting for socket readiness
#define SOCKET_TIMEOUT_SEC	5
#define SOCKET_TIMEOUT_USEC	0

#ifdef WIN32
struct timeval socket_timeout;
#else
#define INFINITE 0xFFFFFFFF
#endif // WIN32


int ModbusTcp_GetHeaderSize(void) {
	return TCP_HEADER_SIZE;
}

bool ModbusTcp_CloseDevice(SOCKET socket, LOCK_HANDLE hLock)
{   
	if (LOCK_OK == Lock(hLock))
	{
		LogError("Device communicate lock could not be acquired.");
		return false;
	}
	shutdown(socket, 1);

	int byteRcvd = 1;
	BYTE buffer[MODBUS_RESPONSE_MAX_LENGTH];
	while (0 < byteRcvd)
	{
		byteRcvd = recv(socket, (char*)buffer, MODBUS_RESPONSE_MAX_LENGTH, 0);
	}
#ifdef WIN32
    closesocket(socket);
	WSACleanup();
#else
    close(socket);
#endif
    Unlock(hLock);

	LogInfo("Socket Closed.");
	return true;
}

int ModbusTcp_SetReadRequest(CapabilityType capabilityType, void* capability, byte unitId)
{
	switch (capabilityType)
	{
		case Telemetry:
		{
			ModbusTelemetry* telemetry = (ModbusTelemetry*)capability;
			UINT16 modbusAddress = 0;

			if (!ModbusConnectionHelper_GetFunctionCode(telemetry->StartAddress, true, &(telemetry->ReadRequest.TcpRequest.Payload.FunctionCode), &modbusAddress))
			{
				LogError("Failed to get Modbus function code for telemetry \"%s\".", telemetry->Name);
				return -1;
			}

			telemetry->ReadRequest.TcpRequest.MBAP.ProtocolID_Hi = 0x00;
			telemetry->ReadRequest.TcpRequest.MBAP.ProtocolID_Lo = 0x00;
			telemetry->ReadRequest.TcpRequest.MBAP.Length_Hi = 0x00;
			telemetry->ReadRequest.TcpRequest.MBAP.Length_Lo = 0x06;
			telemetry->ReadRequest.TcpRequest.MBAP.UnitID = unitId;

			telemetry->ReadRequest.TcpRequest.Payload.StartAddr_Hi = (modbusAddress >> 8) & 0xff;
			telemetry->ReadRequest.TcpRequest.Payload.StartAddr_Lo = modbusAddress & 0xff;
			telemetry->ReadRequest.TcpRequest.Payload.ReadLen_Hi = (telemetry->Length >> 8) & 0xff;
			telemetry->ReadRequest.TcpRequest.Payload.ReadLen_Lo = telemetry->Length & 0xff;
			break;
		}
		case Property:
		{
			ModbusProperty* property = (ModbusProperty*)capability;
			UINT16 modbusAddress = 0;
			if (!ModbusConnectionHelper_GetFunctionCode(property->StartAddress, true, &(property->ReadRequest.TcpRequest.Payload.FunctionCode), &modbusAddress))
			{
				LogError("Failed to get Modbus function code for property \"%s\".", property->Name);
				return -1;
			}

			property->ReadRequest.TcpRequest.MBAP.ProtocolID_Hi = 0x00;
			property->ReadRequest.TcpRequest.MBAP.ProtocolID_Lo = 0x00;
			property->ReadRequest.TcpRequest.MBAP.Length_Hi = 0x00;
			property->ReadRequest.TcpRequest.MBAP.Length_Lo = 0x06;
			property->ReadRequest.TcpRequest.MBAP.UnitID = unitId;

			property->ReadRequest.TcpRequest.Payload.StartAddr_Hi = (modbusAddress >> 8) & 0xff;
			property->ReadRequest.TcpRequest.Payload.StartAddr_Lo = modbusAddress & 0xff;
			property->ReadRequest.TcpRequest.Payload.ReadLen_Hi = (property->Length >> 8) & 0xff;
			property->ReadRequest.TcpRequest.Payload.ReadLen_Lo = property->Length & 0xff;
			break;
		}
		default:
			LogError("Modbus read if not supported for the capability.");
			return -1;
	}

	return 0;
}

int ModbusTcp_SetWriteRequest(CapabilityType capabilityType, void* capability, char* valueStr)
{
	UINT16 binaryData = 0;

	switch (capabilityType)
	{
		case Command:
		{
			ModbusCommand* command = (ModbusCommand*)capability;
			UINT16 modbusAddress = 0;

			if (!ModbusConnectionHelper_GetFunctionCode(command->StartAddress, false, &(command->WriteRequest.TcpRequest.Payload.FunctionCode), &modbusAddress))
			{
				LogError("Failed to get Modbus function code for command \"%s\".", command->Name);
				return -1;
			}

			if (!ModbusConnectionHelper_ConvertValueStrToUInt16(command->DataType, command->WriteRequest.TcpRequest.Payload.FunctionCode, valueStr, &binaryData))
			{
				LogError("Failed to convert data \"%s\" to byte array command \"%s\".", valueStr, command->Name);
				return -1;
			}

			command->WriteRequest.TcpRequest.MBAP.ProtocolID_Hi = 0x00;
			command->WriteRequest.TcpRequest.MBAP.ProtocolID_Lo = 0x00;
			command->WriteRequest.TcpRequest.MBAP.Length_Hi = 0x00;
			command->WriteRequest.TcpRequest.MBAP.Length_Lo = 0x06;

			command->WriteRequest.TcpRequest.Payload.RegAddr_Hi = (modbusAddress >> 8) & 0xff;
			command->WriteRequest.TcpRequest.Payload.RegAddr_Lo = modbusAddress & 0xff;
			command->WriteRequest.TcpRequest.Payload.Value_Hi = (binaryData >> 8) & 0xff;
			command->WriteRequest.TcpRequest.Payload.Value_Lo = binaryData & 0xff;

			break;
		}
		case Property:
		{
			ModbusProperty* property = (ModbusProperty*)capability;
			UINT16 modbusAddress = 0;
			if (!ModbusConnectionHelper_GetFunctionCode(property->StartAddress, false, &(property->WriteRequest.TcpRequest.Payload.FunctionCode), &modbusAddress))
			{
				LogError("Failed to get Modbus function code for property \"%s\".", property->Name);
				return -1;
			}

			if (!ModbusConnectionHelper_ConvertValueStrToUInt16(property->DataType, property->WriteRequest.TcpRequest.Payload.FunctionCode, valueStr, &binaryData))
			{
				LogError("Failed to convert data \"%s\" to byte array command \"%s\".", valueStr, property->Name);
				return -1;
			}

			property->WriteRequest.TcpRequest.MBAP.ProtocolID_Hi = 0x00;
			property->WriteRequest.TcpRequest.MBAP.ProtocolID_Lo = 0x00;
			property->WriteRequest.TcpRequest.MBAP.Length_Hi = 0x00;
			property->WriteRequest.TcpRequest.MBAP.Length_Lo = 0x06;

			property->WriteRequest.TcpRequest.Payload.RegAddr_Hi = (modbusAddress >> 8) & 0xff;
			property->WriteRequest.TcpRequest.Payload.RegAddr_Lo = modbusAddress & 0xff;
			property->WriteRequest.TcpRequest.Payload.Value_Hi = (binaryData >> 8) & 0xff;
			property->WriteRequest.TcpRequest.Payload.Value_Lo = binaryData & 0xff;
			break;
		}
		default:
			LogError("Modbus read if not supported for the capability.");
			return -1;
		}

	return 0;
}

int ModbusTcp_SendRequest(SOCKET socket, byte *requestArr, DWORD arrLen)
{
	if (INVALID_SOCKET == socket  || 0 == socket || NULL == requestArr)
	{
		LogError("Failed to send request: connection handler and/or the request array is null.");
		return -1;
	}
#ifdef WIN32
	fd_set fds;

	FD_ZERO(&fds);
	socket_timeout.tv_sec = SOCKET_TIMEOUT_SEC;
	socket_timeout.tv_usec = SOCKET_TIMEOUT_USEC;

	// Check handler readiness
	FD_SET(socket, &fds);
	int socketReady = select(32, NULL, &fds, NULL, &socket_timeout);
	if (0 == socketReady)
	{
		LogError("Socket not ready.");
		return -1;
	}
	else if (SOCKET_ERROR == socketReady)
	{
		LogError("Failed to get socket readiness with error: %ld.", WSAGetLastError());
		return -1;
	}
#endif
	int totalBytesSent = send(socket, (const char*)requestArr, arrLen, 0);

	if (SOCKET_ERROR == totalBytesSent) {
		//error setting serial port state
#ifdef WIN32
        LogError("Failed to send request through socket with error: %ld.", WSAGetLastError());
#else
        LogError("Failed to send request through socket.");
#endif
	}

	return totalBytesSent;
}

int ModbusTcp_ReadResponse(SOCKET socket, byte *response, DWORD arrLen)
{
	if (INVALID_SOCKET == socket || 0 == socket || NULL == response)
	{
		LogError("Failed to read response: connection handler and/or the request array is null.");
		return -1;
	}

	int bytesReceived = 0;
	int totalBytesReceived = 0;
#ifdef WIN32
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(socket, &fds);

	// Check handler readiness
	int socketReady = select(32, NULL, &fds, NULL, &socket_timeout);
	if (0 == socketReady)
	{
		LogError("Socket not ready.");
		return -1;
	}
	else if (SOCKET_ERROR == socketReady)
	{
		LogError("Failed to get socket readiness with error: %ld.", WSAGetLastError());
		return -1;
	}
#endif
	bytesReceived = recv(socket, (char*)(response + totalBytesReceived), arrLen - totalBytesReceived, 0);

	if (bytesReceived > 0)
	{
		totalBytesReceived += bytesReceived;
	}
	else if (SOCKET_ERROR == bytesReceived)
	{
#ifdef WIN32
		LogError("Failed to read from socket with error: %ld.", WSAGetLastError());
#else
        LogError("Failed to read from socket.");
#endif
		return -1;
	}

	return totalBytesReceived;
}