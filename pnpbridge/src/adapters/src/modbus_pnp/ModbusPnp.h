// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pnpbridge.h>
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/condition.h"
#include <ctype.h>
#ifdef WIN32
#include <Windows.h>
#include <cfgmgr32.h>
#define INVALID_FILE INVALID_HANDLE_VALUE
#else
typedef int HANDLE;
typedef int SOCKET;

#define INVALID_FILE -1
#define INVALID_HANDLE_VALUE -1
#define iscsym(c)   (isalnum(c) || ((c) == '_'))
#include <errno.h>
#include <string.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define ONESTOPBIT 0
#define ONE5STOPBITS 1
#define TWOSTOPBITS 2

//parity
#define NOPARITY 0
#define ODDPARITY 1
#define EVENPARITY 2
#define MARKPARITY 3
#define SPACEPARITY 4


#define UNREFERENCED_PARAMETER AZURE_UNREFERENCED_PARAMETER
#endif

#include "ModbusEnum.h"

    typedef struct _MODBUS_RTU_CONFIG
    {
        char* Port;
        uint32_t BaudRate;
        uint8_t DataBits;
        uint8_t StopBits;
        uint8_t Parity;
    } MODBUS_RTU_CONFIG;

    typedef struct _MODBUS_TCP_CONFIG
    {
        char* Host;
        uint16_t Port;
    } MODBUS_TCP_CONFIG;

    typedef union _MODBUS_CONNECTION_CONFIG
    {
        MODBUS_RTU_CONFIG RtuConfig;
        MODBUS_TCP_CONFIG TcpConfig;
    } MODBUS_CONNECTION_CONFIG;

    typedef struct ModbusDeviceConfig
    {
        uint8_t UnitId;
        MODBUS_CONNECTION_TYPE ConnectionType;
        MODBUS_CONNECTION_CONFIG ConnectionConfig;
    } ModbusDeviceConfig, *PModbusDeviceConfig;

    typedef struct ModbusInterfaceConfig
    {
        const char* Id;
        SINGLYLINKEDLIST_HANDLE Events;
        SINGLYLINKEDLIST_HANDLE Properties;
        SINGLYLINKEDLIST_HANDLE Commands;
    } ModbusInterfaceConfig, *PModbusInterfaceConfig;

    typedef struct _MODBUS_DEVICE_CONTEXT {
        HANDLE hDevice;
        LOCK_HANDLE hConnectionLock;
        IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient;
        THREAD_HANDLE ModbusDeviceWorker;

        PModbusDeviceConfig DeviceConfig;
        PModbusInterfaceConfig InterfaceConfig;
        THREAD_HANDLE* PollingTasks;
        char * ComponentName;
    } MODBUS_DEVICE_CONTEXT, *PMODBUS_DEVICE_CONTEXT;

    typedef struct _MODBUS_ADAPTER_CONTEXT {
        SINGLYLINKEDLIST_HANDLE InterfaceDefinitions;
    } MODBUS_ADAPTER_CONTEXT, * PMODBUS_ADAPTER_CONTEXT;

    int ModbusPnp_GetListCount(SINGLYLINKEDLIST_HANDLE list);

    typedef int(*MODBUS_COMMAND_EXECUTE_CALLBACK)(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

    typedef void(*MODBUS_PROPERTY_UPDATE_CALLBACK)(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version,
        void* userContextCallback);

    // Modbus Adapter Config
    #define PNP_CONFIG_ADAPTER_MODBUS_IDENTITY "modbus_identity"
    #define PNP_CONFIG_ADAPTER_INTERFACE_UNITID "unit_id"
    #define PNP_CONFIG_ADAPTER_INTERFACE_TCP "tcp"
    #define PNP_CONFIG_ADAPTER_INTERFACE_RTU "rtu"

    // TODO: Fix this missing reference
    #ifndef AZURE_UNREFERENCED_PARAMETER
    #define AZURE_UNREFERENCED_PARAMETER(param)   (void)(param)
    #endif

#ifdef __cplusplus
}
#endif
