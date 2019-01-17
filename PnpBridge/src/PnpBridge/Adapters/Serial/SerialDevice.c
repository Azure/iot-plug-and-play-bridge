// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "common.h"

#include "azure_c_shared_utility/base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/lock.h"

#include "DiscoveryAdapterInterface.h"
#include "PnPAdapterInterface.h"

#include <Windows.h>
#include <cfgmgr32.h>

PNPBRIDGE_NOTIFY_DEVICE_CHANGE SerialDeviceChangeCallback = NULL;

#include <ctype.h>

typedef struct SerialPnPPacketHeader
{
	byte StartOfFrame;
	USHORT Length;
	byte PacketType;
} SerialPnPPacketHeader;

typedef enum Schema
{
	Invalid = 0,
	Byte,
	Float,
	Double,
	Int,
	Long,
	Boolean,
	String
} Schema;

typedef struct FieldDefinition
{
	char* Name;
	char* DisplayName;
	char* Description;
} FieldDefinition;


typedef struct EventDefinition
{
	FieldDefinition defintion;
	Schema DataSchema;
	char* Units;
} EventDefinition;

typedef struct PropertyDefinition
{
	FieldDefinition defintion;
	char* Units;
	bool Required;
	bool Writeable;
	Schema DataSchema;
} PropertyDefinition;

typedef struct CommandDefinition
{
	FieldDefinition defintion;
	Schema RequestSchema;
	Schema ResponseSchema;
} CommandDefinition;

typedef struct InterfaceDefinition
{
	char* Id;
	int Index;
	SINGLYLINKEDLIST_HANDLE Events;
	SINGLYLINKEDLIST_HANDLE Properties;
	SINGLYLINKEDLIST_HANDLE Commands;
} InterfaceDefinition;

SINGLYLINKEDLIST_HANDLE InterfaceDefinitions;

typedef struct _SERIAL_DEVICE_CONTEXT {
	HANDLE hSerial;
	PNP_INTERFACE_CLIENT_HANDLE* InterfaceHandle;
	byte RxBuffer[4096]; // Todo: maximum buffer size
	unsigned int RxBufferIndex;
	bool RxEscaped;
} SERIAL_DEVICE_CONTEXT, *PSERIAL_DEVICE_CONTEXT;

void Start() {
	LogInfo("Opening serial port");
}

void RxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte** receivedPacket, DWORD* length, char packetType);
void TxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte* OutPacket, int Length);
void UnsolicitedPacket(PSERIAL_DEVICE_CONTEXT device, byte* packet, DWORD length);

byte* StringSchemaToBinary(Schema Schema, byte* data, int* length);

int PnP_Sample_SendEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data);

int UartReceiver(void* context)
{
	BYTE msgBuffer[2048];
	PBYTE p = msgBuffer;
	UINT16 SizeToRead = 0;
	int result = 0;

	PSERIAL_DEVICE_CONTEXT deviceContext = (PSERIAL_DEVICE_CONTEXT) context;

	while (result >= 0)
	{
		byte* desc = NULL;
		DWORD length;

		RxPacket(deviceContext->hSerial, &desc, &length, 0x00);
		UnsolicitedPacket(deviceContext, desc, length);

		if (desc != NULL) {
			free(desc);
		}
	}

	return 0;
}


void TxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte* OutPacket, int Length)
{
	int txLength = 1 + Length;
	// First iterate through and find out our new length
	for (int i = 0; i<Length; i++)
	{
		if ((OutPacket[i] == 0x5A) || (OutPacket[i] == 0xEF))
		{
			txLength++;
		}
	}

	// Now construct outgoing buffer
	byte* TxPacket = malloc(sizeof(byte) *txLength);

	txLength = 1;
	TxPacket[0] = 0x5A; // Start of frame
	for (int i = 0; i<Length; i++)
	{
		// Escape these bytes where necessary
		if ((OutPacket[i] == 0x5A) || (OutPacket[i] == 0xEF))
		{
			TxPacket[txLength++] = 0xEF;
			TxPacket[txLength++] = (byte)(OutPacket[i] - 1);
		}
		else
		{
			TxPacket[txLength++] = OutPacket[i];
		}
	}

	DWORD write_size = 0;
	if (!WriteFile(serialDevice->hSerial, TxPacket, txLength, &write_size, NULL))
	{
		LogError("write failed");
	}

	free(TxPacket);
}


