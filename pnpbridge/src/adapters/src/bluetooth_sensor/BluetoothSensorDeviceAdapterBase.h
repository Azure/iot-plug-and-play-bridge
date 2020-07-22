// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include "BluetoothSensorDeviceAdapter.h"

// This base class contains the digital twin implementation of the BluetoothSensorDeviceAdapter.
// It is expected for each platform to inherit this class and implement StartTelemetryReporting() and
// StopTelemetryReporting() as that is platform specific bluetooth code.
class BluetoothSensorDeviceAdapterBase :
    public BluetoothSensorDeviceAdapter
{
public:
    BluetoothSensorDeviceAdapterBase(
        const std::string& componentName,
        const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor);

    virtual ~BluetoothSensorDeviceAdapterBase() = default;

    DIGITALTWIN_INTERFACE_CLIENT_HANDLE GetPnpInterfaceClientHandle() override;

protected:
    void ReportSensorDataTelemetry(const std::vector<unsigned char>& payload);

private:
    static void OnInterfaceRegisteredCallback(
        IOTHUB_CLIENT_RESULT interfaceStatus,
        _In_ void* userInterfaceContext);

    static void OnTelemetryCallback(
        _In_ IOTHUB_CLIENT_RESULT telemetryStatus,
        _In_ void* userContextCallback);

    static void OnPropertyCallback(
        _In_ const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* clientPropertyUpdate,
        _In_ void* userInterfaceContext);

    static void OnCommandCallback(
        _In_ const DIGITALTWIN_CLIENT_COMMAND_REQUEST* commandRequest,
        _Out_ DIGITALTWIN_CLIENT_COMMAND_RESPONSE* commandResponse,
        _In_ void* userInterfaceContext);

    DIGITALTWIN_INTERFACE_CLIENT_HANDLE m_handle;
    const std::shared_ptr<InterfaceDescriptor> m_interfaceDescriptor;
};