// Copyright (c) Microsoft Corporation.  All rights reserved.
//
#include "pch.h"
#include "CameraStatConsumer.h"

GUID g_FrameServerEventProvider = { 0x9E22A3ED, 0x7B32, 0x4B99, { 0xB6, 0xC2, 0x21, 0xDD, 0x6A, 0xCE, 0x01, 0xE1 } };
USHORT g_FrameServerCaptureStatId = 10;

// Cameras under Windows can be registered in multiple categories.
// These three interface categories are generally treated as "webcam".
// We need to check all three since some cameras may show up under
// a subset of these categories.  We also need to make sure we
// remove duplicates as a camera registered under multiple categories
// will trigger our device watcher for each of the categories we're
// monitoring.
//
// To avoid including the entire DShow header, we can just define
// the video input device category locally:

GUID CLSID_VideoInputDeviceCategory = { 0x860BB310, 0x5D01, 0x11d0, { 0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86 } };

static GUID s_CameraInterfaceCategories[] = {
    KSCATEGORY_VIDEO_CAMERA,
    KSCATEGORY_SENSOR_CAMERA,
    CLSID_VideoInputDeviceCategory
};

CameraStatConsumer::CameraStatConsumer(
    )
:   m_hTrace(INVALID_PROCESSTRACE_HANDLE)
,   m_hTraceSession(INVALID_PROCESSTRACE_HANDLE)
,   m_eState(CameraStatConsumerState_Stopped)
,   m_guidSession(GUID_NULL)
,   m_hFlushThread(nullptr)
,   m_fCoInit(false)
,   m_fMfStartup(false)
,   m_stats({ 0 })
{
    ZeroMemory(m_buffer, sizeof(m_buffer));
    ZeroMemory(&m_TraceLogfile, sizeof(m_TraceLogfile));
    m_pTraceProperties = (PEVENT_TRACE_PROPERTIES)m_buffer;
    m_pwzSessionName = (LPWSTR)(m_pTraceProperties + 1);

    m_SymbolicLinkNameReceived = L"nullptr";
}

CameraStatConsumer::~CameraStatConsumer(
    )
{
    AutoLock lock(&m_lock);

    InternalStop();
    InternalShutdown();
}

