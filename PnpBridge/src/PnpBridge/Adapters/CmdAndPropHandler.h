#define CONC(A, B) CONC_(A, B)
#define CONC_(A, B) A##B

typedef void(*PropertyHandlersFunction)(unsigned const char* propertyInitial,
    size_t propertyInitialLen, unsigned const char* propertyDataUpdated, size_t propertyDataUpdatedLen, int desiredVersion, void* userContextCallback);

typedef void(*CommandHandlersFunction)(const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext, PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, void* userContextCallback);
