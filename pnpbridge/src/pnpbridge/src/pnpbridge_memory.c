#include <stdio.h>
#include "azure_c_shared_utility/refcount.h"
#include "pnpbridge_memory.h"

int PnpMemory_Create(PPNPMEMORY_ATTRIBUTES params, int size, PNPMEMORY* memory) {
    PPNPBRIDGE_MEMORY_TAG mem = calloc(1, sizeof(PNPBRIDGE_MEMORY_TAG) + size);
    if (NULL == mem) {
        return -1;
    }

    INIT_REF_VAR(mem->count);
    mem->size = size;

    if (params) {
        mem->params = *params;
    }

    mem->memory = mem + 1;

    *memory = (PNPMEMORY*)mem;
 
    return 0;
}

void PnpMemory_AddReference(PNPMEMORY memory) {
    PPNPBRIDGE_MEMORY_TAG mem = (PPNPBRIDGE_MEMORY_TAG)memory;
    INC_REF_VAR(mem->count);
}

void PnpMemory_ReleaseReference(PNPMEMORY memory) {
    PPNPBRIDGE_MEMORY_TAG mem = (PPNPBRIDGE_MEMORY_TAG)memory;
    if (DEC_RETURN_ZERO == DEC_REF_VAR(mem->count)) {
        if (NULL != mem->params.destroyCallback) {
            mem->params.destroyCallback(memory);
        }

        free(mem);
    }
}

void* PnpMemory_GetBuffer(PNPMEMORY memory, int* size) {
    PPNPBRIDGE_MEMORY_TAG mem = (PPNPBRIDGE_MEMORY_TAG)memory;
    if (NULL != size) {
        *size = mem->size;
    }
    return mem->memory;
}