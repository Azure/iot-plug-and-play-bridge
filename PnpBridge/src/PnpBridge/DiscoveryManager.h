#pragma once

typedef struct _DISCOVERY_MANAGER {
	MAP_HANDLE DiscoveryModuleMap;
} DISCOVERY_MANAGER, *PDISCOVERY_MANAGER;


PNPBRIDGE_RESULT DiscoveryExtensions_Create(PDISCOVERY_MANAGER* discoveryManager);

/**
* @brief    DiscoveryExtensions_Start starts the discovery extensions based on the PnpBridge config.
*
* @param    config            String containing JSON config
*
* @returns  PNPBRIDGE_OK on success and other PNPBRIDGE_RESULT values on failure.
*/
PNPBRIDGE_RESULT DiscoveryExtensions_Start(PDISCOVERY_MANAGER discoveryManager, JSON_Object* args);

void DiscoveryModules_Stop(PDISCOVERY_MANAGER discoveryManager);