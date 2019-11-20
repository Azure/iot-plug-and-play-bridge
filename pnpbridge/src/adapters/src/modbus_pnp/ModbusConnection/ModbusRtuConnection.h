#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "azure_c_shared_utility/xlogging.h"
#include "../ModbusCapability.h"
#include "ModbusConnectionHelper.h"

#define RTU_REQUEST_SIZE 8
#define RTU_HEADER_SIZE 1

int ModbusRtu_GetHeaderSize(void);
bool ModbusRtu_CloseDevice(HANDLE hDevice, LOCK_HANDLE lock);

int ModbusRtu_SetReadRequest(CapabilityType capabilityType, void* capability, byte unitId);
int ModbusRtu_SetWriteRequest(CapabilityType capabilityType, void* capability, char* valueStr);
int ModbusRtu_SendRequest(HANDLE handler, byte *requestArr, DWORD arrLen);
int ModbusRtu_ReadResponse(HANDLE handler, byte *response, DWORD arrLen);

#ifdef __cplusplus
}
#endif