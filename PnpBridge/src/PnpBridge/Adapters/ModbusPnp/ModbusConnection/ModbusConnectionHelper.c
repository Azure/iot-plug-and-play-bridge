#include "Adapters/ModbusPnp/ModbusConnection/ModbusConnectionHelper.h"

bool ModbusConnectionHelper_GetFunctionCode(const char* startAddress, bool isRead, byte* functionCode, UINT16* modbusAddress)
{
	char entityChar = startAddress[0];
	byte entityType = atoi(&entityChar);
	switch (entityType)
	{
	case CoilStatus:
	{
		*functionCode = isRead ? ReadCoils : WriteCoil;
		break;
	}
	case InputStatus:
	{
		*functionCode = ReadInputs;
		break;
	}
	case HoldingRegister:
	{
		*functionCode = isRead ? ReadHoldingRegisters : WriteHoldingRegister;
		break;
	}
	case InputRegister:
	{
		*functionCode = ReadInputRegisters;
		break;
	}
	default:
		return false;
	}

	char subAddress[5] = "";
	strncpy_s(subAddress, 5, &(startAddress[1]), 4);
	*modbusAddress = atoi(subAddress) - 1;

	return true;
}

bool ModbusConnectionHelper_ConvertValueStrToUInt16(ModbusDataType dataType, FunctionCodeType functionCodeType, char* valueStr, UINT16* value)
{
	switch (dataType)
	{
		case FLAG:
			if (functionCodeType <= ReadInputs || functionCodeType == WriteCoil)
			{
				if (strcmp(valueStr, "true") == 0)
				{
					//ON
					*value = 0xFF00;
				}
				else
				{
					//OFF
					*value = 0x0000;
				}
			}
			return true;
		case NUMERIC:
			*value = atoi(valueStr);
			return true;
		case STRING:
		default:
			break;
	}

	return false;
}


