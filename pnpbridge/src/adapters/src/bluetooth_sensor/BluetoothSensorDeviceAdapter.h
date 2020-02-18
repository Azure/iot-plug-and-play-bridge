// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <PnpBridge.h>

#include "InterfaceDescriptor.h"

class BluetoothSensorDeviceAdapter
{
public:
    static std::unique_ptr<BluetoothSensorDeviceAdapter> MakeUnique(
        _In_ PNPADAPTER_CONTEXT adapterHandle,
        _In_ PNPMESSAGE payload,
        _In_ std::shared_ptr<InterfaceDescriptor> interfaceDescriptor);

    void Initialize(
        _In_ PNPADAPTER_CONTEXT adapterHandle,
        _In_ PNPMESSAGE payload,
        _In_ std::shared_ptr<InterfaceDescriptor> interfaceDescriptor);

    void ParseManufacturerData(_In_ const std::vector<unsigned char>& buffer);

    uint64_t GetBluetoothAddress();

protected:
    static void BluetoothSensorPnpCallback_ProcessCommandUpdate(
        _In_ const DIGITALTWIN_CLIENT_COMMAND_REQUEST* clientCommandContext,
        _In_ DIGITALTWIN_CLIENT_COMMAND_RESPONSE* clientCommandResponseContext,
        _In_ void* userContextCallback);

    static int OnBluetoothSensorInterfaceStart(_In_ PNPADAPTER_INTERFACE_HANDLE pnpInterface);

    static int OnBluetoothSensorInterfaceRelease(_In_ PNPADAPTER_INTERFACE_HANDLE pnpInterface);

    static void OnDigitalTwinInterfaceClientCreate(
        _In_ DIGITALTWIN_CLIENT_RESULT interfaceStatus,
        _In_ void* userInterfaceContext);

    static void OnDigitalTwinInterfaceSendTelemetry(
        _In_ DIGITALTWIN_CLIENT_RESULT pnpTelemetryStatus,
        _In_opt_ void* userContextCallback);

    void ReportSensorData(_In_ const ParsedSensorData& payload);

    void ReportTelemetry(_In_ const std::string& telemetryName, _In_ const std::string& message);

    uint64_t m_bluetoothAddress{};
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE m_pnpClientInterface{};
    std::shared_ptr<InterfaceDescriptor> m_interfaceDescriptor{};
};

