//
// Serial PnP Device-Side Library
//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//
#include "SerialPnP.h"
#include <string.h>
#include <stdlib.h>

#define SERIALPNP_PROTOCOL_VERSION          0x01
#define SERIALPNP_PROTOCOL_PACKETSTART      0x5A
#define SERIALPNP_PROTOCOL_ESCAPE           0xEF

#define SERIALPNP_PACKETTYPE_NONE           0
#define SERIALPNP_PACKETTYPE_RESETREQ       1
#define SERIALPNP_PACKETTYPE_RESETRESP      2
#define SERIALPNP_PACKETTYPE_DESCREQ        3
#define SERIALPNP_PACKETTYPE_DESCRESP       4
#define SERIALPNP_PACKETTYPE_COMMANDREQ     5
#define SERIALPNP_PACKETTYPE_COMMANDRESP    6
#define SERIALPNP_PACKETTYPE_PROPREQ        7
#define SERIALPNP_PACKETTYPE_PROPRESP       8
#define SERIALPNP_PACKETTYPE_EVENT          10

#define SERIALPNP_DESCRIPTORTYPE_INTERFACE  5
#define SERIALPNP_DESCRIPTORTYPE_COMMAND    1
#define SERIALPNP_DESCRIPTORTYPE_PROPERTY   2
#define SERIALPNP_DESCRIPTORTYPE_EVENT      3

//
// Struct & Type Definitions
//
typedef struct _SerialPnPPacketHeader {
    uint16_t                    Length;
    uint8_t                     PacketType;
    uint8_t                     Reserved;
    char                        Body[0];
} SerialPnPPacketHeader;

typedef struct _SerialPnPPacketBody {
    uint8_t                     InterfaceId;
    uint8_t                     NameLength;
    char                        Payload[0];
} SerialPnPPacketBody;

typedef struct _SerialPnPDescriptorEntry {
    struct _SerialPnPDescriptorEntry
                                *Next;
    uint16_t                    ContentSize;
    char                        Content[0];
} SerialPnPDescriptorEntry;

typedef struct _SerialPnPCallback {
    SerialPnPDescriptorEntry*   DescriptorEntry;
    void*                       Callback;
} SerialPnPCallback;

//
// Global Variables
//
SerialPnPDescriptorEntry*       g_SerialPnPDescriptor = 0;
char                            g_SerialPnPRxBuffer[SERIALPNP_RXBUFFER_SIZE];
bool                            g_SerialPnPRxEscaped = false;
uint8_t                         g_SerialPnPRxBufferIndex = 0;
SerialPnPCallback               g_SerialPnPCallbacks[SERIALPNP_MAX_CALLBACK_COUNT];

//
// Internal Function Definitions
//
void
SerialPnP_SendEventRaw(
    const char*                 Name,
    void*                       Value,
    uint8_t                     ValueSize
);

void
SerialPnP_SerialWriteBuffer(
    char*                       Buffer,
    uint16_t                    BufferSize
);

void
SerialPnP_SerialWriteChar(
    char                        Out
);

void
SerialPnP_AppendDescriptor(
    SerialPnPDescriptorEntry*   NewEntry
);

void
SerialPnP_AddCallback(
    SerialPnPDescriptorEntry*   DescriptorEntry,
    void*                       Callback
);

SerialPnPCallback*
SerialPnP_FindCallback(
    uint8_t                     Type,
    const char*                 Name,
    uint8_t                     NameSize
);

void
SerialPnP_ProcessPacket(
    SerialPnPPacketHeader       *Packet
);

//
// Public Function Implementations
//
void
SerialPnP_Setup(
    const char*     DeviceName
)
{
    char* rawDescriptorEntry;
    uint8_t deviceNameLength = strlen(DeviceName);

    // Reset state
    g_SerialPnPRxBufferIndex = 0;
    g_SerialPnPRxEscaped = false;

    // First record holds version, name length, and name.
    g_SerialPnPDescriptor = malloc(sizeof(SerialPnPDescriptorEntry) +
                                   deviceNameLength + 2);
    g_SerialPnPDescriptor->ContentSize = deviceNameLength + 2;
    g_SerialPnPDescriptor->Next = 0;

    rawDescriptorEntry = g_SerialPnPDescriptor->Content;
    rawDescriptorEntry[0] = SERIALPNP_PROTOCOL_VERSION;
    rawDescriptorEntry[1] = deviceNameLength;

    memcpy(&rawDescriptorEntry[2],
           DeviceName,
           deviceNameLength);

    memset(g_SerialPnPCallbacks, 0, sizeof(g_SerialPnPCallbacks));

    // Call platform-specific initialization function for serial port.
    SerialPnP_PlatformSerialInit();
}

