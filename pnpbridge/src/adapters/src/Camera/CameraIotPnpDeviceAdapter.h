// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <string>

#include <digitaltwin_device_client.h>
#include <digitaltwin_interface_client.h>

// Represents a PnP interface. It will register for its respective digital twin and then watch for
// camera arrivals and removals. When the camera device arrives, it will create a CameraIotPnpDevice
// for it and start sending telemetry if Start() has been called. When the camera device is removed
// the CameraIotPnpDevice instance is removed.
class CameraIotPnpDeviceAdapter
{
public:
    static std::unique_ptr<CameraIotPnpDeviceAdapter> MakeUnique(
        const std::string& interfaceId,
        const std::string& componentName,
        const std::string& cameraId,
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE* interfaceClient);

    CameraIotPnpDeviceAdapter(
        const std::string& cameraId);

    void Initialize(
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceClientHandle);

    // Allows properties, telemetry, and to be sent
    HRESULT Start();

    // Stops any properties and telemetry from being sent
    void Stop();

private:
    static void CameraPnpCallback_ProcessCommandUpdate(
        const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtCommandRequest,
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse,
        void* userInterfaceContext);

    static void CameraPnpCallback_ProcessPropertyUpdate(
        const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* dtClientPropertyUpdate,
        void* userContextCallback);

    void OnCameraArrivalRemoval();

    void TakePhoto(
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext);

    void TakeVideo(
        DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext);

    void GetURI(DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext);

    const std::string m_cameraId;
    bool m_hasStartedReporting{};

    std::mutex m_cameraDeviceLock;
    std::unique_ptr<CameraIotPnpDevice> m_cameraDevice;
    std::unique_ptr<CameraPnpDiscovery> m_cameraDiscovery;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE m_interfaceClientHandle;
};