// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge_common.h"

MAP_RESULT 
Map_Add_Index(
    MAP_HANDLE handle,
    const char* key,
    int value
    )
{
    char str[12];
    sprintf(str, "%d", value);

    return Map_Add(handle, key, str);
}

int 
Map_GetIndexValueFromKey(
    MAP_HANDLE handle,
    const char* key
    )
{
    const char* index = Map_GetValueFromKey(handle, key);
    return atoi(index);
}