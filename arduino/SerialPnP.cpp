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
    Serial.flush();
}

void
SerialPnP::NewInterface(
    const char* InterfaceId
) {
    s_Descriptor[s_DescriptorIndex++] = 0x05;

    uint16_t ilen = strlen(InterfaceId);

    s_Descriptor[s_DescriptorIndex++] = ilen & 0xFF;
    s_Descriptor[s_DescriptorIndex++] = ilen >> 8;

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
    s_Descriptor[s_DescriptorIndex++] = 0;
}

void
SerialPnP::NewProperty(const char* Name,
                       const char* DisplayName,
                       const char* Description,
                       const char* Units,
                       SerialPnPSchema Schema,
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

    s_Descriptor[s_DescriptorIndex++] = Schema;
    s_Descriptor[s_DescriptorIndex++] = 0;

    ulen = 0x00;
    if (Required) ulen |= (1 << 1);
    if (Writeable) ulen |= (1 << 0);

    // flags
    s_Descriptor[s_DescriptorIndex++] = ulen;
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
    s_Descriptor[s_DescriptorIndex++] = 0;
    s_Descriptor[s_DescriptorIndex++] = OutputSchema;
    s_Descriptor[s_DescriptorIndex++] = 0;
}

void
SerialPnP::Process()
{
    //
    // Process input
    //
    while (Serial.available()) {
        char inb = Serial.read();

        if (inb == 0x5A) {
            s_RxBufferIndex = 0;
            s_Escaped = false;
            continue;
        }

        if (inb == 0xEF) {
            s_Escaped = true;
            continue;
        }

        if (s_Escaped) {
            inb++;
            s_Escaped = false;
        }

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

    if (Packet->PacketType == SerialPnPPacketType::ResetReq) {
        out.Length = sizeof(SerialPnPPacketHeader);
        out.PacketType = SerialPnPPacketType::ResetResp;

        Serial.write(0x5A);
        SerialWrite((uint8_t*) &out, sizeof(SerialPnPPacketHeader));
    
    } else if (Packet->PacketType == SerialPnPPacketType::GetDescriptor) {
        out.Length = sizeof(SerialPnPPacketHeader) + s_DescriptorIndex;
        out.PacketType = SerialPnPPacketType::DescriptorResponse;

        Serial.write(0x5A); // need to send sync
        SerialWrite((uint8_t*) &out, sizeof(SerialPnPPacketHeader));
        SerialWrite((uint8_t*) s_Descriptor, s_DescriptorIndex);

    } else if ((Packet->PacketType == SerialPnPPacketType::SetProperty)) {
        // find entry in table
        SerialPnPPacketBody* body = (SerialPnPPacketBody*) Packet->Body;

        SerialPnPCallback* cb = nullptr;
        cb = FindCallback(0x02, //property type
                          body->NameLength,
                          body->Payload);

        if (cb) {
            uint32_t outp = 0;

            // If there's no data, call it for output only
            if (Packet->Length - (sizeof(SerialPnPPacketHeader) +
                                  sizeof(SerialPnPPacketBody) +
                                  body->NameLength) == 0) {

                ((SerialPnPCb) cb->Callback)(nullptr, &outp);
            } else {
                ((SerialPnPCb) cb->Callback)(body->Payload + body->NameLength, &outp);
            }

            out.Length = sizeof(SerialPnPPacketHeader) +
                         sizeof(SerialPnPPacketBody) +
                         body->NameLength +
                         sizeof(outp); // payload size; uint32

            out.PacketType = SerialPnPPacketType::PropertyResponse;

            Serial.write(0x5A); // need to send sync
            SerialWrite((uint8_t*) &out, sizeof(out));
            SerialWrite(0); // Interface ID = 0
            SerialWrite(body->NameLength);
            SerialWrite((uint8_t*) body->Payload, body->NameLength);
            SerialWrite((uint8_t*) &outp, sizeof(outp));
        }
    } else if ((Packet->PacketType == SerialPnPPacketType::Command)) {
        // find entry in table
        SerialPnPPacketBody* body = (SerialPnPPacketBody*) Packet->Body;

        SerialPnPCallback* cb = nullptr;
        cb = FindCallback(0x01, //method type
                          body->NameLength,
                          body->Payload);

        if (cb) {
            uint32_t outp = 0;

            ((SerialPnPCb) cb->Callback)(body->Payload + body->NameLength, &outp);

            out.Length = sizeof(SerialPnPPacketHeader) +
                         sizeof(SerialPnPPacketBody) +
                         body->NameLength +
                         sizeof(outp); // payload size; uint32

            out.PacketType = SerialPnPPacketType::CommandResponse;

            Serial.write(0x5A); // need to send sync
            SerialWrite((uint8_t*) &out, sizeof(out));
            SerialWrite(0); // Interface ID = 0
            SerialWrite(body->NameLength);
            SerialWrite((uint8_t*) body->Payload, body->NameLength);
            SerialWrite((uint8_t*) &outp, sizeof(outp));
        }
    }
}

void
SerialPnP::SendEvent(const char* Name,
                     uint8_t* Value,
                     uint8_t ValueSize) {

    SerialPnPPacketHeader out = {0};

    uint8_t nlen = strlen(Name);

    out.Length = sizeof(SerialPnPPacketHeader) +
                 sizeof(SerialPnPPacketBody) +
                 nlen +
                 ValueSize;

    out.PacketType = SerialPnPPacketType::Event;

    Serial.write(0x5A); // need to send sync
    SerialWrite((uint8_t*) &out, sizeof(SerialPnPPacketHeader));
    SerialWrite(0); // interface id = 0
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

    for (c = 0; c < Count; c++) {
        if ((Buffer[c] == 0x5A) || (Buffer[c] == 0xEF)) {
            Serial.write(0xEF);
            Serial.write(Buffer[c] - 1);
        } else {
            Serial.write(Buffer[c]);
        }
    }
}

void
SerialPnP::SerialWrite(uint8_t Char)
{
    if ((Char == 0x5A) || (Char == 0xEF)) {
        Serial.write(0xEF);
        Serial.write(Char - 1);
    } else {
        Serial.write(Char);
    }
}