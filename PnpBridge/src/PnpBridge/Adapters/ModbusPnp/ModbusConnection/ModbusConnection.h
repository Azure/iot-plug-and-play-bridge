#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "azure_c_shared_utility/lock.h"
#include "Adapters/ModbusPnp/ModbusPnp.h"
#include "Adapters/ModbusPnp/ModbusCapability.h"
#include "Adapters/ModbusPnp/ModbusConnection/ModbusConnectionHelper.h"
#include "Adapters/ModbusPnp/ModbusConnection/ModbusRtuConnection.h"
#include "Adapters/ModbusPnp/ModbusConnection/ModbusTcpConnection.h"


// ModbusConnection "Public" methods

bool ModbusPnp_CloseDevice(MODBUS_CONNECTION_TYPE connectionType, HANDLE *hDevice, HANDLE lock);

int ModbusPnp_SetReadRequest(ModbusDeviceConfig* deviceConfig, CapabilityType capabilityType, void* capability);
int ModbusPnp_ReadCapability(CapabilityContext* capabilityContext, CapabilityType capabilityType, byte* resultedData);
int ModbusPnp_WriteToCapability(CapabilityContext* capabilityContext, CapabilityType capabilityType, char* requestStr, byte* resultedData);

#ifdef __cplusplus
}
#endif