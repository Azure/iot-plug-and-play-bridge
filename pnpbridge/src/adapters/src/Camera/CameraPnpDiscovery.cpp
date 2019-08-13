// Copyright (c) Microsoft Corporation.  All rights reserved.
//
#include "pch.h"
#include "CameraPnpDiscovery.h"

// Cameras under Windows can be registered in multiple categories.
// These three interface categories are generally treated as "webcam".
// We need to check all three since some cameras may show up under
// a subset of these categories.  We also need to make sure we
// remove duplicates as a camera registered under multiple categories
// will trigger our device watcher for each of the categories we're
// monitoring.

static GUID s_CameraInterfaceCategories[] = {
    KSCATEGORY_VIDEO_CAMERA,
    KSCATEGORY_SENSOR_CAMERA
};

CameraPnpDiscovery::CameraPnpDiscovery(
    )
:   m_fShutdown(false)
,   m_pfnCallback(nullptr)
{

}

CameraPnpDiscovery::~CameraPnpDiscovery(
    )
{
    Shutdown();
}

HRESULT 
CameraPnpDiscovery::InitializePnpDiscovery(
    _In_ PNPBRIDGE_NOTIFY_CHANGE pfnCallback,
    _In_ PNPMEMORY deviceArgs,
    _In_ PNPMEMORY adapterArgs
    )
{
    bool fInvokeCallback = false;

    {
        AutoLock lock(&m_lock);

        RETURN_IF_FAILED (CheckShutdown());

        // For now, we don't use these parameters.
        UNREFERENCED_PARAMETER (deviceArgs);
        UNREFERENCED_PARAMETER (adapterArgs);

        RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), m_vecWatcherHandles.size() != 0);

        for (ULONG i = 0; i < _countof(s_CameraInterfaceCategories); i++)
        {
            CONFIGRET           cr = CR_SUCCESS;
            CM_NOTIFY_FILTER    cmFilter = { };
            HCMNOTIFICATION     hNotification = nullptr;

            cmFilter.cbSize                         = sizeof(cmFilter);
            cmFilter.Flags                          = 0;
            cmFilter.FilterType                     = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
            cmFilter.Reserved                       = 0;
            cmFilter.u.DeviceInterface.ClassGuid    = s_CameraInterfaceCategories[i];

            cr = CM_Register_Notification(&cmFilter, (PVOID)this, CameraPnpDiscovery::PcmNotifyCallback, &hNotification);
            RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);
            m_vecWatcherHandles.push_back(hNotification);
        }

        // Once we've registered for the callbacks, go ahead and enumerate
        m_cameraUniqueIds.clear();
        for (ULONG i = 0; i < _countof(s_CameraInterfaceCategories); i++)
        {
            RETURN_IF_FAILED (EnumerateCameras(s_CameraInterfaceCategories[i]));
        }

        m_pfnCallback = pfnCallback;
        fInvokeCallback = (m_cameraUniqueIds.size() > 0);
    }

    if (fInvokeCallback && pfnCallback != nullptr)
    {
        std::unique_ptr<JsonWrapper>    pjson = std::make_unique<JsonWrapper>();
        std::unique_ptr<JsonWrapper>    pmatchjson = std::make_unique<JsonWrapper>();
        PNPMESSAGE payload = NULL;
        PNPMESSAGE_PROPERTIES* props = NULL;

        RETURN_IF_FAILED (pjson->Initialize());
        RETURN_IF_FAILED (pjson->AddFormatString("identity", "camera-health-monitor"));
        RETURN_IF_FAILED (pmatchjson->Initialize());
        RETURN_IF_FAILED (pmatchjson->AddFormatString("hardware_id", "UVC_Webcam_00"));
        RETURN_IF_FAILED (pjson->AddObject("match_parameters", pmatchjson->GetMessageW()));

        PnpMessage_CreateMessage(&payload);
        PnpMessage_SetMessage(payload, pjson->GetMessageW());
        props = PnpMessage_AccessProperties(payload);

        props->Context = this;
        props->ChangeType = PNPMESSAGE_CHANGE_ARRIVAL;


        // We didn't have a camera, and we just got one (or more).
        pfnCallback(payload);

        // $$LEAKLEAK!!!
        pmatchjson->Detach();
        pjson->Detach();
        PnpMemory_ReleaseReference(payload);
    }

    return S_OK;
}

void
CameraPnpDiscovery::Shutdown(
    )
{
    std::vector<HCMNOTIFICATION> vecWatcherHandles;

    {
        AutoLock lock(&m_lock);

        if (FAILED(CheckShutdown()))
        {
            // We're already shutdown...
            return;
        }

        m_fShutdown = true;
        vecWatcherHandles = std::move(m_vecWatcherHandles);
    }

    for (std::vector<HCMNOTIFICATION>::iterator itr = vecWatcherHandles.begin(); itr != vecWatcherHandles.end(); itr++)
    {
        (void) CM_Unregister_Notification((*itr));
    }
}

