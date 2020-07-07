// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <memory>

#include <azure_c_shared_utility/xlogging.h>

#include <robuffer.h>
#include <windows.storage.streams.h>

#include "BluetoothSensorDeviceAdapterWin.h"

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices::Bluetooth;
using namespace ABI::Windows::Devices::Bluetooth::Advertisement;
using namespace ABI::Windows::Storage::Streams;
using namespace Windows::Storage::Streams;
using namespace Microsoft::WRL::Wrappers;
using namespace Microsoft::WRL;

BluetoothSensorDeviceAdapterWin::BluetoothSensorDeviceAdapterWin(
    const std::string& componentName,
    uint64_t bluetoothAddress,
    const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor) :
    BluetoothSensorDeviceAdapterBase(componentName, interfaceDescriptor),
    m_bluetoothAddress(bluetoothAddress),
    m_initialize(RO_INIT_MULTITHREADED)
{
    if (FAILED(m_initialize))
    {
        throw std::runtime_error("Failed to initialize RoInitializeWrapper");
    }

    InitializeBluetoothWatcher(interfaceDescriptor->GetCompanyId());
}

BluetoothSensorDeviceAdapterWin::~BluetoothSensorDeviceAdapterWin()
{
    m_watcher->remove_Received(m_bleSensorWatcherRecievedToken);
    m_watcher->remove_Stopped(m_bleSensorWatcherStoppedToken);
}

void BluetoothSensorDeviceAdapterWin::InitializeBluetoothWatcher(uint16_t companyId)
{
    HStringReference blePublisherClassName(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementPublisher);
    ComPtr<IBluetoothLEAdvertisementPublisher> blePublisher;
    if (FAILED(ActivateInstance(blePublisherClassName.Get(), blePublisher.GetAddressOf())))
    {
        throw std::runtime_error("Failed to activate BLE publisher.");
    }

    ComPtr<IBluetoothLEAdvertisement> bleAdvertisement;
    if (FAILED(blePublisher->get_Advertisement(bleAdvertisement.GetAddressOf())))
    {
        throw std::runtime_error("Failed to get advertisement from publisher.");
    }

    ComPtr<IVector<BluetoothLEManufacturerData*>> manufacturerDataList;
    if (FAILED(bleAdvertisement->get_ManufacturerData(manufacturerDataList.GetAddressOf())))
    {
        throw std::runtime_error("Failed to get manufacturer data from advertisement.");
    }

    ComPtr<IBluetoothLEManufacturerData> manufacturerData;
    HStringReference manufacturerDataClassName(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEManufacturerData);
    if (FAILED(ActivateInstance(manufacturerDataClassName.Get(), manufacturerData.GetAddressOf())))
    {
        throw std::runtime_error("Failed to active manufacturer class.");
    }

    if (FAILED(manufacturerData->put_CompanyId(companyId)))
    {
        throw std::runtime_error("Failed to set company ID on manufacturer data.");
    }

    if (FAILED(manufacturerDataList->Append(manufacturerData.Detach())))
    {
        throw std::runtime_error("Failed to append new manufacturer data to list.");
    }

    HStringReference bleWatcherClassName(
    RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher);
    if (FAILED(ActivateInstance(bleWatcherClassName.Get(), m_watcher.GetAddressOf())))
    {
        throw std::runtime_error("Failed to active BLE watcher.");
    }

    ComPtr<IBluetoothLEAdvertisementFilter> advertisementFilter;
    if (FAILED(m_watcher->get_AdvertisementFilter(advertisementFilter.GetAddressOf())))
    {
        throw std::runtime_error("Failed to get advertisement filter from BLE watcher.");
    }

    if (FAILED(advertisementFilter->put_Advertisement(bleAdvertisement.Get())))
    {
        throw std::runtime_error("Failed to set advertisement filter.");
    }

    if (FAILED(m_watcher->add_Received(
        Callback<ITypedEventHandler<BluetoothLEAdvertisementWatcher*,
        BluetoothLEAdvertisementReceivedEventArgs*>>([this](
                IBluetoothLEAdvertisementWatcher* sender,
                IBluetoothLEAdvertisementReceivedEventArgs* eventArgs) -> HRESULT
            {
                try
                {
                    this->OnAdvertisementReceived(sender, eventArgs);
                }
                catch (std::exception& e)
                {
                    LogError("Failed to parse new advertisement: %s", e.what());
                }

                return S_OK;
            }).Get(),
        &m_bleSensorWatcherRecievedToken)))
    {
        throw std::runtime_error("Failed to register for BLE watcher receive notifications.");
    }

    if (FAILED(m_watcher->add_Stopped(
        Callback<ITypedEventHandler<BluetoothLEAdvertisementWatcher*,
        BluetoothLEAdvertisementWatcherStoppedEventArgs*>>([](
                IBluetoothLEAdvertisementWatcher*,
                IBluetoothLEAdvertisementWatcherStoppedEventArgs* eventArgs) -> HRESULT
            {
                LogInfo("Bluetooth watcher stopped, extracting error from event...");
                BluetoothError btError;
                eventArgs->get_Error(&btError);
                LogInfo("...error was: %u", btError);

                return S_OK;
            }).Get(),
        &m_bleSensorWatcherStoppedToken)
    ))
    {
        throw std::runtime_error("Failed to register for BLE watcher stoppped notifications.");
    }
}

void BluetoothSensorDeviceAdapterWin::StartTelemetryReporting()
{
    if (FAILED(m_watcher->Start()))
    {
        throw std::runtime_error("Failed to start BLE sensor watcher");
    }
}

void BluetoothSensorDeviceAdapterWin::StopTelemetryReporting()
{
    if (FAILED(m_watcher->Stop()))
    {
        throw std::runtime_error("Failed to stop BLE sensor watcher");
    }
}

void BluetoothSensorDeviceAdapterWin::OnAdvertisementReceived(
    IBluetoothLEAdvertisementWatcher* /* sender */,
    IBluetoothLEAdvertisementReceivedEventArgs* eventArgs)
{
    UINT64 advertisementAddress{};
    if (FAILED(eventArgs->get_BluetoothAddress(&advertisementAddress)))
    {
        throw std::runtime_error("Failed to get BLE address from advertisement notification event.");
    }

    LogInfo("Recieved BLE advertisement from address %llu.", advertisementAddress);

    if (advertisementAddress == m_bluetoothAddress)
    {
        LogInfo("Parsing new bluetooth advertisement payload for BT address %llu", advertisementAddress);

        ComPtr<IBluetoothLEAdvertisement> advertisement;
        if (FAILED(eventArgs->get_Advertisement(advertisement.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get advertisement from advertisement notification event.");
        }

        ComPtr<IVector<BluetoothLEManufacturerData*>> manufacturerDataList;
        if (FAILED(advertisement->get_ManufacturerData(manufacturerDataList.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data from advertisement.");
        }

        ComPtr<IBluetoothLEManufacturerData> manufacturerData;
        if (FAILED(manufacturerDataList->GetAt(0, manufacturerData.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data list from advertisement.");
        }

        ComPtr<IBuffer> dataBuffer;
        if (FAILED(manufacturerData->get_Data(dataBuffer.GetAddressOf())))
        {
            throw std::runtime_error("Failed to get manufacturer data from advertisement.");
        }

        ComPtr<IBufferByteAccess> bufferAccess;
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
        ReportSensorDataTelemetry(bufferAsVector);
    }
}
