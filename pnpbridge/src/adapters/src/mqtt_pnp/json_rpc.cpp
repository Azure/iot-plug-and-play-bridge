#pragma once

#include "parson.h"
#include <stdexcept>
#include <map>
#include <mutex>
#include <string>
#include <cstring>
#include "json_rpc.hpp"

JsonRpc::JsonRpc(
    JsonRpcNotificationCallback     NotificationCallback,
    JsonRpcMethodCallback           MethodCallback,
    JsonRpcResultCallback           ResultCallback,
    void*                           Context
)
{
    s_NotificationCallback = NotificationCallback;
    s_MethodCallback = MethodCallback;
    s_ResultCallback = ResultCallback;
    s_Context = Context;
}

void
JsonRpc::Process(
    const char*     Payload,
    size_t          PayloadSize
)
{
    char *pl = nullptr;
    JSON_Value* packet;
    JSON_Object* packobj;
    const char* packstr;

    // Null-terminate the packet
    pl = new char[PayloadSize + 1];
    std::memcpy(pl, Payload, PayloadSize);
    pl[PayloadSize] = 0;

    // Parse the JSON
    packet = json_parse_string(pl);
    delete pl;

    if (!packet) {
        throw std::invalid_argument("JSON-RPC packet error parsing");
    }

    packobj = json_value_get_object(packet);
    if (!packobj) {
        throw std::invalid_argument("JSON-RPC packet without top-level object");
    }

    // Validate this is json rpc packet
    // TODO: Handle allowed case where input is an array of requests
    packstr = json_object_get_string(packobj, "jsonrpc");
    if (!packstr) {
        json_value_free(packet);
        throw std::invalid_argument("JSON-RPC packet missing jsonrpc version");
    }

    if (std::strcmp(packstr, "2.0") != 0) {
        json_value_free(packet);
        throw new std::invalid_argument("JSON-RPC packet with incorrect jsonrpc version");
    }

    // If method field is present, this is an incoming request / notification
    if (json_object_has_value(packobj, "method")) {
        ProcessRequest(packobj);

    // Otherwise id and either result or error indicate a method
    // response.
    } else if (json_object_has_value(packobj, "id") &&
               (json_object_has_value(packobj, "result") ||
                json_object_has_value(packobj, "error"))) {

        ProcessResponse(packobj);
    }

    // Missing required fields.
    else {
        json_value_free(packet);
        throw new std::invalid_argument("JSON-RPC packet missing required field");
    }

    json_value_free(packet);
}

void
JsonRpc::ProcessRequest(
    JSON_Object*        Packet
)
{
    JSON_Value *params = json_object_get_value(Packet, "params");
    const char* method = json_object_get_string(Packet, "method");
    bool hasId = false;
    JSON_Value* call = nullptr;

    if (json_object_has_value(Packet, "id")) {
        hasId = true;
        call = json_value_deep_copy(json_object_get_value(Packet, "id"));

        if (call == nullptr) {
            throw new std::invalid_argument("Unable to copy JSON-RPC id");
        }
    }

    if (hasId && s_MethodCallback) {
        s_MethodCallback(s_Context,
                         method,
                         params,
                         (void*) call);
    }

    else if (s_NotificationCallback) {
        s_NotificationCallback(s_Context,
                               method,
                               params);
    }

    else {
        if (call) {
            json_value_free(call);
        }

        throw new std::invalid_argument("JSON-RPC no matching callback");
    }
}

void
JsonRpc::ProcessResponse(
    JSON_Object*        Packet
)
{
    size_t id = (size_t) json_object_get_number(Packet, "id");

    // If it's zero, it's probably been returned as a string so handle
    // accordingly.
    if (id == 0) {
        const char *ids = json_object_get_string(Packet, "id");
        if (ids == nullptr) {
            throw new std::invalid_argument("JSON-RPC response with invalid ID");
        }

        id = std::stoi(ids, nullptr, 10);
    }

    void* context = nullptr;
    bool success = (json_object_has_value(Packet, "result") != 0);

    s_OutstandingCallsMutex.lock();
    auto iterator = s_OutstandingCalls.find(id);
    if (iterator != s_OutstandingCalls.end()) {
        context = iterator->second;
        s_OutstandingCalls.erase(iterator);
        s_OutstandingCallsMutex.unlock();
    } else {
        s_OutstandingCallsMutex.unlock();
        throw new std::invalid_argument("JSON-RPC response with no outstanding call");
    }

    s_ResultCallback(s_Context,
                     success,
                     success ? json_object_get_value(Packet, "result") :
                               json_object_get_value(Packet, "error"),
                     context);
}

