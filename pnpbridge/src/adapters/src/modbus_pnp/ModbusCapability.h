#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#ifdef WIN32
    #include <Windows.h>
    #include <cfgmgr32.h>
#else
    #include <string.h>
    typedef int HANDLE;
#endif

#include <digitaltwin_interface_client.h>
#include "azure_c_shared_utility/lock.h"
#include "ModbusConnection/ModbusConnectionHelper.h"

typedef enum ModbusAccessType
{
    READ_ONLY = 1,
    READ_WRITE = 2
} ModbusAccessType;

typedef struct ModbusTelemetry {
    const char*  Name;
    const char*  StartAddress;
    uint16_t Length;
    ModbusDataType  DataType;
    double ConversionCoefficient;
    MODBUS_READ_REQUEST  ReadRequest;
    int    DefaultFrequency;
    CapabilityType Type;
} ModbusTelemetry, *PModbusTelemetry;

typedef void(*MODBUS_TELEMETRY_POLLING_TASK)(ModbusTelemetry* userContextCallback);

typedef struct ModbusProperty {
    const char*  Name;
    const char*  StartAddress;
    uint16_t Length;
    ModbusDataType  DataType;
    double ConversionCoefficient;
    MODBUS_READ_REQUEST  ReadRequest;
    MODBUS_WRITE_1_REG_REQUEST WriteRequest;
    int    DefaultFrequency;
    ModbusAccessType Access;
    CapabilityType Type;
    //ModbusDevice HostDevice;
} ModbusProperty, *PModbusProperty;

typedef struct ModbusCommand {
    const char*  Name;
    const char*  StartAddress;
    uint16_t Length;
    ModbusDataType  DataType;
    double ConversionCoefficient;
    MODBUS_WRITE_1_REG_REQUEST WriteRequest;
    CapabilityType Type;
    //ModbusDevice HostDevice;
} ModbusCommand, *PModbusCommand;

typedef struct CapabilityContext {
    void* capability;
    HANDLE hDevice;
    LOCK_HANDLE hLock;
    MODBUS_CONNECTION_TYPE connectionType;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE InterfaceClient;
}CapabilityContext;

DIGITALTWIN_CLIENT_RESULT ModbusPnp_StartPollingAllTelemetryProperty(void* context);
void StopPollingTasks();

void ModbusPnp_CommandHandler(const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtClientCommandContext, 
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtClientCommandResponseContext, void* userContextCallback);

void ModbusPnp_PropertyHandler(const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate, void* userContextCallback);


#ifdef __cplusplus
}
#endif