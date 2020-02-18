// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <PnpBridge.h>

int BluetoothSensorPnpStartDiscovery(
    _In_ PNPMEMORY deviceArgs,
    _In_ PNPMEMORY adapterArgs) noexcept;

int BluetoothSensorPnpStopDiscovery() noexcept;

int BluetoothSensorPnpInterfaceInitialize(_In_ const char* adapterArgs) noexcept;

int BluetoothSensorPnpInterfaceShutdown() noexcept;

int BluetoothSensorPnpInterfaceBind(
    _In_ PNPADAPTER_CONTEXT adapterHandle,
    _In_ PNPMESSAGE payload) noexcept;

extern DISCOVERY_ADAPTER BluetoothSensorPnpAdapter;
extern PNP_ADAPTER BluetoothSensorPnpInterface;

#ifdef __cplusplus
}
#endif
