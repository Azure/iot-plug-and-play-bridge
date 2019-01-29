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

SINGLYLINKEDLIST_HANDLE InterfaceDefinitions;

typedef struct _SERIAL_DEVICE_CONTEXT {
    HANDLE hSerial;
    PNP_INTERFACE_CLIENT_HANDLE* InterfaceHandle;
    byte RxBuffer[4096]; // Todo: maximum buffer size
    unsigned int RxBufferIndex;
    bool RxEscaped;
    THREAD_HANDLE SerialDeviceWorker;
    PNPBRIDGE_NOTIFY_DEVICE_CHANGE SerialDeviceChangeCallback;
} SERIAL_DEVICE_CONTEXT, *PSERIAL_DEVICE_CONTEXT;

#ifdef __cplusplus
}
#endif