#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

typedef enum ModbusDataType
{
	NUMERIC,
	FLAG,
	STRING,
	HEXSTRING,
	INVALID
} ModbusDataType;

typedef enum EntityType
{
	CoilStatus = 0,
	InputStatus = 1,
	InputRegister = 3,
	HoldingRegister = 4
} EntityType;

typedef enum FunctionCodeType
{
	ReadCoils = 1,
	ReadInputs = 2,
	ReadHoldingRegisters = 3,
	ReadInputRegisters = 4,
	WriteCoil = 5,
	WriteHoldingRegister = 6
} FunctionCodeType;

typedef enum CapabilityType {
	Telemetry,
	Property,
	Command
} CapabilityType;

typedef enum MODBUS_CONNECTION_TYPE
{
	UNKOWN,
	RTU,
	TCP,
}MODBUS_CONNECTION_TYPE;

#ifdef __cplusplus
}
#endif