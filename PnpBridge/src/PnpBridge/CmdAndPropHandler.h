#define CONC(A, B) CONC_(A, B)
#define CONC_(A, B) A##B

static void SerialPnp_PropertyUpdateHandler(const char* propertyName, unsigned const char* propertyInitial, size_t propertyInitialLen, unsigned const char* propertyDataUpdated, size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback);

#define PROP_HANDLER(index) void CONC(SerialPnp_PropertyUpdateHandler, CONC(CMD_PROP_HANDLER_ADAPTER_NAME, index))(unsigned const char* propertyInitial, \
    size_t propertyInitialLen, unsigned const char* propertyDataUpdated, size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback) \
    {PROP_HANDLER_ADAPTER_METHOD(index, propertyInitial, propertyInitialLen, propertyDataUpdated, propertyDataUpdatedLen, \
     desiredVersion, userContextCallback);};

PROP_HANDLER(0);
PROP_HANDLER(1);
PROP_HANDLER(2);
PROP_HANDLER(3);
PROP_HANDLER(4);
PROP_HANDLER(5);
PROP_HANDLER(6);
PROP_HANDLER(7);
PROP_HANDLER(8);
PROP_HANDLER(9);
PROP_HANDLER(10);
PROP_HANDLER(11);
PROP_HANDLER(12);
PROP_HANDLER(13);
PROP_HANDLER(14);
PROP_HANDLER(15);
PROP_HANDLER(16);
PROP_HANDLER(17);
PROP_HANDLER(18);
PROP_HANDLER(19);

#define PROP_HANDLER_METHOD(index) &CONC(SerialPnp_PropertyUpdateHandler, CONC(CMD_PROP_HANDLER_ADAPTER_NAME, index))

void* PredefinedPropertyHandlerTables[] = {
    PROP_HANDLER_METHOD(0),
    PROP_HANDLER_METHOD(1),
    PROP_HANDLER_METHOD(2),
    PROP_HANDLER_METHOD(3),
    PROP_HANDLER_METHOD(4),
    PROP_HANDLER_METHOD(5),
    PROP_HANDLER_METHOD(6),
    PROP_HANDLER_METHOD(7),
    PROP_HANDLER_METHOD(8),
    PROP_HANDLER_METHOD(9),
    PROP_HANDLER_METHOD(10),
    PROP_HANDLER_METHOD(11),
    PROP_HANDLER_METHOD(12),
    PROP_HANDLER_METHOD(13),
    PROP_HANDLER_METHOD(14),
    PROP_HANDLER_METHOD(15),
    PROP_HANDLER_METHOD(16),
    PROP_HANDLER_METHOD(17),
    PROP_HANDLER_METHOD(18),
    PROP_HANDLER_METHOD(19)
};

#define CMD_HANDLER(index) void CONC(SerialPnp_CommandUpdateHandler, CONC(CMD_PROP_HANDLER_ADAPTER_NAME, index))( \
    const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext, PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, void* userContextCallback) \
    {CMD_HANDLER_ADAPTER_METHOD(index, pnpClientCommandContext, pnpClientCommandResponseContext, userContextCallback);};

CMD_HANDLER(0);
CMD_HANDLER(1);
CMD_HANDLER(2);
CMD_HANDLER(3);
CMD_HANDLER(4);
CMD_HANDLER(5);
CMD_HANDLER(6);
CMD_HANDLER(7);
CMD_HANDLER(8);
CMD_HANDLER(9);
CMD_HANDLER(10);
CMD_HANDLER(11);
CMD_HANDLER(12);
CMD_HANDLER(13);
CMD_HANDLER(14);
CMD_HANDLER(15);
CMD_HANDLER(16);
CMD_HANDLER(17);
CMD_HANDLER(18);
CMD_HANDLER(19);

#define CMD_HANDLER_METHOD(index) &CONC(SerialPnp_CommandUpdateHandler, CONC(CMD_PROP_HANDLER_ADAPTER_NAME, index))

void* PredefinedCommandHandlerTables[] = {
    CMD_HANDLER_METHOD(0),
    CMD_HANDLER_METHOD(1),
    CMD_HANDLER_METHOD(2),
    CMD_HANDLER_METHOD(3),
    CMD_HANDLER_METHOD(4),
    CMD_HANDLER_METHOD(5),
    CMD_HANDLER_METHOD(6),
    CMD_HANDLER_METHOD(7),
    CMD_HANDLER_METHOD(8),
    CMD_HANDLER_METHOD(9),
    CMD_HANDLER_METHOD(10),
    CMD_HANDLER_METHOD(11),
    CMD_HANDLER_METHOD(12),
    CMD_HANDLER_METHOD(13),
    CMD_HANDLER_METHOD(14),
    CMD_HANDLER_METHOD(15),
    CMD_HANDLER_METHOD(16),
    CMD_HANDLER_METHOD(17),
    CMD_HANDLER_METHOD(18),
    CMD_HANDLER_METHOD(19)
};