HRESULT 
CameraPnpDiscovery::GetFirstCamera(
    _Out_ std::wstring& cameraName
    )
{
    AutoLock lock(&m_lock);

    RETURN_IF_FAILED (CheckShutdown());
    RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_NOT_FOUND), m_cameraUniqueIds.size() == 0);

    cameraName = m_cameraUniqueIds.front()->m_UniqueId;

    return S_OK;
}

/// Protected methods...
HRESULT
CameraPnpDiscovery::EnumerateCameras(
    _In_ REFGUID guidCategory
    )
{
    CONFIGRET                   cr = CR_SUCCESS;
    std::unique_ptr<WCHAR[]>    pwzInterfaceList;
    ULONG                       cchInterfaceList = 0;
    LPWSTR                      pwz = nullptr;
    ULONG                       cch = 0;

    // Do this in a loop, there's a very small chance that after the CM_Get_Device_Interface_List_Size
    // call but before we call CM_Get_Device_Interface_List, we can get a new SG published.
    do
    {
        cchInterfaceList = 0;
        cr = CM_Get_Device_Interface_List_Size(&cchInterfaceList, (GUID*)&guidCategory, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (cr != CR_SUCCESS || cchInterfaceList == 0)
        {
            // No Sensor Groups available, this is not an error, it just makes this
            // test a no-op.  Leave.
            return S_OK;
        }

        pwzInterfaceList = std::make_unique<WCHAR[]>(cchInterfaceList);
        cr = CM_Get_Device_Interface_List((GUID*)&guidCategory, nullptr, pwzInterfaceList.get(), cchInterfaceList, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (cr != CR_SUCCESS)
        {
            pwzInterfaceList = nullptr;
        }
    }
    while (cr == CR_BUFFER_SMALL);

    // This should never be null...
    RETURN_HR_IF_NULL (E_UNEXPECTED, pwzInterfaceList.get());
    if (cchInterfaceList <= 1)
    {
        // This is empty...leave.
        return S_OK;
    }

    pwz = pwzInterfaceList.get();
    while (*pwz != L'\0' && cch < cchInterfaceList)
    {
        bool fDupe = false;
        CameraUniqueIdPtr uniqueId;

        uniqueId = std::make_unique<CameraUniqueId>();
        RETURN_IF_FAILED (CameraPnpDiscovery::MakeUniqueId(pwz, uniqueId->m_UniqueId, uniqueId->m_HWId));

        // Filter out duplicates.
        for (CameraUniqueIdPtrList::iterator itr = m_cameraUniqueIds.begin(); itr != m_cameraUniqueIds.end(); itr++)
        {
            if (_wcsicmp((*itr)->m_UniqueId.c_str(), uniqueId->m_UniqueId.c_str()) == 0)
            {
                // Dupe.
                fDupe = true;
                break;
            }
        }

        if (!fDupe)
        {
            m_cameraUniqueIds.push_back(std::move(uniqueId));
        }

        while (*pwz != L'\0')
        {
            cch++;
            pwz++;
        }
        if (*pwz == L'\0' && cch < cchInterfaceList)
        {
            cch++;
            pwz++;
        }
    }

    return S_OK;
}

HRESULT 
CameraPnpDiscovery::OnWatcherNotification(
    _In_ CM_NOTIFY_ACTION action,
    _In_reads_bytes_(eventDataSize) PCM_NOTIFY_EVENT_DATA eventData,
    _In_ DWORD eventDataSize
    )
{
    CameraUniqueIdPtrList           cameraUniqueIds;
    PNPBRIDGE_NOTIFY_CHANGE  pfn = nullptr;

    AZURE_UNREFERENCED_PARAMETER(action);

    // EventData may be empty.  This can happen if we're forcing ourselves
    // to check the Windows PnP state since we may have been started when
    // the devices we care about were already installed.
    if ((eventData == nullptr && eventDataSize != 0) ||
        (eventData != nullptr && eventDataSize == 0))
    {
        RETURN_IF_FAILED (E_INVALIDARG);
    }

    {
        AutoLock                lock(&m_lock);

        RETURN_IF_FAILED (CheckShutdown());

        pfn = m_pfnCallback;

        // Steal the old list, so the EnumerateCameras can rebuild
        // the new list.  This will allow us to compare the two
        // lists to determine which camera is new, which camera
        // was removed and which camera got "replaced".
        cameraUniqueIds = std::move(m_cameraUniqueIds);
        for (ULONG i = 0; i < _countof(s_CameraInterfaceCategories); i++)
        {
            RETURN_IF_FAILED (EnumerateCameras(s_CameraInterfaceCategories[i]));
        }
    }

    // $$WARNING!!!
    // There's an inherent race condition between the lock above and the
    // callback function here.  We could get the StopDiscovery while we're
    // trying to invoke the callback.  The DA (DeviceAggregator) must 
    // handle this scenario correctly.  For now, since everything is statically
    // linked, DA can just flip a flag to indicate once StopDiscovery is called
    // all subsequent callbacks are just no-op.  But once we move to a full
    // DLL model, life time of that DLL must be managed by the consumer
    // component (i.e., not DA).
    if (pfn)
    {
        PNPMESSAGE payload = nullptr;
        PNPMESSAGE_PROPERTIES* props = nullptr;
        std::unique_ptr<JsonWrapper>    pjson = std::make_unique<JsonWrapper>();

        RETURN_IF_FAILED (pjson->Initialize());
        RETURN_IF_FAILED (pjson->AddFormatString("HardwareId", "UVC_Webcam_00"));

        PnpMessage_CreateMessage(&payload);
        PnpMessage_SetMessage(payload, pjson->GetMessageW());
        props = PnpMessage_AccessProperties(payload);
        props->Context = this;

        
        if (cameraUniqueIds.size() == 0 && m_cameraUniqueIds.size() == 0)
        {
            // We have no camera, we may have been called as a part of
            // our initialization without any camera installed.  This is
            // a no-op.
            return S_OK;
        }
        else if (cameraUniqueIds.size() == 0 && m_cameraUniqueIds.size() > 0)
        {
            // We didn't have a camera, and we just got one (or more).
            props->ChangeType = PNPMESSAGE_CHANGE_ARRIVAL;
            pfn(payload);
        }
        else if (cameraUniqueIds.size() > 0 && m_cameraUniqueIds.size() == 0)
        {
            // We had a camera, and now we don't.
            props->ChangeType = PNPMESSAGE_CHANGE_REMOVAL;
            pfn(payload);
        }
        else
        {
            if (_wcsicmp(cameraUniqueIds.front()->m_UniqueId.c_str(), m_cameraUniqueIds.front()->m_UniqueId.c_str()) != 0)
            {
                // We lost our old camera, and got a new one.  Remove and
                // start over.
                props->ChangeType = PNPMESSAGE_CHANGE_REMOVAL;
                pfn(payload);

                props->ChangeType = PNPMESSAGE_CHANGE_ARRIVAL;
                pfn(payload);
            }
        }
    }

    return S_OK;
}

HRESULT
CameraPnpDiscovery::GetDeviceProperty(
    _In_z_ LPCWSTR deviceName,
    _In_ const DEVPROPKEY* pKey,
    _Out_writes_bytes_to_(cbBuffer,*pcbData) PBYTE pbBuffer,
    _In_ ULONG cbBuffer,
    _Out_ ULONG* pcbData
    )
{
    CONFIGRET       cr = CR_SUCCESS;
    wchar_t         wz[MAX_DEVICE_ID_LEN] = { 0 };
    ULONG           cb = sizeof(wz);
    DEVPROPTYPE     propType;
    DEVINST         hDevInstance = 0;

    RETURN_HR_IF_NULL (E_INVALIDARG, deviceName);
    RETURN_HR_IF_NULL (E_POINTER, pcbData);
    *pcbData = 0;

    cr = CM_Get_Device_Interface_Property(deviceName,
                                          &DEVPKEY_Device_InstanceId,
                                          &propType,
                                          (PBYTE)wz,
                                          &cb,
                                          0);
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);
    cr = CM_Locate_DevNode(&hDevInstance, wz, CM_LOCATE_DEVNODE_NORMAL);
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);
    cb = 0;
    cr = CM_Get_DevNode_Property(hDevInstance, pKey, &propType, nullptr, &cb, 0);
    if (cr == CR_BUFFER_SMALL)
    {
        *pcbData = cb;
        if (pbBuffer == nullptr || cbBuffer == 0)
        {
            // If the in param for the buffer, we're just asking for the
            // size, so return S_OK along with the size required.
            return S_OK;
        }

        // Otherwise, if the input buffer is not large enough, return an
        // error.
        RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), cb > cbBuffer);
        cb = cbBuffer;
        cr = CM_Get_DevNode_Property(hDevInstance, pKey, &propType, pbBuffer, &cb, 0);
    }
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);

    return S_OK;
}

