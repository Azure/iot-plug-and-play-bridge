// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"

#include "CameraPnPBridgeInterface.h"
#include "CameraIotPnpDeviceAdapter.h"

IOTHUB_CLIENT_RESULT Camera_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Starting the PnP interface: %p", PnpComponentHandle);

    const auto cameraDevice = static_cast<CameraIotPnpDeviceAdapter*>(
        PnpComponentHandleGetContext(PnpComponentHandle));
    if (cameraDevice)
    {
        if (cameraDevice->Start() != S_OK)
        {
            return IOTHUB_CLIENT_ERROR;
        }
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT Camera_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Stopping PnP interface: %p", PnpComponentHandle);

    const auto cameraDevice = static_cast<CameraIotPnpDeviceAdapter*>(
        PnpComponentHandleGetContext(PnpComponentHandle));
    if (cameraDevice)
    {
        // Blocks until the worker is stopped.
        cameraDevice->Stop();
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT Camera_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle) noexcept
{
    LogInfo("Destroying PnP interface: %p", PnpComponentHandle);

    DigitalTwin_InterfaceClient_Destroy(PnpComponentHandle);

    auto cameraDevice = static_cast<CameraIotPnpDeviceAdapter*>(
        PnpComponentHandleGetContext(PnpComponentHandle));
    if (cameraDevice)
    {
        // Blocks until the worker is stopped.
        cameraDevice->Stop();

        delete cameraDevice;
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT Camera_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */,
    const char* componentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* pnpInterfaceClient) noexcept
{
    static constexpr char g_cameraIdentityName[] = "hardware_id";
    const auto cameraId = json_object_get_string(AdapterComponentConfig, g_cameraIdentityName);
    if (!cameraId)
    {
        LogError("Failed to get camera hardware ID from interface defined in config");
        return IOTHUB_CLIENT_INVALID_ARG;
    }
    std::string cameraIdStr(cameraId);

    auto newCameraDevice = CameraIotPnpDeviceAdapter::MakeUnique(
        componentName,
        cameraIdStr,
        pnpInterfaceClient);

    PnpComponentHandleSetContext(
        PnpComponentHandle,
        static_cast<void*>(newCameraDevice.get()));

    // Interface context now owns object
    newCameraDevice.release();

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT Camera_CreatePnpAdapter(
    const JSON_Object* /* adapterGlobalConfig */,
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */) noexcept
{
    LogInfo("Creating the camera PnP adapter.");

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT Camera_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE /* adapterHandle */) noexcept
{
    LogInfo("Destroying the camera PnP adapter.");

    return IOTHUB_CLIENT_OK;
}

PNP_ADAPTER CameraPnpInterface = {
    "camera-pnp-adapter",         // identity
    Camera_CreatePnpAdapter,      // createAdapter
    Camera_CreatePnpComponent,    // createPnpComponent
    Camera_StartPnpComponent,     // startPnpComponent
    Camera_StopPnpComponent,      // stopPnpComponent
    Camera_DestroyPnpComponent,   // destroyPnpComponent
    Camera_DestroyPnpAdapter      // destroyAdapter
};
