// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include "pch.h"

class CameraUniqueId
{
public:
    std::wstring        m_UniqueId;
    std::wstring        m_cameraId;
};

typedef std::unique_ptr<CameraUniqueId>         CameraUniqueIdPtr;
typedef std::list<CameraUniqueIdPtr>            CameraUniqueIdPtrList;

class CameraPnpDiscovery
{
public:
    CameraPnpDiscovery();
    virtual ~CameraPnpDiscovery();

    /// Public methods.
    HRESULT                             InitializePnpDiscovery(_In_ PNPBRIDGE_NOTIFY_CHANGE pfnCallback, _In_ PNPMEMORY deviceArgs, _In_ PNPMEMORY adapterArgs);
    void                                Shutdown();
    HRESULT                             GetFirstCamera(_Out_ std::wstring& cameraName);
    HRESULT                             GetUniqueIdByCameraId(_In_ std::string& cameraId, _Out_ std::wstring& uniqueId);
    
    // Some static helpers.
    static HRESULT                      MakeUniqueId(_In_ std::wstring& deviceName, _Inout_ std::wstring& uniqueId, _Inout_ std::wstring& hwId);
    static HRESULT                      GetDeviceProperty(_In_z_ LPCWSTR deviceName,
        _In_ const DEVPROPKEY* pKey,
        _Out_writes_bytes_to_(cbBuffer, *pcbData) PBYTE pbBuffer,
        _In_ ULONG cbBuffer,
        _Out_ ULONG* pcbData);
    static HRESULT                      GetDeviceInterfaceProperty(_In_z_ LPCWSTR deviceName,
        _In_ const DEVPROPKEY* pKey,
        _Out_writes_bytes_to_(cbBuffer, *pcbData) PBYTE pbBuffer,
        _In_ ULONG cbBuffer,
        _Out_ ULONG* pcbData);

protected:
    HRESULT                             EnumerateCameras(_In_ REFGUID guidCategory);
    virtual HRESULT                     OnWatcherNotification(_In_ CM_NOTIFY_ACTION action,
                                                              _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
                                                              _In_ DWORD eventDataSize);
    __inline HRESULT                    CheckShutdown() const { return (m_fShutdown ? HRESULT_FROM_WIN32(ERROR_SYSTEM_SHUTDOWN) : S_OK); }

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
    PNPBRIDGE_NOTIFY_CHANGE             m_pfnCallback;
};

