// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <PnpBridge.h>

#include "azure_c_shared_utility/base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <ctype.h>

#include "parson.h"

#include "Adapters\SerialPnp\SerialPnp.h"

// BEGIN - PNPCSDK#20
// https://github.com/Azure/azure-iot-sdk-c-pnp/issues/20
// Temporary redirection for property and command handling since PnP C SDK property change 
// callback doesn't provide the command name.
void SerialPnp_PropertyUpdateHandlerRedirect(int index, unsigned const char* propertyInitial,
	size_t propertyInitialLen, unsigned const char* propertyDataUpdated,
	size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback);

void SerialPnp_CommandUpdateHandlerRedirect(int index,
	const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
	PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
	void* userContextCallback);

#define CMD_PROP_HANDLER_ADAPTER_NAME SerialPnp
#define PROP_HANDLER_ADAPTER_METHOD SerialPnp_PropertyUpdateHandlerRedirect
#define CMD_HANDLER_ADAPTER_METHOD SerialPnp_CommandUpdateHandlerRedirect

#include "adapters/CmdAndPropHandler.h"
// END - PNPCSDK#20

int SerialPnp_UartReceiver(void* context)
{
	BYTE msgBuffer[2048];
	PBYTE p = msgBuffer;
	UINT16 SizeToRead = 0;
	int result = 0;
	PSERIAL_DEVICE_CONTEXT deviceContext = (PSERIAL_DEVICE_CONTEXT)context;

	while (result >= 0) {
		byte* desc = NULL;
		DWORD length;

		SerialPnp_RxPacket(deviceContext, &desc, &length, 0x00);
		SerialPnp_UnsolicitedPacket(deviceContext, desc, length);

		if (desc != NULL) {
			free(desc);
		}
	}

	return 0;
}


void SerialPnp_TxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte* OutPacket, int Length)
{
	int txLength = 1 + Length;
	// First iterate through and find out our new length
	for (int i = 0; i < Length; i++)
	{
		if ((OutPacket[i] == 0x5A) || (OutPacket[i] == 0xEF))
		{
			txLength++;
		}
	}

	// Now construct outgoing buffer
	byte* SerialPnp_TxPacket = malloc(sizeof(byte) *txLength);

	txLength = 1;
	SerialPnp_TxPacket[0] = 0x5A; // Start of frame
	for (int i = 0; i < Length; i++)
	{
		// Escape these bytes where necessary
		if ((OutPacket[i] == 0x5A) || (OutPacket[i] == 0xEF))
		{
			SerialPnp_TxPacket[txLength++] = 0xEF;
			SerialPnp_TxPacket[txLength++] = (byte)(OutPacket[i] - 1);
		}
		else
		{
			SerialPnp_TxPacket[txLength++] = OutPacket[i];
		}
	}

	DWORD write_size = 0;
	if (!WriteFile(serialDevice->hSerial, SerialPnp_TxPacket, txLength, &write_size, NULL))
	{
		LogError("write failed");
	}

	free(SerialPnp_TxPacket);
}

const EventDefinition* SerialPnp_LookupEvent(SINGLYLINKEDLIST_HANDLE interfaceDefinitions, char* EventName, int InterfaceId)
{
	const InterfaceDefinition* interfaceDef;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);

	for (int i = 0; i < InterfaceId - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE events = interfaceDef->Events;
	LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(events);
	const EventDefinition* ev;
	while (eventDef != NULL) {
		ev = singlylinkedlist_item_get_value(eventDef);
		if (strcmp(ev->defintion.Name, EventName) == 0) {
			return ev;
		}
		eventDef = singlylinkedlist_get_next_item(eventDef);
	}

	return NULL;
}

byte* SerialPnp_StringSchemaToBinary(Schema Schema, byte* data, int* length)
{
	byte* bd = NULL;

	if ((Schema == Float) || (Schema == Int))
	{
		bd = malloc(sizeof(byte) * 4);
		*length = 4;

		if (Schema == Float)
		{
			float x = 0;
			x = (float)atof(data);
			memcpy(bd, &x, sizeof(float));
		}
		else if (Schema == Int)
		{
			int x;
			x = atoi(data);
			memcpy(bd, &x, sizeof(int));
		}
	}
	else if (Schema == Boolean)
	{
		bd = malloc(sizeof(byte) * 1);
		*length = 1;
		if (stricmp(data, "true")) {
			bd[0] = 1;
		}
		else if (stricmp(data, "false")) {
			bd[0] = 1;
		}
		else {
			free(bd);
			*length = 0;
			bd = NULL;
		}
	}

	return bd;
}

char* SerialPnp_BinarySchemaToString(Schema Schema, byte* Data, byte length)
{
	char* rxstrdata = malloc(256);

	if ((Schema == Float) && (length == 4))
	{
		float *fa = malloc(sizeof(float));
		memcpy(fa, Data, 4);
		sprintf_s(rxstrdata, 256, "%.6f", *fa);
	}
	else if (((Schema == Int) && (length == 4)))
	{
		int *fa = malloc(sizeof(int));
		memcpy(fa, Data, 4);
		sprintf_s(rxstrdata, 256, "%d", *fa);
	}

	return rxstrdata;
}

