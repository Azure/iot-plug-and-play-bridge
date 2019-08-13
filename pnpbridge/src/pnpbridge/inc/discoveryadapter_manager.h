// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _DISCOVERY_MANAGER {
    MAP_HANDLE DiscoveryAdapterMap;

    // List of thread handles for startdiscovery
   // SINGLYLINKEDLIST_HANDLE startDiscoveryThreadHandles;
} DISCOVERY_MANAGER, *PDISCOVERY_MANAGER;


PNPBRIDGE_RESULT DiscoveryAdapterManager_Create(PDISCOVERY_MANAGER* DiscoveryManager);

/**
* @brief    DiscoveryAdapterManager_Start starts the discovery extensions based on the PnpBridge config.
*
* @param    config            String containing JSON config
*
* @returns  PNPBRIDGE_OK on success and other PNPBRIDGE_RESULT values on failure.
*/
PNPBRIDGE_RESULT DiscoveryAdapterManager_Start(PDISCOVERY_MANAGER DiscoveryManager, PNPBRIDGE_CONFIGURATION* Config);

//PNPBRIDGE_RESULT DiscoveryAdapterManager_PublishAlwaysInterfaces(PDISCOVERY_MANAGER discoveryManager, JSON_Value* config);

void DiscoveryAdapterManager_Release(PDISCOVERY_MANAGER discoveryManager);

#define DISCOVERY_MANAGER_ALWAY_PUBLISH_PAYLOAD "{ \
                                                     \"Identity\": \"pnpbridge-core\", \
                                                     \"InterfaceId\": \"%s\", \
                                                     \"PublishMode\": \"Always\", \
                                                     \"MatchParameters\": %s \
                                                   }"

int PnpBridge_ProcessPnpMessage(PNPMESSAGE DeviceChangePayload);

#ifdef __cplusplus
}
#endif