class JsonRpcProtocolHandler : public MqttProtocolHandler {
public:
    void
    OnReceive(
        const char*     Topic,
        const char*     Message,
        size_t          MessageSize
    );

    void
    Initialize(
        class MqttConnectionManager*
                                ConnectionManager,
        JSON_Value*             ProtocolHandlerConfig
    );

    // TODO: Handle command input
    void
    OnPnpMethodCall(
        const DIGITALTWIN_CLIENT_COMMAND_REQUEST*   CommandRequest,
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE*        CommandResponse
    );

    void
    AssignDigitalTwin(
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE DtHandle
    );

private:
    MqttConnectionManager*              s_ConnectionManager = nullptr;
    // Maps method name to 
    std::map<std::string, std::string>  s_Telemetry;
    std::map<std::string, std::pair<std::string, std::string>>
                                        s_Commands;
    JsonRpc*                            s_JsonRpc = nullptr;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE s_DtInterface = nullptr;

    static
    void
    JsonRpcProtocolHandler::RpcResultCallback(
        void*           Context,
        bool            Success,
        JSON_Value*     Parameters,
        void*           CallContext
    );

    static
    void
    RpcNotificationCallback(
        void*           Context,
        const char*     Method,
        JSON_Value*     Parameters
    );
};

class JsonRpcCallContext {
public:
    void*                                       Event;
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST*   CommandRequest;
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE*        CommandResponse;
};