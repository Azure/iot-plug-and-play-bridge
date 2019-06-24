#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <Windows.h>
#include <cfgmgr32.h>
#include <ctype.h>
#include <digitaltwin_interface_client.h>

#include "ModbusConnection/ModbusConnectionHelper.h"

typedef enum ModbusAccessType
{
	READ_ONLY = 1,
	READ_WRITE = 2
} ModbusAccessType;

typedef struct ModbusTelemetry {
	const char*  Name;
	const char*  StartAddress;
	UINT16 Length;
	ModbusDataType  DataType;
	double ConversionCoefficient;
	MODBUS_READ_REQUEST  ReadRequest;
	int    DefaultFrequency;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE InterfaceClient;
	CapabilityType Type;
} ModbusTelemetry, *PModbusTelemetry;

typedef void(*MODBUS_TELEMETRY_POLLING_TASK)(ModbusTelemetry* userContextCallback);

typedef struct ModbusProperty {
	const char*  Name;
	const char*  StartAddress;
	UINT16 Length;
	ModbusDataType  DataType;
	double ConversionCoefficient;
	MODBUS_READ_REQUEST  ReadRequest;
	MODBUS_WRITE_1_REG_REQUEST WriteRequest;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE InterfaceClient;
	int    DefaultFrequency;
	ModbusAccessType Access;
	CapabilityType Type;
	//ModbusDevice HostDevice;
} ModbusProperty, *PModbusProperty;

typedef struct ModbusCommand {
	const char*  Name;
	const char*  StartAddress;
	UINT16 Length;
	ModbusDataType  DataType;
	double ConversionCoefficient;
	MODBUS_WRITE_1_REG_REQUEST WriteRequest;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE InterfaceClient;
	CapabilityType Type;
	//ModbusDevice HostDevice;
} ModbusCommand;

typedef struct CapabilityContext {
	void* capability;
	HANDLE hDevice;
	HANDLE hLock;
	MODBUS_CONNECTION_TYPE connectionType;
}CapabilityContext;

int ModbusPnp_StartPollingAllTelemetryProperty(void* context);
void StopPollingTasks();

void ModbusPnp_CommandHandler(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, 
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback);

int ModbusPnp_PropertyHandler(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback);


#ifdef __cplusplus
}
#endif