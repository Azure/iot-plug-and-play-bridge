// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <map>
#include <mutex>

#include <azure_c_shared_utility/xlogging.h>
#include <azure_c_shared_utility/const_defines.h>
#include <pnpadapter_api.h>
#include <parson.h>

#include "InterfaceDescriptor.h"
#include "BluetoothSensorDeviceAdapter.h"
#include "BluetoothSensorDeviceAdapterBase.h"
#include "BluetoothSensorPnPBridgeInterface.h"

InterfaceDescriptorMap g_interfaceDescriptorMap;

IOTHUB_CLIENT_RESULT BluetoothSensor_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Starting PnP interface: %p", PnpComponentHandle);

    auto deviceAdapter = static_cast<BluetoothSensorDeviceAdapter*>(PnpComponentHandleGetContext(
        PnpComponentHandle));

    if (!deviceAdapter)
    {
        LogError("Device context is null, unable to start component");
        return IOTHUB_CLIENT_ERROR;
    }
    
    IOTHUB_DEVICE_CLIENT_HANDLE deviceHandle = PnpComponentHandleGetIotHubDeviceClient(PnpComponentHandle);
    deviceAdapter->SetIotHubDeviceClientHandle(deviceHandle);

    try
    {
        deviceAdapter->StartTelemetryReporting();
    }
    catch (std::exception& e)
    {
        LogError("Failed to start BLE sensor reporting for PnP interface %p: %s",
            PnpComponentHandle,
            e.what());

        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Stopping PnP interface: %p", PnpComponentHandle);

    auto deviceAdapter = static_cast<BluetoothSensorDeviceAdapter*>(PnpComponentHandleGetContext(
        PnpComponentHandle));
    try
    {
        deviceAdapter->StopTelemetryReporting();
    }
    catch (std::exception& e)
    {
        LogError("Failed to stop BLE sensor reporting for PnP interface %p: %s",
            PnpComponentHandle,
            e.what());

        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Destroying PnP interface: %p", PnpComponentHandle);

    delete static_cast<BluetoothSensorDeviceAdapter*>(PnpComponentHandleGetContext(
        PnpComponentHandle));

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT BluetoothSensor_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */,
    const char* componentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Creating PnP interface: %p", PnpComponentHandle);

    static constexpr char g_bluetoothAddressName[] = "bluetooth_address";
    static constexpr char g_bluetoothIdentityName[] = "blesensor_identity";

    if (strlen(componentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", componentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        PnpComponentHandle = NULL;
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    const auto bluetoothAddressStr = json_object_get_string(AdapterComponentConfig, g_bluetoothAddressName);
    if (!bluetoothAddressStr)
    {
        LogError("Failed to get bluetooth address from interface defined in config");
        return IOTHUB_CLIENT_INVALID_ARG;
    }

    const uint64_t bluetoothAddress = std::stoull(bluetoothAddressStr);

    const auto identity = json_object_get_string(AdapterComponentConfig, g_bluetoothIdentityName);
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
            PnpComponentHandle,
            e.what());

        return IOTHUB_CLIENT_ERROR;
    }

    PnpComponentHandleSetContext(PnpComponentHandle, newDeviceAdapter.get());
    PnpComponentHandleSetPropertyUpdateCallback(PnpComponentHandle, BluetoothSensorDeviceAdapterBase::OnPropertyCallback);
    PnpComponentHandleSetCommandCallback(PnpComponentHandle, BluetoothSensorDeviceAdapterBase::OnCommandCallback);

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
    BluetoothSensor_CreatePnpComponent,  // createPnpComponent
    BluetoothSensor_StartPnpComponent,   // startPnpComponent
    BluetoothSensor_StopPnpComponent,    // stopPnpComponent
    BluetoothSensor_DestroyPnpComponent, // destroyPnpComponent
    BluetoothSensor_DestroyPnpAdapter    // destroyAdapter
};
