// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <functional>
#include <parson.h>
#include <stdexcept>
#include <string>
#include <digitaltwin_client_common.h>

static void DestroyJsonValue(JSON_Value* val)
{
    json_value_free(val);
};

using JsonValuePtr = std::unique_ptr<JSON_Value, decltype(&DestroyJsonValue)>;

class DigitalTwinException : public std::runtime_error
{
public:
    DigitalTwinException(
        _In_ const std::string& message,
        _In_ DIGITALTWIN_CLIENT_RESULT result) :
        runtime_error((message + " " + std::to_string(result)).c_str())
    {
    }
};

class AzurePnpException : public std::runtime_error
{
public:
    AzurePnpException(
        _In_ const std::string& message,
        _In_ int result) :
        runtime_error((message + " " + std::to_string(result)).c_str())
    {
    }
};
