// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Discovery and Pnp Adapter headers
#include <pnpbridge.h>

extern DISCOVERY_ADAPTER SerialPnpDiscovery;
extern DISCOVERY_ADAPTER ModbusPnpDeviceDiscovery;

extern PNP_ADAPTER SerialPnpInterface;
extern PNP_ADAPTER ModbusPnpInterface;

#ifdef WIN32

extern DISCOVERY_ADAPTER CameraPnpAdapter;
extern DISCOVERY_ADAPTER CoreDeviceDiscovery;

PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[] = {
    &CameraPnpAdapter,
    &SerialPnpDiscovery,
    &CoreDeviceDiscovery,
    &ModbusPnpDeviceDiscovery
};

extern PNP_ADAPTER CameraPnpInterface;
extern PNP_ADAPTER CoreDeviceHealth;

PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &CameraPnpInterface,
    &SerialPnpInterface,
    &CoreDeviceHealth,
    &ModbusPnpInterface
};

#else //WIN32

PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[] = {
    &SerialPnpDiscovery,
    &ModbusPnpDeviceDiscovery
};

PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &SerialPnpInterface,
    &ModbusPnpInterface
};

#endif

const int DiscoveryAdapterCount = sizeof(DISCOVERY_ADAPTER_MANIFEST) / sizeof(PDISCOVERY_ADAPTER);
const int PnpAdapterCount = sizeof(PNP_ADAPTER_MANIFEST) / sizeof(PPNP_ADAPTER);

