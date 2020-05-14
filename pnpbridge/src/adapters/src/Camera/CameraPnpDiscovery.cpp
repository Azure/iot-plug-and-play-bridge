// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "pch.h"
#include "CameraPnpDiscovery.h"
#include "util.h"

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

CameraPnpDiscovery::CameraPnpDiscovery()
    : m_fShutdown(false)
{

}

CameraPnpDiscovery::~CameraPnpDiscovery()
{
    Shutdown();
}

HRESULT CameraPnpDiscovery::InitializePnpDiscovery() try
{
    {
        AutoLock lock(&m_lock);

        RETURN_IF_FAILED(CheckShutdown());

        for (ULONG i = 0; i < _countof(s_CameraInterfaceCategories); i++)
        {
            RETURN_IF_FAILED(EnumerateCameras(s_CameraInterfaceCategories[i]));
        }
    }

    return S_OK;
}
catch (std::exception e)
{
    LogInfo("Exception occurred in camera discovery adapter's StartDiscovery. Continuing without starting up the adapter (%s)", e.what());
    return S_OK;
}

void CameraPnpDiscovery::Shutdown() try
{
    AutoLock lock(&m_lock);

    for (const auto& notificationHandle : m_onCameraArrivalRemovalHandles)
    {
        CM_Unregister_Notification(notificationHandle);
    }

    if (SUCCEEDED(CheckShutdown()))
    {
        m_fShutdown = true;
    }
}
catch (std::exception e)
{
    LogInfo("Exception occurred while shutting down the camera discovery adapter (%s)", e.what());
    return;
}
catch (std::exception e)
{
    LogInfo("Exception occurred in camera discovery adapter's StartDiscovery. Continuing without starting up the adapter (%s)", e.what());
    return S_OK;
}

HRESULT CameraPnpDiscovery::GetUniqueIdByCameraId(_In_ const std::string& cameraId, _Out_ std::wstring& uniqueId) try
{
    AutoLock lock(&m_lock);
    RETURN_IF_FAILED(CheckShutdown());
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), m_cameraUniqueIds.size() == 0);

    uniqueId = L"";
    for (auto iter = m_cameraUniqueIds.begin(); iter != m_cameraUniqueIds.end(); iter++)
    {
        std::string cameraIdStr = wstr2str((*iter)->m_cameraId);
        if (cameraIdStr.find(cameraId.c_str()) != std::string::npos)
        {
            uniqueId = (*iter)->m_UniqueId;
        }
    }

    // No camera in our list matches the input camera Id
    if (uniqueId == L"")
    {
        return E_NOINTERFACE;
    }
    else
    {
        return S_OK;
    }
} 
catch (std::exception e)
{
    LogInfo("Exception occurred while shutting down the camera discovery adapter (%s)", e.what());
    return;
}
catch (std::exception e)
{
    LogInfo("Exception occurred getting camera ID (%s)", e.what());
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
catch (std::exception e)
{
    LogInfo("Exception occurred getting camera ID (%s)", e.what());
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

HRESULT CameraPnpDiscovery::GetUniqueIdByCameraId(_In_ std::string& cameraId, _Out_ std::wstring& uniqueId) try
{
    AutoLock lock(&m_lock);
    RETURN_IF_FAILED(CheckShutdown());
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), m_cameraUniqueIds.size() == 0);

    uniqueId = L"";
    for (auto iter = m_cameraUniqueIds.begin(); iter != m_cameraUniqueIds.end(); iter++)
    {
        std::string cameraIdStr = wstr2str((*iter)->m_cameraId);
        if (cameraIdStr == cameraId.c_str())
        {
            uniqueId = (*iter)->m_UniqueId;
        }
    }
    
    // no camera in our list matches the input camera Id
    if (uniqueId == L"")
    {
        return E_FAIL;
    }
    else 
    {
        return S_OK;
    }
}
catch (std::exception e)
{
    LogInfo("Exception occurred getting camera ID (%s)", e.what());
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}



