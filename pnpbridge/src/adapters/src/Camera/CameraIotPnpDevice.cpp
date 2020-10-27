// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "util.h"
#include <fstream>
#include "CameraIotPnpDevice.h"
#include <pnpbridge_common.h>
#include <pnpbridge.h>

#define ONE_SECOND_IN_100HNS        10000000    // 10,000,000 100hns in 1 second.
#define ONE_MILLISECOND_IN_100HNS   10000       // 10,000 100hns in 1 millisecond.

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Storage::FileProperties;

CameraIotPnpDevice::CameraIotPnpDevice(std::wstring& deviceName, std::string& componentName)
    : m_hWorkerThread(nullptr),
      m_hShutdownEvent(nullptr),
      m_deviceName(deviceName),
      m_componentName(componentName)
{
    // Alternatively, declare and initialize a CameraMediaCaptureFrameRreader 
    // instance to enable FrameReader scenarios
    m_spCameraMediaCapture = std::make_unique<CameraMediaCapture>();
    m_spCameraMediaCapture->InitMediaCapture(m_deviceName);
}

CameraIotPnpDevice::~CameraIotPnpDevice()
{
    Shutdown();
}

void CameraIotPnpDevice::SetIotHubDeviceClientHandle(IOTHUB_DEVICE_CLIENT_HANDLE DeviceClientHandle)
{
    m_deviceClient = DeviceClientHandle;
}

HRESULT CameraIotPnpDevice::Initialize() try
{

    // Can't initialize twice...
    RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_cameraStats.get() != nullptr);

    m_cameraStats = std::make_unique<CameraStatConsumer>();
    RETURN_IF_FAILED (m_cameraStats->Initialize(nullptr, GUID_NULL, m_deviceName.c_str()));
    RETURN_IF_FAILED (m_cameraStats->AddProvider(g_FrameServerEventProvider));

    return S_OK;
}
catch (std::exception e)
{
    LogInfo("Exception occurred initializing Camera (%s)", e.what());
    return E_UNEXPECTED;
}

HRESULT CameraIotPnpDevice::StartTelemetryWorker()
{
    AutoLock lock(&m_lock);

    RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), nullptr != m_hWorkerThread);

    m_hShutdownEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(GetLastError()), m_hShutdownEvent);
    m_hWorkerThread = CreateThread(nullptr, 0, CameraIotPnpDevice::TelemetryWorkerThreadProc, (PVOID)this, 0, nullptr);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(GetLastError()), m_hWorkerThread);

    return S_OK;
}

HRESULT CameraIotPnpDevice::StopTelemetryWorker()
{
    AutoLock lock(&m_lock);
    DWORD    dwWait = 0;

    if (nullptr == m_hWorkerThread || nullptr == m_hShutdownEvent)
    {
        // nothing to do.
        return S_OK;
    }
    SetEvent(m_hShutdownEvent);
    CloseHandle(m_hShutdownEvent);
    m_hShutdownEvent = nullptr;

    dwWait = WaitForSingleObject(m_hWorkerThread, INFINITE);
    switch (dwWait)
    {
    case WAIT_OBJECT_0:
        return S_OK;
    case WAIT_TIMEOUT: // How?
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(WAIT_TIMEOUT));
    case WAIT_ABANDONED:
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(ERROR_WAIT_NO_CHILDREN));
    case WAIT_FAILED:
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(GetLastError()));
    default:
        RETURN_IF_FAILED (E_UNEXPECTED);
    }
    CloseHandle(m_hWorkerThread);
    m_hWorkerThread = nullptr;

    return S_OK;
}

void CameraIotPnpDevice::Shutdown()
{
    (VOID) StopTelemetryWorker();

    if (m_cameraStats.get() != nullptr)
    {
        (VOID) m_cameraStats->Shutdown();
    }
}

