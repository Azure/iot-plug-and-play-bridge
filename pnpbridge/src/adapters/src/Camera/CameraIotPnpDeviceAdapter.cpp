// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"

#include "CameraIotPnpDeviceAdapter.h"
#include "NetworkCameraIotPnpDevice.h"
#include "CameraIotPnpDevice.h"

// static
std::unique_ptr<CameraIotPnpDeviceAdapter> CameraIotPnpDeviceAdapter::MakeUnique(
    const std::string& interfaceId,
    const std::string& componentName,
    const std::string& cameraId,
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE* interfaceClient)
{
    auto newCameraDevice = std::make_unique<CameraIotPnpDeviceAdapter>(
        cameraId);

    auto result = DigitalTwin_InterfaceClient_Create(
        interfaceId.c_str(),
        componentName.c_str(),
        nullptr,
        newCameraDevice.get(),
        interfaceClient);
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        throw std::exception("Failed to create digital twin interface");
    }

    result = DigitalTwin_InterfaceClient_SetCommandsCallback(
        *interfaceClient,
        CameraIotPnpDeviceAdapter::CameraPnpCallback_ProcessCommandUpdate,
        static_cast<void*>(newCameraDevice.get()));
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        throw std::exception("Failed to set digital twin command callback");
    }

    result = DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback(
        *interfaceClient,
        CameraIotPnpDeviceAdapter::CameraPnpCallback_ProcessPropertyUpdate,
        static_cast<void*>(newCameraDevice.get()));
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        throw std::exception("Failed to set digital twin property callback");
    }

    newCameraDevice->Initialize(*interfaceClient);
    return newCameraDevice;
}

CameraIotPnpDeviceAdapter::CameraIotPnpDeviceAdapter(
    const std::string& cameraId) :
    m_cameraId(cameraId)
{
}

void CameraIotPnpDeviceAdapter::Initialize(
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceClientHandle)
{
    m_interfaceClientHandle = interfaceClientHandle;

    m_cameraDiscovery = std::make_unique<CameraPnpDiscovery>();
    m_cameraDiscovery->InitializePnpDiscovery();
    auto result = m_cameraDiscovery->RegisterForCameraArrivalRemovals(
        std::bind(&CameraIotPnpDeviceAdapter::OnCameraArrivalRemoval, this));

    if (result != S_OK)
    {
        throw std::exception("Failed to register for camera arrivals.");
    }

    // Manually trigger camera arrival/removal once to immediately query for the camera device
    OnCameraArrivalRemoval();
}

void CameraIotPnpDeviceAdapter::OnCameraArrivalRemoval()
{
    std::wstring cameraName;
    std::lock_guard<std::mutex> lock(m_cameraDeviceLock);
    if (m_cameraDiscovery->GetUniqueIdByCameraId(m_cameraId, cameraName) == S_OK && !m_cameraDevice)
    {
        LogError("Camera device has arrived, starting telemetry");

        // For now, assume:
        //   - IP camera IDs begin with "uuid"
        //   - Other camera IDs begin with "USB\"..
        std::unique_ptr<CameraIotPnpDeviceAdapter> newCameraDevice;
        if (m_cameraId.rfind("uuid", 0) == 0)
        {
            m_cameraDevice = std::make_unique<NetworkCameraIotPnpDevice>(cameraName);
        }
        else
        {
            m_cameraDevice = std::make_unique<CameraIotPnpDevice>(cameraName);
        }

        if (m_cameraDevice->Initialize(m_interfaceClientHandle) == S_OK)
        {
            if (m_hasStartedReporting)
            {
                m_cameraDevice->StartTelemetryWorker();
            }
        }
        else
        {
            m_cameraDevice = nullptr;
        }
    }
    else if (m_cameraDiscovery->GetUniqueIdByCameraId(m_cameraId, cameraName) != S_OK && m_cameraDevice)
    {
        LogError("Camera device has been removed, stopping telemetry");

        m_cameraDevice->StopTelemetryWorker();
        m_cameraDevice->Shutdown();
        m_cameraDevice.reset();
    }
}

