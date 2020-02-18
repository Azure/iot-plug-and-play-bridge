// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <unordered_map>
#include <memory>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.devices.bluetooth.h>
#include <windows.devices.bluetooth.advertisement.h>
#include <wrl\event.h>
#include <wrl\wrappers\corewrappers.h>

#include "InterfaceDescriptor.h"
#include "BluetoothSensorDiscoveryAdapter.h"

// Specialization of BluetoothSensorPnpDiscovery which handles all platform specific operations on
// Windows related to BLE.
class BluetoothSensorPnpDiscoveryWin final :
    public BluetoothSensorPnpDiscovery,
    public std::enable_shared_from_this<BluetoothSensorPnpDiscoveryWin>
{
public:
    BluetoothSensorPnpDiscoveryWin();

    ~BluetoothSensorPnpDiscoveryWin() override = default;

    void InitializePnpDiscovery(
        _In_ const PNPMEMORY deviceArgs,
        _In_ const InterfaceDescriptorMap interfaceDescriptorMap) override;

private:
    struct BleSensorWatcher
    {
        EventRegistrationToken bleSensorWatcherRecievedToken;
        EventRegistrationToken bleSensorWatcherStoppedToken;
        Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher> watcher;

        ~BleSensorWatcher()
        {
            if (watcher)
            {
                watcher->remove_Received(bleSensorWatcherRecievedToken);
                watcher->remove_Stopped(bleSensorWatcherStoppedToken);
            }
        }
    };

    void OnAdvertisementReceived(
        _In_ ABI::Windows::Devices::Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher* sender,
        _In_ ABI::Windows::Devices::Bluetooth::Advertisement::IBluetoothLEAdvertisementReceivedEventArgs* eventArgs);

    Microsoft::WRL::Wrappers::RoInitializeWrapper m_initialize;
    std::vector<std::shared_ptr<BleSensorWatcher>> m_bleSensorWatchers;
};