void
SerialPnP_Ready()
{
    // Send reset completion notification.
    SerialPnPPacketHeader out = {0};

    out.Length = sizeof(SerialPnPPacketHeader);
    out.PacketType = SERIALPNP_PACKETTYPE_RESETRESP;

    SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_PACKETSTART);
    SerialPnP_SerialWriteBuffer((char*) &out,
                                sizeof(SerialPnPPacketHeader));
}

void
SerialPnP_NewInterface(
    const char*     InterfaceIdUri
)
{
    char* rawDescriptorEntry;
    uint16_t interfaceIdUriLength = strlen(InterfaceIdUri);

    // Allocate a record to hold the interface definition
    SerialPnPDescriptorEntry* entry = malloc(sizeof(SerialPnPDescriptorEntry) +
                                             interfaceIdUriLength + 3);
    entry->ContentSize = interfaceIdUriLength + 3;
    rawDescriptorEntry = entry->Content;
    rawDescriptorEntry[0] = SERIALPNP_DESCRIPTORTYPE_INTERFACE;
    rawDescriptorEntry[1] = interfaceIdUriLength & 0xFF;
    rawDescriptorEntry[2] = interfaceIdUriLength >> 8;

    memcpy(&rawDescriptorEntry[3],
           InterfaceIdUri,
           interfaceIdUriLength);

    SerialPnP_AppendDescriptor(entry);
}

void
SerialPnP_NewEvent(
    const char*     Name,
    const char*     DisplayName,
    const char*     Description,
    SerialPnPSchema Schema,
    const char*     Units
)
{
    char* rawDescriptorEntry;
    uint16_t rawDescriptorOffset = 0;

    uint8_t nlen = strlen(Name);
    uint8_t dnlen = strlen(DisplayName);
    uint8_t desclen = strlen(Description);
    uint8_t ulen = strlen(Units);

    SerialPnPDescriptorEntry* entry = malloc(sizeof(SerialPnPDescriptorEntry) +
                                             nlen + dnlen + desclen + ulen +
                                             7);
    entry->ContentSize = nlen + dnlen + desclen + ulen + 7;
    rawDescriptorEntry = entry->Content;
    rawDescriptorEntry[rawDescriptorOffset++] = SERIALPNP_DESCRIPTORTYPE_EVENT;

    rawDescriptorEntry[rawDescriptorOffset++] = nlen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Name,
           nlen);
    rawDescriptorOffset += nlen;

    rawDescriptorEntry[rawDescriptorOffset++] = dnlen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           DisplayName,
           dnlen);
    rawDescriptorOffset += dnlen;

    rawDescriptorEntry[rawDescriptorOffset++] = desclen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Description,
           desclen);
    rawDescriptorOffset += desclen;

    rawDescriptorEntry[rawDescriptorOffset++] = ulen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Units,
           ulen);
    rawDescriptorOffset += ulen;

    // Schema is 2 bytes wide
    rawDescriptorEntry[rawDescriptorOffset++] = Schema;
    rawDescriptorEntry[rawDescriptorOffset++] = 0;

    SerialPnP_AppendDescriptor(entry);
}

