// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum PNP_BRIDGE_STATE {
    PNP_BRIDGE_UNINITIALIZED,
    PNP_BRIDGE_INITIALIZED,
    PNP_BRIDGE_TEARING_DOWN,
    PNP_BRIDGE_DESTROYED
} PNP_BRIDGE_STATE;

// Device aggregator context
typedef struct _PNP_BRIDGE {
    MX_IOT_HANDLE_TAG IotHandle;

    PPNP_ADAPTER_MANAGER PnpMgr;

    PNPBRIDGE_CONFIGURATION Configuration;

    COND_HANDLE ExitCondition;

    LOCK_HANDLE ExitLock;
} PNP_BRIDGE, *PPNP_BRIDGE;


void PnpBridge_Release(PPNP_BRIDGE pnpBridge);

#ifdef __cplusplus
}
#endif