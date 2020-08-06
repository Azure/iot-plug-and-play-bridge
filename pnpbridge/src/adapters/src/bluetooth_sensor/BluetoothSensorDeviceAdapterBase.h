// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include "BluetoothSensorDeviceAdapter.h"

// This base class contains the implementation of the BluetoothSensorDeviceAdapter.
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

    void SetIotHubDeviceClientHandle(IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle) override;

    static void OnPropertyCallback(
        _In_ PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        _In_ const char* PropertyName,
        _In_ JSON_Value* PropertyValue,
        _In_ int version,
        _In_ void* userContextCallback);

    static int OnCommandCallback(
        _In_ PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        _In_ const char* CommandName,
        _In_ JSON_Value* CommandValue,
        _Out_ unsigned char** CommandResponse,
        _Out_ size_t* CommandResponseSize);

protected:
    void ReportSensorDataTelemetry(const std::vector<unsigned char>& payload);

private:
    static void OnInterfaceRegisteredCallback(
        IOTHUB_CLIENT_RESULT interfaceStatus,
        _In_ void* userInterfaceContext);

    static void OnTelemetryCallback(
        _In_ IOTHUB_CLIENT_CONFIRMATION_RESULT telemetryStatus,
        _In_ void* userContextCallback);

    const std::shared_ptr<InterfaceDescriptor> m_interfaceDescriptor;
    std::string m_componentName;
    IOTHUB_DEVICE_CLIENT_HANDLE m_deviceClient;
};