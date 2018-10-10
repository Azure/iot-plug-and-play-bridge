enum SerialPnPSchema {
    SchemaNone = 0,
    SchemaByte,
    SchemaFloat,
    SchemaDouble,
    SchemaInt,
    SchemaLong,
    SchemaBoolean,
    SchemaString,
};

typedef struct _SerialPnPPacketHeader {
    uint16_t                Length;
    uint8_t                 PacketType;
    uint8_t                 Reserved;
    char                    Body[0];
} SerialPnPPacketHeader;

typedef struct _SerialPnPPacketBody {
    uint8_t                 InterfaceId;
    uint8_t                 NameLength;
    char                    Payload[0];
} SerialPnPPacketBody;

typedef struct _SerialPnPCallback {
    char*   DescriptorIndex;
    void*   Callback;
} SerialPnPCallback;

typedef void (*SerialPnPCb)(void*, void*);

class SerialPnP {
public:
    static void
    Setup(const char* Name);

    static void
    NewInterface(const char* InterfaceId);

    static void
    NewEvent(const char* Name,
             const char* DisplayName,
             const char* Description,
             SerialPnPSchema Schema,
             const char* Units);

    static void
    NewProperty(const char* Name,
                const char* DisplayName,
                const char* Description,
                const char* Units,
                SerialPnPSchema Schema,
                bool Required,
                bool Writeable,
                void* Callback);

    static void
    NewCommand(const char* Name,
               const char* DisplayName,
               const char* Description,
               SerialPnPSchema InputSchema,
               SerialPnPSchema OutputSchema,
               void* Callback);

    static void
    Process();

    static void
    SendEvent(const char* Name,
              uint8_t* Value,
              uint8_t ValueSize);

    static void
    SendEvent(const char* Name,
              float Value);

    static void
    SendEvent(const char* Name,
              uint16_t Value);

private:
    static char s_Descriptor[384];
    static char s_RxBuffer[64];
    static uint16_t s_DescriptorIndex;
    static uint8_t s_RxBufferIndex;
    static bool s_Escaped;

    static SerialPnPCallback s_Callbacks[8];

    static void
    ProcessPacket(SerialPnPPacketHeader *Packet);

    static void
    AddCallback(uint8_t DescriptorIndex,
                void* Callback);

    static SerialPnPCallback*
    FindCallback(uint8_t Type,
                 uint8_t NameSize,
                 const char* Name);

    static void
    SerialWrite(uint8_t* Buffer, size_t Count);

    static void
    SerialWrite(uint8_t Out);
};

enum SerialPnPPacketType {
    None = 0,
    ResetReq = 1,
    ResetResp = 2,
    GetDescriptor = 3,
    DescriptorResponse = 4,
    Command = 5,
    CommandResponse = 6,
    SetProperty = 7,
    PropertyResponse = 8,
    Event = 10
};