HRESULT CameraIotPnpDevice::ReportProperty(
    _In_ bool readOnly,
    _In_ const char* propertyName,
    _In_ unsigned const char* propertyData,
    _In_ size_t propertyDataLen)
{
    AZURE_UNREFERENCED_PARAMETER(readOnly);
    AZURE_UNREFERENCED_PARAMETER(propertyDataLen);

    AutoLock lock(&m_lock);

    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    STRING_HANDLE jsonToSend = NULL;

    if ((jsonToSend = PnP_CreateReportedProperty(m_componentName.c_str(), propertyName, (const char*) propertyData)) == NULL)
    {
        LogError("Unable to build reported property response for propertyName=%s, propertyValue=%s", propertyName, propertyData);
    }
    else
    {
        const char* jsonToSendStr = STRING_c_str(jsonToSend);
        size_t jsonToSendStrLen = strlen(jsonToSendStr);

        if ((result = IoTHubDeviceClient_SendReportedState(m_deviceClient, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
            CameraIotPnpDevice_PropertyCallback, (void*)propertyName)) != IOTHUB_CLIENT_OK)
        {
            LogError("Camera Component: Unable to send reported state for property=%s, error=%d",
                                propertyName, result);
        }
        else
        {
            LogInfo("Camera Component: Sending device information property to IoTHub. propertyName=%s, propertyValue=%s",
                        propertyName, propertyData);
        }

        STRING_delete(jsonToSend);
    }

    return HResultFromPnpClient(result);
}

