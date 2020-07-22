// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pnpadapter_manager.h>

IOTHUB_CLIENT_RESULT IotComms_InitializeIotHandle(MX_IOT_HANDLE_TAG* IotHandle, bool TraceOn, PCONNECTION_PARAMETERS ConnectionParams);

#ifdef __cplusplus
}
#endif