/// Protected methods...
HRESULT CameraPnpDiscovery::EnumerateCameras(_In_ REFGUID guidCategory) try
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
            // No sensor groups available, this is not an error, it just makes this
            // test a no-op, leave
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

    // This should never be null
    RETURN_HR_IF_NULL (E_UNEXPECTED, pwzInterfaceList.get());
    if (cchInterfaceList <= 1)
    {
        // This is empty, leave
        return S_OK;
    }

    pwz = pwzInterfaceList.get();
    while (*pwz != L'\0' && cch < cchInterfaceList)
    {
        bool fDupe = false;
        CameraUniqueIdPtr uniqueId;

        uniqueId = std::make_unique<CameraUniqueId>();
        std::wstring deviceName(pwz);
        RETURN_IF_FAILED (CameraPnpDiscovery::MakeUniqueId(deviceName, uniqueId->m_UniqueId, uniqueId->m_cameraId));

        // Filter out duplicates
        for (CameraUniqueIdPtrList::iterator itr = m_cameraUniqueIds.begin(); itr != m_cameraUniqueIds.end(); itr++)
        {
            if (_wcsicmp((*itr)->m_UniqueId.c_str(), uniqueId->m_UniqueId.c_str()) == 0)
            {
                // Dupe
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
catch (std::exception e)
{
    LogInfo("Exception occurred enumerating cameras on device (%s)", e.what());
    return E_UNEXPECTED;
}
catch (std::exception e)
{
    LogInfo("Exception occurred enumerating cameras on device (%s)", e.what());
    return E_UNEXPECTED;
}

HRESULT CameraPnpDiscovery::RegisterForCameraArrivalRemovals(
    std::function<void()> onCameraArrivalRemoval)
{
    m_onCameraArrivalRemoval = onCameraArrivalRemoval;

    for (const auto& interfaceCategory : s_CameraInterfaceCategories)
    {
        CM_NOTIFY_FILTER cmFilter{};
        cmFilter.cbSize = sizeof(cmFilter);
        cmFilter.Flags = 0;
        cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        cmFilter.Reserved = 0;
        cmFilter.u.DeviceInterface.ClassGuid = interfaceCategory;

        HCMNOTIFICATION notificationHandle;
        CONFIGRET cr = CM_Register_Notification(
            &cmFilter,
            this,
            CameraPnpDiscovery::PcmNotifyCallback,
            &notificationHandle);
        RETURN_HR_IF(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);
        m_onCameraArrivalRemovalHandles.push_back(notificationHandle);
    }

    return S_OK;
}

DWORD CameraPnpDiscovery::PcmNotifyCallback(
    _In_ HCMNOTIFICATION /* hNotify */,
    _In_opt_ PVOID Context,
    _In_ CM_NOTIFY_ACTION /* Action */,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA /* EventData */,
    _In_ DWORD /* EventDataSize */)
{
    ((CameraPnpDiscovery*)Context)->OnCameraArrivalRemoval();

    // Must always return this since we don't want to "cancel" a device add/remove
    return ERROR_SUCCESS;
}

void CameraPnpDiscovery::OnCameraArrivalRemoval()
{
    // Refresh the camera list since an arrival or removal has occurred
    for (ULONG i = 0; i < _countof(s_CameraInterfaceCategories); i++)
    {
        if (EnumerateCameras(s_CameraInterfaceCategories[i]) != S_OK)
        {
            return;
        }
    }

    // Notify client
    if (m_onCameraArrivalRemoval)
    {
        m_onCameraArrivalRemoval();
    }
}
catch (std::exception e)
{
    LogInfo("Exception occurred processing camera event (%s)", e.what());
    return E_UNEXPECTED;
}

HRESULT CameraPnpDiscovery::GetDeviceProperty(
    _In_z_ LPCWSTR deviceName,
    _In_ const DEVPROPKEY* pKey,
    _Out_writes_bytes_to_(cbBuffer,*pcbData) PBYTE pbBuffer,
    _In_ ULONG cbBuffer,
    _Out_ ULONG* pcbData)
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
            // size, so return S_OK along with the size required
            return S_OK;
        }

        // Otherwise, if the input buffer is not large enough, return an
        // error
        RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), cb > cbBuffer);
        cb = cbBuffer;
        cr = CM_Get_DevNode_Property(hDevInstance, pKey, &propType, pbBuffer, &cb, 0);
    }
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);

    return S_OK;
}