HRESULT CameraIotPnpDevice::ReportTelemetry(
    _In_z_ const char* telemetryName,
    _In_reads_bytes_(messageDataLen) const unsigned char* messageData,
    _In_ size_t messageDataLen)
{
    AZURE_UNREFERENCED_PARAMETER(messageDataLen);
    AutoLock lock(&m_lock);

    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

    char telemetryMessage[512] = { 0 };
    sprintf(telemetryMessage, "{\"%s\":%s}", telemetryName, messageData);

    if ((messageHandle = PnP_CreateTelemetryMessageHandle(m_componentName.c_str(), telemetryMessage)) == NULL)
    {
        LogError("Camera Component %s: PnP_CreateTelemetryMessageHandle failed.", m_componentName.c_str());
    }
    else if ((result = IoTHubDeviceClient_SendEventAsync(m_deviceClient, messageHandle,
            CameraIotPnpDevice_TelemetryCallback, (void*)(telemetryName))) != IOTHUB_CLIENT_OK)
    {
        LogError("Camera Component %s: Failed to report sensor data telemetry %s, error=%d",
            m_componentName.c_str(), telemetryName, result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return HResultFromPnpClient(result);
}

HRESULT CameraIotPnpDevice::ReportTelemetry(_In_ std::string& telemetryName, _In_ std::string& message)
{
    AutoLock lock(&m_lock);

    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

    char telemetryMessage[512] = { 0 };
    sprintf(telemetryMessage, "{\"%s\":%s}", telemetryName.c_str(), message.c_str());

    if ((messageHandle = PnP_CreateTelemetryMessageHandle(m_componentName.c_str(), telemetryMessage)) == NULL)
    {
        LogError("Camera Component %s: PnP_CreateTelemetryMessageHandle failed.", m_componentName.c_str());
    }
    else if ((result = IoTHubDeviceClient_SendEventAsync(m_deviceClient, messageHandle,
            CameraIotPnpDevice_TelemetryCallback, (void*)(telemetryName.c_str()))) != IOTHUB_CLIENT_OK)
    {
        LogError("Camera Component %s: Failed to report sensor data telemetry %s, error=%d",
            m_componentName.c_str(), telemetryName.c_str(), result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return HResultFromPnpClient(result);
}

HRESULT CameraIotPnpDevice::LoopTelemetry(_In_ DWORD dwMilliseconds)
{
    bool                fShutdown = false;

    RETURN_IF_FAILED (m_cameraStats->Start());

    do
    {
        FSStatisticsEntry   stats_pre = { };
        FSStatisticsEntry   stats_post = { };
        DWORD               dwWait = 0;
        LONGLONG            llStart = 0;
        LONGLONG            llEnd = 0;
        DWORD               dwSleepDuration = (dwMilliseconds > 30000 ? 30000 : dwMilliseconds);

        // Take a snap shot of the stats immedately before we wait for
        // the pipeline.
        // NOTE:  If the prestat return 0 output/0 dropped, that means
        // the pipeline hasn't been primed.  We should avoid publishing
        // this information.  So for that, we want to skip reporting 
        // telemetry.
        RETURN_IF_FAILED (m_cameraStats->PreStats(stats_pre, &llStart));
        if ((stats_pre.m_dropped + stats_pre.m_output) == 0)
        {
            dwSleepDuration = 1000;
        }
        dwWait = WaitForSingleObject(m_hShutdownEvent, dwSleepDuration);
        switch (dwWait)
        {
        case WAIT_OBJECT_0:
            // Shutdown, return
            fShutdown = true;
            break;
        case WAIT_TIMEOUT:
            // Just keep looping
            if ((stats_pre.m_dropped + stats_pre.m_output) > 0)
            {
                RETURN_IF_FAILED (m_cameraStats->PostStats(stats_post, &llEnd));
            }
            break;
        case WAIT_ABANDONED:
            RETURN_IF_FAILED (HRESULT_FROM_WIN32(ERROR_WAIT_NO_CHILDREN));
        case WAIT_FAILED:
            RETURN_IF_FAILED (HRESULT_FROM_WIN32(GetLastError()));
        default:
            RETURN_IF_FAILED (E_UNEXPECTED);
        }

        if (!fShutdown && ((stats_pre.m_dropped + stats_pre.m_output) > 0)
            // Don't log if pre and post stats are the same (stream has stopped and stats are unchanged)
            && !(stats_pre.m_dropped == stats_post.m_dropped && stats_pre.m_output == stats_post.m_output))
        {
            LONGLONG                        llDelta = (llEnd - llStart);
            LONGLONG                        llTotalFrames = 0;
            double                          dblActualFps = 0.0;
            double                          dblExpectedFps = 0.0;
            std::unique_ptr<JsonWrapper>    pjson;

            LogInfo("stats_pre[strmid=%u,source=%u,flags=0x%X,request=%I64d,input=%I64d,output=%I64d,dropped=%I64d,avgdelay=%f,status=0x%08X]",
                    stats_pre.m_streamid,
                    stats_pre.m_statsource,
                    stats_pre.m_flags,
                    stats_pre.m_request,
                    stats_pre.m_input,
                    stats_pre.m_output,
                    stats_pre.m_dropped,
                    (double)(stats_pre.m_delayrmscounter == 0 ? 0 : sqrt(stats_pre.m_delayrmsacc / stats_pre.m_delayrmscounter)),
                    stats_pre.m_hrstatus);
            LogInfo("stats_post[strmid=%u,source=%u,flags=0x%X,request=%I64d,input=%I64d,output=%I64d,dropped=%I64d,avgdelay=%f,status=0x%08X]",
                    stats_post.m_streamid,
                    stats_post.m_statsource,
                    stats_post.m_flags,
                    stats_post.m_request,
                    stats_post.m_input,
                    stats_post.m_output,
                    stats_post.m_dropped,
                    (double)(stats_post.m_delayrmscounter == 0 ? 0 : sqrt(stats_post.m_delayrmsacc / stats_post.m_delayrmscounter)),
                    stats_post.m_hrstatus);

            if (llDelta != 0)
            {
                llTotalFrames = ((stats_post.m_dropped + stats_post.m_output) - (stats_pre.m_dropped + stats_pre.m_output));
                dblActualFps = ((double)(llTotalFrames * ONE_SECOND_IN_100HNS) / llDelta);
            }
            if (stats_post.m_expectedframedelay > 0)
            {
                dblExpectedFps = (double)(ONE_SECOND_IN_100HNS / stats_post.m_expectedframedelay);
            }
            LogInfo ("total_frames=%I64d (delta=%I64d), fps=%.3f(%.3f)", llTotalFrames, llDelta, dblActualFps, dblExpectedFps);

            pjson = std::make_unique<JsonWrapper>();
            RETURN_IF_FAILED (pjson->Initialize());
            RETURN_IF_FAILED (pjson->AddValueAsString("streamId", stats_post.m_streamid));
            RETURN_IF_FAILED (pjson->AddValueAsString("frames", stats_post.m_output));
            RETURN_IF_FAILED (pjson->AddValueAsString("dropped", stats_post.m_dropped));
            RETURN_IF_FAILED (pjson->AddValueAsString("expected_framerate", dblExpectedFps));
            RETURN_IF_FAILED (pjson->AddValueAsString("actual_framerate", dblActualFps));
            RETURN_IF_FAILED (ReportTelemetry("CameraStats", (const unsigned char *)pjson->GetMessage(), pjson->GetSize()));
        }
    } while (!fShutdown);

    RETURN_IF_FAILED (m_cameraStats->Stop());

    return S_OK;
}

HRESULT GetFileame(IStorageFile* pStorageFile, std::string& filename) try
{
    HRESULT hr = S_OK;
    ComPtr<IStorageItem> spFile;
    HSTRING name;
    RETURN_IF_FAILED(pStorageFile->QueryInterface(IID_PPV_ARGS(&spFile)));
    RETURN_IF_FAILED(spFile->get_Name(&name));
    std::wstring ws(WindowsGetStringRawBuffer(name, nullptr));
    std::string fileStr = wstr2str(ws);
    filename = fileStr;
    return hr;
}
catch (std::exception e)
{
    LogInfo("Exception occurred in Camera Iot Device (%s)", e.what());
    return E_UNEXPECTED;
}

// TODO: Can just upload from memory or stream?
HRESULT CameraIotPnpDevice::UploadStorageFileToBlob(IStorageFile* pStorageFile) try
{
    HRESULT hr = S_OK;
    ComPtr<IStorageItem> spFile;
    ComPtr<IBasicProperties> spIBasicProperties;
    ComPtr<IAsyncOperation<BasicProperties*>> spPropsOp;
    HSTRING name;
    HSTRING path;
    UINT64 ui64InputSize;
    int pnpResult = PNPBRIDGE_OK;

    RETURN_IF_FAILED(pStorageFile->QueryInterface(IID_PPV_ARGS(&spFile)));

    RETURN_IF_FAILED(spFile->get_Name(&name));

    std::wstring ws(WindowsGetStringRawBuffer(name, nullptr));
    std::string fileStr = wstr2str(ws);

    RETURN_IF_FAILED(spFile->get_Path(&path));
    std::wstring wpath(WindowsGetStringRawBuffer(path, nullptr));
    std::string fullPathStr = wstr2str(wpath);

    RETURN_IF_FAILED(spFile->GetBasicPropertiesAsync(&spPropsOp));
    hr = AwaitTypedResult(spPropsOp, BasicProperties*, spIBasicProperties);
    RETURN_IF_FAILED(hr);
    RETURN_IF_FAILED(spIBasicProperties->get_Size(&ui64InputSize));

    char* buffer = (char*)malloc((size_t)ui64InputSize);
    std::ifstream fin(fullPathStr, std::ios::in | std::ios::binary);
    fin.read(buffer, ui64InputSize);
    size_t actRead = (size_t)fin.gcount();

    pnpResult = PnpBridge_UploadToBlobAsync(fileStr.c_str(),
        (unsigned char*)buffer,
        actRead,
        CameraIotPnpDevice_BlobUploadCallback,
        (void*)this);

    LogInfo("Uploading %s to blob storage, res=%d %s", fileStr.c_str(), pnpResult, pnpResult == PNPBRIDGE_OK ? "(Success)" : "");
    switch (pnpResult)
    {
    case PNPBRIDGE_OK:
        break;
    case PNPBRIDGE_INSUFFICIENT_MEMORY:
        RETURN_IF_FAILED(E_OUTOFMEMORY);
    case PNPBRIDGE_INVALID_ARGS:
        RETURN_IF_FAILED(E_INVALIDARG);
        break;
    case PNPBRIDGE_CONFIG_READ_FAILED:
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_BAD_CONFIGURATION));
        break;
    case PNPBRIDGE_DUPLICATE_ENTRY:
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_DUP_NAME));
    case PNPBRIDGE_FAILED:
    default:
        RETURN_IF_FAILED(E_UNEXPECTED);
        break;
    }
    fin.close();
    free(buffer);

    return hr;
}
catch (std::exception e)
{
    LogInfo("Exception occurred uploading to blob storage (%s)", e.what());
    return E_UNEXPECTED;
}

