// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <azure_c_shared_utility/xlogging.h>
#include <robuffer.h>
#include <windows.devices.bluetooth.advertisement.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.storage.streams.h>
#include <wrl\wrappers\corewrappers.h>

#include "BluetoothSensorDiscoveryAdapterWin.h"

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices::Bluetooth;
using namespace ABI::Windows::Devices::Bluetooth::Advertisement;
using namespace ABI::Windows::Storage::Streams;

BluetoothSensorPnpDiscoveryWin::BluetoothSensorPnpDiscoveryWin() :
    m_initialize(RO_INIT_MULTITHREADED)
{
}

void BluetoothSensorPnpDiscoveryWin::InitializePnpDiscovery(
    const PNPMEMORY /* deviceArgs */,
    const InterfaceDescriptorMap interfaceDescriptorMap)
{
    if (FAILED(m_initialize))
    {
        throw std::runtime_error("Failed to initialize RoInitializeWrapper");
    }

    for (auto interfaceDescriptor : interfaceDescriptorMap)
    {
        auto bleSensorWatcher = std::make_shared<BleSensorWatcher>();

        Microsoft::WRL::Wrappers::HStringReference blePublisherClassName(
            RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementPublisher);
        Microsoft::WRL::ComPtr<IBluetoothLEAdvertisementPublisher> blePublisher;
        if (FAILED(ActivateInstance(blePublisherClassName.Get(), blePublisher.GetAddressOf())))
        {
            throw std::runtime_error("Failed to activate BLE publisher.");
        }

        Microsoft::WRL::ComPtr<IBluetoothLEAdvertisement> bleAdvertisement;
        if (FAILED(blePublisher->get_Advertisement(bleAdvertisement.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get advertisement from publisher.");
        }

        Microsoft::WRL::ComPtr<IVector<BluetoothLEManufacturerData*>> manufacturerDataList;
        if (FAILED(bleAdvertisement->get_ManufacturerData(manufacturerDataList.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data from advertisement.");
        }

        Microsoft::WRL::ComPtr<IBluetoothLEManufacturerData> manufacturerData;
        Microsoft::WRL::Wrappers::HStringReference manufacturerDataClassName(
            RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEManufacturerData);
        if (FAILED(ActivateInstance(manufacturerDataClassName.Get(), manufacturerData.GetAddressOf())))
        {
            throw std::runtime_error("Failed to active manufacturer class.");
        }

        if (FAILED(manufacturerData->put_CompanyId(interfaceDescriptor.second->GetCompanyId())))
        {
            throw std::runtime_error("Failed to set company ID on manufacturer data.");
        }

        if (FAILED(manufacturerDataList->Append(manufacturerData.Detach())))
        {
            throw std::runtime_error("Failed to append new manufacturer data to list.");
        }

        Microsoft::WRL::Wrappers::HStringReference bleWatcherClassName(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher);
        if (FAILED(ActivateInstance(bleWatcherClassName.Get(), bleSensorWatcher->watcher.GetAddressOf())))
        {
            throw std::runtime_error("Failed to active BLE watcher.");
        }

        Microsoft::WRL::ComPtr<IBluetoothLEAdvertisementFilter> advertisementFilter;
        if (FAILED(bleSensorWatcher->watcher->get_AdvertisementFilter(advertisementFilter.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get advertisement filter from BLE watcher.");
        }

        if (FAILED(advertisementFilter->put_Advertisement(bleAdvertisement.Get())))
        {
            throw std::runtime_error("Failed to set advertisement filter.");
        }

        if (FAILED(
            bleSensorWatcher->watcher->add_Received(
                Microsoft::WRL::Callback<ITypedEventHandler<BluetoothLEAdvertisementWatcher*, BluetoothLEAdvertisementReceivedEventArgs*>>(
                    [weakDiscoveryAdapter = weak_from_this()](IBluetoothLEAdvertisementWatcher* sender, IBluetoothLEAdvertisementReceivedEventArgs* eventArgs) -> HRESULT
                    {
                        auto strongDiscoveryAdapter = weakDiscoveryAdapter.lock();
                        if (strongDiscoveryAdapter)
                        {
                            try
                            {
                                strongDiscoveryAdapter->OnAdvertisementReceived(sender, eventArgs);
                            }
                            catch (std::exception& e)
                            {
                                LogError("Failed to parse new advertisement: %s", e.what());
                            }

                            return S_OK;
                        }

                        return RPC_E_DISCONNECTED;
                    }).Get(),
                        &bleSensorWatcher->bleSensorWatcherRecievedToken)
        ))
        {
            throw std::runtime_error("Failed to register for BLE watcher receive notifications.");
        }

        if (FAILED(
            bleSensorWatcher->watcher->add_Stopped(
                Microsoft::WRL::Callback<ITypedEventHandler<BluetoothLEAdvertisementWatcher*, BluetoothLEAdvertisementWatcherStoppedEventArgs*>>(
                    [](IBluetoothLEAdvertisementWatcher*, IBluetoothLEAdvertisementWatcherStoppedEventArgs* eventArgs) -> HRESULT
                    {
                        LogInfo("Bluetooth watcher stopped, extracting error from event...");
                        BluetoothError btError;
                        eventArgs->get_Error(&btError);
                        LogInfo("...error was: %u", btError);

                        return S_OK;
                    }).Get(),
                        &bleSensorWatcher->bleSensorWatcherStoppedToken)
        ))
        {
            throw std::runtime_error("Failed to register for BLE watcher stoppped notifications.");
        }

        if (FAILED(bleSensorWatcher->watcher->Start()))
        {
            throw std::runtime_error("Failed to start BLE sensor watcher");
        }

        m_bleSensorWatchers.push_back(std::move(bleSensorWatcher));
    }
}

void BluetoothSensorPnpDiscoveryWin::OnAdvertisementReceived(
    IBluetoothLEAdvertisementWatcher* /* sender */,
    IBluetoothLEAdvertisementReceivedEventArgs* eventArgs)
{
    LogInfo("Recieved BLE advertisement.");

    UINT64 bleAddress{};
    if (FAILED(eventArgs->get_BluetoothAddress(&bleAddress)))
    {
        throw std::runtime_error("Failed to get BLE address from advertisement notification event.");
    }

    if (m_sensorDevices.count(bleAddress))
    {
        LogInfo("BLE address is already known (%llu), reporting its payload as telemetry.", bleAddress);

        Microsoft::WRL::ComPtr<IBluetoothLEAdvertisement> advertisement;
        if (FAILED(eventArgs->get_Advertisement(advertisement.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get advertisement from advertisement notification event.");
        }

        Microsoft::WRL::ComPtr<IVector<BluetoothLEManufacturerData*>> manufacturerDataList;
        if (FAILED(advertisement->get_ManufacturerData(manufacturerDataList.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data from advertisement.");
        }

        Microsoft::WRL::ComPtr<IBluetoothLEManufacturerData> manufacturerData;
        if (FAILED(manufacturerDataList->GetAt(0, manufacturerData.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data list from advertisement.");
        }

        Microsoft::WRL::ComPtr<IBuffer> dataBuffer;
        if (FAILED(manufacturerData->get_Data(dataBuffer.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data from advertisement.");
        }

        Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferAccess;
        if (FAILED(dataBuffer.As(&bufferAccess)))
        {
            throw std::runtime_error("Failed to cast manufacturer data from to byte buffer.");
        }

        UINT32 bufferLength;
        if (FAILED(dataBuffer->get_Length(&bufferLength)))
        {
            throw std::runtime_error("Failed to get length of manufacturer data buffer.");
        }

        BYTE* buffer;
        if (FAILED(bufferAccess->Buffer(&buffer)))
        {
            throw std::runtime_error("Failed to access manufacturer data buffer.");
        }

        std::vector<byte> bufferAsVector;
        bufferAsVector.assign(buffer, buffer + bufferLength);

        m_sensorDevices.at(bleAddress)->ParseManufacturerData(bufferAsVector);
    }
    else
    {
        LogInfo("BLE address is not known (%llu), attempting to create new device", bleAddress);

        OnNewBLESensorArrival(bleAddress);
    }
}
