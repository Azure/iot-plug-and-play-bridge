class MqttProtocolHandler {
public:
    virtual
    void
    OnReceive(
        const char*     Topic,
        const char*     Message,
        size_t          MessageSize
    ) = 0;

    virtual
    void
    Initialize(
        class MqttConnectionManager*
                                ConnectionManager,
        JSON_Value*             ProtocolHandlerConfig
    ) = 0;

    virtual
    void
    OnPnpMethodCall(
        const DIGITALTWIN_CLIENT_COMMAND_REQUEST*   CommandRequest,
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE*        CommandResponse
    ) = 0;

    virtual
    void
    AssignDigitalTwin(
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE DtHandle
    ) = 0;

    virtual
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE
    GetDigitalTwin() = 0;
};