HRESULT 
CameraStatConsumer::Initialize(
    _In_opt_z_ LPCWSTR SessionName, 
    _In_ REFGUID guidSession,
    _In_opt_z_ LPCWSTR SymbolicName
    )
{
    AutoLock lock(&m_lock);

    RETURN_IF_FAILED (CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    m_fCoInit = true;
    RETURN_IF_FAILED (MFStartup(MF_VERSION));
    m_fMfStartup = true;

    if (nullptr == SessionName)
    {
        RETURN_IF_FAILED (StringCchPrintf(m_pwzSessionName, MAX_PATH, L"CameraStatConsumer_%u_%I64d", GetCurrentProcessId(), MFGetSystemTime()));
    }
    else
    {
        RETURN_IF_FAILED (StringCchCopy(m_pwzSessionName, MAX_PATH, SessionName));
    }

    m_guidSession = guidSession;
    if (m_guidSession == GUID_NULL)
    {
        RPC_STATUS  rpc = UuidCreate(&m_guidSession);

        if (rpc != RPC_S_OK)
        {
            return HRESULT_FROM_WIN32(rpc);
        }
    }

    if (nullptr != SymbolicName && *SymbolicName != L'\0')
    {
        m_SymbolicLinkName = SymbolicName;
    }
    else
    {
        m_SymbolicLinkName.clear();
    }

    return S_OK;
}

HRESULT 
CameraStatConsumer::AddProvider(
    _In_ REFGUID guidProvider
    )
{
    AutoLock lock(&m_lock);

    m_listProviders.push_back(guidProvider);

    return S_OK;
}

HRESULT 
CameraStatConsumer::Start(
    )
{
    AutoLock lock(&m_lock);

    return InternalStart();
}

HRESULT 
CameraStatConsumer::Stop(
    )
{
    AutoLock lock(&m_lock);

    return InternalStop();
}

void 
CameraStatConsumer::Shutdown(
    )
{
    AutoLock lock(&m_lock);

    (void) InternalStop();
    (void) InternalShutdown();
}

HRESULT 
CameraStatConsumer::PreStats(
    _Inout_ FSStatisticsEntry& stats_pre, 
    _Out_ LONGLONG* pllPreTs
    )
{
    AutoLock lock(&m_lock);

    RETURN_HR_IF_NULL (E_POINTER, pllPreTs);
    *pllPreTs = MFGetSystemTime();

    stats_pre = m_stats;

    return S_OK;
}

HRESULT 
CameraStatConsumer::PostStats(
    _Inout_ FSStatisticsEntry& stats_post, 
    _Out_ LONGLONG* pllPostTs
    )
{
    AutoLock lock(&m_lock);

    RETURN_HR_IF_NULL (E_POINTER, pllPostTs);
    *pllPostTs = MFGetSystemTime();

    stats_post = m_stats;

    return S_OK;
}

HRESULT 
CameraStatConsumer::TakePhoto(
    )
{
    CameraCaptureEngine     camera;
    ComPtr<IMFSample>       jpegFrame;
    std::wstring            symbolicName;

    m_lock.Lock();
    symbolicName = m_SymbolicLinkName;
    m_lock.Unlock();

    LogInfo ("Taking photo...");
    RETURN_IF_FAILED (camera.TakePhoto((symbolicName.empty() ? nullptr : symbolicName.c_str()), &jpegFrame));

    if (jpegFrame.Get() != nullptr)
    {
        DWORD  cBufCount = 0;
        ComPtr<IMFMediaBuffer>  spJpegBuffer;
        ULONG cbJpegSize = 0;
        PBYTE pbJpeg = nullptr;
        errno_t err = 0;

        RETURN_IF_FAILED (jpegFrame->GetBufferCount(&cBufCount));
        RETURN_HR_IF (E_UNEXPECTED, cBufCount != 1);
        RETURN_IF_FAILED (jpegFrame->ConvertToContiguousBuffer(&spJpegBuffer));
        RETURN_IF_FAILED (spJpegBuffer->Lock(&pbJpeg, nullptr, (DWORD*)&cbJpegSize));

        m_lock.Lock();
        m_jpegFrameSize = cbJpegSize;
        m_jpegFrame = std::make_unique<BYTE[]>(cbJpegSize);
        err = memcpy_s(m_jpegFrame.get(), cbJpegSize, pbJpeg, cbJpegSize);
        m_lock.Unlock();

        RETURN_HR_IF (E_UNEXPECTED, err != 0);
    }

    return S_OK;
}

HRESULT 
CameraStatConsumer::GetJpegFrameSize(
    _Out_ ULONG* pcbJpegFrameSize
    )
{
    AutoLock lock(&m_lock);

    RETURN_HR_IF_NULL (E_POINTER, pcbJpegFrameSize);
    *pcbJpegFrameSize = (ULONG)m_jpegFrameSize;

    return S_OK;
}

HRESULT 
CameraStatConsumer::GetJpegFrame(
    _Inout_ PBYTE pbJpegFrame, 
    _In_ ULONG cbJpegFrame, 
    _Out_opt_ ULONG* pcbWritten
    )
{
    AutoLock lock(&m_lock);

    ComPtr<IMFMediaBuffer>  spJpegBuffer;
    errno_t                 err = 0;
    
    RETURN_HR_IF_NULL (E_INVALIDARG, pbJpegFrame);
    RETURN_HR_IF (E_INVALIDARG, cbJpegFrame == 0);

    // There should be one buffer (we shouldn't see multi-buffer
    // samples since that's only for network streaming samples).
    RETURN_HR_IF (HRESULT_FROM_WIN32(ERROR_NOT_FOUND), m_jpegFrame.get() == nullptr);

    if (m_jpegFrameSize > cbJpegFrame)
    {
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    }
    // errno_t doesn't translate to Win32 errors, just return
    // E_INVALIDARG since that's what will cause memcpy_s to 
    // fail.
    err = memcpy_s (pbJpegFrame, cbJpegFrame, m_jpegFrame.get(), m_jpegFrameSize);
    RETURN_HR_IF (E_INVALIDARG, err != 0);

    if (pcbWritten)
    {
        *pcbWritten = (ULONG)m_jpegFrameSize;
    }

    return S_OK;
}


/// Protected methods.
void 
CameraStatConsumer::ProcessETWEventCallback(
    _In_ PEVENT_RECORD pRecord
    )
{
    // Only send this into the consumer when the event record matches our
    // FrameServer capture statistics (and we only care about the capture
    // stat payload for now).
    // Also make sure the payload contains the data we're expecting.
    if (pRecord != nullptr && pRecord->UserContext != nullptr &&
        pRecord->EventHeader.ProviderId == g_FrameServerEventProvider &&
        pRecord->EventHeader.EventDescriptor.Id == g_FrameServerCaptureStatId &&
        pRecord->UserData != nullptr &&
        pRecord->UserDataLength >= sizeof(FSStatisticsEntry))
    {
        FSStatisticsEntry* pstats = (FSStatisticsEntry*)pRecord->UserData;
        LPCWSTR symname = nullptr;

        if (pRecord->UserDataLength >= (sizeof(FSStatisticsEntry) + sizeof(WCHAR)))
        {
            // Device symbolic name should be a null terminated string right
            // past the FSStatisticsEntry structure in our ETW payload.
            PBYTE pb = (PBYTE)pRecord->UserData;
            symname = (LPCWSTR)(pb + sizeof(FSStatisticsEntry));
        }
        (void) ((CameraStatConsumer*)(pRecord->UserContext))->AddStat(pstats, symname);
    }

    return;
}

DWORD 
CameraStatConsumer::ETWProcessThreadProc(
    _In_opt_ PVOID pv
    )
{
    CameraStatConsumer* p = (CameraStatConsumer*)pv;

    if (p != nullptr)
    {
        while (ProcessTrace(&(p->m_hTrace), 1, nullptr, nullptr) == ERROR_SUCCESS)
        {
            // Keep spinning...
        }
    }

    return 0;
}

HRESULT 
CameraStatConsumer::AddStat(
    _In_ FSStatisticsEntry* pstats, 
    _In_opt_z_ LPCWSTR DeviceSymbolicName
    )
{
    // Nothing to do if the record is empty or the user data is null
    RETURN_HR_IF_NULL (S_OK, pstats);

    if (FAILED(m_stats.m_hrstatus))
    {
        // Do nothing.  We don't want to lose the old state information.
        return S_OK;
    }

    if (!m_SymbolicLinkName.empty())
    {
        std::wstring uniqueId;
        std::wstring hwId;

        RETURN_IF_FAILED (CameraPnpDiscovery::MakeUniqueId(DeviceSymbolicName, uniqueId, hwId));

        if (_wcsicmp(m_SymbolicLinkName.c_str(), uniqueId.c_str()) != 0)
        {
            // Skip this since it's not the correct camera.
            return S_OK;
        }
    }

    // We only want to look at the FSStream stats, the other component stats
    // while useful for detailed diagnostics, for monitor the pipeline health
    // we just need to see the last component in the pipeline chain to determine
    // the camera health.
    if (pstats->m_statsource == (UINT32)FSCapStatSource_FSStream)
    {
        // Look for the first stream we recieve and ignore all 
        // others.  NOTE:  We're assuming only one camera will
        // be active at any given time, if multiple cameras, we'll
        // want to filter based on the symbolic name.
        if (m_stats.m_streamid == UINT32_MAX)
        {
            m_stats.m_streamid = pstats->m_streamid;
        }
        if (m_stats.m_streamid == pstats->m_streamid)
        {
            // $$TODO - Clear out the stats based on when the flag
            // flips to ensure we don't accidentally accumlate stats
            // based on different frame rates.  For now, we're assuming
            // the frame rate won't change across sessions.
            m_stats.m_flags = pstats->m_flags;
            m_stats.m_request = pstats->m_request;
            m_stats.m_input = pstats->m_input;
            m_stats.m_output = pstats->m_output;
            m_stats.m_dropped = pstats->m_dropped;
            m_stats.m_delayrmsacc = pstats->m_delayrmsacc;
            m_stats.m_delayrmscounter = pstats->m_delayrmscounter;
            m_stats.m_expectedframedelay = pstats->m_expectedframedelay;

            m_SymbolicLinkNameReceived = (DeviceSymbolicName == nullptr ? L"nullptr" : DeviceSymbolicName);
        }
    }
    return S_OK;
}

HRESULT 
CameraStatConsumer::InternalStart(
    )
{
    ULONG ullResult = ERROR_SUCCESS;

    RETURN_IF_FAILED (CheckShutdown());

    m_pTraceProperties->Wnode.BufferSize = sizeof(m_buffer);
    m_pTraceProperties->Wnode.Guid = m_guidSession;
    m_pTraceProperties->Wnode.ClientContext = 1;
    m_pTraceProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    m_pTraceProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    m_pTraceProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);


    // Attempt to stop the trace, it's possible we may have the same trace
    // running (we may want to revisit how we create the session name above).
    (void)ControlTrace(NULL, m_pwzSessionName, m_pTraceProperties, EVENT_TRACE_CONTROL_STOP);

    ullResult = StartTrace(&m_hTraceSession, m_pwzSessionName, m_pTraceProperties);
    RETURN_IF_FAILED ((ullResult != ERROR_SUCCESS ? HRESULT_FROM_WIN32(ullResult) : S_OK));

    m_TraceLogfile.LoggerName = m_pwzSessionName;
    m_TraceLogfile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    m_TraceLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
    m_TraceLogfile.EventRecordCallback = CameraStatConsumer::ProcessETWEventCallback;
    m_TraceLogfile.Context = (PVOID)this;

    m_hTrace = OpenTrace(&m_TraceLogfile);
    RETURN_IF_FAILED ((m_hTrace == INVALID_PROCESSTRACE_HANDLE ? HRESULT_FROM_WIN32(GetLastError()) : S_OK));

    m_hFlushThread = CreateThread(nullptr, 0, CameraStatConsumer::ETWProcessThreadProc, (LPVOID)this, 0, nullptr);
    RETURN_IF_FAILED ((m_hFlushThread == nullptr ? HRESULT_FROM_WIN32(GetLastError()) : S_OK));
    for (GUID guidProvider : m_listProviders)
    {
        ullResult = EnableTraceEx(&guidProvider, &m_guidSession, m_hTraceSession, TRUE, TRACE_LEVEL_VERBOSE, 0, 0, 0, NULL);
        RETURN_IF_FAILED ((ullResult != ERROR_SUCCESS ? HRESULT_FROM_WIN32(ullResult) : S_OK));
    }    
    m_eState = CameraStatConsumerState_Running;

    return S_OK;
}

