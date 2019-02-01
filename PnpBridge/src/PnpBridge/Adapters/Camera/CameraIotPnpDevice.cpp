#include "pch.h"
#include "CameraIotPnpDevice.h"

#define ONE_SECOND_IN_100HNS        10000000    // 10,000,000 100hns in 1 second.
#define ONE_MILLISECOND_IN_100HNS   10000       // 10,000 100hns in 1 millisecond.

CameraIotPnpDevice::CameraIotPnpDevice()
:   m_PnpClientInterface(nullptr)
,   m_hWorkerThread(nullptr)
,   m_hShutdownEvent(nullptr)
{

}

CameraIotPnpDevice::~CameraIotPnpDevice()
{
    // We don't close this handle, it's closed by the DeviceAggregator
    // when the interface is released.
    m_PnpClientInterface = nullptr;

    Shutdown();
}

HRESULT
CameraIotPnpDevice::Initialize(
    _In_ PNP_INTERFACE_CLIENT_HANDLE hPnpClientInterface, 
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