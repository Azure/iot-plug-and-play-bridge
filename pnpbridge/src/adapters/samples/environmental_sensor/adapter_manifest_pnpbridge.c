// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Pnp Adapter headers
#include <pnpadapter_api.h>

extern PNP_ADAPTER EnvironmentSensorInterface;

PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &EnvironmentSensorInterface
};

const int PnpAdapterCount = sizeof(PNP_ADAPTER_MANIFEST) / sizeof(PPNP_ADAPTER);

