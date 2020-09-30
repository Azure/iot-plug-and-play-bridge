// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

//Pnp Adapter headers
#include <pnpbridge.h>

extern PNP_ADAPTER SerialPnpInterface;
extern PNP_ADAPTER ModbusPnpInterface;
extern PNP_ADAPTER MqttPnpInterface;

#ifdef WIN32

extern PNP_ADAPTER BluetoothSensorPnpInterface;
extern PNP_ADAPTER CameraPnpInterface;
extern PNP_ADAPTER CoreDeviceHealth;

PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &CameraPnpInterface,
    &CoreDeviceHealth,
    &BluetoothSensorPnpInterface,
    &ModbusPnpInterface,
    &MqttPnpInterface,
    &SerialPnpInterface
};

#else //WIN32

PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &ModbusPnpInterface,
    &MqttPnpInterface,
    &SerialPnpInterface
};

#endif

const int PnpAdapterCount = sizeof(PNP_ADAPTER_MANIFEST) / sizeof(PPNP_ADAPTER);

