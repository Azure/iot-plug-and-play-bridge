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
        int Index;
        SINGLYLINKEDLIST_HANDLE Events;
        SINGLYLINKEDLIST_HANDLE Properties;
        SINGLYLINKEDLIST_HANDLE Commands;
    } ModbusInterfaceConfig;

    typedef struct _MODBUS_DEVICE_CONTEXT {
        HANDLE hDevice;
        LOCK_HANDLE hConnectionLock;
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
