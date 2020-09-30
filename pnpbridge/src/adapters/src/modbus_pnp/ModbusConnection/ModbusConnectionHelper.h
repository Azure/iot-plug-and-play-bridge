// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
#include <iothub_device_client.h>
#include "../ModbusEnum.h"

#define MODBUS_EXCEPTION_CODE 0x80
#define MODBUS_RESPONSE_MAX_LENGTH 32

    // Modbus Operation
    typedef struct _MODBUS_TCP_MBAP_HEADER
    {
        uint8_t transacID_Hi;  // Hight uint8_t of Transaction ID: used for transaction pairing.
        uint8_t TransacID_Lo;  // Low uint8_t of Transaction ID
        uint8_t ProtocolID_Hi; // Hight uint8_t of Protocol ID: 0 for Modbus
        uint8_t ProtocolID_Lo; // Low uint8_t of Protocol ID: 0 for Modbus
        uint8_t Length_Hi;     // Hight uint8_t of Length of following field: Unit ID + PDU (Data field)
        uint8_t Length_Lo;     // Low uint8_t of lendth Length.
        uint8_t UnitID;        // Unit ID: used for intra-system routing purpose.
    } MODBUS_TCP_MBAP_HEADER;

    // Read request
    typedef struct _MODBUS_READ_PAYLOAD
    {
        uint8_t  FunctionCode;  // Function Code
        uint8_t  StartAddr_Hi;  // High uint8_t for starting address
        uint8_t  StartAddr_Lo;  // Low uint8_t for starting address
        uint8_t  ReadLen_Hi;    // High uint8_t of Number of registers to read
        uint8_t  ReadLen_Lo;    // Low uint8_t of Number of registers to read
    } MODBUS_READ_REG_PAYLOAD;

    typedef struct _MODBUS_TCP_READ_REG_REQUEST
    {
        MODBUS_TCP_MBAP_HEADER MBAP;       // MBAP header for Modbus TCP/IP
        MODBUS_READ_REG_PAYLOAD Payload;   // Request payload
    } MODBUS_TCP_READ_REG_REQUEST;

    typedef struct _MODBUS_RTU_READ_REQUEST
    {
        uint8_t UnitID;                    // Unit ID: slave address for the Modbus device.
        MODBUS_READ_REG_PAYLOAD Payload;   // Request payload
        uint16_t CRC;                      // CRC
    } MODBUS_RTU_READ_REQUEST;

    typedef union _MODBUS_READ_REQUEST
    {
        uint8_t TcpArr[sizeof(MODBUS_TCP_READ_REG_REQUEST)];
        uint8_t RtuArr[sizeof(MODBUS_RTU_READ_REQUEST)];
        MODBUS_TCP_READ_REG_REQUEST TcpRequest;
        MODBUS_RTU_READ_REQUEST RtuRequest;
    } MODBUS_READ_REQUEST;

    // Write Single Value request

    typedef struct _MODBUS_WRITE_1_REG_PAYLOAD
    {
        uint8_t  FunctionCode;  // Function Code
        uint8_t  RegAddr_Hi;    // High uint8_t for starting address
        uint8_t  RegAddr_Lo;    // Low uint8_t for starting address
        uint8_t  Value_Hi;      // High uint8_t of Number of registers to read
        uint8_t  Value_Lo;      // Low uint8_t of Number of registers to read
    } MODBUS_WRITE_1_REG_PAYLOAD;

    typedef struct _MODBUS_TCP_WRITE_1_REG_REQUEST
    {
        MODBUS_TCP_MBAP_HEADER MBAP;        // MBAP header for Modbus TCP/IP
        MODBUS_WRITE_1_REG_PAYLOAD Payload; // Request payload
    } MODBUS_TCP_WRITE_1_REG_REQUEST;

    typedef struct _MODBUS_RTU_WRITE_1_REG_REQUEST
    {
        uint8_t UnitID;                       // Unit ID: slave address for the Modbus device.
        MODBUS_WRITE_1_REG_PAYLOAD Payload;   // Request payload
        uint16_t CRC;                         // CRC
    } MODBUS_RTU_WRITE_1_REG_REQUEST;

    typedef union _MODBUS_WRITE_1_REG_REQUEST
    {
        uint8_t TcpArr[sizeof(MODBUS_TCP_WRITE_1_REG_REQUEST)];
        uint8_t RtuArr[sizeof(MODBUS_RTU_WRITE_1_REG_REQUEST)];
        MODBUS_TCP_WRITE_1_REG_REQUEST TcpRequest;
        MODBUS_RTU_WRITE_1_REG_REQUEST RtuRequest;
    } MODBUS_WRITE_1_REG_REQUEST;

#pragma region functions
    bool ModbusConnectionHelper_GetFunctionCode(const char* startAddress, bool isRead, uint8_t* functionCode, uint16_t* modbusAddress);
    bool ModbusConnectionHelper_ConvertValueStrToUInt16(ModbusDataType dataType, FunctionCodeType functionCodeType, char* valueStr, uint16_t* value);
#pragma endregion

#ifdef __cplusplus
}
#endif