HRESULT
CameraPnpDiscovery::GetDeviceInterfaceProperty(
    _In_z_ LPCWSTR deviceName,
    _In_ const DEVPROPKEY* pKey,
    _Out_writes_bytes_to_(cbBuffer,*pcbData) PBYTE pbBuffer,
    _In_ ULONG cbBuffer,
    _Out_ ULONG* pcbData
    )
{
    CONFIGRET       cr = CR_SUCCESS;
    DEVPROPTYPE     propType = 0;
    ULONG           cb = 0;

    RETURN_HR_IF_NULL (E_INVALIDARG, deviceName);
    RETURN_HR_IF_NULL (E_POINTER, pcbData);
    *pcbData = 0;

    cr = CM_Get_Device_Interface_Property(deviceName, pKey, &propType, nullptr, &cb, 0);
    if (cr == CR_BUFFER_SMALL)
    {
        *pcbData = cb;
        if (pbBuffer == nullptr || cbBuffer == 0)
        {
            // If the in param for the buffer, we're just asking for the
            // size, so return S_OK along with the size required.
            return S_OK;
        }

        // Otherwise, if the input buffer is not large enough, return an
        // error.
        RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), cb > cbBuffer);
        cb = cbBuffer;
        cr = CM_Get_Device_Interface_Property(deviceName, pKey, &propType, (PBYTE)pbBuffer, &cb, 0);
    }
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);

    return S_OK;
}

