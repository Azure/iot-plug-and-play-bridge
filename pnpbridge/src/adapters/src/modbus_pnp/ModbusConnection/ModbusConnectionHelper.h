#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <Windows.h>
#include <cfgmgr32.h>

#include <digitaltwin_interface_client.h>
#include "../ModbusEnum.h"

#define MODBUS_EXCEPTION_CODE 0x80
#define MODBUS_RESPONSE_MAX_LENGTH 32

	// Modbus Operation
	typedef struct _MODBUS_TCP_MBAP_HEADER
	{
		BYTE transacID_Hi;  // Hight byte of Transaction ID: used for transaction pairing.
		BYTE TransacID_Lo;  // Low byte of Transaction ID
		BYTE ProtocolID_Hi; // Hight byte of Protocol ID: 0 for Modbus
		BYTE ProtocolID_Lo; // Low byte of Protocol ID: 0 for Modbus
		BYTE Length_Hi;     // Hight byte of Length of following field: Unit ID + PDU (Data field)
		BYTE Length_Lo;     // Low byte of lendth Length.
		BYTE UnitID;        // Unit ID: used for intra-system routing purpose.
	} MODBUS_TCP_MBAP_HEADER;

	// Read request
	typedef struct _MODBUS_READ_PAYLOAD
	{
		UINT8  FunctionCode;  // Function Code
		UINT8  StartAddr_Hi;  // High byte for starting address
		UINT8  StartAddr_Lo;  // Low byte for starting address
		UINT8  ReadLen_Hi;    // High byte of Number of registers to read
		UINT8  ReadLen_Lo;	  // Low byte of Number of registers to read
	} MODBUS_READ_REG_PAYLOAD;

	typedef struct _MODBUS_TCP_READ_REG_REQUEST
	{
		MODBUS_TCP_MBAP_HEADER MBAP;       // MBAP header for Modbus TCP/IP
		MODBUS_READ_REG_PAYLOAD Payload;   // Request payload
	}  MODBUS_TCP_READ_REG_REQUEST;

	typedef struct _MODBUS_RTU_READ_REQUEST
	{
		UINT8 UnitID;                      // Unit ID: slave address for the Modbus device.
		MODBUS_READ_REG_PAYLOAD Payload;   // Request payload
		UINT16 CRC;		     			   // CRC
	}  MODBUS_RTU_READ_REQUEST;

	typedef union _MODBUS_READ_REQUEST
	{
		byte TcpArr[sizeof(MODBUS_TCP_READ_REG_REQUEST)];
		byte RtuArr[sizeof(MODBUS_RTU_READ_REQUEST)];
		MODBUS_TCP_READ_REG_REQUEST TcpRequest;
		MODBUS_RTU_READ_REQUEST RtuRequest;
	}MODBUS_READ_REQUEST;

	// Write Signle Value request

	typedef struct _MODBUS_WRITE_1_REG_PAYLOAD
	{
		UINT8  FunctionCode;  // Function Code
		UINT8  RegAddr_Hi;    // High byte for starting address
		UINT8  RegAddr_Lo;	  // Low byte for starting address
		UINT8  Value_Hi;      // High byte of Number of registers to read
		UINT8  Value_Lo;	  // Low byte of Number of registers to read
	} MODBUS_WRITE_1_REG_PAYLOAD;

	typedef struct _MODBUS_TCP_WRITE_1_REG_REQUEST
	{
		MODBUS_TCP_MBAP_HEADER MBAP;        // MBAP header for Modbus TCP/IP
		MODBUS_WRITE_1_REG_PAYLOAD Payload; // Request payload
	}  MODBUS_TCP_WRITE_1_REG_REQUEST;

	typedef struct _MODBUS_RTU_WRITE_1_REG_REQUEST
	{
		UINT8 UnitID;                       // Unit ID: slave address for the Modbus device.
		MODBUS_WRITE_1_REG_PAYLOAD Payload; // Request payload
		UINT16 CRC;					        // CRC
	}  MODBUS_RTU_WRITE_1_REG_REQUEST;

	typedef union _MODBUS_WRITE_1_REG_REQUEST
	{
		byte TcpArr[sizeof(MODBUS_TCP_WRITE_1_REG_REQUEST)];
		byte RtuArr[sizeof(MODBUS_RTU_WRITE_1_REG_REQUEST)];
		MODBUS_TCP_WRITE_1_REG_REQUEST TcpRequest;
		MODBUS_RTU_WRITE_1_REG_REQUEST RtuRequest;
	}MODBUS_WRITE_1_REG_REQUEST;

#pragma region functions
	bool ModbusConnectionHelper_GetFunctionCode(const char* startAddress, bool isRead, byte* functionCode, UINT16* modbusAddress);
	bool ModbusConnectionHelper_ConvertValueStrToUInt16(ModbusDataType dataType, FunctionCodeType functionCodeType, char* valueStr, UINT16* value);
#pragma endregion

#ifdef __cplusplus
}
#endif
