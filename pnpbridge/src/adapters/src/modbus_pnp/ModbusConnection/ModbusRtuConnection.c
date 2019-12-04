
#include "azure_c_shared_utility/threadapi.h"
#include "ModbusRtuConnection.h"
#ifndef WIN32
#include <unistd.h>
#endif // WIN32

uint16_t GetCRC(uint8_t *message, size_t length)
{
    if (NULL == message)
    {
        // TODO: This was -1. Check what should be the correct return value.
        return 0;
    }

    uint16_t crcFull = 0xFFFF;

    for (int i = 0; i < (int)length; ++i)
    {
        crcFull = (uint16_t)(crcFull ^ message[i]);

        for (int j = 0; j < 8; ++j)
        {
            uint8_t  crcLsb = (uint8_t)(crcFull & 0x0001);
            crcFull = (uint16_t)((crcFull >> 1) & 0x7FFF);
            if (crcLsb == 1)
            {
                crcFull = (uint16_t)(crcFull ^ 0xA001);
            }
        }
    }
    return crcFull;
}

int ModbusRtu_GetHeaderSize(void) {
    return RTU_HEADER_SIZE;
}

bool ModbusRtu_CloseDevice(HANDLE hDevice, LOCK_HANDLE hLock)
{
    bool result = -1;

    if (LOCK_OK != Lock(hLock))
    {
        LogError("Device communicate lock could not be acquired.");
        return result;
    }
#ifdef WIN32
    result = CloseHandle(hDevice);
#else
    result = !(close(hDevice));
#endif
    
    Unlock(hLock);
    LogInfo("Serial Port Closed.");
    return result;
}

int ModbusRtu_SetReadRequest(CapabilityType capabilityType, void* capability, uint8_t unitId)
{
    uint16_t modbusAddress = 0;
    switch (capabilityType)
    {
        case Telemetry:
        {
            ModbusTelemetry* telemetry = (ModbusTelemetry*)capability;

            if (!ModbusConnectionHelper_GetFunctionCode(telemetry->StartAddress, true, &(telemetry->ReadRequest.RtuRequest.Payload.FunctionCode), &modbusAddress))
            {
                LogError("Failed to get Modbus function code for telemetry \"%s\".", telemetry->Name);
                return -1;
            }

            telemetry->ReadRequest.RtuRequest.UnitID = unitId;
            telemetry->ReadRequest.RtuRequest.Payload.StartAddr_Hi = (modbusAddress >> 8) & 0xff;
            telemetry->ReadRequest.RtuRequest.Payload.StartAddr_Lo = modbusAddress & 0xff;
            telemetry->ReadRequest.RtuRequest.Payload.ReadLen_Hi = (telemetry->Length >> 8) & 0xff;
            telemetry->ReadRequest.RtuRequest.Payload.ReadLen_Lo = telemetry->Length & 0xff;
            telemetry->ReadRequest.RtuRequest.CRC = GetCRC(telemetry->ReadRequest.RtuArr, RTU_REQUEST_SIZE - 2);

            break;
        }
        case Property:
        {
            ModbusProperty* property = (ModbusProperty*)capability;
            if (!ModbusConnectionHelper_GetFunctionCode(property->StartAddress, true, &(property->ReadRequest.RtuRequest.Payload.FunctionCode), &modbusAddress))
            {
                LogError("Failed to get Modbus function code for property \"%s\".", property->Name);
                return -1;
            }

            property->ReadRequest.RtuRequest.UnitID = unitId;
            property->ReadRequest.RtuRequest.Payload.StartAddr_Hi = (modbusAddress >> 8) & 0xff;
            property->ReadRequest.RtuRequest.Payload.StartAddr_Lo = modbusAddress & 0xff;
            property->ReadRequest.RtuRequest.Payload.ReadLen_Hi = (property->Length >> 8) & 0xff;
            property->ReadRequest.RtuRequest.Payload.ReadLen_Lo = property->Length & 0xff;
            property->ReadRequest.RtuRequest.CRC = GetCRC(property->ReadRequest.RtuArr, RTU_REQUEST_SIZE - 2);
            break;
        }
        default:
            LogError("Modbus read if not supported for the capability.");
            return -1;
    }

    return 0;
}