HRESULT
CameraIotPnpDevice::TakePhotoOp(_Out_ std::string& strResponse) try
{
    ComPtr<IStorageFile> spStorageFile;
    
    //RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_NOT_READY), m_cameraStats.get());
    
    RETURN_IF_FAILED(m_spCameraMediaCapture->TakePhoto(&spStorageFile));
    RETURN_IF_FAILED(UploadStorageFileToBlob(spStorageFile.Get()));

    std::string filename;
    GetFileame(spStorageFile.Get(), filename);
    strResponse = "\"" + filename + "\"";
    
    return S_OK;
}
catch (std::exception e)
{
    LogInfo("Exception occurred in take photo operation (%s)", e.what());
    return E_UNEXPECTED;
}

HRESULT CameraIotPnpDevice::TakeVideoOp(_In_ DWORD dwMilliseconds, _Out_ std::string& strResponse) try
{
    ComPtr<IStorageFile> spStorageFile;

    RETURN_IF_FAILED(m_spCameraMediaCapture->TakeVideo(dwMilliseconds, &spStorageFile));
    RETURN_IF_FAILED(UploadStorageFileToBlob(spStorageFile.Get()));
    
    std::string filename;
    GetFileame(spStorageFile.Get(), filename);
    strResponse = "\"" + filename + "\"";

    return S_OK;
}
catch (std::exception e)
{
    LogInfo("Exception occurred in take video operation (%s)", e.what());
    return E_UNEXPECTED;
}

