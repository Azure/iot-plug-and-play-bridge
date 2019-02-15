#include "pch.h"
#include "CameraIotPnpDevice.h"
#include <pnpbridgecommon.h>
#include <pnpbridge.h>

#define ONE_SECOND_IN_100HNS        10000000    // 10,000,000 100hns in 1 second.
#define ONE_MILLISECOND_IN_100HNS   10000       // 10,000 100hns in 1 millisecond.

CameraIotPnpDevice::CameraIotPnpDevice()
:   m_PnpClientInterface(nullptr)
,   m_hWorkerThread(nullptr)
,   m_hShutdownEvent(nullptr)
,   m_PnpDeviceClient(nullptr)
{

}

CameraIotPnpDevice::~CameraIotPnpDevice()
{
    // We don't close this handle, it's closed by the DeviceAggregator
    // when the interface is released.
    m_PnpClientInterface = nullptr;
    m_PnpDeviceClient = nullptr;

    Shutdown();
}

HRESULT
CameraIotPnpDevice::Initialize(
    _In_ PNP_INTERFACE_CLIENT_HANDLE hPnpClientInterface, 
    _In_ PNP_DEVICE_CLIENT_HANDLE hPnpDeviceClient,
    _In_opt_z_ LPCWSTR deviceName
    )
{
    // Can't initialize twice...
    RETURN_HR_IF_NULL (E_INVALIDARG, hPnpClientInterface);
    RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_cameraStats.get() != nullptr);

    m_cameraStats = std::make_unique<CameraStatConsumer>();
    RETURN_IF_FAILED (m_cameraStats->Initialize(nullptr, GUID_NULL, deviceName));
    RETURN_IF_FAILED (m_cameraStats->AddProvider(g_FrameServerEventProvider));
    m_PnpClientInterface = hPnpClientInterface;
    m_PnpDeviceClient = hPnpDeviceClient;

    return S_OK;
}

HRESULT
CameraIotPnpDevice::StartTelemetryWorker(
    )
{
    AutoLock lock(&m_lock);

    RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), nullptr != m_hWorkerThread);

    m_hShutdownEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(GetLastError()), m_hShutdownEvent);
    m_hWorkerThread = CreateThread(nullptr, 0, CameraIotPnpDevice::TelemetryWorkerThreadProc, (PVOID)this, 0, nullptr);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(GetLastError()), m_hWorkerThread);

    return S_OK;
}

HRESULT
CameraIotPnpDevice::StopTelemetryWorker(
    )
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

void
CameraIotPnpDevice::Shutdown(
    )
{
    (VOID) StopTelemetryWorker();

    if (m_cameraStats.get() != nullptr)
    {
        (VOID) m_cameraStats->Shutdown();
    }
}

HRESULT             
CameraIotPnpDevice::ReportProperty(
    _In_ bool readOnly,
    _In_ const char* propertyName,
    _In_ unsigned const char* propertyData,
    _In_ size_t propertyDataLen)
{
    AutoLock lock(&m_lock);

    PNP_CLIENT_RESULT result;

    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), m_PnpClientInterface);

    result = PnP_InterfaceClient_ReportReadOnlyPropertyStatusAsync(m_PnpClientInterface,
                                                                   propertyName,
                                                                   (const unsigned char*)propertyData, 
                                                                   propertyDataLen,
                                                                   CameraIotPnpDevice_PropertyCallback, 
                                                                   (void*)propertyName);
    if (result != PNP_CLIENT_OK)
    {
        LogError("DEVICE_INFO: Reporting property=<%s> failed, error=<%s>", propertyName, ENUM_TO_STRING(PNP_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("DEVICE_INFO: Queued async report read only property for %s", propertyName);
    }

    return HResultFromPnpClient(result);
}

HRESULT
CameraIotPnpDevice::ReportTelemetry(
    _In_z_ const char* telemetryName,
    _In_reads_bytes_(messageDataLen) const unsigned char* messageData,
    _In_ size_t messageDataLen)
{
    AutoLock lock(&m_lock);

    PNP_CLIENT_RESULT result;

    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), m_PnpClientInterface);

    result = PnP_InterfaceClient_SendTelemetryAsync(m_PnpClientInterface,
                                                    telemetryName,
                                                    (const unsigned char*)messageData,
                                                    messageDataLen,
                                                    CameraIotPnpDevice_TelemetryCallback,
                                                    (void*)telemetryName);
    if (result != PNP_CLIENT_OK)
    {
        LogError("DEVICE_INFO: Reporting telemetry=<%s> failed, error=<%s>", telemetryName, ENUM_TO_STRING(PNP_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("DEVICE_INFO: Queued async report telemetry for %s", telemetryName);
    }

    return HResultFromPnpClient(result);
}

HRESULT
CameraIotPnpDevice::LoopTelemetry(
    _In_ DWORD dwMilliseconds)
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
            // Time to shutdown...return from this.
            fShutdown = true;
            break;
        case WAIT_TIMEOUT:
            // Just keep looping.
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

        if (!fShutdown && ((stats_pre.m_dropped + stats_pre.m_output) > 0))
        {
            LONGLONG                        llDelta = (llEnd - llStart);
            LONGLONG                        llTotalFrames = 0;
            double                          dblActualFps = 0.0;
            double                          dblExpectedFps = 0.0;
            size_t                          cch = 0;
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
            RETURN_IF_FAILED (ReportTelemetry("camera_health", (const unsigned char *)pjson->GetMessage(), pjson->GetSize()));
        }
    } while (!fShutdown);

    RETURN_IF_FAILED (m_cameraStats->Stop());

    return S_OK;
}

