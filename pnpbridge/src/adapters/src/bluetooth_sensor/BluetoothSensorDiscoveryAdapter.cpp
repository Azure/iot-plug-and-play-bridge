// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <azure_c_shared_utility/xlogging.h>

#include "Common.h"
#include "InterfaceDescriptor.h"
#include "BluetoothSensorDiscoveryAdapter.h"
#if defined(WIN32)
#include "BluetoothSensorDiscoveryAdapterWin.h"
#endif

std::mutex BluetoothSensorPnpDiscovery::m_singletonLock;
std::weak_ptr<BluetoothSensorPnpDiscovery> BluetoothSensorPnpDiscovery::m_singleton;

// static
std::shared_ptr<BluetoothSensorPnpDiscovery> BluetoothSensorPnpDiscovery::GetOrMakeShared(
    const PNPMEMORY deviceArgs,
    const InterfaceDescriptorMap interfaceDescriptorMap)
{
    std::lock_guard<std::mutex> guard(m_singletonLock);

    auto singleton = m_singleton.lock();
    if (!singleton)
    {
        singleton = CreateForPlatform(deviceArgs, interfaceDescriptorMap);
        m_singleton = singleton;
    }
    return singleton;
}

// static
std::shared_ptr<BluetoothSensorPnpDiscovery> BluetoothSensorPnpDiscovery::GetShared()
{
    std::lock_guard<std::mutex> guard(m_singletonLock);
    return m_singleton.lock();
}

// static
std::shared_ptr<BluetoothSensorPnpDiscovery> BluetoothSensorPnpDiscovery::CreateForPlatform(
    const PNPMEMORY deviceArgs,
    const InterfaceDescriptorMap interfaceDescriptorMap)
{
    std::shared_ptr<BluetoothSensorPnpDiscoveryWin> newAdapter;

#if defined(WIN32)
    newAdapter = std::make_shared<BluetoothSensorPnpDiscoveryWin>();
#else
    throw std::runtime_error("BluetoothSensorPnpDiscovery is not implemented on this platform.");
#endif

    newAdapter->InitializePnpDiscovery(deviceArgs, interfaceDescriptorMap);
    return newAdapter;
}

void BluetoothSensorPnpDiscovery::AddSensorDevice(
    uint64_t bluetoothAddress,
    std::unique_ptr<BluetoothSensorDeviceAdapter> device)
{
    if (m_sensorDevices.find(bluetoothAddress) != m_sensorDevices.end())
    {
        LogError("Already registered device with BT address %llu, overriding.",
            bluetoothAddress);
    }

    m_sensorDevices[bluetoothAddress] = std::move(device);
}

void BluetoothSensorPnpDiscovery::RemoveSensorDevice(uint64_t bluetoothAddress)
{
    if (m_sensorDevices.find(bluetoothAddress) == m_sensorDevices.end())
    {
        LogError("Attempted to remove device with BT address %llu, but it does not exist.",
            bluetoothAddress);
    }
    else
    {
        m_sensorDevices.erase(bluetoothAddress);
    }
}

void BluetoothSensorPnpDiscovery::OnNewBLESensorArrival(uint64_t bluetoothAddress)
{
    std::string deviceChangeMessageformat = "{ \
                                                \"identity\": \"bluetooth-discovery-adapter\", \
                                                \"match_parameters\": { \
                                                \"bluetooth_address\": \"";
    deviceChangeMessageformat += std::to_string(bluetoothAddress);
    deviceChangeMessageformat += "\"}}";

    PNPMESSAGE msg{};
    auto result = PnpMessage_CreateMessage(&msg);
    if (result != 0)
    {
        throw AzurePnpException("Could not create PnP message for sensor arrival", result);
    }

    result = PnpMessage_SetMessage(msg, deviceChangeMessageformat.c_str());
    if (result != 0)
    {
        throw AzurePnpException("Could not set PnP message for sensor arrival", result);
    }

    LogInfo("Reporting BLE address %llu to bridge", bluetoothAddress);

    result = DiscoveryAdapter_ReportDevice(msg);
    if (result != 0)
    {
        throw AzurePnpException("Could not report the device to the discovery adapter", result);
    }

    auto props = PnpMessage_AccessProperties(msg);
    if (!props)
    {
        throw std::runtime_error("Could not access message properties.");
    }

    props->Context = this;
    PnpMemory_ReleaseReference(msg);
}
