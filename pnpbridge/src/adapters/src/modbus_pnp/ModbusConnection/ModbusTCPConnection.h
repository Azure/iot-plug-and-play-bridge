#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "azure_c_shared_utility/xlogging.h"
#include "../ModbusCapability.h"
#include "ModbusConnectionHelper.h"

#define TCP_HEADER_SIZE 7 // TransactionID (2 bytes) + ProtocolID (2 bytes) + Length (2 bytes) + UnitID (1 byte)

int ModbusTcp_GetHeaderSize(void);
bool ModbusTcp_CloseDevice(SOCKET hDevice, HANDLE lock);

int ModbusTcp_SetReadRequest(CapabilityType capabilityType, void* capability, byte unitId);
int ModbusTcp_SetWriteRequest(CapabilityType capabilityType, void* capability, char* valueStr);
int ModbusTcp_SendRequest(SOCKET handler, byte *requestArr, DWORD arrLen);
int ModbusTcp_ReadResponse(SOCKET handler, byte *response, DWORD arrLen);

#ifdef __cplusplus
}
#endif