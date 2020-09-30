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

    HRESULT InitializePnpDiscovery();

    void Shutdown();

    HRESULT GetUniqueIdByCameraId(_In_ const std::string& cameraId, _Out_ std::wstring& uniqueId);

    static HRESULT MakeUniqueId(_In_ std::wstring& deviceName, _Inout_ std::wstring& uniqueId, _Inout_ std::wstring& hwId);

    static HRESULT GetDeviceProperty(
        _In_z_ LPCWSTR deviceName,
        _In_ const DEVPROPKEY* pKey,
        _Out_writes_bytes_to_(cbBuffer, *pcbData) PBYTE pbBuffer,
        _In_ ULONG cbBuffer,
        _Out_ ULONG* pcbData);

    static HRESULT GetDeviceInterfaceProperty(
        _In_z_ LPCWSTR deviceName,
        _In_ const DEVPROPKEY* pKey,
        _Out_writes_bytes_to_(cbBuffer, *pcbData) PBYTE pbBuffer,
        _In_ ULONG cbBuffer,
        _Out_ ULONG* pcbData);

    // Registers for camera arrival and removal events. It is expected for the client to call
    // GetUniqueIdByCameraId() when this event fires to requery the camera on the system.
    // Only one client can be registered at a time.
    HRESULT RegisterForCameraArrivalRemovals(std::function<void()> onCameraArrivalRemoval);

protected:
    static DWORD CALLBACK PcmNotifyCallback(
        _In_ HCMNOTIFICATION hNotify,
        _In_opt_ PVOID Context,
        _In_ CM_NOTIFY_ACTION Action,
        _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
        _In_ DWORD EventDataSize);

    void OnCameraArrivalRemoval();
    HRESULT EnumerateCameras(_In_ REFGUID guidCategory);
    __inline HRESULT CheckShutdown() const { return (m_fShutdown ? HRESULT_FROM_WIN32(ERROR_SYSTEM_SHUTDOWN) : S_OK); }

    static bool IsDeviceInterfaceEnabled(_In_z_ LPCWSTR deviceName);

    CritSec m_lock;
    CameraUniqueIdPtrList m_cameraUniqueIds;
    bool m_fShutdown;
    std::function<void()> m_onCameraArrivalRemoval;
    std::list<HCMNOTIFICATION> m_onCameraArrivalRemovalHandles;
};

