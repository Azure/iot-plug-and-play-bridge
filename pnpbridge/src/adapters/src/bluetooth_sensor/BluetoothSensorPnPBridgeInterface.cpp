// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <map>
#include <mutex>

#include <azure_c_shared_utility/xlogging.h>
#include <azure_c_shared_utility/const_defines.h>
#include <pnpbridge.h>
#include <parson.h>

#include "InterfaceDescriptor.h"
#include "BluetoothSensorDeviceAdapter.h"
#include "BluetoothSensorPnPBridgeInterface.h"

InterfaceDescriptorMap g_interfaceDescriptorMap;

IOTHUB_CLIENT_RESULT BluetoothSensor_StartPnpInterface(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */,
    PNPBRIDGE_INTERFACE_HANDLE pnpInterfaceHandle) noexcept
{
    LogInfo("Starting PnP interface: %p", pnpInterfaceHandle);

    auto deviceAdapter = static_cast<BluetoothSensorDeviceAdapter*>(PnpInterfaceHandleGetContext(
        pnpInterfaceHandle));
    try
    {
        deviceAdapter->StartTelemetryReporting();
    }
    catch (std::exception& e)
    {
        LogError("Failed to start BLE sensor reporting for PnP interface %p: %s",
            pnpInterfaceHandle,
            e.what());

        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_StopPnpInterface(
    PNPBRIDGE_INTERFACE_HANDLE pnpInterfaceHandle) noexcept
{
    LogInfo("Stopping PnP interface: %p", pnpInterfaceHandle);

    auto deviceAdapter = static_cast<BluetoothSensorDeviceAdapter*>(PnpInterfaceHandleGetContext(
        pnpInterfaceHandle));
    try
    {
        deviceAdapter->StopTelemetryReporting();
    }
    catch (std::exception& e)
    {
        LogError("Failed to stop BLE sensor reporting for PnP interface %p: %s",
            pnpInterfaceHandle,
            e.what());

        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_DestroyPnpInterface(
    PNPBRIDGE_INTERFACE_HANDLE pnpInterfaceHandle) noexcept
{
    LogInfo("Destroying PnP interface: %p", pnpInterfaceHandle);

    DigitalTwin_InterfaceClient_Destroy(pnpInterfaceHandle);
    delete static_cast<BluetoothSensorDeviceAdapter*>(PnpInterfaceHandleGetContext(
        pnpInterfaceHandle));

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_CreatePnpInterface(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */,
    const char* componentName,
    const JSON_Object* adapterInterfaceConfig,
    PNPBRIDGE_INTERFACE_HANDLE pnpInterfaceHandle,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* pnpInterfaceClient) noexcept
{
    LogInfo("Creating PnP interface: %p", pnpInterfaceHandle);

    static constexpr char g_bluetoothAddressName[] = "bluetooth_address";
    static constexpr char g_bluetoothIdentityName[] = "blesensor_identity";

    const auto bluetoothAddressStr = json_object_get_string(adapterInterfaceConfig, g_bluetoothAddressName);
    if (!bluetoothAddressStr)
    {
        LogError("Failed to get bluetooth address from interface defined in config");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    const uint64_t bluetoothAddress = std::stoull(bluetoothAddressStr);

    const auto identity = json_object_get_string(adapterInterfaceConfig, g_bluetoothIdentityName);
    if (!identity)
    {
        LogError("Failed to get interface identity defined in config");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    const auto interfaceDescriptor = g_interfaceDescriptorMap.find(std::string(identity));
    if (interfaceDescriptor == g_interfaceDescriptorMap.end())
    {
        LogError("Could not find an interface identity for %s", identity);
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    std::unique_ptr<BluetoothSensorDeviceAdapter> newDeviceAdapter;
    try
    {
        newDeviceAdapter = BluetoothSensorDeviceAdapter::MakeUnique(
            componentName,
            bluetoothAddress,
            interfaceDescriptor->second);
    }
    catch (std::exception& e)
    {
        LogError("Failed to create BLE sensor device adapter for PnP interface %p: %s",
            pnpInterfaceHandle,
            e.what());

        return IOTHUB_CLIENT_ERROR;
    }

    *pnpInterfaceClient = newDeviceAdapter->GetPnpInterfaceClientHandle();
    PnpInterfaceHandleSetContext(pnpInterfaceHandle, newDeviceAdapter.get());
    // PnP interface now owns the pointer
    newDeviceAdapter.release();

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_CreatePnpAdapter(
    const JSON_Object* adapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */) noexcept
{
    LogInfo("Creating the bluetooth sensor PnP adapter.");

    try
    {
        g_interfaceDescriptorMap = InterfaceDescriptor::MakeDescriptorsFromJson(adapterGlobalConfig);
    }
    catch (std::exception& e)
    {
        LogError("Failed to create BLE sensor adapter: %s", e.what());

        return IOTHUB_CLIENT_ERROR;
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */) noexcept
{
    LogInfo("Destroying the bluetooth sensor PnP adapter.");

    return IOTHUB_CLIENT_OK;
}

PNP_ADAPTER BluetoothSensorPnpInterface = {
    "bluetooth-sensor-pnp-adapter",      // identity
    BluetoothSensor_CreatePnpAdapter,    // createAdapter
    BluetoothSensor_CreatePnpInterface,  // createPnpInterface
    BluetoothSensor_StartPnpInterface,   // startPnpInterface
    BluetoothSensor_StopPnpInterface,    // stopPnpInterface
    BluetoothSensor_DestroyPnpInterface, // destroyPnpInterface
    BluetoothSensor_DestroyPnpAdapter    // destroyAdapter
};
