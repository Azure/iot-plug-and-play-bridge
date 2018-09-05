#include "Arduino.h"
#include "SerialPnP.h"

char SerialPnP::s_Descriptor[384];
uint16_t SerialPnP::s_DescriptorIndex;

char SerialPnP::s_RxBuffer[64];
uint8_t SerialPnP::s_RxBufferIndex;

bool SerialPnP::s_Escaped;

SerialPnPCallback SerialPnP::s_Callbacks[8];

void
SerialPnP::Setup(const char* Name) {
    s_DescriptorIndex = 0;
    s_RxBufferIndex = 0;

    s_Escaped = false;

    uint8_t nlen = strlen(Name);

    s_Descriptor[s_DescriptorIndex++] = 0x01; // version
    s_Descriptor[s_DescriptorIndex++] = nlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Name,
           nlen);

    memset(s_Callbacks, 0, sizeof(s_Callbacks));

    s_DescriptorIndex += nlen;

    Serial.begin(115200);
}

void
SerialPnP::NewInlineInterface(
    const char* InterfaceId
) {
    s_Descriptor[s_DescriptorIndex++] = 0x04;

    uint8_t ilen = strlen(InterfaceId);

    s_Descriptor[s_DescriptorIndex++] = ilen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           InterfaceId,
           ilen);

    s_DescriptorIndex += ilen;
}

void
SerialPnP::NewEvent(const char* Name,
                    const char* DisplayName,
                    const char* Description,
                    SerialPnPSchema Schema,
                    const char* Units)
{
    s_Descriptor[s_DescriptorIndex++] = 0x03;

    uint8_t nlen = strlen(Name);
    uint8_t dnlen = strlen(DisplayName);
    uint8_t desclen = strlen(Description);
    uint8_t ulen = strlen(Units);

    s_Descriptor[s_DescriptorIndex++] = nlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Name,
           nlen);
    s_DescriptorIndex += nlen;

    s_Descriptor[s_DescriptorIndex++] = dnlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           DisplayName,
           dnlen);
    s_DescriptorIndex += dnlen;

    s_Descriptor[s_DescriptorIndex++] = desclen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Description,
           desclen);
    s_DescriptorIndex += desclen;

    s_Descriptor[s_DescriptorIndex++] = ulen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Units,
           ulen);
    s_DescriptorIndex += ulen;

    s_Descriptor[s_DescriptorIndex++] = Schema;
}

void
SerialPnP::NewProperty(const char* Name,
                       const char* DisplayName,
                       const char* Description,
                       const char* Units,
                       bool Required,
                       bool Writeable,
                       void* Callback)
{
    s_Descriptor[s_DescriptorIndex++] = 0x02;

    uint8_t nlen = strlen(Name);
    uint8_t dnlen = strlen(DisplayName);
    uint8_t desclen = strlen(Description);
    uint8_t ulen = strlen(Units);

    AddCallback(s_DescriptorIndex - 1, Callback);

    s_Descriptor[s_DescriptorIndex++] = nlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Name,
           nlen);
    s_DescriptorIndex += nlen;

    s_Descriptor[s_DescriptorIndex++] = dnlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           DisplayName,
           dnlen);
    s_DescriptorIndex += dnlen;

    s_Descriptor[s_DescriptorIndex++] = desclen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Description,
           desclen);
    s_DescriptorIndex += desclen;

    s_Descriptor[s_DescriptorIndex++] = ulen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Units,
           ulen);
    s_DescriptorIndex += ulen;

    s_Descriptor[s_DescriptorIndex++] = Required;
    s_Descriptor[s_DescriptorIndex++] = Writeable;
}

void
SerialPnP::NewCommand(const char* Name,
                      const char* DisplayName,
                      const char* Description,
                      SerialPnPSchema InputSchema,
                      SerialPnPSchema OutputSchema,
                      void* Callback)
{
    s_Descriptor[s_DescriptorIndex++] = 0x01;

    uint8_t nlen = strlen(Name);
    uint8_t dnlen = strlen(DisplayName);
    uint8_t desclen = strlen(Description);

    AddCallback(s_DescriptorIndex - 1, Callback);

    s_Descriptor[s_DescriptorIndex++] = nlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Name,
           nlen);
    s_DescriptorIndex += nlen;

    s_Descriptor[s_DescriptorIndex++] = dnlen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           DisplayName,
           dnlen);
    s_DescriptorIndex += dnlen;

    s_Descriptor[s_DescriptorIndex++] = desclen;

    memcpy(&s_Descriptor[s_DescriptorIndex],
           Description,
           desclen);
    s_DescriptorIndex += desclen;

    s_Descriptor[s_DescriptorIndex++] = InputSchema;
    s_Descriptor[s_DescriptorIndex++] = OutputSchema;
}

void
SerialPnP::Process()
{
    //
    // Process input
    //
    while (Serial.available() > 0) {
        char inb = Serial.read();
        // Serial.write(inb);

        if (inb == 0xEF && !s_Escaped) {
            s_Escaped = true;
            continue;

        } else if ((inb == 0x5A) && !s_Escaped) {
            s_RxBufferIndex = 0;
        }

        s_Escaped = false;

        s_RxBuffer[s_RxBufferIndex++] = inb;

        if (s_RxBufferIndex >= sizeof(SerialPnPPacketHeader)) {
            SerialPnPPacketHeader *p = (SerialPnPPacketHeader*) s_RxBuffer;

            if (p->Length == s_RxBufferIndex) {
                // we can parse the packet now
                ProcessPacket(p);
                s_RxBufferIndex = 0;
            }
        }
    }
}

