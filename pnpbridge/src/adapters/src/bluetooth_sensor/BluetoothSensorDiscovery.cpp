// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <memory>
#include <unordered_map>
#include <azure_c_shared_utility/xlogging.h>
#include "Common.h"
#include "BluetoothSensorDiscovery.h"
#include "BluetoothSensorDiscoveryAdapter.h"
#include "InterfaceDescriptor.h"

DISCOVERY_ADAPTER BluetoothSensorPnpAdapter = {
    "bluetooth-sensor-discovery-adapter",
    BluetoothSensorPnpStartDiscovery,
    BluetoothSensorPnpStopDiscovery
};

PNP_ADAPTER BluetoothSensorPnpInterface = {
    "bluetooth-sensor-pnp-adapter",
    BluetoothSensorPnpInterfaceInitialize,
    BluetoothSensorPnpInterfaceBind,
    BluetoothSensorPnpInterfaceShutdown
};

std::shared_ptr<BluetoothSensorPnpDiscovery> g_discoveryAdapter{};
InterfaceDescriptorMap g_payloadParser{};

int BluetoothSensorPnpStartDiscovery(
    PNPMEMORY deviceArgs,
    PNPMEMORY adapterArgs) noexcept
{
    LogInfo("Starting discovery adapter.");

    LogInfo("Starting to payload descriptor.");
    const char* adapterJsonString = static_cast<char*>(PnpMemory_GetBuffer(adapterArgs, nullptr));
    JsonValuePtr adapterJsonValue(json_parse_string(adapterJsonString), DestroyJsonValue);
    if (!adapterJsonValue)
    {
        LogError("Failed to parse adapter JSON into a JSON object.");
        return -1;
    }

    JSON_Object* adapterJsonObj = json_value_get_object(adapterJsonValue.get());
    if (!adapterJsonObj)
    {
        LogInfo("Unable to extract adapter arguments.");
        return -2;
    }

    JSON_Array* adapterJsonArray = json_object_dotget_array(adapterJsonObj, "interface_descriptors");
    if (!adapterJsonObj)
    {
        LogInfo("Unable to extract interface_descriptors from adapter arguments.");
        return -3;
    }

    try
    {
        g_payloadParser = InterfaceDescriptor::MakeDescriptorsFromJson(adapterJsonArray);

        LogInfo("Done parsing payload descriptor.");

        g_discoveryAdapter = BluetoothSensorPnpDiscovery::GetOrMakeShared(
            deviceArgs,
            g_payloadParser);
    }
    catch (std::exception& e)
    {
        LogError("Failed to create PnP discovery: %s", e.what());
        return -4;
    }

    LogInfo("Successfully started discovery adapter.");
    return 0;
}

int BluetoothSensorPnpStopDiscovery() noexcept
{
    LogInfo("Stopping discovery adapter.");

    g_discoveryAdapter.reset();

    return 0;
}

int BluetoothSensorPnpInterfaceInitialize(const char* /* adapterArgs */) noexcept
{
    // no-op
    return 0;
}

int BluetoothSensorPnpInterfaceShutdown() noexcept
{
    // no-op
    return 0;
}

int BluetoothSensorPnpInterfaceBind(
    PNPADAPTER_CONTEXT adapterHandle,
    PNPMESSAGE payload) noexcept try
{
    LogInfo("Binding new PnP interface.");

    const char* interfaceId = PnpMessage_GetInterfaceId(payload);
    if (g_payloadParser.find(interfaceId) == g_payloadParser.end())
    {
        LogError("Attempted to bind to interface which is not known: %s", interfaceId);
        return -1;
    }

    auto newDevice = BluetoothSensorDeviceAdapter::MakeUnique(adapterHandle, payload, g_payloadParser[interfaceId]);
    uint64_t bluetoothAddress = newDevice->GetBluetoothAddress();
    g_discoveryAdapter->AddSensorDevice(bluetoothAddress, std::move(newDevice));

    return 0;
}
catch (std::exception& e)
{
    LogError("Failed to create PnP discovery interface: %s", e.what());
    return -2;
};
