#include "pch.h"
#include "CameraIotPnpDevice.h"


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
        //LogError("DEVICE_INFO: Reporting property=<%s> failed, error=<%s>", propertyName, ENUM_TO_STRING(PNP_CLIENT_RESULT, result));
    }
    else
    {
        //LogInfo("DEVICE_INFO: Queued async report read only property for %s", propertyName);
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
        //LogError("DEVICE_INFO: Reporting property=<%s> failed, error=<%s>", propertyName, ENUM_TO_STRING(PNP_CLIENT_RESULT, result));
    }
    else
    {
        //LogInfo("DEVICE_INFO: Queued async report read only property for %s", propertyName);
    }

    return HResultFromPnpClient(result);
}

HRESULT
CameraIotPnpDevice::LoopTelemetry(
    _In_ DWORD dwMilliseconds)
{
    DWORD dwSleepDuration = (dwMilliseconds > 30000 ? 30000 : dwMilliseconds);
    bool  fShutdown = false;

    RETURN_IF_FAILED (m_cameraStats->Start());

    do
    {
        FSStatisticsEntry   stats = { };
        DWORD               dwWait = 0;

        dwWait = WaitForSingleObject(m_hShutdownEvent, dwSleepDuration);
        switch (dwWait)
        {
        case WAIT_OBJECT_0:
            // Time to shutdown...return from this.
            fShutdown = true;
            break;
        case WAIT_TIMEOUT:
            // Just keep looping.
            break;
        case WAIT_ABANDONED:
            RETURN_IF_FAILED (HRESULT_FROM_WIN32(ERROR_WAIT_NO_CHILDREN));
        case WAIT_FAILED:
            RETURN_IF_FAILED (HRESULT_FROM_WIN32(GetLastError()));
        default:
            RETURN_IF_FAILED (E_UNEXPECTED);
        }

        if (!fShutdown)
        {
            RETURN_IF_FAILED (m_cameraStats->GetStats(stats, nullptr, nullptr, 0, nullptr));
            printf("stats:[strmid=%u,source=%u,flags=0x%X,request=%I64d,input=%I64d,output=%I64d,dropped=%I64d,avgdelay=%f,status=0x%08X",
                    stats.m_streamid,
                    stats.m_statsource,
                    stats.m_flags,
                    stats.m_request,
                    stats.m_input,
                    stats.m_output,
                    stats.m_dropped,
                    (double)(stats.m_delayrmscounter == 0 ? 0 : stats.m_delayrmsacc / stats.m_delayrmscounter),
                    stats.m_hrstatus);
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
    TRACELOG("status=%d,context=%s", pnpReportedStatus, (nullptr != userContextCallback ? (const char*)userContextCallback : "nullptr"));
}

void __cdecl
CameraIotPnpDevice::CameraIotPnpDevice_TelemetryCallback(
    _In_ PNP_SEND_TELEMETRY_STATUS pnpTelemetryStatus, 
    _In_opt_ void* userContextCallback)
{
    TRACELOG("status=%d,context=%s", pnpTelemetryStatus, (nullptr != userContextCallback ? (const char*)userContextCallback : "nullptr"));
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