void
SerialPnP::ProcessPacket(
    SerialPnPPacketHeader *Packet
) {
    SerialPnPPacketHeader out = {0};
    out.StartOfFrame = 0x5A;

    if (Packet->PacketType == SerialPnPPacketType::GetDescriptor) {
        out.Length = s_DescriptorIndex + sizeof(SerialPnPPacketHeader);
        out.PacketType = SerialPnPPacketType::DescriptorResponse;

        Serial.write(0x5A); // need to send sync
        SerialWrite((uint8_t*) &out + 1, sizeof(out) - 1);
        SerialWrite((uint8_t*) s_Descriptor, s_DescriptorIndex);

    } else if ((Packet->PacketType == SerialPnPPacketType::GetProperty) ||
               (Packet->PacketType == SerialPnPPacketType::SetProperty)) {
        // find entry in table
        SerialPnPPacketBody* body = (SerialPnPPacketBody*) Packet->Body;

        SerialPnPCallback* cb = nullptr;
        cb = FindCallback(0x02, //property type
                          body->NameLength,
                          body->Payload);

        if (cb) {
            uint32_t outp = 0;

            // schema is u32 for now
            if (Packet->PacketType == SerialPnPPacketType::GetProperty) {
                ((SerialPnPCb) cb->Callback)(nullptr, &outp);

                out.Length = sizeof(SerialPnPPacketHeader) +
                             sizeof(SerialPnPPacketBody) +
                             body->NameLength +
                             sizeof(outp); // payload size; uint32

                out.PacketType = SerialPnPPacketType::GetPropertyResponse;

                Serial.write(0x5A); // need to send sync
                SerialWrite((uint8_t*) &out + 1, sizeof(out) - 1);
                SerialWrite(body->NameLength);
                SerialWrite((uint8_t*) body->Payload, body->NameLength);
                SerialWrite((uint8_t*) &outp, sizeof(outp));

            } else {
                ((SerialPnPCb) cb->Callback)(body->Payload + body->NameLength, nullptr);
            }
        }

    } else if (Packet->PacketType == SerialPnPPacketType::Command) {
        SerialPnPPacketBody* body = (SerialPnPPacketBody*) Packet->Body;

        SerialPnPCallback* cb = nullptr;
        cb = FindCallback(0x01, //property type
                          body->NameLength,
                          body->Payload);

        if (cb) {
            uint32_t outp = 0;

            // we don't care about schema for invocation
            ((SerialPnPCb) cb->Callback)(body->Payload + body->NameLength, &outp);

            // just default to outputting a bool for now
            out.Length = sizeof(SerialPnPPacketHeader) +
                         sizeof(SerialPnPPacketBody) +
                         body->NameLength +
                         1; // payload size; bool for now

            out.PacketType = SerialPnPPacketType::CommandResponse;

            Serial.write(0x5A); // need to send sync
            SerialWrite((uint8_t*) &out + 1, sizeof(out) - 1);
            SerialWrite(body->NameLength);
            SerialWrite((uint8_t*) body->Payload, body->NameLength);
            SerialWrite((uint8_t*) &outp, 1);
        }
    }
}

void
SerialPnP::SendEvent(const char* Name,
                     uint8_t* Value,
                     uint8_t ValueSize) {

    SerialPnPPacketHeader out = {0};
    out.StartOfFrame = 0x5A;

    uint8_t nlen = strlen(Name);

    out.Length = sizeof(out) + 1 + nlen + ValueSize;
    out.PacketType = SerialPnPPacketType::Event;

    Serial.write(0x5A); // need to send sync
    SerialWrite((uint8_t*) &out + 1, sizeof(out) - 1);
    SerialWrite(nlen);
    SerialWrite((uint8_t*) Name, nlen);
    SerialWrite(Value, ValueSize);
}

void
SerialPnP::SendEvent(const char* Name,
                     float Value)
{
    SendEvent(Name, (uint8_t*) &Value, sizeof(float));
}

void
SerialPnP::SendEvent(const char* Name,
                     uint16_t Value)
{
    SendEvent(Name, (uint8_t*) &Value, 2);
}


void
SerialPnP::AddCallback(uint8_t DescriptorIndex,
                       void* Callback)
{
    uint8_t c = 0;

    while (c < 8) {
        SerialPnPCallback* cc = &s_Callbacks[c];

        if (cc->Callback == 0) {
            cc->DescriptorIndex = &s_Descriptor[DescriptorIndex];
            cc->Callback = Callback;
            break;
        }

        c++;
    }
}

SerialPnPCallback*
SerialPnP::FindCallback(uint8_t Type,
                        uint8_t NameSize,
                        const char* Name)
{
    uint8_t c = 0;

    while (c < 8) {
        SerialPnPCallback* cc = &s_Callbacks[c];

        // if len matches
        if ((*cc->DescriptorIndex == Type) &&
            (*(cc->DescriptorIndex+1) == NameSize)) {
            if (strncmp(cc->DescriptorIndex+2, Name, NameSize) == 0) {
                return cc;
            }
        }

        c++;
    }

    return 0;
}

void
SerialPnP::SerialWrite(uint8_t* Buffer, size_t Count)
{
    size_t c;
    size_t start = 0;

    // print as much as we can at a time before we hit a character
    // that needs escaping
    for (c = 0; c < Count; c++) {
        if ((Buffer[c] == 0x5A) || (Buffer[c] == 0xEF)) {

            if (c-start > 0) {
                Serial.write(Buffer + start, (c-start));
                Serial.write(0xEF);
            }

            start = c;
        }
    }

    // print whatever is left after the last escape
    Serial.write(Buffer + start, Count - start);
}

void
SerialPnP::SerialWrite(uint8_t Out)
{
    if ((Out == 0x5A) || (Out == 0xEF)) {
        Serial.write(0xEF);
    }

    Serial.write(Out);
}