HRESULT CameraIotPnpDevice::StartDetection()
{
    //RETURN_IF_FAILED(m_spCameraMediaCaptureFR->StartFrameReader());
    return S_OK;
}

HRESULT CameraIotPnpDevice::GetURIOp(std::string& strResponse) try
{
    strResponse = "\"This operation is not supported for this device\"";
    return S_OK;
}
catch (std::exception e)
{
    LogInfo("Exception occurred in get URI operation (%s)", e.what());
    return E_UNEXPECTED;
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_PropertyCallback(
    _In_ int pnpReportedStatus,
    _In_opt_ void* userContextCallback)
{
    LogInfo("%s:%d pnpstatus=%d,context=0x%p", __FUNCTION__, __LINE__, pnpReportedStatus, userContextCallback);
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_TelemetryCallback(
    _In_ IOTHUB_CLIENT_CONFIRMATION_RESULT pnpTelemetryStatus,
    _In_opt_ void* userContextCallback)
{
    UNREFERENCED_PARAMETER(pnpTelemetryStatus);
    UNREFERENCED_PARAMETER(userContextCallback);
    //LogInfo("%s:%d pnpstatus=%d,context=0x%p", __FUNCTION__, __LINE__, pnpTelemetryStatus, userContextCallback);
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_BlobUploadCallback(
    IOTHUB_CLIENT_FILE_UPLOAD_RESULT result, 
    void* userContextCallback
    )
{    
    UNREFERENCED_PARAMETER(result);
    UNREFERENCED_PARAMETER(userContextCallback);
    //LogInfo("%s:%d upload_result=%d,context=0x%p", __FUNCTION__, __LINE__, result, userContextCallback);
}

DWORD WINAPI CameraIotPnpDevice::TelemetryWorkerThreadProc(_In_opt_ PVOID pv)
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)pv;

    if (p != nullptr)
    {
        p->LoopTelemetry();
    }

    return 0;
}