const EventDefinition* LookupEvent(char* EventName, int InterfaceId)
{
	const InterfaceDefinition* interfaceDef;
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(InterfaceDefinitions);

	for(int i=0; i<InterfaceId-1; i++)
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

byte* StringSchemaToBinary(Schema Schema, byte* data, int* length)
{
	byte* bd = NULL;

	if ((Schema == Float) || (Schema == Int))
	{
		bd = malloc(sizeof(byte)*4);
		*length = 4;

		if (Schema == Float)
		{
			float x = 0;
			x = (float) atof(data);
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

char* BinarySchemaToString(Schema Schema, byte* Data, byte length)
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

void UnsolicitedPacket(PSERIAL_DEVICE_CONTEXT device, byte* packet, DWORD length)
{
	//if (!Operational)
	//{
	//	//Console.WriteLine("Dropped packet due to non-operational state.");
	//	return; // drop the packet if we're not operational
	//}

	// Got an event
	if (packet[2] == 0x0A) {
		byte rxNameLength = packet[5];
		byte rxInterfaceId = packet[4];
		DWORD rxDataSize = length - rxNameLength - 6;

		//string event_name = Encoding.UTF8.GetString(packet, 6, rxNameLength);
		char* event_name = malloc(sizeof(char)*(rxNameLength + 1));
		memcpy(event_name, packet + 6, rxNameLength);
		event_name[rxNameLength] = '\0';

		const EventDefinition* ev = LookupEvent(event_name, rxInterfaceId);

		byte* rxData = malloc(sizeof(byte)*rxDataSize);
		memcpy(rxData, packet + 6 + rxNameLength, rxDataSize);

		char* rxstrdata = BinarySchemaToString(ev->DataSchema, rxData, (byte) rxDataSize);
		LogInfo("%s: %s", ev->defintion.Name, rxstrdata);

		PnP_Sample_SendEventAsync(device->InterfaceHandle, ev->defintion.Name, rxstrdata);

		free(event_name);
		free(rxData);
	}
	// Got a property update
	else if (packet[2] == 0x08)
	{
		// TODO
	}
}

THREAD_HANDLE UartRx;

void ResetDevice(PSERIAL_DEVICE_CONTEXT serialDevice);

void DeviceDescriptorRequest(PSERIAL_DEVICE_CONTEXT serialDevice, byte** desc, DWORD* length);

const PropertyDefinition* LookupProperty(char* propertyName, int InterfaceId)
{
	LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(InterfaceDefinitions);
	const InterfaceDefinition* interfaceDef;

	for (int i = 0; i<InterfaceId - 1; i++)
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

int PropertyHandler(PSERIAL_DEVICE_CONTEXT serialDevice, char* property, char* input)
{
	const PropertyDefinition* prop = LookupProperty(property, 0);

	if (prop == NULL)
	{
		return -1;
	}

	// otherwise serialize data
	int length = 0;
	byte* inputPayload = StringSchemaToBinary(prop->DataSchema, input, &length);

	int nameLength = (int)strlen(property);
	int txlength = 6 + nameLength + length;
	byte* txPacket = malloc(txlength);

	txPacket[0] = (byte)(txlength & 0xFF);
	txPacket[1] = (byte)(txlength >> 8);
	txPacket[2] = 0x07; // property request
						// [3] is reserved
	txPacket[4] = (byte) 0;
	txPacket[5] = (byte) nameLength;

	memcpy(txPacket+6, property, nameLength);
	if (inputPayload != NULL) {
		memcpy(txPacket + 6 + nameLength, inputPayload, length);
	}

	LogInfo("Setting sample rate to %s", input);

	TxPacket(serialDevice->hSerial, txPacket, txlength);

	//ThreadAPI_Sleep(2000);

	if (inputPayload != NULL) {
		free(inputPayload);
	}

	free(txPacket);

	return 0;
}


void ParseDescriptor(byte* descriptor, DWORD length)
{
	int c = 4;

	byte version = descriptor[c++];
	byte display_name_length = descriptor[c++];

	char display_name[128];
	memcpy(display_name, descriptor+c, display_name_length);
	display_name[display_name_length] = '\0';

	LogInfo("Device Version : %d", version);
	LogInfo("Device Name    : %s", display_name);


	InterfaceDefinitions = singlylinkedlist_create();

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
			UINT16 interface_id_length = descriptor[c] | (descriptor[c+1] << 8);
			c += 2;

			char* interface_id = malloc(sizeof(char)*(interface_id_length+1));
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

					UINT16 prequest_schema = (UINT16)(descriptor[c] | (descriptor[c+1] << 8));
					c += 2;
					UINT16 presponse_schema = (UINT16)(descriptor[c] | (descriptor[c+1] << 8));
					c += 2;

					tfdef->RequestSchema = prequest_schema;
					tfdef->ResponseSchema = presponse_schema;

					singlylinkedlist_add(indef->Commands, tfdef);
				}
				else if (ptype == 0x02) // property
				{
					PropertyDefinition* tfdef = calloc(1, sizeof(PropertyDefinition));
					tfdef->defintion = fielddef;

					byte punit_length = descriptor[c++];
					char* punit = malloc(sizeof(char)*(punit_length + 1));
					memcpy(punit, descriptor + c, punit_length);
					punit[punit_length] = '\0';
					c += punit_length;

					LogInfo("\tUnit : %s", punit);

					UINT16 schema = descriptor[c] | (descriptor[c+1] << 8);
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

					UINT16 schema = (UINT16)(descriptor[c] | (descriptor[c+1] << 8));
					c += 2;
					tfdef->DataSchema = schema;

					tfdef->Units = punit;

					singlylinkedlist_add(indef->Events, tfdef);
				}
			}

			singlylinkedlist_add(InterfaceDefinitions, indef);
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

int FindSerialDevices()
{
	CONFIGRET cmResult = CR_SUCCESS;
	char* deviceInterfaceList;
	ULONG bufferSize = 0;

	SerialDeviceList = singlylinkedlist_create();

	cmResult = CM_Get_Device_Interface_List_Size(
					&bufferSize,
					(LPGUID) &GUID_DEVINTERFACE_COMPORT,
					NULL,
					CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (CR_SUCCESS != cmResult) {
		return -1;
	}

	deviceInterfaceList = malloc(bufferSize*sizeof(char));

	cmResult = CM_Get_Device_Interface_ListA(
				(LPGUID) &GUID_DEVINTERFACE_COMPORT,
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
		serialDevs->InterfaceName = malloc((strlen(currentDeviceInterface)+1)*sizeof(char));
		strcpy_s(serialDevs->InterfaceName, strlen(currentDeviceInterface)+1, currentDeviceInterface);
		singlylinkedlist_add(SerialDeviceList, serialDevs);
		SerialDeviceCount++;
	}

	return 0;
}

int OpenDevice(char* port, DWORD baudRate) {
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

	////Set timeouts 
	//COMMTIMEOUTS timeouts = { 0 };
	//timeouts.ReadIntervalTimeout = 50;
	//timeouts.ReadTotalTimeoutConstant = 50;
	//timeouts.ReadTotalTimeoutMultiplier = 10;
	//timeouts.WriteTotalTimeoutConstant = 50;
	//timeouts.WriteTotalTimeoutMultiplier = 10;

	//if (SetCommTimeouts(hSerial, &timeouts) == FALSE)
	//	printf("\n\n    Error! in Setting Time Outs");
	//else
	//	printf("\n\n    Setting Serial Port Timeouts Successfull\n");

	//status = SetCommMask(hSerial, EV_RXCHAR); //Configure Windows to Monitor the serial device for Character Reception

	//if (status == FALSE)
	//	printf("\n\n    Error! in Setting CommMask");
	//else
	//	printf("\n\n    Setting CommMask successfull\n");

	//SerialPnPPacketHeader dr = { 0 };
	//dr.StartOfFrame = 0x5A;
	//dr.Length = sizeof(dr);
	//dr.PacketType = 1; // Get descriptor

	PSERIAL_DEVICE_CONTEXT deviceContext = malloc(sizeof(SERIAL_DEVICE_CONTEXT));
	deviceContext->hSerial = hSerial;
	deviceContext->InterfaceHandle = NULL;

    ResetDevice(deviceContext);

	byte* desc;
	DWORD length;

	DeviceDescriptorRequest(deviceContext, &desc, &length);

	ParseDescriptor(desc, length);

	PropertyHandler(deviceContext, "sample_rate", "300000");

	// TODO: Create a json object
    JSON_Object* serialDeviceChangePayload = json_value_get_object(json_parse_string(""));
    PNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload;

	//payload.Type = PNP_INTERFACE_ARRIVAL;
	payload.Message = serialDeviceChangePayload;
	payload.Context = deviceContext;
	LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(InterfaceDefinitions);
	while (interfaceItem != NULL) {
		const InterfaceDefinition* def = (const InterfaceDefinition*) singlylinkedlist_item_get_value(interfaceItem);
		json_object_set_string(serialDeviceChangePayload, "InterfaceId", def->Id);
		SerialDeviceChangeCallback(&payload);
		interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
	}

	if (ThreadAPI_Create(&UartRx, UartReceiver, deviceContext) != THREADAPI_OK)
	{
		LogError("ThreadAPI_Create failed");
	}

	/*LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(InterfaceDefinitions);
	while (interfaceItem != NULL) {
		InterfaceDefinition* def = singlylinkedlist_item_get_value(interfaceItem);
		DEVICE_CHANGE_PAYLOAD payload;

		PSERIAL_DEVICE_CONTEXT deviceContext = malloc(sizeof(SERIAL_DEVICE_CONTEXT));
		deviceContext->hSerial = hSerial;
		deviceContext->InterfaceHandle = NULL;
		payload.Type = PNP_INTERFACE_ARRIVAL;
		payload.Message = serialDeviceChangePayload;
		payload.Context = deviceContext;

		json_object_set_string(serialDeviceChangePayload, "InterfaceId", def->Id);
		SerialDeviceChangeCallback(&payload);

		if (ThreadAPI_Create(&UartRx, UartReceiver, deviceContext) != THREADAPI_OK)
		{
			LogError("ThreadAPI_Create failed");
		}

		interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
	}*/

	return 0;
}

void RxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte** receivedPacket, DWORD* length, char packetType)
{
	BYTE inb = 0;
	DWORD dwRead = 0;
	*receivedPacket = NULL;

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

void ResetDevice(PSERIAL_DEVICE_CONTEXT serialDevice) {
	// Prepare packet
	byte* resetPacket = calloc(4, sizeof(byte)); // packet header
	byte* responsePacket = NULL;
	resetPacket[0] = 4; // length 4
	resetPacket[1] = 0;
	resetPacket[2] = 0x01; // type reset

	ThreadAPI_Sleep(2000);

	// Send the new packet
	TxPacket(serialDevice->hSerial, resetPacket, 4);

	ThreadAPI_Sleep(2000);

	DWORD length;
	RxPacket(serialDevice, &responsePacket, &length, 0x02);

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

//New changes Start
void StopDevice(HANDLE hSerial) {
	CloseHandle(hSerial);
}

void DeviceDescriptorRequest(PSERIAL_DEVICE_CONTEXT serialDevice, byte** desc, DWORD* length)
{
	// Prepare packet
	byte txPacket[4] = { 0 }; // packet header
	txPacket[0] = 4; // length 4
	txPacket[1] = 0;
	txPacket[2] = 0x03; // type descriptor request

						// Get ready to receieve a reset response


	// Send the new packets
	TxPacket(serialDevice->hSerial, txPacket, 4);
	LogInfo("Sent descriptor request");

	RxPacket(serialDevice->hSerial, desc, length, 0x04);

	if ((*desc)[2] != 0x04)
	{
		LogError("Bad descriptor response");
		return;
	}

	LogInfo("Receieved descriptor response, of length %d", *length);
}

int SerialReportDevice(PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback, JSON_Object* deviceArgs, JSON_Object* adapterArgs) {
	if (deviceArgs == NULL) {
		return -1;
	}

	const char* port = (const char*) json_object_dotget_string(deviceArgs, "ComPort");

	SerialDeviceChangeCallback = DeviceChangeCallback;
	JSON_Object* SerialDeviceInterfaceInfo = deviceArgs;

	const char* baudRateParam = (const char*) json_object_dotget_string(deviceArgs, "BaudRate");
	DWORD baudRate = atoi(baudRateParam);

	if (FindSerialDevices() < 0) {
		LogError("Failed to get com port %s", port);
	}

	LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(SerialDeviceList);
	if (item == NULL) {
		LogError("No serial device was found %s", port);
		return -1;
	}

	const PSERIAL_DEVICE seriaDevice = (const PSERIAL_DEVICE) singlylinkedlist_item_get_value(item);

	LogInfo("Opening com port %s", port);

	//OpenDevice(seriaDevice->InterfaceName, baudRate);
	return 0;
}

int SerialStopDevice() {
	return 0;
}

void SerialDataSendEventCallback(PNP_SEND_TELEMETRY_STATUS pnpSendEventStatus, void* userContextCallback)
{
	LogInfo("SerialDataSendEventCallback called, result=%d, userContextCallback=%p", pnpSendEventStatus, userContextCallback);
}

int PnP_Sample_SendEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data)
{
	int result;
	PNP_CLIENT_RESULT pnpClientResult;

	if (pnpInterface == NULL) {
		return 0;
	}

	char msg[512];
	sprintf_s(msg, 512, "{\"%s\":\"%s\"}", eventName, data);

	if ((pnpClientResult = PnP_InterfaceClient_SendTelemetryAsync(pnpInterface, eventName, (const unsigned char*)msg, strlen(msg), SerialDataSendEventCallback, NULL)) != PNP_CLIENT_OK)
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


// PNP_READWRITE_PROPERTY_SAMPLE_STATE maintains state of system tied to specific interface.  Actual implementations
// will tend to have actual program state as part of this.
typedef struct PNP_READWRITE_PROPERTY_SAMPLE_STATE_TAG
{
	PNP_INTERFACE_CLIENT_HANDLE pnpInterfaceClientHandle;
} PNP_READWRITE_PROPERTY_SAMPLE_STATE;

static void SampleRate(unsigned const char* propertyInitial, size_t propertyInitialLen, unsigned const char* propertyDataUpdated, size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback)
{
	LogInfo("Processed telemetry frequency property updated.  propertyUpdated = %.*s", (int)propertyDataUpdatedLen, propertyDataUpdated);

	PSERIAL_DEVICE_CONTEXT deviceContext = (PSERIAL_DEVICE_CONTEXT)userContextCallback;

	JSON_Value* propValue = json_parse_string(propertyDataUpdated);
	JSON_Object* jsonObject = json_value_get_object(propValue);

	int sample_rate = (int)json_object_dotget_number(jsonObject, "value");
	char c[7];
	int i = 1;

	sprintf_s(c, 7, "%d", sample_rate*1000);

	PropertyHandler(deviceContext->hSerial, "sample_rate", c);

	PNP_CLIENT_RESULT pnpClientResult;

	PNP_CLIENT_READWRITE_PROPERTY_RESPONSE propertyResponse;

	propertyResponse.version = 1;
	propertyResponse.propertyData = propertyDataUpdated;
	propertyResponse.propertyDataLen = propertyDataUpdatedLen;
	propertyResponse.responseVersion = desiredVersion;
	propertyResponse.statusCode = 200;
	propertyResponse.statusDescription = "Property Updated Successfully";

	pnpClientResult = PnP_InterfaceClient_ReportReadWritePropertyStatusAsync(deviceContext->InterfaceHandle, "sample_rate", &propertyResponse, NULL, NULL);
}

const char* propertyNames[] = { "sample_rate" };

const PNP_READWRITE_PROPERTY_UPDATE_CALLBACK propertyUpdateTable[] = { SampleRate };

static const PNP_CLIENT_READWRITE_PROPERTY_UPDATED_CALLBACK_TABLE serialPropertyTable =
{
	1, // version of structure
	1, // number of properties and callbacks to map to
	(const char**)propertyNames,
	(const PNP_READWRITE_PROPERTY_UPDATE_CALLBACK*) propertyUpdateTable
};


int SerialCreatePnpInterface(PNPADAPTER_INTERFACE_HANDLE Interface, PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD args) {
	PSERIAL_DEVICE_CONTEXT deviceContext = (PSERIAL_DEVICE_CONTEXT) args->Context;
	deviceContext->InterfaceHandle = PnpAdapter_GetPnpInterface(Interface);

	//PnP_InterfaceClient_BindCallbacks(deviceContext->InterfaceHandle, &serialPropertyTable, NULL, deviceContext);

	return 0;
}

DISCOVERY_ADAPTER ArduinoSerialDiscovery = {
    .Identity = "arduino-serial-module-discovery",
	.StartDiscovery = SerialReportDevice,
	.StopDiscovery = SerialStopDevice
};

PNP_INTERFACE_MODULE SerialPnpInterface = {
    .Identity = "arduino-serial-module",
	.CreatePnpInterface = SerialCreatePnpInterface,
	.ReleaseInterface = NULL
};
