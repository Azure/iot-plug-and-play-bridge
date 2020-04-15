#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "azure_c_shared_utility/xlogging.h"
#include "../ModbusCapability.h"
#include "ModbusConnectionHelper.h"
#ifndef WIN32
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (~0)
#endif

#define TCP_HEADER_SIZE 7 // TransactionID (2 bytes) + ProtocolID (2 bytes) + Length (2 bytes) + UnitID (1 uint8_t)

int ModbusTcp_GetHeaderSize(void);
bool ModbusTcp_CloseDevice(SOCKET hDevice, LOCK_HANDLE lock);

DIGITALTWIN_CLIENT_RESULT ModbusTcp_SetReadRequest(CapabilityType capabilityType, void* capability, uint8_t unitId);
DIGITALTWIN_CLIENT_RESULT ModbusTcp_SetWriteRequest(CapabilityType capabilityType, void* capability, char* valueStr);
int ModbusTcp_SendRequest(SOCKET handler, uint8_t *requestArr, uint32_t arrLen);
int ModbusTcp_ReadResponse(SOCKET handler, uint8_t *response, uint32_t arrLen);

#ifdef __cplusplus
}
#endif