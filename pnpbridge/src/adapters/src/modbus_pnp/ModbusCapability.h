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

#include <pnpbridge.h>
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
} ModbusProperty, *PModbusProperty;

typedef struct ModbusCommand {
    const char*  Name;
    const char*  StartAddress;
    uint16_t Length;
    ModbusDataType  DataType;
    double ConversionCoefficient;
    MODBUS_WRITE_1_REG_REQUEST WriteRequest;
    CapabilityType Type;
} ModbusCommand, *PModbusCommand;

typedef struct CapabilityContext {
    void* capability;
    HANDLE hDevice;
    LOCK_HANDLE hLock;
    MODBUS_CONNECTION_TYPE connectionType;
    IOTHUB_DEVICE_CLIENT_HANDLE deviceClient;
    char * componentName;
}CapabilityContext;

IOTHUB_CLIENT_RESULT ModbusPnp_StartPollingAllTelemetryProperty(void* context);
void StopPollingTasks();

int ModbusPnp_CommandHandler(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize);

void ModbusPnp_PropertyHandler(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    void* userContextCallback);


#ifdef __cplusplus
}
#endif