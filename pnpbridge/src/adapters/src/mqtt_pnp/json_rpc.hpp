// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#define JSONRPC_ERROR_PARSE_ERROR       -32700
#define JSONRPC_ERROR_INVALID_REQUEST   -32600
#define JSONRPC_ERROR_METHOD_NOT_FOUND  -32601
#define JSONRPC_ERROR_INVALID_PARAMS    -32602
#define JSONRPC_ERROR_INTERNAL_ERROR    -32603

// Opaque datatype used for completing a call.
typedef void* JsonRpcCall;

// Called by JsonRpc when a new notification arrives.
typedef void
(*JsonRpcNotificationCallback)(
    void*,          /* Context */
    const char*,    /* Method */
    JSON_Value*     /* Parameters */
);

// Called by JsonRpc when a new method call arrives.
// This callback should initiate processing of the call, and use the
// Call Handle to complete the call at a later time by calling CompleteCall.
// Parameter memory will be freed after this function executes.
typedef void
(*JsonRpcMethodCallback)(
    void*,          /* Context */
    const char*,    /* Method */
    JSON_Value*,    /* Parameters */
    JsonRpcCall     /* Call Handle (for completion) */
);

// Called by JsonRpc when a call initiated with SendMethodCall is completed
// by the server and a result is received.
// Result memory will be freed after this function executes.
typedef void (*JsonRpcResultCallback)(
    void*,          /* Context */
    bool,           /* Success */
    JSON_Value*,    /* Result  - if not successful, this will contain
                                 error information. */
    void*           /* Call Context */
);

class JsonRpc {
public:
    JsonRpc(
        JsonRpcNotificationCallback     NotificationCallback,
        JsonRpcMethodCallback           MethodCallback,
        JsonRpcResultCallback           ResultCallback,
        void*                           Context
    );

    // Function should be called whenever new data arrives on the medium for
    // processing as JSON RPC.
    void
    Process(
        const char*     Payload,
        size_t          PayloadSize
    );

    //
    // RPC Server Functions
    //
    const char*
    CompleteCall(
        JsonRpcCall     Call,
        JSON_Value*     Result
    );

    const char*
    CompleteCallError(
        JsonRpcCall     Call,
        int             Error,
        JSON_Value*     CustomError
    );

    //
    // RPC Client Functions
    //
    const char*
    RpcCall(
        const char*         Method,
        JSON_Value*         Parameters,
        void*               CallContext
    );

    const char*
    RpcNotification(
        const char*         Method,
        JSON_Value*         Parameters
    );

private:
    std::map<size_t, void*>                     s_OutstandingCalls;
    std::mutex                                  s_OutstandingCallsMutex;
    size_t                                      s_NextCallId = 0;
    JsonRpcNotificationCallback                 s_NotificationCallback;
    JsonRpcMethodCallback                       s_MethodCallback;
    JsonRpcResultCallback                       s_ResultCallback;
    void*                                       s_Context;

    void
    ProcessRequest(
        JSON_Object*        Packet
    );

    void
    ProcessResponse(
        JSON_Object*        Packet
    );
};