HRESULT CameraPnpDiscovery::GetDeviceInterfaceProperty(
    _In_z_ LPCWSTR deviceName,
    _In_ const DEVPROPKEY* pKey,
    _Out_writes_bytes_to_(cbBuffer,*pcbData) PBYTE pbBuffer,
    _In_ ULONG cbBuffer,
    _Out_ ULONG* pcbData)
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
            // size, so return S_OK along with the size required
            return S_OK;
        }

        // Otherwise, if the input buffer is not large enough, return an
        // error
        RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), cb > cbBuffer);
        cb = cbBuffer;
        cr = CM_Get_Device_Interface_Property(deviceName, pKey, &propType, (PBYTE)pbBuffer, &cb, 0);
    }
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);

    return S_OK;
}

bool CameraPnpDiscovery::IsDeviceInterfaceEnabled(_In_z_ LPCWSTR deviceName)
{
    CONFIGRET       cr = CR_SUCCESS;
    DEVPROPTYPE     propType = 0;
    DEVPROP_BOOLEAN fdpEnabled = DEVPROP_FALSE;
    ULONG           cb = sizeof(fdpEnabled);

    // Empty string, it's disabled
    if (nullptr == deviceName || *deviceName == L'\0')
    {
        return false;
    }

    cr = CM_Get_Device_Interface_Property(deviceName, &DEVPKEY_DeviceInterface_Enabled, &propType, (PBYTE)&fdpEnabled, &cb, 0);
    RETURN_HR_IF (HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), cr != CR_SUCCESS);

    return (fdpEnabled == DEVPROP_TRUE);
}

HRESULT CameraPnpDiscovery::MakeUniqueId(
    _In_    std::wstring& deviceName,
    _Inout_ std::wstring& uniqueId,
    _Inout_ std::wstring& cameraId) try
{
    bool fFound = false;
    std::unique_ptr<BYTE[]> pbCameraId;
    ULONG cb = 0;

    if (deviceName == L"")
    {
        return E_INVALIDARG;
    }

    // Reset the ID
    uniqueId.clear();
    cameraId.clear();

    for (int i = 0; i < _countof(s_CameraInterfaceCategories); i++)
    {
        CONFIGRET   cr;
        WCHAR       wzAlias[MAX_PATH] = { 0 };
        ULONG       cch = 0;

        cch = ARRAYSIZE(wzAlias);
        cr = CM_Get_Device_Interface_Alias(deviceName.c_str(),
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
    // leave
    RETURN_HR_IF (E_INVALIDARG, uniqueId.empty());

    // If it's not a IP camera, get the hardware ID via the propkey and use it as the camera ID.
    // If it is a IP camera, use the UUID as the camera ID.

    if (wcsstr(deviceName.c_str(), L"NetworkCamera") != NULL)
    {
        const wchar_t* pUUIDptr = wcsstr(deviceName.c_str(), L"uuid:");
        pbCameraId = std::make_unique<BYTE[]>(42 * sizeof(wchar_t)); // 42 = length of the string "uuid:<guid>"
        pbCameraId[41] = '\0';
        wcsncpy_s((wchar_t*)pbCameraId.get(), 42, pUUIDptr, 41);
    }
    else
    {

        RETURN_IF_FAILED(CameraPnpDiscovery::GetDeviceProperty(uniqueId.c_str(),
            &DEVPKEY_Device_HardwareIds,
            nullptr,
            0,
            &cb));
        pbCameraId = std::make_unique<BYTE[]>(cb);
        RETURN_IF_FAILED(CameraPnpDiscovery::GetDeviceProperty(uniqueId.c_str(),
            &DEVPKEY_Device_HardwareIds,
            pbCameraId.get(),
            cb,
            &cb));
    }
    // HWID has the "most unique" ID as the first of the null terminated
    // multi-string. So we can just take that and assign it to our
    // HWID
    cameraId = (LPCWSTR)pbCameraId.get();

    return S_OK;
}
catch (std::exception e)
{
    LogInfo("Exception (%s)", e.what());
    return E_UNEXPECTED;
}
