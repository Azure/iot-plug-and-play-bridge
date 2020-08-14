// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <string>

// Represents a PnP interface. It will = watch for camera arrivals and removals. When the camera 
// device arrives, it will create a CameraIotPnpDevice for it and start sending telemetry if Start()
// has been called. When the camera device is removed the CameraIotPnpDevice instance is removed.
class CameraIotPnpDeviceAdapter
{
public:
    static std::unique_ptr<CameraIotPnpDeviceAdapter> MakeUnique(
        const std::string& componentName,
        const std::string& cameraId);

    CameraIotPnpDeviceAdapter(
        const std::string& cameraId,
        const std::string& componentName);

    void Initialize();

    // Allows properties, telemetry, and to be sent
    HRESULT Start();

    // Stops any properties and telemetry from being sent
    void Stop();

    void SetIotHubDeviceClientHandle(IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle);

    static int CameraPnpCallback_ProcessCommandUpdate(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

    static void CameraPnpCallback_ProcessPropertyUpdate(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version,
        void* userContextCallback);

private:

    int TakeVideo(
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

    int TakePhoto(
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

    int GetURI(
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

    void OnCameraArrivalRemoval();

    const std::string m_cameraId;
    bool m_hasStartedReporting{};

    std::mutex m_cameraDeviceLock;
    std::unique_ptr<CameraIotPnpDevice> m_cameraDevice;
    std::unique_ptr<CameraPnpDiscovery> m_cameraDiscovery;
    IOTHUB_DEVICE_CLIENT_HANDLE m_deviceClient;
    std::string m_componentName;
};