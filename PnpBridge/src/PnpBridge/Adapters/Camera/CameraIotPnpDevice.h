//*@@@+++@@@@*******************************************************************
//
// Microsoft Windows Media
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@*******************************************************************
#pragma once

#include "pch.h"

class CameraIotPnpDevice
{
public:
    CameraIotPnpDevice();
    virtual ~CameraIotPnpDevice();

    virtual HRESULT             Initialize(_In_ PNP_INTERFACE_CLIENT_HANDLE hPnpClientInterface, _In_ PNP_DEVICE_CLIENT_HANDLE hPnpDeviceClient, _In_opt_z_ LPCWSTR deviceName);
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
    virtual HRESULT             LoopTelemetry(_In_ DWORD dwMilliseconds = 15000);
    virtual HRESULT             TakePhotoOp(_Out_ std::string& strResponse);

    static void __cdecl         CameraIotPnpDevice_PropertyCallback(_In_ PNP_REPORTED_PROPERTY_STATUS pnpReportedStatus, _In_opt_ void* userContextCallback);
    static void __cdecl         CameraIotPnpDevice_TelemetryCallback(_In_ PNP_SEND_TELEMETRY_STATUS pnpTelemetryStatus, _In_opt_ void* userContextCallback);
    static void __cdecl         CameraIotPnpDevice_BlobUploadCallback(IOTHUB_CLIENT_FILE_UPLOAD_RESULT result, void* userContextCallback);

protected:
    static DWORD WINAPI                         TelemetryWorkerThreadProc(_In_opt_ PVOID pv);

    HRESULT                                     WritePhotoToTemp(_In_reads_bytes_(cbJpeg) PBYTE pbJpeg, _In_ ULONG cbJpeg);

    CritSec                                     m_lock;
    PNP_INTERFACE_CLIENT_HANDLE                 m_PnpClientInterface;
    PNP_DEVICE_CLIENT_HANDLE                    m_PnpDeviceClient;
    HANDLE                                      m_hWorkerThread;
    HANDLE                                      m_hShutdownEvent;
    std::unique_ptr<CameraStatConsumer>         m_cameraStats;
};

