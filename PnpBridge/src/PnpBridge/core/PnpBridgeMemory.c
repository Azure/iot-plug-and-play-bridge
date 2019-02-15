#include <stdio.h>
#include "azure_c_shared_utility/refcount.h"
#include "core/PnpBridgeMemory.h"

int PnpBridgeMemory_Create(PPNPBRIDGE_OBJECT_PARAMS params, int size, PNPBRIDGE_MEMORY* memory) {
    if (NULL == params || NULL == params->destroyCallback) {
        return -1;
    }

    PPNPBRIDGE_MEMORY_TAG mem = calloc(1, sizeof(PNPBRIDGE_MEMORY_TAG) + size);
    if (NULL == mem) {
        return -1;
    }

    INIT_REF_VAR(mem->count);
    mem->size = size;

    *memory = (PNPBRIDGE_MEMORY*)mem;
 
    return 0;
}

void PnpBridgeMemory_AddReference(PNPBRIDGE_MEMORY memory) {
    PPNPBRIDGE_MEMORY_TAG mem = (PPNPBRIDGE_MEMORY_TAG)memory;
    DEC_REF_VAR(mem->count);
}

void PnpBridgeMemory_ReleaseReference(PNPBRIDGE_MEMORY memory) {
    PPNPBRIDGE_MEMORY_TAG mem = (PPNPBRIDGE_MEMORY_TAG)memory;
    if (DEC_RETURN_ZERO == DEC_REF_VAR(mem->count)) {
        mem->params.destroyCallback();
    }
}

void* PnpMemory_GetBuffer(PNPBRIDGE_MEMORY memory, int* size) {
    PPNPBRIDGE_MEMORY_TAG mem = (PPNPBRIDGE_MEMORY_TAG)memory;
    if (NULL != size) {
        *size = mem->size;
    }
    return mem->memory;
}