void SerialPnp_UnsolicitedPacket(PSERIAL_DEVICE_CONTEXT device, byte* packet, DWORD length)
{
	// Got an event
	if (packet[2] == 0x0A) {
		byte rxNameLength = packet[5];
		byte rxInterfaceId = packet[4];
		DWORD rxDataSize = length - rxNameLength - 6;

		//string event_name = Encoding.UTF8.GetString(packet, 6, rxNameLength);
		char* event_name = malloc(sizeof(char)*(rxNameLength + 1));
		memcpy(event_name, packet + 6, rxNameLength);
		event_name[rxNameLength] = '\0';

		const EventDefinition* ev = SerialPnp_LookupEvent(device->InterfaceDefinitions, event_name, rxInterfaceId);

		byte* rxData = malloc(sizeof(byte)*rxDataSize);
		memcpy(rxData, packet + 6 + rxNameLength, rxDataSize);

		char* rxstrdata = SerialPnp_BinarySchemaToString(ev->DataSchema, rxData, (byte)rxDataSize);
		LogInfo("%s: %s", ev->defintion.Name, rxstrdata);

		SerialPnp_SendEventAsync(PnpAdapterInterface_GetPnpInterfaceClient(device->pnpAdapterInterface), ev->defintion.Name, rxstrdata);

		free(event_name);
		free(rxData);
	}
	// Got a command update
	else if (packet[2] == 0x08)
	{
		// TODO
	}
	else if (packet[2] == 0x08) {

	}
}

const PropertyDefinition* SerialPnp_LookupProperty(SINGLYLINKEDLIST_HANDLE interfaceDefinitions, const char* propertyName, int InterfaceId)
{
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);
	const InterfaceDefinition* interfaceDef;

	for (int i = 0; i < InterfaceId - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE property = interfaceDef->Properties;
	LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(property);
	const PropertyDefinition* ev;
	while (eventDef != NULL) {
		ev = singlylinkedlist_item_get_value(eventDef);
		if (strcmp(ev->defintion.Name, propertyName) == 0) {
			return ev;
		}
		eventDef = singlylinkedlist_get_next_item(eventDef);
	}

	return NULL;
}

const CommandDefinition* SerialPnp_LookupCommand(SINGLYLINKEDLIST_HANDLE interfaceDefinitions, const char* commandName, int InterfaceId)
{
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);
	const InterfaceDefinition* interfaceDef;

	for (int i = 0; i < InterfaceId - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE command = interfaceDef->Commands;
	LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(command);
	const CommandDefinition* ev;
	while (eventDef != NULL) {
		ev = singlylinkedlist_item_get_value(eventDef);
		if (strcmp(ev->defintion.Name, commandName) == 0) {
			return ev;
		}
		eventDef = singlylinkedlist_get_next_item(eventDef);
	}

	return NULL;
}

int SerialPnp_PropertyHandler(PSERIAL_DEVICE_CONTEXT serialDevice, const char* property, char* input)
{
	const PropertyDefinition* prop = SerialPnp_LookupProperty(serialDevice->InterfaceDefinitions, property, 0);

	if (prop == NULL)
	{
		return -1;
	}

	// otherwise serialize data
	int length = 0;
	byte* inputPayload = SerialPnp_StringSchemaToBinary(prop->DataSchema, input, &length);

	int nameLength = (int)strlen(property);
	int txlength = 6 + nameLength + length;
	byte* txPacket = malloc(txlength);

	txPacket[0] = (byte)(txlength & 0xFF);
	txPacket[1] = (byte)(txlength >> 8);
	txPacket[2] = 0x07; // command request
						// [3] is reserved
	txPacket[4] = (byte)0;
	txPacket[5] = (byte)nameLength;

	memcpy(txPacket + 6, property, nameLength);
	if (inputPayload != NULL) {
		memcpy(txPacket + 6 + nameLength, inputPayload, length);
	}

	LogInfo("Setting property %s to %s", property, input);

	SerialPnp_TxPacket(serialDevice, txPacket, txlength);

	//ThreadAPI_Sleep(2000);

	if (inputPayload != NULL) {
		free(inputPayload);
	}

	free(txPacket);

	return 0;
}


int SerialPnp_CommandHandler(PSERIAL_DEVICE_CONTEXT serialDevice, const char* command, char* input, char** response)
{
	const CommandDefinition* cmd = SerialPnp_LookupCommand(serialDevice->InterfaceDefinitions, command, 0);

	if (cmd == NULL)
	{
		return -1;
	}

	// otherwise serialize data
	int length = 0;
	byte* inputPayload = SerialPnp_StringSchemaToBinary(cmd->RequestSchema, input, &length);

	int nameLength = (int)strlen(command);
	int txlength = 6 + nameLength + length;
	byte* txPacket = malloc(txlength);

	txPacket[0] = (byte)(txlength & 0xFF);
	txPacket[1] = (byte)(txlength >> 8);
	txPacket[2] = 0x05; // command request
						// [3] is reserved
	txPacket[4] = (byte)0;
	txPacket[5] = (byte)nameLength;

	memcpy(txPacket + 6, command, nameLength);
	if (inputPayload != NULL) {
		memcpy(txPacket + 6 + nameLength, inputPayload, length);
	}

	LogInfo("Invoking command %s to %s", command, input);

	SerialPnp_TxPacket(serialDevice, txPacket, txlength);

	byte* responsePacket;
	SerialPnp_RxPacket(serialDevice, &responsePacket, &length, 0x02);

	char* stval = SerialPnp_BinarySchemaToString(cmd->ResponseSchema, responsePacket, length);
	*response = responsePacket;

	if (inputPayload != NULL) {
		free(inputPayload);
	}

	free(txPacket);

	return 0;
}