void
SerialPnP_NewProperty(
    const char*     Name,
    const char*     DisplayName,
    const char*     Description,
    const char*     Units,
    SerialPnPSchema Schema,
    bool            Required,
    bool            Writeable,
    SerialPnPCb*    Callback
)
{
    char* rawDescriptorEntry;
    uint16_t rawDescriptorOffset = 0;

    uint8_t nlen = strlen(Name);
    uint8_t dnlen = strlen(DisplayName);
    uint8_t desclen = strlen(Description);
    uint8_t ulen = strlen(Units);

    SerialPnPDescriptorEntry* entry = malloc(sizeof(SerialPnPDescriptorEntry) +
                                             nlen + dnlen + desclen + ulen +
                                             8);
    entry->ContentSize = nlen + dnlen + desclen + ulen + 8;
    rawDescriptorEntry = entry->Content;
    rawDescriptorEntry[rawDescriptorOffset++] = SERIALPNP_DESCRIPTORTYPE_PROPERTY;

    rawDescriptorEntry[rawDescriptorOffset++] = nlen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Name,
           nlen);
    rawDescriptorOffset += nlen;

    rawDescriptorEntry[rawDescriptorOffset++] = dnlen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           DisplayName,
           dnlen);
    rawDescriptorOffset += dnlen;

    rawDescriptorEntry[rawDescriptorOffset++] = desclen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Description,
           desclen);
    rawDescriptorOffset += desclen;

    rawDescriptorEntry[rawDescriptorOffset++] = ulen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Units,
           ulen);
    rawDescriptorOffset += ulen;

    // Schema is 2 bytes wide
    rawDescriptorEntry[rawDescriptorOffset++] = Schema;
    rawDescriptorEntry[rawDescriptorOffset++] = 0;

    // Set flags
    ulen = 0;
    if (Required) ulen |= (1 << 1);
    if (Writeable) ulen |= (1 << 0);

    rawDescriptorEntry[rawDescriptorOffset++] = ulen;

    SerialPnP_AppendDescriptor(entry);
    SerialPnP_AddCallback(entry, Callback);
}

void
SerialPnP_NewCommand(
    const char*     Name,
    const char*     DisplayName,
    const char*     Description,
    SerialPnPSchema InputSchema,
    SerialPnPSchema OutputSchema,
    SerialPnPCb*    Callback
)
{
    char* rawDescriptorEntry;
    uint16_t rawDescriptorOffset = 0;

    uint8_t nlen = strlen(Name);
    uint8_t dnlen = strlen(DisplayName);
    uint8_t desclen = strlen(Description);

    SerialPnPDescriptorEntry* entry = malloc(sizeof(SerialPnPDescriptorEntry) +
                                             nlen + dnlen + desclen +
                                             8);
    entry->ContentSize = nlen + dnlen + desclen + 8;
    rawDescriptorEntry = entry->Content;
    rawDescriptorEntry[rawDescriptorOffset++] = SERIALPNP_DESCRIPTORTYPE_COMMAND;

    rawDescriptorEntry[rawDescriptorOffset++] = nlen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Name,
           nlen);
    rawDescriptorOffset += nlen;

    rawDescriptorEntry[rawDescriptorOffset++] = dnlen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           DisplayName,
           dnlen);
    rawDescriptorOffset += dnlen;

    rawDescriptorEntry[rawDescriptorOffset++] = desclen;
    memcpy(&rawDescriptorEntry[rawDescriptorOffset],
           Description,
           desclen);
    rawDescriptorOffset += desclen;

    // Schema is 2 bytes wide
    rawDescriptorEntry[rawDescriptorOffset++] = InputSchema;
    rawDescriptorEntry[rawDescriptorOffset++] = 0;

    rawDescriptorEntry[rawDescriptorOffset++] = OutputSchema;
    rawDescriptorEntry[rawDescriptorOffset++] = 0;

    SerialPnP_AppendDescriptor(entry);
    SerialPnP_AddCallback(entry, Callback);
}

void
SerialPnP_Process()
{
    while (SerialPnP_PlatformSerialAvailable()) {
        char inb = SerialPnP_PlatformSerialRead();
        
        if (inb == SERIALPNP_PROTOCOL_PACKETSTART) {
            g_SerialPnPRxBufferIndex = 0;
            g_SerialPnPRxEscaped = false;
            continue;
        }

        if (inb == SERIALPNP_PROTOCOL_ESCAPE) {
            g_SerialPnPRxEscaped = true;
            continue;
        }

        if (g_SerialPnPRxEscaped) {
            inb++;
            g_SerialPnPRxEscaped = false;
        }

        if (g_SerialPnPRxBufferIndex >= SERIALPNP_RXBUFFER_SIZE) {
            g_SerialPnPRxBufferIndex = SERIALPNP_RXBUFFER_SIZE - 1;
        }

        g_SerialPnPRxBuffer[g_SerialPnPRxBufferIndex++] = inb;

        if (g_SerialPnPRxBufferIndex >= sizeof(SerialPnPPacketHeader)) {
            SerialPnPPacketHeader *p = (SerialPnPPacketHeader*) g_SerialPnPRxBuffer;

            if (p->Length == g_SerialPnPRxBufferIndex) {
                // we can parse the packet now
                SerialPnP_ProcessPacket(p);
                g_SerialPnPRxBufferIndex = 0;
            }
        }
    }
}

