//*@@@+++@@@@*******************************************************************
//
// Microsoft Windows Media
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@*******************************************************************
#pragma once

#include "pch.h"

class CameraUniqueId
{
public:
    std::wstring        m_UniqueId;
    std::wstring        m_HWId;
};

typedef std::unique_ptr<CameraUniqueId>         CameraUniqueIdPtr;
typedef std::list<CameraUniqueIdPtr>            CameraUniqueIdPtrList;

class CameraPnpDiscovery
{
public:
    CameraPnpDiscovery();
    virtual ~CameraPnpDiscovery();

    /// Public methods.
    HRESULT                             InitializePnpDiscovery(_In_ PNPBRIDGE_NOTIFY_DEVICE_CHANGE pfnCallback, _In_ void* pvContext);
    void                                Shutdown();
    HRESULT                             GetFirstCamera(_Out_ std::wstring& cameraName);

    // Some static helpers.
    static HRESULT                      MakeUniqueId(_In_z_ LPCWSTR deviceName, _Inout_ std::wstring& uniqueId, _Inout_ std::wstring& hwId);

protected:
    HRESULT                             EnumerateCameras(_In_ REFGUID guidCategory);
    virtual HRESULT                     OnWatcherNotification(_In_ CM_NOTIFY_ACTION action,
                                                              _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
                                                              _In_ DWORD eventDataSize);
    __inline HRESULT                    CheckShutdown() const { return (m_fShutdown ? HRESULT_FROM_WIN32(ERROR_SYSTEM_SHUTDOWN) : S_OK); }

    static HRESULT                      GetDeviceProperty(_In_z_ LPCWSTR deviceName,
                                                          _In_ const DEVPROPKEY* pKey,
                                                          _Out_writes_bytes_to_(cbBuffer,*pcbData) PBYTE pbBuffer,
                                                          _In_ ULONG cbBuffer,
                                                          _Out_ ULONG* pcbData);
    static HRESULT                      GetDeviceInterfaceProperty(_In_z_ LPCWSTR deviceName,
                                                                   _In_ const DEVPROPKEY* pKey,
                                                                   _Out_writes_bytes_to_(cbBuffer,*pcbData) PBYTE pbBuffer,
                                                                   _In_ ULONG cbBuffer,
                                                                   _Out_ ULONG* pcbData);
    static DWORD CALLBACK               PcmNotifyCallback(_In_ HCMNOTIFICATION hNotify,
                                                          _In_opt_ PVOID context,
                                                          _In_ CM_NOTIFY_ACTION action,
                                                          _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
                                                          _In_ DWORD eventDataSize);
    static bool                         IsDeviceInterfaceEnabled(_In_z_ LPCWSTR deviceName);




    CritSec                             m_lock;
    CameraUniqueIdPtrList               m_cameraUniqueIds;
    bool                                m_fShutdown;

    std::vector<HCMNOTIFICATION>        m_vecWatcherHandles;
    PNPBRIDGE_NOTIFY_DEVICE_CHANGE      m_pfnCallback;
    void*                               m_pvCallbackContext;
};

