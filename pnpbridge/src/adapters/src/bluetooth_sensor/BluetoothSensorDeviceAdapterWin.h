// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.devices.bluetooth.h>
#include <windows.devices.bluetooth.advertisement.h>
#include <wrl\event.h>
#include <wrl\wrappers\corewrappers.h>

#include "BluetoothSensorDeviceAdapterBase.h"

// Windows specific implementation of the BluetoothSensorDeviceAdapter. The Bluetooth WinRT APIs
// are used to listen for Bluetooth advertisements.
class BluetoothSensorDeviceAdapterWin :
    public BluetoothSensorDeviceAdapterBase
{
public:
    BluetoothSensorDeviceAdapterWin(
        const std::string& interfaceId,
        const std::string& componentName,
        uint64_t bluetoothAddress,
        const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor);

    ~BluetoothSensorDeviceAdapterWin() override;

    void StartTelemetryReporting() override;

    void StopTelemetryReporting() override;

private:
    void InitializeBluetoothWatcher(uint16_t companyId);

    void OnAdvertisementReceived(
        _In_ ABI::Windows::Devices::Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher* sender,
        _In_ ABI::Windows::Devices::Bluetooth::Advertisement::IBluetoothLEAdvertisementReceivedEventArgs* eventArgs);

    const uint64_t m_bluetoothAddress;

    Microsoft::WRL::Wrappers::RoInitializeWrapper m_initialize;
    EventRegistrationToken m_bleSensorWatcherRecievedToken;
    EventRegistrationToken m_bleSensorWatcherStoppedToken;
    Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher> m_watcher;
};