int ModbusRtu_SetWriteRequest(CapabilityType capabilityType, void* capability, char* valueStr)
{
    uint16_t binaryData = 0;
    uint16_t modbusAddress = 0;

    switch (capabilityType)
    {
        case Command:
        {
            ModbusCommand* command = (ModbusCommand*)capability;


            if (!ModbusConnectionHelper_GetFunctionCode(command->StartAddress, false, &(command->WriteRequest.RtuRequest.Payload.FunctionCode), &modbusAddress))
            {
                LogError("Failed to get Modbus function code for command \"%s\".", command->Name);
                return -1;
            }

            if (!ModbusConnectionHelper_ConvertValueStrToUInt16(command->DataType, command->WriteRequest.RtuRequest.Payload.FunctionCode, valueStr, &binaryData))
            {
                LogError("Failed to convert data \"%s\" to byte array command \"%s\".", valueStr, command->Name);
                return -1;
            }

            command->WriteRequest.RtuRequest.Payload.RegAddr_Hi = (modbusAddress >> 8) & 0xff;
            command->WriteRequest.RtuRequest.Payload.RegAddr_Lo = modbusAddress & 0xff;
            command->WriteRequest.RtuRequest.Payload.Value_Hi = (binaryData >> 8) & 0xff;
            command->WriteRequest.RtuRequest.Payload.Value_Lo = binaryData & 0xff;
            command->WriteRequest.RtuRequest.CRC = GetCRC(command->WriteRequest.RtuArr, RTU_REQUEST_SIZE - 2);

            break;
        }
        case Property:
        {
            ModbusProperty* property = (ModbusProperty*)capability;
            if (!ModbusConnectionHelper_GetFunctionCode(property->StartAddress, false, &(property->WriteRequest.RtuRequest.Payload.FunctionCode), &modbusAddress))
            {
                LogError("Failed to get Modbus function code for property \"%s\".", property->Name);
                return -1;
            }

            if (!ModbusConnectionHelper_ConvertValueStrToUInt16(property->DataType, property->WriteRequest.RtuRequest.Payload.FunctionCode, valueStr, &binaryData))
            {
                LogError("Failed to convert data \"%s\" to byte array command \"%s\".", valueStr, property->Name);
                return -1;
            }

            property->WriteRequest.RtuRequest.Payload.RegAddr_Hi = (modbusAddress >> 8) & 0xff;
            property->WriteRequest.RtuRequest.Payload.RegAddr_Lo = modbusAddress & 0xff;
            property->WriteRequest.RtuRequest.Payload.Value_Hi = (binaryData >> 8) & 0xff;
            property->WriteRequest.RtuRequest.Payload.Value_Lo = binaryData & 0xff;
            property->WriteRequest.RtuRequest.CRC = GetCRC(property->WriteRequest.RtuArr, RTU_REQUEST_SIZE - 2);
            break;
        }
        default:
            LogError("Modbus read if not supported for the capability.");
            return -1;
    }

    return 0;
}

int ModbusRtu_SendRequest(HANDLE handler, uint8_t *requestArr, uint32_t arrLen)
{
#ifdef WIN32
    if (NULL == handler || NULL == requestArr)
    {
        LogError("Failed to read response: connection handler and/or the request array is null.");
        return -1;
    }
#else
    if (!handler || NULL == requestArr)
    {
        LogError("Failed to read response: connection handler and/or the request array is null.");
        return -1;
    }
#endif

    uint32_t totalBytesSent = 0;

#ifdef WIN32
    DWORD bytesSent = 0;
    if (!WriteFile((HANDLE)handler,
        requestArr,
        arrLen,
        &bytesSent,
        NULL))
    {
        LogError("Failed to send request: %x", GetLastError());
        return -1;
    }
    totalBytesSent = bytesSent;
#else
    totalBytesSent = write(handler, requestArr, arrLen);
    if (totalBytesSent != arrLen)
    {
        LogError("Failed to send request.");
        return -1;
    }
#endif

    return  totalBytesSent;
}

int ModbusRtu_ReadResponse(HANDLE handler, uint8_t *response, uint32_t arrLen)
{
#ifdef WIN32
    if (NULL == handler || NULL == response)
    {
        LogError("Failed to read response: connection handler and/or the response is null.");
        return -1;
    }
#else
    if (!handler || NULL == response)
    {
        LogError("Failed to read response: connection handler and/or the response is null.");
        return -1;
    }
#endif

    

    bool result = false;
    uint32_t bytesReceived = 0;
    uint32_t totalBytesReceived = 0;
    int retry = 0;
    const int retryLimit = 3;

    while (totalBytesReceived < RTU_HEADER_SIZE && retry < retryLimit)
    {
#ifdef WIN32
        DWORD received = 0;
        result = (FALSE != ReadFile(handler,
            (response + totalBytesReceived),
            (arrLen - totalBytesReceived - 1),
            &received,
            NULL));
        bytesReceived = received;
#else
        bytesReceived = read(handler, response, (arrLen - totalBytesReceived - 1));
        result = (bytesReceived > 0);
#endif

        if (bytesReceived > 0)
        {
            totalBytesReceived += bytesReceived;
        }
        else
        {
            retry++;
            ThreadAPI_Sleep(2 * 1000);
        }
    }

    if (response[1] != WriteCoil && response[1] != WriteHoldingRegister)
    {
        // Get data length from the PDU
        uint16_t length = (uint16_t)(response[1] >= MODBUS_EXCEPTION_CODE ? 2 : response[2] + 2);
        uint32_t expectedResponseLength = length + RTU_HEADER_SIZE + 2; //HEADER_SIZE + CRC_SIZE;

        if (expectedResponseLength != arrLen)
        {
            //TODO
            //Array.Resize<uint8_t>(ref response, expectedResponseLength);
        }

        while (totalBytesReceived < expectedResponseLength && retry < retryLimit)
        {
#ifdef WIN32
            DWORD received = 0;
            result = (FALSE != ReadFile(handler,
                (response + totalBytesReceived),
                (arrLen - totalBytesReceived - 1),
                &received,
                NULL));
            bytesReceived = received;
#else
            bytesReceived = read(handler, response, (arrLen - totalBytesReceived - 1));
            result = (bytesReceived > 0);
#endif

            if (bytesReceived > 0)
            {
                totalBytesReceived += bytesReceived;
            }
            else
            {
                retry++;
                ThreadAPI_Sleep(2 * 1000);
            }
        }
    }

    if (!result)
    {
#ifdef WIN32
        LogError("Failed to read response: %x.", GetLastError());
#else
        LogError("Failed to read response.");
#endif
        return -1;
    }

    return totalBytesReceived;
}