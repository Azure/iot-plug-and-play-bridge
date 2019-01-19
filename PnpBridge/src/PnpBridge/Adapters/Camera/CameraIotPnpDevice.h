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

    virtual HRESULT             Initialize(_In_ PNP_INTERFACE_CLIENT_HANDLE hPnpClientInterface, _In_opt_z_ LPCWSTR deviceName);
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
    virtual HRESULT             LoopTelemetry(_In_ DWORD dwMilliseconds = 5500);

    static void __cdecl         CameraIotPnpDevice_PropertyCallback(_In_ PNP_REPORTED_PROPERTY_STATUS pnpReportedStatus, _In_opt_ void* userContextCallback);
    static void __cdecl         CameraIotPnpDevice_TelemetryCallback(_In_ PNP_SEND_TELEMETRY_STATUS pnpTelemetryStatus, _In_opt_ void* userContextCallback);
    static HRESULT              JsonSetJsonValueAsString(_Inout_ JSON_Object* json_obj, _In_z_ LPCSTR name, _In_ UINT32 val);
    static HRESULT              JsonSetJsonValueAsString(_Inout_ JSON_Object* json_obj, _In_z_ LPCSTR name, _In_ LONGLONG val);
    static HRESULT              JsonSetJsonValueAsString(_Inout_ JSON_Object* json_obj, _In_z_ LPCSTR name, _In_ double val);
    static HRESULT              JsonSetJsonHresultAsString(_Inout_ JSON_Object* json_obj, _In_z_ LPCSTR name, _In_ HRESULT hr);

protected:
    static DWORD WINAPI                         TelemetryWorkerThreadProc(_In_opt_ PVOID pv);

    CritSec                                     m_lock;
    PNP_INTERFACE_CLIENT_HANDLE                 m_PnpClientInterface;
    HANDLE                                      m_hWorkerThread;
    HANDLE                                      m_hShutdownEvent;
    std::unique_ptr<CameraStatConsumer>         m_cameraStats;
};

