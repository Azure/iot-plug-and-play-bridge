// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <string>

#include <pnpbridge.h>

#include "InterfaceDescriptor.h"

// The BluetoothSensorDeviceAdapter adapter is responsible for listening to bluetooth sensor
// advertisements and sending the corresponding telemetry up to the PnP bridge.
class BluetoothSensorDeviceAdapter
{
public:
    // Creates a BluetoothSensorDeviceAdapter that registers a PnP interface using the given
    // interface ID and instance name. The adapter will listen for bluetooth advertisements
    // that have the given bluetooth address. This function will throw if it fails to create
    // the BluetoothSensorDeviceAdapter.
    static std::unique_ptr<BluetoothSensorDeviceAdapter> MakeUnique(
        const std::string& componentName,
        uint64_t bluetoothAddress,
        const std::shared_ptr<InterfaceDescriptor>& interfaceDescriptor);

    virtual ~BluetoothSensorDeviceAdapter() = default;

    // Starts listening for bluetooth sensor advertisements and the corresponding telemetry to the
    // PnP bridge. This function is a no-op if StartTelemetryReporting() is called twice in a row.
    virtual void StartTelemetryReporting() = 0;

    // Stops listening for bluetooth sensor advertisements. No telemetry will be sent after this
    // function returns. This function is a no-op if there was no preceding StartTelemetryReporting()
    // call. Clients are expected to stop reporting before destructing this object if it was
    // previously started.
    virtual void StopTelemetryReporting() = 0;

    virtual void SetIotHubDeviceClientHandle(IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle) = 0;

};
