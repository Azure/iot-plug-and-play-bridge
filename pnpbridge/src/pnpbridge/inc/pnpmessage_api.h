// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PNPBRIDGE_PNPMESSAGE_INTERFACE_H
#define PNPBRIDGE_PNPMESSAGE_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _PNPMESSAGE_CHANGE_TYPE {
    PNPMESSAGE_CHANGE_INVALID,
    PNPMESSAGE_CHANGE_ARRIVAL,
    PNPBRIDGE_INTERFACE_CHANGE_PERSIST,
    PNPMESSAGE_CHANGE_REMOVAL
} PNPMESSAGE_CHANGE_TYPE;

// List of properties which should only contain value types
typedef struct _PNPMESSAGE_CHANGE_PROPERTIES {
    void* Context;
    PNPMESSAGE_CHANGE_TYPE ChangeType;
    char* ComponentName;
} PNPMESSAGE_PROPERTIES;

typedef void* PNPMESSAGE;

const char*
PnpMessage_GetInterfaceId(
    _In_ PNPMESSAGE Message
    );

int
PnpMessage_SetInterfaceId(
    PNPMESSAGE Message,
    const char* InterfaceId
    );

int 
PnpMessage_CreateMessage(
    _Out_ PNPMESSAGE* Message
    );

int 
PnpMessage_SetMessage(
    _In_ PNPMESSAGE Message,
    const char* Payload
    );

const char*
PnpMessage_GetMessage(
    _In_ PNPMESSAGE Message
    );

PNPMESSAGE_PROPERTIES* 
PnpMessage_AccessProperties(
    _In_ PNPMESSAGE Message
    );

void
PnpMessage_AddReference(
    _In_ PNPMESSAGE Message
    );

void 
PnpMessage_ReleaseReference(
    _In_ PNPMESSAGE Message
    );

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_PNPMESSAGE_INTERFACE_H */