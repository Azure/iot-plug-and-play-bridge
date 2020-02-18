// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <mutex>
#include <PnpBridge.h>

#include "InterfaceDescriptor.h"
#include "BluetoothSensorDeviceAdapter.h"

class BluetoothSensorPnpDiscovery
{
public:
    // Returns a BluetoothSensorPnpDiscovery instance if one already exists, otherwise creates a
    // new one based off of the given arguments
    static std::shared_ptr<BluetoothSensorPnpDiscovery> GetOrMakeShared(
        _In_ const PNPMEMORY deviceArgs,
        _In_ InterfaceDescriptorMap interfaceDescriptorMap);

    // Returns the current BluetoothSensorPnpDiscovery instance, may be nullptr
    static std::shared_ptr<BluetoothSensorPnpDiscovery> GetShared();

    virtual ~BluetoothSensorPnpDiscovery() = default;

    void OnNewBLESensorArrival(uint64_t bluetoothAddress);

    virtual void InitializePnpDiscovery(
        _In_ const PNPMEMORY deviceArgs,
        _In_ const InterfaceDescriptorMap interfaceDescriptorMap) = 0;

    void AddSensorDevice(
        uint64_t bluetoothAddress,
        _In_ std::unique_ptr<BluetoothSensorDeviceAdapter> device);

    void RemoveSensorDevice(uint64_t bluetoothAddress);

protected:
    std::unordered_map<uint64_t, std::unique_ptr<BluetoothSensorDeviceAdapter>> m_sensorDevices;

private:
    // Creates the platform specific flavor of BluetoothSensorPnpDiscovery
    static std::shared_ptr<BluetoothSensorPnpDiscovery> CreateForPlatform(
        _In_ const PNPMEMORY deviceArgs,
        _In_ const InterfaceDescriptorMap interfaceDescriptorMap);

    static std::mutex m_singletonLock;
    static std::weak_ptr<BluetoothSensorPnpDiscovery> m_singleton;
};

