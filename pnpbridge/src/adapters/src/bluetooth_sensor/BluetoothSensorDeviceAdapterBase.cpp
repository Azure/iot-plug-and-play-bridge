// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <memory>

#include <azure_c_shared_utility/const_defines.h>
#include <azure_c_shared_utility/xlogging.h>

#include "BluetoothSensorDeviceAdapterBase.h"
#include "BluetoothSensorDeviceAdapterWin.h"

// static
std::unique_ptr<BluetoothSensorDeviceAdapter> BluetoothSensorDeviceAdapter::MakeUnique(
    const std::string& interfaceId,
    const std::string& componentName,
    uint64_t bluetoothAddress,
    const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor)
{
#if defined(WIN32)
    return std::make_unique<BluetoothSensorDeviceAdapterWin>(
        interfaceId,
        componentName,
        bluetoothAddress,
        interfaceDescriptor);
#else
    throw std::runtime_error("BluetoothSensorDeviceAdapter is not implemented on this platform.");
#endif
}

BluetoothSensorDeviceAdapterBase::BluetoothSensorDeviceAdapterBase(
    const std::string& interfaceId,
    const std::string& componentName,
    const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor) :
    m_interfaceDescriptor(interfaceDescriptor)
{
    auto result = DigitalTwin_InterfaceClient_Create(
        interfaceId.c_str(),
        componentName.c_str(),
        OnInterfaceRegisteredCallback,
        this,
        &m_handle);

    if (result != DIGITALTWIN_CLIENT_OK)
    {
        throw std::runtime_error(
            ("Failed to create digital twin interface: " + std::to_string(result)).c_str());
    }

    result = DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback(
        m_handle,
        OnPropertyCallback,
        this);

    if (result != DIGITALTWIN_CLIENT_OK)
    {
        throw std::runtime_error(
            ("Failed to register for property callbacks: " + std::to_string(result)).c_str());
    }

    result = DigitalTwin_InterfaceClient_SetCommandsCallback(
        m_handle,
        OnCommandCallback,
        this);

    if (result != DIGITALTWIN_CLIENT_OK)
    {
        throw std::runtime_error(
            ("Failed to register for command callbacks: " + std::to_string(result)).c_str());
    }
}

void BluetoothSensorDeviceAdapterBase::ReportSensorDataTelemetry(
    const std::vector<unsigned char>& payload)
{
    auto parsedSensorData = m_interfaceDescriptor->GetPayloadParser()->ParsePayload(payload);
    for (const auto& sensorData : parsedSensorData)
    {
        auto telemetryName = sensorData.first;
        auto telemetryMessage = sensorData.second;
        char telemetryPayload[512] = { 0 };
        sprintf(telemetryPayload, "{\"%s\":%s}", telemetryName.c_str(), telemetryMessage.c_str());
        LogInfo("Reporting telemetry: %s", telemetryPayload);

        std::vector<char> telemetryNameBuffer(
            telemetryName.c_str(), telemetryName.c_str() + telemetryName.size() + 1);
        auto result = DigitalTwin_InterfaceClient_SendTelemetryAsync(GetPnpInterfaceClientHandle(),
            reinterpret_cast<unsigned char*>(telemetryPayload),
            strlen(telemetryPayload),
            OnTelemetryCallback,
            static_cast<void*>(telemetryNameBuffer.data()));

        if (result != DIGITALTWIN_CLIENT_OK)
        {
            LogError("Failed to report sensor data telemetry for %s: %s",
                telemetryName.c_str(), MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT,
                result));
        }
    }
}

DIGITALTWIN_INTERFACE_CLIENT_HANDLE BluetoothSensorDeviceAdapterBase::GetPnpInterfaceClientHandle()
{
    return m_handle;
}

// static
void BluetoothSensorDeviceAdapterBase::OnInterfaceRegisteredCallback(
    DIGITALTWIN_CLIENT_RESULT interfaceStatus,
    void* /* userInterfaceContext */)
{
    if (interfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        LogInfo("Bluetooth sensor interface successfully registered.");
    }
    else
    {
        LogError("Bluetooth sensor interface received failed: %s",
            MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, interfaceStatus));
    }
}

// static
void BluetoothSensorDeviceAdapterBase::OnTelemetryCallback(
    DIGITALTWIN_CLIENT_RESULT telemetryStatus,
    void* /* userContextCallback */)
{
    if (telemetryStatus != DIGITALTWIN_CLIENT_OK)
    {
        LogError("Telemetry callback reported error: %s",
            MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, telemetryStatus));
    }
}

// static
void BluetoothSensorDeviceAdapterBase::OnPropertyCallback(
    const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* /* clientPropertyUpdate */,
    void* /* userInterfaceContext */)
{
    // No properties are supportedon BT sensors, no-op
}

// static
void BluetoothSensorDeviceAdapterBase::OnCommandCallback(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* /* commandRequest */,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* commandResponse,
    void* /* userInterfaceContext */)
{
    // No commands are supported on BT sensors, return 501 not found response
    commandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
    commandResponse->status = 501;
}