HRESULT             
CameraIotPnpDevice::TakePhotoOp(
    _Out_ std::string& strResponse
    )
{
    ULONG                   cbJpeg = 0;
    ULONG                   cbWritten = 0;
    std::unique_ptr<BYTE[]> pbJpeg;
    FILETIME                ft = { };
    SYSTEMTIME              st = { };
    char                    szFile[MAX_PATH] = { };
    int                     pnpResult = PNPBRIDGE_OK;

    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(ERROR_NOT_READY), m_PnpDeviceClient);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(ERROR_NOT_READY), m_cameraStats.get());

    RETURN_IF_FAILED (m_cameraStats->TakePhoto());
    RETURN_IF_FAILED (m_cameraStats->GetJpegFrameSize(&cbJpeg));
    
    pbJpeg = std::make_unique<BYTE[]>(cbJpeg);
    RETURN_IF_FAILED (m_cameraStats->GetJpegFrame(pbJpeg.get(), cbJpeg, &cbWritten));

    GetSystemTimePreciseAsFileTime(&ft);
    RETURN_HR_IF (HRESULT_FROM_WIN32(GetLastError()), !FileTimeToSystemTime(&ft, &st));
    RETURN_IF_FAILED (StringCchPrintfA(szFile, ARRAYSIZE(szFile), "photo_%04d-%02d-%02d-%02d-%02d-%02d.%03d.jpg", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds));


    // Write the photo to the local temp folder.  This will keep overwriting
    // the old photo so we will only keep the most current photo in our
    // temp folder--this is to avoid filling up our file system with
    // old photos.
    WritePhotoToTemp(pbJpeg.get(), cbJpeg);



    pnpResult = PnpBridge_UploadToBlobAsync(szFile,
                                            pbJpeg.get(),
                                            cbJpeg,
                                            CameraIotPnpDevice_BlobUploadCallback,
                                            (void*)this);
    switch(pnpResult)
    {
    case PNPBRIDGE_OK:
        break;
    case PNPBRIDGE_INSUFFICIENT_MEMORY:
        RETURN_IF_FAILED (E_OUTOFMEMORY);
    case PNPBRIDGE_INVALID_ARGS:
        RETURN_IF_FAILED (E_INVALIDARG);
        break;
    case PNPBRIDGE_CONFIG_READ_FAILED:
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(ERROR_BAD_CONFIGURATION));
        break;
    case PNPBRIDGE_DUPLICATE_ENTRY:
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(ERROR_DUP_NAME));
    case PNPBRIDGE_FAILED:
    default:
        RETURN_IF_FAILED (E_UNEXPECTED);
        break;
    }

    // Send back the file name here...
    strResponse = "{ photo : " + std::string(szFile) + " }";

    return S_OK;
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_PropertyCallback(
    _In_ PNP_REPORTED_PROPERTY_STATUS pnpReportedStatus,
    _In_opt_ void* userContextCallback)
{
    LogInfo("%s:%d pnpstatus=%d,context=0x%p", __FUNCTION__, __LINE__, pnpReportedStatus, userContextCallback);
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_TelemetryCallback(
    _In_ PNP_SEND_TELEMETRY_STATUS pnpTelemetryStatus, 
    _In_opt_ void* userContextCallback)
{
    LogInfo("%s:%d pnpstatus=%d,context=0x%p", __FUNCTION__, __LINE__, pnpTelemetryStatus, userContextCallback);
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_BlobUploadCallback(
    IOTHUB_CLIENT_FILE_UPLOAD_RESULT result, 
    void* userContextCallback
    )
{    
    LogInfo("%s:%d upload_result=%d,context=0x%p", __FUNCTION__, __LINE__, result, userContextCallback);
}

DWORD 
WINAPI 
CameraIotPnpDevice::TelemetryWorkerThreadProc(
    _In_opt_ PVOID pv
    )
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)pv;

    if (p != nullptr)
    {
        p->LoopTelemetry();
    }

    return 0;
}

HRESULT
CameraIotPnpDevice::WritePhotoToTemp(
    _In_reads_bytes_(cbJpeg) PBYTE pbJpeg, 
    _In_ ULONG cbJpeg
    )
{
    HANDLE  hFile = INVALID_HANDLE_VALUE;
    char    szTemp[MAX_PATH + 1] = { };
    size_t  cch = 0;
    BOOL    fResult = FALSE;
    ULONG   cbWritten = 0;

    RETURN_HR_IF_NULL (E_INVALIDARG, pbJpeg);
    RETURN_HR_IF (E_INVALIDARG, cbJpeg == 0);

    // Use the ANSI version instead of the wide-char.
    RETURN_HR_IF (HRESULT_FROM_WIN32(GetLastError()), 0 == GetTempPathA(ARRAYSIZE(szTemp), szTemp));
    RETURN_IF_FAILED (StringCchCatA(szTemp, ARRAYSIZE(szTemp), "current_photo.jpg"));

    LogInfo ("Writing photo to temp folder (%s)", szTemp);

    hFile = CreateFileA(szTemp, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    RETURN_HR_IF (HRESULT_FROM_WIN32(GetLastError()), hFile == INVALID_HANDLE_VALUE);
    fResult = WriteFile(hFile, pbJpeg, cbJpeg, &cbWritten, nullptr);
    (void) CloseHandle(hFile);
    RETURN_HR_IF (HRESULT_FROM_WIN32(GetLastError()), !fResult);

    return S_OK;
}