void SerialPnp_ParseDescriptor(SINGLYLINKEDLIST_HANDLE interfaceDefinitions, byte* descriptor, DWORD length)
{
	int c = 4;

	byte version = descriptor[c++];
	byte display_name_length = descriptor[c++];

	char display_name[128];
	memcpy(display_name, descriptor + c, display_name_length);
	display_name[display_name_length] = '\0';

	LogInfo("Device Version : %d", version);
	LogInfo("Device Name    : %s", display_name);

	c += display_name_length;

	while (c < (int)length)
	{
		if (descriptor[c++] == 0x05)
		{
			// inline interface
			InterfaceDefinition* indef = calloc(1, sizeof(InterfaceDefinition));
			indef->Events = singlylinkedlist_create();
			indef->Properties = singlylinkedlist_create();
			indef->Commands = singlylinkedlist_create();

			// parse ID5
			UINT16 interface_id_length = descriptor[c] | (descriptor[c + 1] << 8);
			c += 2;

			char* interface_id = malloc(sizeof(char)*(interface_id_length + 1));
			memcpy(interface_id, descriptor + c, interface_id_length);
			interface_id[interface_id_length] = '\0';
			LogInfo("Interface ID : %s", interface_id);
			c += interface_id_length;

			indef->Id = interface_id;

			// now process the different types
			while (c < (int)length)
			{
				if (descriptor[c] > 0x03) break; // not in a type of nested entry anymore

				FieldDefinition fielddef = { 0 };

				// extract common field properties
				byte ptype = descriptor[c++];

				byte pname_length = descriptor[c++];
				char* pname = malloc(sizeof(char)*(pname_length + 1));
				memcpy(pname, descriptor + c, pname_length);
				pname[pname_length] = '\0';
				c += pname_length;

				byte pdisplay_name_length = descriptor[c++];
				char* pdisplay_name = malloc(sizeof(char)*(pdisplay_name_length + 1));
				memcpy(pdisplay_name, descriptor + c, pdisplay_name_length);
				pdisplay_name[pdisplay_name_length] = '\0';
				c += pdisplay_name_length;

				byte pdescription_length = descriptor[c++];
				char* pdescription = malloc(sizeof(char)*(pdescription_length + 1));
				memcpy(pdescription, descriptor + c, pdescription_length);
				pdescription[pdescription_length] = '\0';
				c += pdescription_length;

				LogInfo("\tProperty type : %d", ptype);
				LogInfo("\tName : %s", pname);
				LogInfo("\tDisplay Name : %s", pdisplay_name);
				LogInfo("\tDescription : %s", pdescription);

				fielddef.Name = pname;
				fielddef.DisplayName = pdisplay_name;
				fielddef.Description = pdescription;

				if (ptype == 0x01) // command
				{
					CommandDefinition* tfdef = calloc(1, sizeof(CommandDefinition));
					tfdef->defintion = fielddef;

					UINT16 prequest_schema = (UINT16)(descriptor[c] | (descriptor[c + 1] << 8));
					c += 2;
					UINT16 presponse_schema = (UINT16)(descriptor[c] | (descriptor[c + 1] << 8));
					c += 2;

					tfdef->RequestSchema = prequest_schema;
					tfdef->ResponseSchema = presponse_schema;

					singlylinkedlist_add(indef->Commands, tfdef);
				}
				else if (ptype == 0x02) // command
				{
					PropertyDefinition* tfdef = calloc(1, sizeof(PropertyDefinition));
					tfdef->defintion = fielddef;

					byte punit_length = descriptor[c++];
					char* punit = malloc(sizeof(char)*(punit_length + 1));
					memcpy(punit, descriptor + c, punit_length);
					punit[punit_length] = '\0';
					c += punit_length;

					LogInfo("\tUnit : %s", punit);

					UINT16 schema = descriptor[c] | (descriptor[c + 1] << 8);
					c += 2;
					tfdef->DataSchema = schema;

					byte flags = descriptor[c++];

					bool prequired = (flags & (1 << 1)) != 0;
					bool pwriteable = (flags & (1 << 0)) != 0;

					tfdef->Units = punit;
					tfdef->Required = prequired;
					tfdef->Writeable = pwriteable;

					singlylinkedlist_add(indef->Properties, tfdef);
				}
				else if (ptype == 0x03) // event
				{
					EventDefinition* tfdef = calloc(1, sizeof(EventDefinition));
					tfdef->defintion = fielddef;

					byte punit_length = descriptor[c++];
					char* punit = malloc(sizeof(char)*(punit_length + 1));
					memcpy(punit, descriptor + c, punit_length);
					punit[punit_length] = '\0';
					c += punit_length;

					LogInfo("\tUnit : %s", punit);

					UINT16 schema = (UINT16)(descriptor[c] | (descriptor[c + 1] << 8));
					c += 2;
					tfdef->DataSchema = schema;

					tfdef->Units = punit;

					singlylinkedlist_add(indef->Events, tfdef);
				}
			}

			singlylinkedlist_add(interfaceDefinitions, indef);
		}
		else
		{
			LogError("Unsupported descriptor\n");
		}
	}
}

