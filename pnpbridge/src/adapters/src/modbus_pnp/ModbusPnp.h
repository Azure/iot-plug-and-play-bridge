#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pnpbridge.h>

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <ctype.h>

#include "ModbusEnum.h"

	typedef struct _MODBUS_RTU_CONFIG
	{
		char* Port;
		DWORD BaudRate;
		BYTE DataBits;
		BYTE StopBits;
		BYTE Parity;
	} MODBUS_RTU_CONFIG;

	typedef struct _MODBUS_TCP_CONFIG
	{
		char* Host;
		UINT16 Port;
	} MODBUS_TCP_CONFIG;

	typedef union _MODBUS_CONNECTION_CONFIG
	{
		MODBUS_RTU_CONFIG RtuConfig;
		MODBUS_TCP_CONFIG TcpConfig;
	} MODBUS_CONNECTION_CONFIG;

	typedef struct ModbusDeviceConfig
	{
		BYTE UnitId;
		MODBUS_CONNECTION_TYPE ConnectionType;
		MODBUS_CONNECTION_CONFIG ConnectionConfig;
	} ModbusDeviceConfig, *PModbusDeviceConfig;

	typedef struct ModbusInterfaceConfig
	{
		const char* Id;
		int Index;
		SINGLYLINKEDLIST_HANDLE Events;
		SINGLYLINKEDLIST_HANDLE Properties;
		SINGLYLINKEDLIST_HANDLE Commands;
	} ModbusInterfaceConfig;

	typedef struct _MODBUS_DEVICE_CONTEXT {
		HANDLE hDevice;
		HANDLE hConnectionLock;
		PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;

		THREAD_HANDLE ModbusDeviceWorker;
		PNPBRIDGE_NOTIFY_CHANGE ModbusDeviceChangeCallback;

		// list of interface definitions on this modbus device
		PModbusDeviceConfig DeviceConfig;
		SINGLYLINKEDLIST_HANDLE InterfaceDefinitions;
		THREAD_HANDLE* PollingTasks;
	} MODBUS_DEVICE_CONTEXT, *PMODBUS_DEVICE_CONTEXT;

	int ModbusPnp_GetListCount(SINGLYLINKEDLIST_HANDLE list);

    // TODO: Fix this missing reference
    #ifndef AZURE_UNREFERENCED_PARAMETER
    #define AZURE_UNREFERENCED_PARAMETER(param)   (void)(param)
    #endif

#ifdef __cplusplus
}
#endif
