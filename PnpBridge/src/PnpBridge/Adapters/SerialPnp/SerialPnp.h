#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

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

	typedef enum DefinitionType {
		Telemetry,
		Property,
		Command
	} DefinitionType;

	typedef struct _SERIAL_DEVICE_CONTEXT {
		HANDLE hSerial;
        PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface;

		byte RxBuffer[4096]; // Todo: maximum buffer size
		unsigned int RxBufferIndex;
		bool RxEscaped;
		//OVERLAPPED osReader;
		THREAD_HANDLE SerialDeviceWorker;
		PNPBRIDGE_NOTIFY_DEVICE_CHANGE SerialDeviceChangeCallback;
        
        // list of interface definitions on this serial device
		SINGLYLINKEDLIST_HANDLE InterfaceDefinitions;
	} SERIAL_DEVICE_CONTEXT, *PSERIAL_DEVICE_CONTEXT;

	void SerialPnp_RxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte** receivedPacket, DWORD* length, char packetType);

	void SerialPnp_TxPacket(PSERIAL_DEVICE_CONTEXT serialDevice, byte* OutPacket, int Length);

	void SerialPnp_UnsolicitedPacket(PSERIAL_DEVICE_CONTEXT device, byte* packet, DWORD length);

	void SerialPnp_ResetDevice(PSERIAL_DEVICE_CONTEXT serialDevice);

	void SerialPnp_DeviceDescriptorRequest(PSERIAL_DEVICE_CONTEXT serialDevice, byte** desc, DWORD* length);

	byte* SerialPnp_StringSchemaToBinary(Schema Schema, byte* data, int* length);

	int SerialPnp_SendEventAsync(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data);

    int SerialPnp_ReleasePnpInterface(PNPADAPTER_INTERFACE_HANDLE pnpInterface);


#ifdef __cplusplus
}
#endif