const char*
JsonRpc::CompleteCall(
    JsonRpcCall     Call,
    JSON_Value*     Result
)
{
    JSON_Value* new_json = json_value_init_object();
    JSON_Object* packet = json_value_get_object(new_json);

    json_object_set_string(packet, "jsonrpc", "2.0");
    json_object_set_value(packet, "id", (JSON_Value*) Call);
    json_object_set_value(packet, "result", Result);

    const char *out_packet = json_serialize_to_string(new_json);

    json_value_free(new_json);
    json_value_free((JSON_Value*) Call);

    return out_packet;
}

const char*
JsonRpc::CompleteCallError(
    JsonRpcCall     Call,
    int             Error,
    JSON_Value*     CustomError
)
{
    const char* err_message = nullptr;

    JSON_Value* error_value = CustomError;

    if (!CustomError) {
        switch (Error) {
        case JSONRPC_ERROR_PARSE_ERROR:
            err_message = "Parse error";
            break;
        case JSONRPC_ERROR_INVALID_REQUEST:
            err_message = "Parse error";
            break;
        case JSONRPC_ERROR_METHOD_NOT_FOUND:
            err_message = "Parse error";
            break;
        case JSONRPC_ERROR_INVALID_PARAMS:
            err_message = "Parse error";
            break;
        case JSONRPC_ERROR_INTERNAL_ERROR:
            err_message = "Parse error";
            break;
        default:
            throw new std::invalid_argument("Unknown error code provided");
        }

        error_value = json_value_init_object();
        JSON_Object* error_object = json_value_get_object(error_value);

        json_object_set_number(error_object, "code", Error);
        json_object_set_string(error_object, "message", err_message);
    }

    JSON_Value* new_json = json_value_init_object();
    JSON_Object* packet = json_value_get_object(new_json);

    json_object_set_string(packet, "jsonrpc", "2.0");
    json_object_set_value(packet, "id", (JSON_Value*) Call);
    json_object_set_value(packet, "error", error_value);

    const char *out_packet = json_serialize_to_string(new_json);

    if (!CustomError) {
        json_value_free(error_value);
    }

    json_value_free(new_json);
    json_value_free((JSON_Value*) Call);

    return out_packet;
}

const char*
JsonRpc::RpcCall(
    const char*         Method,
    JSON_Value*         Parameters,
    void*               CallContext
)
{
    if (!Method) {
        throw new std::invalid_argument("RPC method must be provided");
    }

    JSON_Value* new_json = json_value_init_object();
    JSON_Object* packet = json_value_get_object(new_json);
    size_t call_id;

    s_OutstandingCallsMutex.lock();
    call_id = s_NextCallId++;
    // Workaround for RPC servers which force cast call ID to a string.
    if (call_id == 0) {
        call_id = s_NextCallId++;
    }
    s_OutstandingCallsMutex.unlock();

    json_object_set_string(packet, "jsonrpc", "2.0");
    json_object_set_string(packet, "method", Method);

    if (Parameters) {
        json_object_set_value(packet, "params", Parameters);
    }

    json_object_set_number(packet, "id", (double) call_id);

    const char *out_packet = json_serialize_to_string(new_json);

    s_OutstandingCallsMutex.lock();
    s_OutstandingCalls.insert(std::pair<size_t,void*>(call_id, CallContext));
    s_OutstandingCallsMutex.unlock();

    json_value_free(new_json);

    return out_packet;
}

const char*
JsonRpc::RpcNotification(
    const char*         Method,
    JSON_Value*         Parameters
)
{
    if (!Method) {
        throw new std::invalid_argument("RPC method must be provided");
    }

    JSON_Value* new_json = json_value_init_object();
    JSON_Object* packet = json_value_get_object(new_json);

    json_object_set_string(packet, "jsonrpc", "2.0");
    json_object_set_string(packet, "method", Method);

    if (Parameters) {
        json_object_set_value(packet, "params", Parameters);
    }

    const char *out_packet = json_serialize_to_string(new_json);

    json_value_free(new_json);

    return out_packet;
}