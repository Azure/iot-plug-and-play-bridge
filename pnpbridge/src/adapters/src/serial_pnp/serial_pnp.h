// Copyright (c) Microsoft. All rights reserved. 
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/condition.h"

#define MAX_BUFFER_SIZE 4096

#define SERIALPNP_RESET_OR_DESCRIPTOR_MAX_RETRIES 3

#define SERIALPNP_MIN_PACKET_LENGTH 4
#define SERIALPNP_START_OF_FRAME_BYTE 0x5A
#define SERIALPNP_ESCAPE_BYTE         0xEF

// Offsets of fields within the packet relative to the start of packet
#define SERIALPNP_PACKET_PACKET_LENGTH_OFFSET    0
#define SERIALPNP_PACKET_PACKET_TYPE_OFFSET      2
#define SERIALPNP_PACKET_PAYLOAD_OFFSET          4
#define SERIALPNP_PACKET_INTERFACE_NUMBER_OFFSET 4
#define SERIALPNP_PACKET_NAME_LENGTH_OFFSET      5
#define SERIALPNP_PACKET_NAME_OFFSET             6

// Offsets of fields within the packet relative to the start of payload
#define SERIALPNP_PAYLOAD_INTERFACE_NUMBER_OFFSET 0
#define SERIALPNP_PAYLOAD_NAME_LENGTH_OFFSET      1
#define SERIALPNP_PAYLOAD_NAME_OFFSET             2

// Packet type codes
#define SERIALPNP_PACKET_TYPE_RESET_REQUEST         0x01
#define SERIALPNP_PACKET_TYPE_RESET_RESPONSE        0x02
#define SERIALPNP_PACKET_TYPE_DESCRIPTOR_REQUEST    0x03
#define SERIALPNP_PACKET_TYPE_DESCRIPTOR_RESPONSE   0x04
#define SERIALPNP_PACKET_TYPE_COMMAND_REQUEST       0x05
#define SERIALPNP_PACKET_TYPE_COMMAND_RESPONSE      0x06
#define SERIALPNP_PACKET_TYPE_PROPERTY_REQUEST      0x07
#define SERIALPNP_PACKET_TYPE_PROPERTY_NOTIFICATION 0x08
#define SERIALPNP_PACKET_TYPE_EVENT_NOTIFICATION    0x0A

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
        IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient;
        char * ComponentName;
        byte RxBuffer[MAX_BUFFER_SIZE]; // Temporary buffer that gets filled by the reading thread. TODO: maximum buffer size
        byte* pbMainBuffer;             // pointer used to pass buffers back to the main thread
        LOCK_HANDLE CommandLock;
        LOCK_HANDLE CommandResponseWaitLock;
        COND_HANDLE CommandResponseWaitCondition;
#ifdef WIN32
        OVERLAPPED osReader;
        OVERLAPPED osWriter;
#endif
        unsigned int RxBufferIndex;
        bool RxEscaped;
        THREAD_HANDLE SerialDeviceWorker;
        THREAD_HANDLE TelemetryWorkerHandle;
        // list of interface definitions on this serial device
        SINGLYLINKEDLIST_HANDLE InterfaceDefinitions;
    } SERIAL_DEVICE_CONTEXT, *PSERIAL_DEVICE_CONTEXT;

    IOTHUB_CLIENT_RESULT SerialPnp_RxPacket(
        PSERIAL_DEVICE_CONTEXT serialDevice,
        byte** receivedPacket,
        DWORD* length,
        char packetType);

    IOTHUB_CLIENT_RESULT SerialPnp_TxPacket(
        PSERIAL_DEVICE_CONTEXT serialDevice,
        byte* OutPacket,
        int Length);

    void SerialPnp_UnsolicitedPacket(
        PSERIAL_DEVICE_CONTEXT device,
        byte* packet,
        DWORD length);

    IOTHUB_CLIENT_RESULT SerialPnp_ResetDevice(PSERIAL_DEVICE_CONTEXT serialDevice);

    IOTHUB_CLIENT_RESULT SerialPnp_DeviceDescriptorRequest(
        PSERIAL_DEVICE_CONTEXT serialDevice,
        byte** desc,
        DWORD* length);

    byte* SerialPnp_StringSchemaToBinary(
        Schema Schema,
        byte* data,
        int* length);

    IOTHUB_CLIENT_RESULT SerialPnp_SendEventAsync(
        PSERIAL_DEVICE_CONTEXT DeviceContext,
        char* TelemetryName,
        char* TelemetryData);

    // Serial Pnp Adapter Config
    #define PNP_CONFIG_ADAPTER_SERIALPNP_COMPORT "com_port"
    #define PNP_CONFIG_ADAPTER_SERIALPNP_USEDEFAULT "use_com_device_interface"
    #define PNP_CONFIG_ADAPTER_SERIALPNP_BAUDRATE "baud_rate"

#ifdef __cplusplus
}
#endif