HRESULT
CameraStatConsumer::InternalStop(
    )
{
    ULONG ullResult = ERROR_SUCCESS;
    HANDLE hFlushThread = m_hFlushThread;

    // Avoid using the RETURN_IF_FAILED macro here since we may be
    // stopped multiple times (no need to spam).
    if (MF_E_SHUTDOWN == CheckShutdown())
    {
        return MF_E_SHUTDOWN;
    }

    if (m_hTrace != INVALID_PROCESSTRACE_HANDLE)
    {
        (void)CloseTrace(m_hTrace);
        m_hTrace = INVALID_PROCESSTRACE_HANDLE;
    }

    // We have to release the lock here since we can't wait for
    // the flush thread to exit since it's likely to be waiting
    // for our lock as well.
    m_hFlushThread = nullptr;
    m_lock.Unlock();
    if (nullptr != hFlushThread)
    {
        ::WaitForSingleObject(hFlushThread, INFINITE);
        CloseHandle(hFlushThread);
        hFlushThread = nullptr;
    }
    m_lock.Lock();
    RETURN_IF_FAILED (CheckShutdown());

    if (m_hTraceSession != INVALID_PROCESSTRACE_HANDLE)
    {
        for (GUID guidProvider : m_listProviders)
        {
            ullResult = EnableTraceEx(&guidProvider, &m_guidSession, m_hTraceSession, FALSE, 0, 0, 0, 0, NULL);
            RETURN_IF_FAILED ((ullResult != ERROR_SUCCESS ? HRESULT_FROM_WIN32(ullResult) : S_OK));
        }

        (void)ControlTrace(m_hTraceSession, m_pwzSessionName, m_pTraceProperties, EVENT_TRACE_CONTROL_FLUSH);
        ullResult = ControlTrace(m_hTraceSession, m_pwzSessionName, m_pTraceProperties, EVENT_TRACE_CONTROL_STOP);
        RETURN_IF_FAILED ((ullResult != ERROR_SUCCESS ? HRESULT_FROM_WIN32(ullResult) : S_OK));
        m_hTraceSession = INVALID_PROCESSTRACE_HANDLE;
    }
    m_eState = CameraStatConsumerState_Stopped;

    return S_OK;
}

HRESULT
CameraStatConsumer::InternalShutdown(
    )
{
    // Avoid using the RETURN_IF_FAILED macro here since we may be
    // shutdown multiple times (no need to spam).
    if (MF_E_SHUTDOWN == CheckShutdown())
    {
        return MF_E_SHUTDOWN;
    }

    m_eState = CameraStatConsumerState_Shutdown;

    if (m_fCoInit)
    {
        CoUninitialize();
    }
    if (m_fMfStartup)
    {
        (void) MFShutdown();
    }

    return S_OK;
}
