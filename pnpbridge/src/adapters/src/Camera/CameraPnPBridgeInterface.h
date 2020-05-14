// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <PnpBridge.h>

void CameraPnpCallback_TakePhoto(
    _In_ const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    _In_ DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    _In_ void* userContextCallback);

void CameraPnpCallback_TakeVideo(
    _In_ const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    _In_ DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    _In_ void* userContextCallback);

void CameraPnpCallback_GetURI(
    _In_ const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    _In_ DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    _In_ void* userContextCallback);

extern PNP_ADAPTER CameraPnpInterface;

#ifdef __cplusplus
}
#endif
