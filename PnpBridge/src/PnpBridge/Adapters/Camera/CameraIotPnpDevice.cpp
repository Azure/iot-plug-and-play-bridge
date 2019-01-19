#include "pch.h"
#include "CameraIotPnpDevice.h"

#define MAX_32BIT_DECIMAL_DIGIT     11          // maximum number of digits possible for a 32-bit value in decimal (plus null terminator).
#define MAX_64BIT_DECIMAL_DIGIT     21
#define MAX_32BIT_HEX_DIGIT         11          // Max hex digit is 8, but we need "0x" and null terminator.
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
    DWORD               dwSleepDuration = (dwMilliseconds > 30000 ? 30000 : dwMilliseconds);
    bool                fShutdown = false;
    FSStatisticsEntry   old_stats = { };

    RETURN_IF_FAILED (m_cameraStats->Start());

    do
    {
        FSStatisticsEntry   stats = { };
        DWORD               dwWait = 0;
        LONGLONG            llStart = MFGetSystemTime();
        LONGLONG            llEnd = 0;

        dwWait = WaitForSingleObject(m_hShutdownEvent, dwSleepDuration);
        switch (dwWait)
        {
        case WAIT_OBJECT_0:
            // Time to shutdown...return from this.
            fShutdown = true;
            break;
        case WAIT_TIMEOUT:
            // Just keep looping.
            llEnd = MFGetSystemTime();
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
            JSON_Value*             json_val = nullptr;
            JSON_Object*            json_obj = nullptr;
            LONGLONG                llDelta = (llEnd - llStart);
            double                  dblActualFps = 0.0;
            double                  dblExpectedFps = 0.0;
            std::unique_ptr<char[]> psz;
            size_t                  cch = 0;

            RETURN_IF_FAILED (m_cameraStats->GetStats(stats, nullptr, nullptr, 0, nullptr));
            LogInfo("stats[strmid=%u,source=%u,flags=0x%X,request=%I64d,input=%I64d,output=%I64d,dropped=%I64d,avgdelay=%f,status=0x%08X]",
                    stats.m_streamid,
                    stats.m_statsource,
                    stats.m_flags,
                    stats.m_request,
                    stats.m_input,
                    stats.m_output,
                    stats.m_dropped,
                    (double)(stats.m_delayrmscounter == 0 ? 0 : sqrt(stats.m_delayrmsacc / stats.m_delayrmscounter)),
                    stats.m_hrstatus);

            if (llDelta != 0)
            {
                if ((old_stats.m_output == 0 && old_stats.m_dropped == 0))
                {
                    dblActualFps = ((double)((stats.m_output + stats.m_dropped) * ONE_SECOND_IN_100HNS) / llDelta);
                }
                else
                {
                    dblActualFps = ((double)(((stats.m_output + stats.m_dropped) - (old_stats.m_output + old_stats.m_dropped)) * ONE_SECOND_IN_100HNS) / llDelta);
                }
            }
            if (stats.m_expectedframedelay > 0)
            {
                dblExpectedFps = (double)(ONE_SECOND_IN_100HNS / stats.m_expectedframedelay);
            }
            old_stats = stats;

            json_val = json_value_init_object();
            RETURN_HR_IF_NULL (E_OUTOFMEMORY, json_val);
            json_obj = json_value_get_object(json_val);
            RETURN_HR_IF_NULL (E_UNEXPECTED, json_obj);

            RETURN_IF_FAILED (CameraIotPnpDevice::JsonSetJsonValueAsString(json_obj, "streamId", stats.m_streamid));
            RETURN_IF_FAILED (CameraIotPnpDevice::JsonSetJsonValueAsString(json_obj, "frames", stats.m_output));
            RETURN_IF_FAILED (CameraIotPnpDevice::JsonSetJsonValueAsString(json_obj, "dropped", stats.m_dropped));
            RETURN_IF_FAILED (CameraIotPnpDevice::JsonSetJsonValueAsString(json_obj, "expected_framerate", dblExpectedFps));
            RETURN_IF_FAILED (CameraIotPnpDevice::JsonSetJsonValueAsString(json_obj, "actual_framerate", dblActualFps));

            cch = json_serialization_size(json_val);
            if (cch > 0)
            {
                psz = std::make_unique<char[]>(cch + 1);
                RETURN_HR_IF (E_UNEXPECTED, JSONFailure == json_serialize_to_buffer(json_val, psz.get(), cch));
                psz.get()[cch] = '\0';
                RETURN_IF_FAILED (ReportTelemetry("camera_health", (const unsigned char*)psz.get(), cch));
            }
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

HRESULT
CameraIotPnpDevice::JsonSetJsonValueAsString(
    _Inout_ JSON_Object* json_obj, 
    _In_z_ LPCSTR name, 
    _In_ UINT32 val
    )
{
    char sz[MAX_32BIT_DECIMAL_DIGIT] = { };

    RETURN_HR_IF_NULL (E_INVALIDARG, json_obj);
    RETURN_HR_IF_NULL (E_INVALIDARG, name);

    RETURN_IF_FAILED (StringCchPrintfA(sz, _countof(sz), "%u", val));

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF (E_OUTOFMEMORY, JSONFailure == json_object_set_string(json_obj, name, sz));

    return S_OK;
}

HRESULT
CameraIotPnpDevice::JsonSetJsonValueAsString(
    _Inout_ JSON_Object* json_obj, 
    _In_z_ LPCSTR name, 
    _In_ LONGLONG val
    )
{
    char sz[MAX_64BIT_DECIMAL_DIGIT] = { };

    RETURN_HR_IF_NULL (E_INVALIDARG, json_obj);
    RETURN_HR_IF_NULL (E_INVALIDARG, name);

    RETURN_IF_FAILED (StringCchPrintfA(sz, _countof(sz), "%I64d", val));

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF (E_OUTOFMEMORY, JSONFailure == json_object_set_string(json_obj, name, sz));

    return S_OK;
}

HRESULT
CameraIotPnpDevice::JsonSetJsonValueAsString(
    _Inout_ JSON_Object* json_obj, 
    _In_z_ LPCSTR name, 
    _In_ double val
    )
{
    HRESULT hr = S_OK;
    char    sz[MAX_64BIT_DECIMAL_DIGIT] = { };

    RETURN_HR_IF_NULL (E_INVALIDARG, json_obj);
    RETURN_HR_IF_NULL (E_INVALIDARG, name);

    hr = StringCchPrintfA(sz, _countof(sz), "%.3f", val);
    if (hr == S_OK || hr == STRSAFE_E_INSUFFICIENT_BUFFER)
    {
        // If we successfully converted the string or if
        // we truncated the string, assume we're fine.
        hr = S_OK;
    }
    RETURN_IF_FAILED (hr);

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF (E_OUTOFMEMORY, JSONFailure == json_object_set_string(json_obj, name, sz));

    return S_OK;
}

HRESULT
CameraIotPnpDevice::JsonSetJsonHresultAsString(
    _Inout_ JSON_Object* json_obj, 
    _In_z_ LPCSTR name, 
    _In_ HRESULT hr
    )
{
    char sz[MAX_32BIT_HEX_DIGIT] = { };

    RETURN_HR_IF_NULL (E_INVALIDARG, json_obj);
    RETURN_HR_IF_NULL (E_INVALIDARG, name);

    RETURN_IF_FAILED (StringCchPrintfA(sz, _countof(sz), "0x%08X", hr));

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF (E_OUTOFMEMORY, JSONFailure == json_object_set_string(json_obj, name, sz));

    return S_OK;
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