HRESULT CameraIotPnpDeviceAdapter::Start()
{
    m_hasStartedReporting = true;

    std::lock_guard<std::mutex> lock(m_cameraDeviceLock);
    if (m_cameraDevice)
    {
        return m_cameraDevice->StartTelemetryWorker();
    }
    return S_OK;
}

void CameraIotPnpDeviceAdapter::Stop()
{
    m_hasStartedReporting = false;

    m_cameraDiscovery->Shutdown();

    std::lock_guard<std::mutex> lock(m_cameraDeviceLock);
    if (m_cameraDevice)
    {
        m_cameraDevice->StopTelemetryWorker();
        m_cameraDevice->Shutdown();
    }
}

// static
void CameraIotPnpDeviceAdapter::CameraPnpCallback_ProcessCommandUpdate(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtCommandRequest,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse,
    void* userInterfaceContext)
{
    auto cameraDevice = static_cast<CameraIotPnpDeviceAdapter*>(userInterfaceContext);
    std::lock_guard<std::mutex> lock(cameraDevice->m_cameraDeviceLock);
    if (cameraDevice->m_cameraDevice)
    {
        if (strcmp(dtCommandRequest->commandName, "TakePhoto") == 0)
        {
            cameraDevice->TakePhoto(dtCommandResponse);
        }
        else if (strcmp(dtCommandRequest->commandName, "TakeVideo") == 0)
        {
            cameraDevice->TakeVideo(dtCommandResponse);
        }
        else if (strcmp(dtCommandRequest->commandName, "GetURI") == 0)
        {
            cameraDevice->GetURI(dtCommandResponse);
        }
        else
        {
            LogError("Unknown command request");
            dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_REQUEST_VERSION_1;
            dtCommandResponse->status = 500;
            dtCommandResponse->responseDataLen = 0;
        }
    }
    else
    {
        LogError("Camera device was not available to process command");
        dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_REQUEST_VERSION_1;
        dtCommandResponse->status = 500;
        dtCommandResponse->responseDataLen = 0;
    }
}

// static
void CameraIotPnpDeviceAdapter::CameraPnpCallback_ProcessPropertyUpdate(
    const DIGITALTWIN_CLIENT_PROPERTY_UPDATE* /* dtClientPropertyUpdate */,
    void* /* userContextCallback */)
{
    // no-op
}

void CameraIotPnpDeviceAdapter::TakePhoto(
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse)
{
    char* responseBuf{};
    std::string strResponse;
    dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
    dtCommandResponse->status =
        (S_OK == m_cameraDevice->TakePhotoOp(strResponse) ? 200 : 500);
    mallocAndStrcpy_s(&responseBuf, strResponse.c_str());
    dtCommandResponse->responseData = (unsigned char*)responseBuf;
    dtCommandResponse->responseDataLen = strlen(responseBuf);
    m_cameraDevice->StartTelemetryWorker();
}

void CameraIotPnpDeviceAdapter::TakeVideo(
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse)
{
    char* responseBuf{};
    std::string strResponse;
    dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
    dtCommandResponse->status =
        (S_OK == m_cameraDevice->TakeVideoOp(10000, strResponse) ? 200 : 500);
    mallocAndStrcpy_s(&responseBuf, strResponse.c_str());
    dtCommandResponse->responseData = (unsigned char*)responseBuf;
    dtCommandResponse->responseDataLen = strlen(responseBuf);
}

void CameraIotPnpDeviceAdapter::GetURI(
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse)
{
    char* responseBuf{};
    std::string strResponse;
    dtCommandResponse->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
    dtCommandResponse->status =
        (S_OK == m_cameraDevice->GetURIOp(strResponse) ? 200 : 500);
    mallocAndStrcpy_s(&responseBuf, strResponse.c_str());
    dtCommandResponse->responseData = (unsigned char*)responseBuf;
    dtCommandResponse->responseDataLen = strlen(responseBuf);
}