void
SerialPnP_SendEventFloat(
    const char*     Name,
    float           Value
)
{
    SerialPnP_SendEventRaw(Name, (void*) &Value, sizeof(float));
}

void
SerialPnP_SendEventInt(
    const char*     Name,
    int32_t         Value
)
{
    SerialPnP_SendEventRaw(Name, (void*) &Value, sizeof(int32_t));
}

//
// Internal Function Implementations
//
void
SerialPnP_SendEventRaw(
    const char*                 Name,
    void*                       Value,
    uint8_t                     ValueSize
)
{
    SerialPnPPacketHeader out = {0};

    uint8_t nlen = strlen(Name);

    out.Length = sizeof(SerialPnPPacketHeader) +
                 sizeof(SerialPnPPacketBody) +
                 nlen +
                 ValueSize;

    out.PacketType = SERIALPNP_PACKETTYPE_EVENT;

    SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_PACKETSTART); // need to send sync
    SerialPnP_SerialWriteBuffer((char*) &out, sizeof(SerialPnPPacketHeader));
    SerialPnP_SerialWriteChar(0); // interface id = 0
    SerialPnP_SerialWriteChar(nlen);
    SerialPnP_SerialWriteBuffer((char*) Name, nlen);
    SerialPnP_SerialWriteBuffer(Value, ValueSize);
}

void
SerialPnP_SerialWriteBuffer(
    char*                       Buffer,
    uint16_t                    BufferSize
)
{
    uint16_t c;

    for (c = 0; c < BufferSize; c++) {
        if ((Buffer[c] == SERIALPNP_PROTOCOL_PACKETSTART) || (Buffer[c] == SERIALPNP_PROTOCOL_ESCAPE)) {
            SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_ESCAPE);
            SerialPnP_PlatformSerialWrite(Buffer[c] - 1);
        } else {
            SerialPnP_PlatformSerialWrite(Buffer[c]);
        }
    }
}

void
SerialPnP_SerialWriteChar(
    char                        Out
)
{
    if ((Out == SERIALPNP_PROTOCOL_PACKETSTART) || (Out == SERIALPNP_PROTOCOL_ESCAPE)) {
        SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_ESCAPE);
        SerialPnP_PlatformSerialWrite(Out - 1);
    } else {
        SerialPnP_PlatformSerialWrite(Out);
    }
}

void
SerialPnP_AppendDescriptor(
    SerialPnPDescriptorEntry*   NewEntry
)
{
    NewEntry->Next = 0;

    SerialPnPDescriptorEntry* lastDescriptor = g_SerialPnPDescriptor;

    while (lastDescriptor->Next) {
        lastDescriptor = lastDescriptor->Next;
    }

    lastDescriptor->Next = NewEntry;
}

void
SerialPnP_AddCallback(
    SerialPnPDescriptorEntry*   DescriptorEntry,
    void*                       Callback
)
{
    uint8_t c = 0;

    while (c < SERIALPNP_MAX_CALLBACK_COUNT) {
        SerialPnPCallback* cc = &g_SerialPnPCallbacks[c];

        if (cc->Callback == 0) {
            cc->DescriptorEntry = DescriptorEntry;
            cc->Callback = Callback;
            break;
        }

        c++;
    }
}

SerialPnPCallback*
SerialPnP_FindCallback(
    uint8_t                     Type,
    const char*                 Name,
    uint8_t                     NameSize
)
{
    uint8_t c = 0;
    char* rawDescriptorEntry;

    while (c < SERIALPNP_MAX_CALLBACK_COUNT) {
        SerialPnPCallback* cc = &g_SerialPnPCallbacks[c];

        c++;

        if (cc->Callback == 0) {
            continue;
        }

        rawDescriptorEntry = cc->DescriptorEntry->Content;

        // if len matches
        if ((*(rawDescriptorEntry) == Type) &&
            (*(rawDescriptorEntry+1) == NameSize)) {
            if (strncmp(rawDescriptorEntry+2, Name, NameSize) == 0) {
                return cc;
            }
        }
    }

    return 0;
}

