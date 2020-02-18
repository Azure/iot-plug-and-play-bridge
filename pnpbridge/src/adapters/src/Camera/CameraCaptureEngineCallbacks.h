// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

typedef std::list<ComPtr<IMFMediaEvent>>                MediaEventList;
typedef std::list<ComPtr<IMFMediaEvent>>::iterator      MediaEventListItr;


class CameraCaptureEngineSampleCallback : public IMFCaptureEngineOnSampleCallback
{
public:
    // lSkipCount is normally 0, this means we track the first frame
    // received.  However, if lSkipCount is non-zero, lSkipCount
    // count of samples will be dropped before we queue a sample.
    static HRESULT CreateInstance(_In_ LONG lSkipCount,
                                  _COM_Outptr_ CameraCaptureEngineSampleCallback** ppCallback);

    ///
    /// IUnknown.
    IFACEMETHOD_(ULONG, AddRef)(void);
    IFACEMETHOD_(ULONG, Release)(void);
    IFACEMETHOD(QueryInterface)(_In_ REFIID riid, _Outptr_ void **ppvObject);

    ///
    /// IMFCaptureEngineOnSampleCallback.
    IFACEMETHOD(OnSample)(_In_ IMFSample* pEvent) override;

    // WaitForSample will block if a sample is not available.  Since samples
    // are tracked by the server, we will only keep the last received sample
    // instead of a queue of samples (to avoid starving the pipeline).
    HRESULT                             WaitForSample(_In_ DWORD dwTimerInMs, _COM_Outptr_ IMFSample** ppSample);

protected:
    CameraCaptureEngineSampleCallback(_In_ LONG lSkipCount);
    virtual ~CameraCaptureEngineSampleCallback();

    virtual HRESULT                     InternalInitialize();

    CritSec                             m_lock;  
    LONG                                m_lRef;
    LONG                                m_lSkipCount;
    LONG                                m_lCounter;
    ComPtr<IMFSample>                   m_spSample;
    HANDLE                              m_hSampleEvent;
};


class CameraCaptureEngineCallback : public IMFCaptureEngineOnEventCallback
{
public:
    static HRESULT CreateInstance(_COM_Outptr_ CameraCaptureEngineCallback** ppCallback);

    ///
    /// IUnknown.
    IFACEMETHOD_(ULONG, AddRef)(void);
    IFACEMETHOD_(ULONG, Release)(void);
    IFACEMETHOD(QueryInterface)(_In_ REFIID riid, _Outptr_ void **ppvObject);

    ///
    /// IMFCaptureEngineOnEventCallback.
    IFACEMETHOD(OnEvent)(_In_ IMFMediaEvent* pEvent) override;

    // WaitForEvent will wait for the requested event until the timer expires.
    // If the event does not fire within the timeout, HRESULT_FROM_WIN32(WAIT_TIMEOUT) will
    // be returned.  If a MF_CAPTURE_ENGINE_ERROR event is received, the WaitForEvent
    // will return immediately with the error code associated with the error event.
    HRESULT                             WaitForEvent(_In_ REFGUID guid, _In_ DWORD dwTimerInMs);

protected:
    CameraCaptureEngineCallback();
    virtual ~CameraCaptureEngineCallback();

    virtual HRESULT                     InternalInitialize();

    CritSec                             m_lock;  
    LONG                                m_lRef;
    MediaEventList                      m_listEvents;
    HANDLE                              m_hCEEvent;
};

