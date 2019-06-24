// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Discovery and Pnp Adapter headers
#include <pnpbridge.h>

extern DISCOVERY_ADAPTER EnvironmentSensorDiscovery;
extern PNP_ADAPTER EnvironmentSensorInterface;

extern DISCOVERY_ADAPTER CoreDeviceDiscovery;
extern PNP_ADAPTER CoreDeviceHealth;

PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[] = {
    &EnvironmentSensorDiscovery,
    &CoreDeviceDiscovery
};

PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &EnvironmentSensorInterface,
    &CoreDeviceHealth
};

const int DiscoveryAdapterCount = sizeof(DISCOVERY_ADAPTER_MANIFEST) / sizeof(PDISCOVERY_ADAPTER);
const int PnpAdapterCount = sizeof(PNP_ADAPTER_MANIFEST) / sizeof(PPNP_ADAPTER);

