// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <memory>

#include <azure_c_shared_utility/const_defines.h>
#include <azure_c_shared_utility/xlogging.h>

#include "BluetoothSensorDeviceAdapterBase.h"
#include "BluetoothSensorDeviceAdapterWin.h"

// static
std::unique_ptr<BluetoothSensorDeviceAdapter> BluetoothSensorDeviceAdapter::MakeUnique(
    const std::string& componentName,
    uint64_t bluetoothAddress,
    const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor)
{
#if defined(WIN32)
    return std::make_unique<BluetoothSensorDeviceAdapterWin>(
        componentName,
        bluetoothAddress,
        interfaceDescriptor);
#else
    throw std::runtime_error("BluetoothSensorDeviceAdapter is not implemented on this platform.");
#endif
}

void BluetoothSensorDeviceAdapterBase::SetIotHubDeviceClientHandle(
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle)
{
    m_deviceClient = DeviceClientHandle;
}

BluetoothSensorDeviceAdapterBase::BluetoothSensorDeviceAdapterBase(
    const std::string& componentName,
    const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor) :
    m_interfaceDescriptor(interfaceDescriptor),
    m_componentName(componentName)
{

}

void BluetoothSensorDeviceAdapterBase::ReportSensorDataTelemetry(
    const std::vector<unsigned char>& payload)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    auto parsedSensorData = m_interfaceDescriptor->GetPayloadParser()->ParsePayload(payload);
    for (const auto& sensorData : parsedSensorData)
    {
        IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

        auto telemetryName = sensorData.first;
        auto telemetryMessage = sensorData.second;
        char telemetryPayload[512] = { 0 };
        sprintf(telemetryPayload, "{\"%s\":%s}", telemetryName.c_str(), telemetryMessage.c_str());
        LogInfo("Reporting telemetry: %s", telemetryPayload);

        std::vector<char> telemetryNameBuffer(
            telemetryName.c_str(), telemetryName.c_str() + telemetryName.size() + 1);

        if ((messageHandle = PnP_CreateTelemetryMessageHandle(m_componentName.c_str(), telemetryPayload)) == NULL)
        {
            LogError("Bluetooth Sensor Component %s: PnP_CreateTelemetryMessageHandle failed.", m_componentName.c_str());
        }
        else if ((result = IoTHubDeviceClient_SendEventAsync(m_deviceClient, messageHandle,
                OnTelemetryCallback, static_cast<void*>(telemetryNameBuffer.data()))) != IOTHUB_CLIENT_OK)
        {
            LogError("Bluetooth Sensor Component %s: Failed to report sensor data telemetry %s, error=%d",
                m_componentName.c_str(), telemetryName.c_str(), result);
        }

        IoTHubMessage_Destroy(messageHandle);

    }
}

// static
void BluetoothSensorDeviceAdapterBase::OnInterfaceRegisteredCallback(
    IOTHUB_CLIENT_RESULT interfaceStatus,
    void* /* userInterfaceContext */)
{
    if ((IOTHUB_CLIENT_OK == interfaceStatus))
    {
        LogInfo("Bluetooth sensor interface successfully registered.");
    }
    else
    {
        LogError("Bluetooth sensor interface received failed: %d",
            interfaceStatus);
    }
}

// static
void BluetoothSensorDeviceAdapterBase::OnTelemetryCallback(
    IOTHUB_CLIENT_CONFIRMATION_RESULT telemetryStatus,
    void* userContextCallback)
{
    if (telemetryStatus != IOTHUB_CLIENT_OK)
    {
        LogError("Bluetooth Sensor Component: Telemetry callback reported error: %d",
            telemetryStatus);
    }
    else
    {
        LogInfo("Bluetooth Sensor Component: Telemetry %s was reported correctly",
            (char*)userContextCallback);
    }
}

// static
void BluetoothSensorDeviceAdapterBase::OnPropertyCallback(
    PNPBRIDGE_COMPONENT_HANDLE /*PnpComponentHandle*/,
    const char* /* PropertyName */,
    JSON_Value* /* PropertyValue */,
    int /* version */,
    void* /* userContextCallback */)
{
    // No properties are supported on BT sensors, no-op
}

// static
int BluetoothSensorDeviceAdapterBase::OnCommandCallback(
    PNPBRIDGE_COMPONENT_HANDLE /* PnpComponentHandle */,
    const char* /* CommandName */,
    JSON_Value* /* CommandValue */,
    unsigned char** /* CommandResponse */,
    size_t* /* CommandResponseSize */)
{
    // No commands are supported on BT sensors
    return PNP_STATUS_SUCCESS;
}