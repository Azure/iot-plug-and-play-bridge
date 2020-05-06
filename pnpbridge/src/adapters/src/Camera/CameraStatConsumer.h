//*@@@+++@@@@*******************************************************************
//
// Microsoft Windows Media
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@*******************************************************************
#pragma once

#include "pch.h"

// The EVENT_TRACE_PROPERTIES structure has to have the log session name immediately
// following it.  So we need to allocate a large enough chunk of memory to store both
// the EVENT_TRACE_PROPERTIES + logger name.
#define SIZE_OF_TRACEPROPERTY_BUFFER    (sizeof(EVENT_TRACE_PROPERTIES) + (sizeof(WCHAR) * (MAX_PATH + 1)))

typedef enum
{
    CameraStatConsumerState_Stopped = 0,
    CameraStatConsumerState_Running,
    CameraStatConsumerState_Shutdown
} CameraStatConsumerState_e;

class CameraStatConsumer
{
public:
    CameraStatConsumer();
    virtual ~CameraStatConsumer();

    /// Public methods.
    HRESULT                                             Initialize(_In_opt_z_ LPCWSTR SessionName, _In_ REFGUID guidSession, _In_opt_z_ LPCWSTR SymbolicName);
    HRESULT                                             AddProvider(_In_ REFGUID guidProvider);
    HRESULT                                             Start();
    HRESULT                                             Stop();
    void                                                Shutdown();
    HRESULT                                             PreStats(_Inout_ FSStatisticsEntry& stats_pre, _Out_ LONGLONG* llPreTs);
    HRESULT                                             PostStats(_Inout_ FSStatisticsEntry& stats_post, _Out_ LONGLONG* llPostTs);
protected:

    static void  WINAPI                                 ProcessETWEventCallback(_In_ PEVENT_RECORD precord);
    static DWORD WINAPI                                 ETWProcessThreadProc(_In_opt_ PVOID pv);
    virtual HRESULT                                     AddStat(_In_ FSStatisticsEntry* pstats, _In_opt_z_ LPCWSTR DeviceSymbolicName);
    HRESULT                                             InternalStart();
    HRESULT                                             InternalStop();
    HRESULT                                             InternalShutdown();
    __inline HRESULT                                    CheckShutdown() const { return (m_eState == CameraStatConsumerState_Shutdown ? MF_E_SHUTDOWN : S_OK); }


    CritSec                                             m_lock;
    std::list<GUID>                                     m_listProviders;
    TRACEHANDLE                                         m_hTrace;
    TRACEHANDLE                                         m_hTraceSession;
    CameraStatConsumerState_e                           m_eState;
    BYTE                                                m_buffer[SIZE_OF_TRACEPROPERTY_BUFFER];
    PEVENT_TRACE_PROPERTIES                             m_pTraceProperties;
    LPWSTR                                              m_pwzSessionName;                           // Pointer into the m_buffer at the start of the session name string (must be null terminated).
    GUID                                                m_guidSession;
    EVENT_TRACE_LOGFILE                                 m_TraceLogfile;
    HANDLE                                              m_hFlushThread;
    FSStatisticsEntry                                   m_stats;
    std::vector<HCMNOTIFICATION>                        m_vecWatcherHandles;
    std::wstring                                        m_SymbolicLinkName;
    std::wstring                                        m_SymbolicLinkNameReceived;
    bool                                                m_fCoInit;
    bool                                                m_fMfStartup;
    std::unique_ptr<BYTE[]>                             m_jpegFrame;
    size_t                                              m_jpegFrameSize;
};
