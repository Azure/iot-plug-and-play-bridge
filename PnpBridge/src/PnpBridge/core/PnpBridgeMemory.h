// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PNPBRIDGE_OBJECT_H
#define PNPBRIDGE_OBJECT_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void* PNPBRIDGE_MEMORY;

    typedef int(*PNPBRIDGE_MEMORY_DESTROY) ();

    typedef struct _PNPBRIDGE_MEMORY_PARAMS {
        PNPBRIDGE_MEMORY_DESTROY destroyCallback;
    } PNPBRIDGE_MEMORY_PARAMS, *PPNPBRIDGE_OBJECT_PARAMS;

    typedef struct _PNPBRIDGE_MEMORY_TAG {
        volatile long count;
        int size;
        void* memory;
        PNPBRIDGE_MEMORY_PARAMS params;
    } PNPBRIDGE_MEMORY_TAG, *PPNPBRIDGE_MEMORY_TAG;

    int PnpBridgeMemory_Create(PPNPBRIDGE_OBJECT_PARAMS params, int size, PNPBRIDGE_MEMORY* memory);

    void PnpBridgeMemory_AddReference(PNPBRIDGE_MEMORY memory);

    void PnpBridgeMemory_ReleaseReference(PNPBRIDGE_MEMORY memory);

    void* PnpMemory_GetBuffer(PNPBRIDGE_MEMORY memory, int* size);

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_OBJECT_H */