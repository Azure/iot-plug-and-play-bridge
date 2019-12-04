#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "azure_c_shared_utility/lock.h"
#include "../ModbusPnp.h"
#include "../ModbusCapability.h"
#include "ModbusConnectionHelper.h"
#include "ModbusRtuConnection.h"
#include "ModbusTCPConnection.h"


// ModbusConnection "Public" methods

bool ModbusPnp_CloseDevice(MODBUS_CONNECTION_TYPE connectionType, HANDLE hDevice, LOCK_HANDLE lock);
int ModbusPnp_SetReadRequest(ModbusDeviceConfig* deviceConfig, CapabilityType capabilityType, void* capability);
int ModbusPnp_ReadCapability(CapabilityContext* capabilityContext, CapabilityType capabilityType, uint8_t* resultedData);
int ModbusPnp_WriteToCapability(CapabilityContext* capabilityContext, CapabilityType capabilityType, char* requestStr, uint8_t* resultedData);

#ifdef __cplusplus
}
#endif