bool
CameraPnpDiscovery::IsDeviceInterfaceEnabled(
    _In_z_ LPCWSTR deviceName
    )
{
    CONFIGRET       cr = CR_SUCCESS;
    DEVPROPTYPE     propType = 0;
    DEVPROP_BOOLEAN fdpEnabled = DEVPROP_FALSE;
    ULONG           cb = sizeof(fdpEnabled);

    // Empty string, it's disabled.
    if (nullptr == deviceName || *deviceName == L'\0')
    {
        return false;
    }

    cr = CM_Get_Device_Interface_Property(deviceName, &DEVPKEY_DeviceInterface_Enabled, &propType, (PBYTE)&fdpEnabled, &cb, 0);
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);

    return (fdpEnabled == DEVPROP_TRUE);
}

HRESULT 
CameraPnpDiscovery::MakeUniqueId(
    _In_z_ LPCWSTR deviceName, 
    _Inout_ std::wstring& uniqueId, 
    _Inout_ std::wstring& hwId
    )
{
    bool fFound = false;
    std::unique_ptr<BYTE[]> pbHWId;
    ULONG cb = 0;

    RETURN_HR_IF_NULL (E_INVALIDARG, deviceName);

    // Reset the ID.
    uniqueId.clear();
    hwId.clear();

    for (int i = 0; i < _countof(s_CameraInterfaceCategories); i++)
    {
        CONFIGRET   cr;
        WCHAR       wzAlias[MAX_PATH] = { 0 };
        ULONG       cch = 0;

        cch = ARRAYSIZE(wzAlias);
        cr = CM_Get_Device_Interface_Alias(deviceName,
                                           &s_CameraInterfaceCategories[i],
                                           wzAlias,
                                           &cch,
                                           0);
        if (cr == CR_SUCCESS && IsDeviceInterfaceEnabled(wzAlias))
        {
            uniqueId = wzAlias;
            fFound = true;
            break;
        }
    }

    // If we haven't found it, then this isn't a camera, so
    // leave.
    RETURN_HR_IF (E_INVALIDARG, uniqueId.empty());
    RETURN_IF_FAILED (CameraPnpDiscovery::GetDeviceProperty(uniqueId.c_str(),
                                                            &DEVPKEY_Device_HardwareIds,
                                                            nullptr,
                                                            0,
                                                            &cb));
    pbHWId = std::make_unique<BYTE[]>(cb);
    RETURN_IF_FAILED (CameraPnpDiscovery::GetDeviceProperty(uniqueId.c_str(),
                                                            &DEVPKEY_Device_HardwareIds,
                                                            pbHWId.get(),
                                                            cb,
                                                            &cb));
    // HWID has the "most unique" ID as the first of the null terminated
    // multi-string.  So we can just take that and assign it to our
    // HWID.
    hwId = (LPCWSTR)pbHWId.get();

    return S_OK;
}

DWORD 
CameraPnpDiscovery::PcmNotifyCallback(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ PVOID Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize
    )
{
    AZURE_UNREFERENCED_PARAMETER(hNotify);

    if (Context != nullptr)
    {
        ((CameraPnpDiscovery*)Context)->OnWatcherNotification(Action, EventData, EventDataSize);
    }

    // Must always return this since we don't want to "cancel" a device add/remove.
    return ERROR_SUCCESS;
}