typedef struct _SERIAL_DEVICE {
	char* InterfaceName;
} SERIAL_DEVICE, *PSERIAL_DEVICE;

SINGLYLINKEDLIST_HANDLE SerialDeviceList = NULL;
int SerialDeviceCount = 0;

int SerialPnp_FindSerialDevices()
{
	CONFIGRET cmResult = CR_SUCCESS;
	char* deviceInterfaceList;
	ULONG bufferSize = 0;

	SerialDeviceList = singlylinkedlist_create();

	cmResult = CM_Get_Device_Interface_List_Size(
		&bufferSize,
		(LPGUID)&GUID_DEVINTERFACE_COMPORT,
		NULL,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (CR_SUCCESS != cmResult) {
		return -1;
	}

	deviceInterfaceList = malloc(bufferSize * sizeof(char));

	cmResult = CM_Get_Device_Interface_ListA(
		(LPGUID)&GUID_DEVINTERFACE_COMPORT,
		NULL,
		deviceInterfaceList,
		bufferSize,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (CR_SUCCESS != cmResult) {
		return -1;
	}

	for (PCHAR currentDeviceInterface = deviceInterfaceList;
		*currentDeviceInterface != '\0';
		currentDeviceInterface += strlen(currentDeviceInterface) + 1)
	{
		PSERIAL_DEVICE serialDevs = malloc(sizeof(SERIAL_DEVICE));
		serialDevs->InterfaceName = malloc((strlen(currentDeviceInterface) + 1) * sizeof(char));
		strcpy_s(serialDevs->InterfaceName, strlen(currentDeviceInterface) + 1, currentDeviceInterface);
		singlylinkedlist_add(SerialDeviceList, serialDevs);
		SerialDeviceCount++;
	}

	return 0;
}

const char* serialDeviceChangeMessageformat = "{ \
                                                 \"Identity\": \"serial-pnp-discovery\" \
                                               }";

int SerialPnp_OpenDeviceWorker(void* context) {
	byte* desc;
	DWORD length;
	PNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload = { 0 };
	PSERIAL_DEVICE_CONTEXT deviceContext = context;

	SerialPnp_ResetDevice(deviceContext);
	SerialPnp_DeviceDescriptorRequest(deviceContext, &desc, &length);

	SerialPnp_ParseDescriptor(deviceContext->InterfaceDefinitions, desc, length);

	STRING_HANDLE asJson = STRING_new_JSON(serialDeviceChangeMessageformat);
	JSON_Value* json = json_parse_string(serialDeviceChangeMessageformat);
	JSON_Object* jsonObject = json_value_get_object(json);

	LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	while (interfaceItem != NULL) {
		const InterfaceDefinition* def = (const InterfaceDefinition*)singlylinkedlist_item_get_value(interfaceItem);
		json_object_set_string(jsonObject, "InterfaceId", def->Id);
		payload.Message = json_serialize_to_string(json_object_get_wrapping_value(jsonObject));
		payload.MessageLength = (int)json_serialization_size(json_object_get_wrapping_value(jsonObject));
		payload.Context = deviceContext;
		deviceContext->SerialDeviceChangeCallback(&payload);
		interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
	}

	SerialPnp_UartReceiver(deviceContext);

	return 0;
}

int SerialPnp_OpenDevice(const char* port, DWORD baudRate, PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback) {
	BOOL status = FALSE;
	HANDLE hSerial = CreateFileA(port,
		GENERIC_READ | GENERIC_WRITE,
		0, // must be opened with exclusive-access
		0, //NULL, no security attributes
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);

	if (hSerial == INVALID_HANDLE_VALUE) {
		// Handle the error
		int error = GetLastError();
		LogError("Failed to open com port %s, %x", port, error);
		if (error == ERROR_FILE_NOT_FOUND) {
			return -1;
		}

		return -1;
	}

	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	if (!GetCommState(hSerial, &dcbSerialParams)) {
		return -1;
	}
	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
	if (!SetCommState(hSerial, &dcbSerialParams)) {
		//error setting serial port state
		return -1;
	}

	PSERIAL_DEVICE_CONTEXT deviceContext = malloc(sizeof(SERIAL_DEVICE_CONTEXT));
	deviceContext->hSerial = hSerial;
	deviceContext->pnpAdapterInterface = NULL;
	deviceContext->SerialDeviceChangeCallback = DeviceChangeCallback;
	deviceContext->InterfaceDefinitions = singlylinkedlist_create();

	//deviceContext->osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// TODO error handling
	// https://docs.microsoft.com/en-us/previous-versions/ff802693(v=msdn.10)

	if (ThreadAPI_Create(&deviceContext->SerialDeviceWorker, SerialPnp_OpenDeviceWorker, deviceContext) != THREADAPI_OK) {
		LogError("ThreadAPI_Create failed");
	}

	return 0;
}

void SerialPnp_RxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte** receivedPacket, DWORD* length, char packetType)
{
	BYTE inb = 0;
	DWORD dwRead = 0;
	*receivedPacket = NULL;
	*length = 0;

	while (ReadFile(serialDevice->hSerial, &inb, 1, &dwRead, NULL)) {
		// Check for a start of packet byte
		if (inb == 0x5A)
		{
			serialDevice->RxBufferIndex = 0;
			serialDevice->RxEscaped = false;
			continue;
		}

		// Check for an escape byte
		if (inb == 0xEF)
		{
			serialDevice->RxEscaped = true;
			continue;
		}

		// If last byte was an escape byte, increment current byte by 1
		if (serialDevice->RxEscaped)
		{
			inb++;
			serialDevice->RxEscaped = false;
		}

		serialDevice->RxBuffer[serialDevice->RxBufferIndex++] = (byte)inb;

		if (serialDevice->RxBufferIndex >= 4096)
		{
			LogError("Filled Rx buffer. Protocol is bad.");
			break;
		}

		// Minimum packet length is 4, so once we are >= 4 begin checking
		// the receieve buffer length against the length field.
		if (serialDevice->RxBufferIndex >= 4)
		{
			int PacketLength = (int)((serialDevice->RxBuffer[0]) | (serialDevice->RxBuffer[1] << 8)); // LSB first, L-endian

			if (serialDevice->RxBufferIndex == PacketLength && (packetType == 0x00 || packetType == serialDevice->RxBuffer[2]))
			{
				*receivedPacket = malloc(serialDevice->RxBufferIndex * sizeof(byte));
				*length = serialDevice->RxBufferIndex;
				memcpy(*receivedPacket, serialDevice->RxBuffer, serialDevice->RxBufferIndex);
				serialDevice->RxBufferIndex = 0; // This should be reset anyway
								   // Deliver the newly receieved packet
								   //Console.Out.Write("\n");
				break;
			}
		}
	}
}

void SerialPnp_ResetDevice(PSERIAL_DEVICE_CONTEXT serialDevice) {
	// Prepare packet
	byte* resetPacket = calloc(4, sizeof(byte)); // packet header
	byte* responsePacket = NULL;
	resetPacket[0] = 4; // length 4
	resetPacket[1] = 0;
	resetPacket[2] = 0x01; // type reset

	ThreadAPI_Sleep(2000);

	// Send the new packet
	SerialPnp_TxPacket(serialDevice, resetPacket, 4);

	ThreadAPI_Sleep(2000);

	DWORD length;
	SerialPnp_RxPacket(serialDevice, &responsePacket, &length, 0x02);

	if (responsePacket == NULL) {
		LogError("received NULL for response packet");
		return;
	}

	if (responsePacket[2] != 0x02)
	{
		LogError("Bad reset response");
	}

	LogInfo("Receieved reset response");
}

void SerialPnp_DeviceDescriptorRequest(PSERIAL_DEVICE_CONTEXT serialDevice, byte** desc, DWORD* length)
{
	// Prepare packet
	byte txPacket[4] = { 0 }; // packet header
	txPacket[0] = 4; // length 4
	txPacket[1] = 0;
	txPacket[2] = 0x03; // type descriptor request

						// Get ready to receieve a reset response


	// Send the new packets
	SerialPnp_TxPacket(serialDevice, txPacket, 4);
	LogInfo("Sent descriptor request");

	SerialPnp_RxPacket(serialDevice, desc, length, 0x04);

	if ((*desc)[2] != 0x04)
	{
		LogError("Bad descriptor response");
		return;
	}

	LogInfo("Receieved descriptor response, of length %d", *length);
}

int SerialPnp_StartDiscovery(PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback, const char* deviceArgs, const char* adapterArgs) {
	if (deviceArgs == NULL) {
		return -1;
	}

	const char* port = NULL;
	const char* useComDevInterfaceStr;
	const char* baudRateParam;
	bool useComDeviceInterface = false;
	JSON_Value* jvalue = json_parse_string(deviceArgs);
	JSON_Object* args = json_value_get_object(jvalue);

	useComDevInterfaceStr = (const char*)json_object_dotget_string(args, "UseComDeviceInterface");
	if ((NULL != useComDevInterfaceStr) && (0 == stricmp(useComDevInterfaceStr, "true"))) {
		useComDeviceInterface = true;
	}

	if (!useComDeviceInterface) {
		port = (const char*)json_object_dotget_string(args, "ComPort");
		if (NULL == port) {
			LogError("ComPort parameter is missing in configuration");
			return -1;
		}
	}

	JSON_Object* SerialDeviceInterfaceInfo = args;

	baudRateParam = (const char*)json_object_dotget_string(args, "BaudRate");
	if (NULL == baudRateParam) {
		LogError("BaudRate parameter is missing in configuration");
		return -1;
	}

	PSERIAL_DEVICE seriaDevice = NULL;
	DWORD baudRate = atoi(baudRateParam);
	if (useComDeviceInterface) {
		if (SerialPnp_FindSerialDevices() < 0) {
			LogError("Failed to get com port %s", port);
		}

		LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(SerialDeviceList);
		if (item == NULL) {
			LogError("No serial device was found %s", port);
			return -1;
		}

		seriaDevice = (PSERIAL_DEVICE)singlylinkedlist_item_get_value(item);
	}


	LogInfo("Opening com port %s", useComDeviceInterface ? seriaDevice->InterfaceName : port);

	SerialPnp_OpenDevice(useComDeviceInterface ? seriaDevice->InterfaceName : port, baudRate, DeviceChangeCallback);
	return 0;
}

int SerialPnp_StopDiscovery() {
	return 0;
}

void SerialPnp_SendEventCallback(PNP_SEND_TELEMETRY_STATUS pnpSendEventStatus, void* userContextCallback)
{
	LogInfo("SerialDataSendEventCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int SerialPnp_SendEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data)
{
	int result;
	PNP_CLIENT_RESULT pnpClientResult;

	if (pnpInterface == NULL) {
		return 0;
	}

	if ((pnpClientResult = PnP_InterfaceClient_SendTelemetryAsync(pnpInterface, eventName, (const unsigned char*)data, strlen(data), SerialPnp_SendEventCallback, NULL)) != PNP_CLIENT_OK)
	{
		LogError("PnP_InterfaceClient_SendEventAsync failed, result=%d\n", pnpClientResult);
		result = __FAILURE__;
	}
	else
	{
		result = 0;
	}

	return result;
}

static void SerialPnp_PropertyUpdateHandler(const char* propertyName, unsigned const char* propertyInitial, size_t propertyInitialLen, unsigned const char* propertyDataUpdated, size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback)
{
	PNP_CLIENT_RESULT pnpClientResult;
	PNP_CLIENT_READWRITE_PROPERTY_RESPONSE propertyResponse;
	PSERIAL_DEVICE_CONTEXT deviceContext;

	LogInfo("Processed property.  propertyUpdated = %.*s", (int)propertyDataUpdatedLen, propertyDataUpdated);

	deviceContext = (PSERIAL_DEVICE_CONTEXT)userContextCallback;

	SerialPnp_PropertyHandler(deviceContext, propertyName, (char*)propertyDataUpdated);

	// TODO: Send the correct response code below
	propertyResponse.version = 1;
	propertyResponse.propertyData = propertyDataUpdated;
	propertyResponse.propertyDataLen = propertyDataUpdatedLen;
	propertyResponse.responseVersion = desiredVersion;
	propertyResponse.statusCode = 200;
	propertyResponse.statusDescription = "Property Updated Successfully";

	pnpClientResult = PnP_InterfaceClient_ReportReadWritePropertyStatusAsync(deviceContext->pnpAdapterInterface, propertyName, &propertyResponse, NULL, NULL);
}

// PnPSampleEnvironmentalSensor_SetCommandResponse is a helper that fills out a PNP_CLIENT_COMMAND_RESPONSE
static void SerialPnp_SetCommandResponse(PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, const char* responseData, int status)
{
	size_t responseLen = strlen(responseData);
	memset(pnpClientCommandResponseContext, 0, sizeof(*pnpClientCommandResponseContext));
	pnpClientCommandResponseContext->version = PNP_CLIENT_COMMAND_RESPONSE_VERSION_1;

	// Allocate a copy of the response data to return to the invoker.  The PnP layer that invoked PnPSampleEnvironmentalSensor_Blink
	// takes responsibility for freeing this data.
	if ((pnpClientCommandResponseContext->responseData = malloc(responseLen + 1)) == NULL)
	{
		LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate response data");
		pnpClientCommandResponseContext->status = 500;
	}
	else
	{
		pnpClientCommandResponseContext->responseData = (unsigned char*)responseData;
		//strcpy_s((char*)pnpClientCommandResponseContext->responseData, responseData);
		pnpClientCommandResponseContext->responseDataLen = responseLen;
		pnpClientCommandResponseContext->status = status;
	}
}


void SerialPnp_CommandUpdateHandler(const char* commandName, const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext, PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, void* userContextCallback) {
	PSERIAL_DEVICE_CONTEXT deviceContext;

	// LogInfo("Processed command frequency property updated.  propertyUpdated = %.*s", (int)propertyDataUpdatedLen, propertyDataUpdated);

	deviceContext = (PSERIAL_DEVICE_CONTEXT)userContextCallback;

	char* response;
	SerialPnp_CommandHandler(deviceContext, commandName, (char*)pnpClientCommandContext->requestData, &response);
	SerialPnp_SetCommandResponse(pnpClientCommandResponseContext, response, 200);
}


void SerialPnp_PropertyUpdateHandlerRedirect(int index, unsigned const char* propertyInitial,
	size_t propertyInitialLen, unsigned const char* propertyDataUpdated,
	size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback)
{
	PSERIAL_DEVICE_CONTEXT deviceContext = userContextCallback;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	const InterfaceDefinition* interfaceDef;

	for (int i = 0; i < 0 - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE property = interfaceDef->Properties;
	LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(property);
	const PropertyDefinition* ev = NULL;
	eventDef = singlylinkedlist_get_head_item(property);
	for (int i = 0; i < index + 1; i++) {
		ev = singlylinkedlist_item_get_value(eventDef);
		eventDef = singlylinkedlist_get_next_item(eventDef);
	}
	if (NULL != ev) {
		SerialPnp_PropertyUpdateHandler(ev->defintion.Name, propertyInitial, propertyInitialLen, propertyDataUpdated, propertyDataUpdatedLen,
			desiredVersion, userContextCallback);
	}
}

void SerialPnp_CommandUpdateHandlerRedirect(int index,
	const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
	PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
	void* userContextCallback)
{
	PSERIAL_DEVICE_CONTEXT deviceContext = userContextCallback;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	const InterfaceDefinition* interfaceDef;

	for (int i = 0; i < 0 - 1; i++)
	{
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	SINGLYLINKEDLIST_HANDLE command = interfaceDef->Commands;
	LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(command);
	const CommandDefinition* ev = NULL;
	eventDef = singlylinkedlist_get_head_item(command);
	for (int i = 0; i < index + 1; i++) {
		ev = singlylinkedlist_item_get_value(eventDef);
		eventDef = singlylinkedlist_get_next_item(eventDef);
	}
	if (NULL != ev) {
		SerialPnp_CommandUpdateHandler(ev->defintion.Name, pnpClientCommandContext, pnpClientCommandResponseContext, userContextCallback);
	}
}

const InterfaceDefinition* SerialPnp_GetInterface(PSERIAL_DEVICE_CONTEXT deviceContext, const char* interfaceId) {
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
	while (interfaceDefHandle != NULL) {
		const InterfaceDefinition* interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);
		if (stricmp(interfaceDef->Id, interfaceId) == 0) {
			return interfaceDef;
		}
		interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
	}
	return NULL;
}

int SerialPnp_GetListCount(SINGLYLINKEDLIST_HANDLE list) {
	if (NULL == list) {
		return 0;
	}

	LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(list);
	int count = 0;
	while (NULL != item) {
		count++;
		item = singlylinkedlist_get_next_item(item);
	}
	return count;
}

int SerialPnp_CreatePnpInterface(PNPADAPTER_CONTEXT adapterHandle, PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD args) {
	PSERIAL_DEVICE_CONTEXT deviceContext = (PSERIAL_DEVICE_CONTEXT)args->Context;
	JSON_Value* jvalue = json_parse_string(args->Message);
	JSON_Object* jmsg = json_value_get_object(jvalue);

	const char* interfaceId = json_object_get_string(jmsg, "InterfaceId");
	int result = 0;

    // Create an Azure Pnp interface for each interface in the SerialPnp descriptor
    LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
    while (interfaceDefHandle != NULL) {
        PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface = NULL;
        PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceClient = NULL;
        char** propertyNames = NULL;
        int propertyCount = 0;
        PNP_READWRITE_PROPERTY_UPDATE_CALLBACK* propertyUpdateTable = NULL;
        PNP_CLIENT_READWRITE_PROPERTY_UPDATED_CALLBACK_TABLE serialPropertyTable = { 0 };
        char** commandNames = NULL;
        int commandCount = 0;
        PNP_COMMAND_EXECUTE_CALLBACK* commandUpdateTable = NULL;
        PNP_CLIENT_COMMAND_CALLBACK_TABLE serialCommandTable = { 0 };
        const InterfaceDefinition* interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

	    // Construct property table
	    {
		    SINGLYLINKEDLIST_HANDLE property = interfaceDef->Properties;
		    propertyCount = SerialPnp_GetListCount(interfaceDef->Properties);

		    if (propertyCount > 0) {
			    LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(property);
			    const PropertyDefinition* ev;

			    propertyNames = malloc(sizeof(char*)*propertyCount);
			    if (NULL == propertyNames) {
				    result = -1;
				    goto exit;
			    }

			    propertyUpdateTable = malloc(sizeof(PNP_READWRITE_PROPERTY_UPDATE_CALLBACK*)*propertyCount);
			    if (NULL == propertyUpdateTable) {
				    result = -1;
				    goto exit;
			    }

			    eventDef = singlylinkedlist_get_head_item(property);
			    int x = 0;
			    while (eventDef != NULL) {
				    ev = singlylinkedlist_item_get_value(eventDef);
				    propertyNames[x] = ev->defintion.Name;
				    propertyUpdateTable[x] = PredefinedPropertyHandlerTables[x];
				    x++;
				    eventDef = singlylinkedlist_get_next_item(eventDef);
			    }

			    serialPropertyTable.numCallbacks = propertyCount;
			    serialPropertyTable.propertyNames = propertyNames;
			    serialPropertyTable.callbacks = propertyUpdateTable;
			    serialPropertyTable.version = 1;
		    }
	    }

	    // Construct command table
	    {
		    SINGLYLINKEDLIST_HANDLE command = interfaceDef->Commands;
		    int commandCount = SerialPnp_GetListCount(command);

		    if (commandCount > 0) {
			    LIST_ITEM_HANDLE cmdDef;
			    int x = 0;
			    const CommandDefinition* cv;

			    commandNames = malloc(sizeof(char*)*commandCount);
			    if (NULL == commandNames) {
				    result = -1;
				    goto exit;
			    }

			    commandUpdateTable = malloc(sizeof(PNP_COMMAND_EXECUTE_CALLBACK*)*commandCount);
			    if (NULL == commandUpdateTable) {
				    result = -1;
				    goto exit;
			    }

			    cmdDef = singlylinkedlist_get_head_item(command);
			    while (cmdDef != NULL) {
				    cv = singlylinkedlist_item_get_value(cmdDef);
				    commandNames[x] = cv->defintion.Name;
				    commandUpdateTable[x] = PredefinedCommandHandlerTables[x];
				    x++;
				    cmdDef = singlylinkedlist_get_next_item(cmdDef);
			    }

			    serialCommandTable.numCommandCallbacks = commandCount;
			    serialCommandTable.commandNames = commandNames;
			    serialCommandTable.commandCallbacks = commandUpdateTable;
			    serialCommandTable.version = 1;
		    }
	    }

	    pnpInterfaceClient = PnP_InterfaceClient_Create(pnpDeviceClientHandle, interfaceId, 
                                propertyCount > 0 ? &serialPropertyTable : NULL,
                                commandCount > 0 ? &serialCommandTable : NULL,
                                deviceContext);
	    if (NULL == pnpInterfaceClient) {
		    result = -1;
		    goto exit;
	    }

        // Create PnpAdapter Interface
        {
            PNPADPATER_INTERFACE_INIT_PARAMS interfaceParams = { 0 };
            interfaceParams.releaseInterface = SerialPnp_ReleasePnpInterface;
            //interfaceParams.pnpInterface = pnpInterfaceClient;

            result = PnpAdapterInterface_Create(adapterHandle, interfaceId, pnpInterfaceClient, &pnpAdapterInterface, &interfaceParams);
            if (result < 0) {
                goto exit;
            }
        }

        if (NULL != propertyUpdateTable) {
            free(propertyUpdateTable);
        }
        if (NULL != propertyNames) {
            free(propertyNames);
        }

        if (NULL != commandUpdateTable) {
            free(commandUpdateTable);
        }
        if (NULL != commandNames) {
            free(commandNames);
        }

        // Save the PnpAdapterInterface in device context
        deviceContext->pnpAdapterInterface = pnpAdapterInterface;

        interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
    }

exit:

	// Cleanup incase of failure
    if (result < 0) {
        // Destroy PnpInterfaceClient
        //if (NULL != pnpInterfaceClient) {
	       // PnP_InterfaceClient_Destroy(pnpInterfaceClient);
        //}
    }
    
	return 0;
}

void SerialPnp_FreeFieldDefinition(FieldDefinition* fdef) {
	if (NULL != fdef->Description) {
		free(fdef->Description);
	}

	if (NULL != fdef->DisplayName) {
		free(fdef->DisplayName);
	}

	if (NULL != fdef->Name) {
		free(fdef->Name);
	}
}

void SerialPnp_FreeEventDefinition(SINGLYLINKEDLIST_HANDLE events) {
	if (NULL == events) {
		return;
	}

	LIST_ITEM_HANDLE eventItem = singlylinkedlist_get_head_item(events);
	while (NULL != eventItem) {
		EventDefinition* e = (EventDefinition*)singlylinkedlist_item_get_value(eventItem);
		SerialPnp_FreeFieldDefinition(&e->defintion);
		if (NULL != e->Units) {
			free(e->Units);
		}
		free(e);
		eventItem = singlylinkedlist_get_next_item(eventItem);
	}
}

void SerialPnp_FreeCommandDefinition(SINGLYLINKEDLIST_HANDLE cmds) {
	if (NULL == cmds) {
		return;
	}

	LIST_ITEM_HANDLE cmdItem = singlylinkedlist_get_head_item(cmds);
	while (NULL != cmdItem) {
		CommandDefinition* c = (CommandDefinition*)singlylinkedlist_item_get_value(cmdItem);
		free(c);
		cmdItem = singlylinkedlist_get_next_item(cmdItem);
	}
}

void SerialPnp_FreePropertiesDefinition(SINGLYLINKEDLIST_HANDLE props) {
	if (NULL == props) {
		return;
	}

	LIST_ITEM_HANDLE propItem = singlylinkedlist_get_head_item(props);
	while (NULL != propItem) {
		PropertyDefinition* p = (PropertyDefinition*)singlylinkedlist_item_get_value(propItem);
		SerialPnp_FreeFieldDefinition(&p->defintion);
		if (NULL != p->Units) {
			free(p->Units);
		}
		free(p);
		propItem = singlylinkedlist_get_next_item(propItem);
	}
}

int SerialPnp_ReleasePnpInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface) {
	PSERIAL_DEVICE_CONTEXT deviceContext = PnpAdapterInterface_GetContext(pnpInterface);

	if (NULL == deviceContext) {
		return 0;
	}

	if (NULL != deviceContext->hSerial) {
		CloseHandle(deviceContext->hSerial);
	}

	if (deviceContext->InterfaceDefinitions) {
		LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
		while (interfaceItem != NULL) {
			InterfaceDefinition* def = (InterfaceDefinition*)singlylinkedlist_item_get_value(interfaceItem);
			SerialPnp_FreeEventDefinition(def->Events);
			SerialPnp_FreeCommandDefinition(def->Commands);
			SerialPnp_FreePropertiesDefinition(def->Properties);

			free(def);
			interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
		}
		singlylinkedlist_destroy(deviceContext->InterfaceDefinitions);
	}

	free(deviceContext);

	return 0;
}

int SerialPnp_Initialize(const char* adapterArgs) {
//    AZURE_UNREFERENCED_PARAMETER(adapterArgs);
	return 0;
}

int SerialPnp_Shutdown() {
	return 0;
}

DISCOVERY_ADAPTER SerialPnpDiscovery = {
	.Identity = "serial-pnp-discovery",
	.StartDiscovery = SerialPnp_StartDiscovery,
	.StopDiscovery = SerialPnp_StopDiscovery
};

PNP_ADAPTER SerialPnpInterface = {
	.identity = "serial-pnp-interface",
	.initialize = SerialPnp_Initialize,
	.shutdown = SerialPnp_Shutdown,
	.createPnpInterface = SerialPnp_CreatePnpInterface
};
