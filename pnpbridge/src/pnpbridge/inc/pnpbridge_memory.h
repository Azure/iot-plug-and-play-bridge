// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PNPBRIDGE_OBJECT_H
#define PNPBRIDGE_OBJECT_H

#ifdef __cplusplus
extern "C"
{
#endif
    #define PNPMEMORY_NO_OBJECT_PARAMS NULL

    typedef void* PNPMEMORY;

    typedef void(*PNPMEMORY_DESTROY) (PNPMEMORY Message);

    typedef struct _PNPMEMORY_ATTRIBUTES {
        PNPMEMORY_DESTROY destroyCallback;
    } PNPMEMORY_ATTRIBUTES, *PPNPMEMORY_ATTRIBUTES;

    typedef struct _PNPBRIDGE_MEMORY_TAG {
        volatile long count;
        int size;
        void* memory;
        PNPMEMORY_ATTRIBUTES params;
    } PNPBRIDGE_MEMORY_TAG, *PPNPBRIDGE_MEMORY_TAG;

    int PnpMemory_Create(PPNPMEMORY_ATTRIBUTES params, int size, PNPMEMORY* memory);

    void PnpMemory_AddReference(PNPMEMORY memory);

    void PnpMemory_ReleaseReference(PNPMEMORY memory);

    void* PnpMemory_GetBuffer(PNPMEMORY memory, int* size);

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_OBJECT_H */