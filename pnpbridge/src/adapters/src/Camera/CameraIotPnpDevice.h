// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include "pch.h"
#include <windows.foundation.h>
#include <windows.storage.h>
#include <windows.storage.fileproperties.h>
#include "CameraMediaCapture.h"

// TODO: mix uses of char* and string

class CameraIotPnpDevice
{
public:
    CameraIotPnpDevice(std::wstring& uniqueId, std::string& componentName);
    virtual ~CameraIotPnpDevice();

    virtual HRESULT             Initialize();
    virtual HRESULT             StartTelemetryWorker();
    virtual HRESULT             StopTelemetryWorker();
    virtual void                Shutdown();

    virtual HRESULT             ReportProperty(_In_ bool readOnly,
                                               _In_ const char* propertyName,
                                               _In_ unsigned const char* propertyData,
                                               _In_ size_t propertyDataLen);
    virtual HRESULT             ReportTelemetry(_In_z_ const char* telemetryName,
                                                _In_reads_bytes_(messageDataLen) const unsigned char* messageData,
                                                _In_ size_t messageDataLen);
    virtual HRESULT             ReportTelemetry(_In_ std::string& telemetryName,
                                                _In_ std::string& message);

    virtual HRESULT             LoopTelemetry(_In_ DWORD dwMilliseconds = 15000);
    virtual HRESULT             TakePhotoOp(_Out_ std::string& strResponse);
    virtual HRESULT             TakeVideoOp(_In_ DWORD dwMilliseconds, _Out_ std::string& strResponse);
    virtual HRESULT             StartDetection();
    virtual HRESULT             GetURIOp(_Out_ std::string& strResponse);

    void                        SetIotHubDeviceClientHandle(IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle);

    static void __cdecl         CameraIotPnpDevice_PropertyCallback(_In_ int pnpReportedStatus, _In_opt_ void* userContextCallback);
    static void __cdecl         CameraIotPnpDevice_TelemetryCallback(_In_ IOTHUB_CLIENT_CONFIRMATION_RESULT pnpTelemetryStatus, _In_opt_ void* userContextCallback);
    static void __cdecl         CameraIotPnpDevice_BlobUploadCallback(IOTHUB_CLIENT_FILE_UPLOAD_RESULT result, void* userContextCallback);

    HRESULT                     UploadStorageFileToBlob(ABI::Windows::Storage::IStorageFile* pStorageFile);
protected:
    static DWORD WINAPI         TelemetryWorkerThreadProc(_In_opt_ PVOID pv);

    // TODO: use Azure C PNP SDK threads and concurrency
    CritSec                                     m_lock;
    HANDLE                                      m_hWorkerThread;
    HANDLE                                      m_hShutdownEvent;
    std::unique_ptr<CameraStatConsumer>         m_cameraStats;

    std::wstring                                m_deviceName;
    std::unique_ptr<CameraMediaCapture>         m_spCameraMediaCapture;
    IOTHUB_DEVICE_CLIENT_HANDLE                 m_deviceClient;
    std::string                                 m_componentName;
};