void
SerialPnP_ProcessPacket(
    SerialPnPPacketHeader       *Packet
)
{
    SerialPnPPacketHeader out = {0};

    // Reset request
    if (Packet->PacketType == SERIALPNP_PACKETTYPE_RESETREQ) {
        SerialPnP_PlatformReset();

    // Descriptor request
    } else if (Packet->PacketType == SERIALPNP_PACKETTYPE_DESCREQ) {
        // First we have to calculate length of the descriptor
        uint16_t descriptorLength = 0;

        for (SerialPnPDescriptorEntry* entry = g_SerialPnPDescriptor;
             entry != 0;
             entry = entry->Next)
        {
            descriptorLength += entry->ContentSize;
        }

        out.Length = sizeof(SerialPnPPacketHeader) + descriptorLength;
        out.PacketType = SERIALPNP_PACKETTYPE_DESCRESP;

        SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_PACKETSTART); // need to send sync
        SerialPnP_SerialWriteBuffer((char*) &out, sizeof(SerialPnPPacketHeader));

        // Send actual descriptor, piece by piece
        for (SerialPnPDescriptorEntry* entry = g_SerialPnPDescriptor;
             entry != 0;
             entry = entry->Next)
        {
            SerialPnP_SerialWriteBuffer(entry->Content, entry->ContentSize);
        }

    // Property write request
    } else if (Packet->PacketType == SERIALPNP_PACKETTYPE_PROPREQ) {
        // find entry in table
        SerialPnPPacketBody* body = (SerialPnPPacketBody*) Packet->Body;

        SerialPnPCallback* cb = 0;
        cb = SerialPnP_FindCallback(SERIALPNP_DESCRIPTORTYPE_PROPERTY, //property type
                                    body->Payload,
                                    body->NameLength);

            uint32_t outp = -1;

            if (cb) {
            // If there's no data, call it for output only
            if (Packet->Length - (sizeof(SerialPnPPacketHeader) +
                                  sizeof(SerialPnPPacketBody) +
                                  body->NameLength) == 0) {

                ((SerialPnPCb) cb->Callback)(0, &outp);
            } else {
                ((SerialPnPCb) cb->Callback)(body->Payload + body->NameLength, &outp);
            }
            }

            out.Length = sizeof(SerialPnPPacketHeader) +
                         sizeof(SerialPnPPacketBody) +
                         body->NameLength +
                         sizeof(outp); // payload size; uint32

            out.PacketType = SERIALPNP_PACKETTYPE_PROPRESP;

            SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_PACKETSTART); // need to send sync
            SerialPnP_SerialWriteBuffer((char*) &out, sizeof(out));
            SerialPnP_SerialWriteChar(0); // Interface ID = 0
            SerialPnP_SerialWriteChar(body->NameLength);
            SerialPnP_SerialWriteBuffer((char*) body->Payload, body->NameLength);
            SerialPnP_SerialWriteBuffer((char*) &outp, sizeof(outp));
        
    } else if (Packet->PacketType == SERIALPNP_PACKETTYPE_COMMANDREQ) {
        // find entry in table
        SerialPnPPacketBody* body = (SerialPnPPacketBody*) Packet->Body;

        SerialPnPCallback* cb = 0;
        cb = SerialPnP_FindCallback(SERIALPNP_DESCRIPTORTYPE_COMMAND, //method type
                                    body->Payload,
                                    body->NameLength);

          int32_t outp = -1;

          if (cb) {
            ((SerialPnPCb) cb->Callback)(body->Payload + body->NameLength, &outp);
          }
   
            out.Length = sizeof(SerialPnPPacketHeader) +
                         sizeof(SerialPnPPacketBody) +
                         body->NameLength +
                         sizeof(outp); // payload size; uint32

            out.PacketType = SERIALPNP_PACKETTYPE_COMMANDRESP;

            SerialPnP_PlatformSerialWrite(SERIALPNP_PROTOCOL_PACKETSTART); // need to send sync
            SerialPnP_SerialWriteBuffer((char*) &out, sizeof(out));
            SerialPnP_SerialWriteChar(0); // Interface ID = 0
            SerialPnP_SerialWriteChar(body->NameLength);
            SerialPnP_SerialWriteBuffer((char*) body->Payload, body->NameLength);
            SerialPnP_SerialWriteBuffer((char*) &outp, sizeof(outp));
    }
}