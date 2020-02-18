// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "pch.h"


HRESULT
CameraCaptureEngineSampleCallback::CreateInstance(
    _In_ LONG lSkipCount,
    _COM_Outptr_ CameraCaptureEngineSampleCallback** ppCallback
    )
{
    HRESULT                             hr = S_OK;
    CameraCaptureEngineSampleCallback*  pCallback = nullptr;

    RETURN_HR_IF_NULL (E_POINTER, ppCallback);
    *ppCallback = nullptr;
    
    // This should throw if we're OOM.
    pCallback = new CameraCaptureEngineSampleCallback(lSkipCount);
    hr = pCallback->InternalInitialize();
    if (FAILED(hr))
    {
        delete pCallback;
        RETURN_IF_FAILED (hr);
    }
    *ppCallback = pCallback;

    return S_OK;
}

CameraCaptureEngineSampleCallback::CameraCaptureEngineSampleCallback(
    _In_ LONG lSkipCount
    )
:   m_lRef(1)
,   m_hSampleEvent(nullptr)
,   m_lSkipCount(lSkipCount)
,   m_lCounter(0)
{

}

CameraCaptureEngineSampleCallback::~CameraCaptureEngineSampleCallback(
    )
{
    if (nullptr != m_hSampleEvent)
    {
        CloseHandle(m_hSampleEvent);
        m_hSampleEvent = nullptr;
    }

    m_spSample = nullptr;
}

/// IUnknown
IFACEMETHODIMP_(ULONG)
CameraCaptureEngineSampleCallback::AddRef(
    )
{
    return InterlockedIncrement(&m_lRef);
}

IFACEMETHODIMP_(ULONG)
CameraCaptureEngineSampleCallback::Release(
    )
{
    LONG cRef = InterlockedDecrement(&m_lRef);

    if (0 == cRef)
    {
        delete this;
    }

    return(cRef);
}

IFACEMETHODIMP
CameraCaptureEngineSampleCallback::QueryInterface(
    _In_ REFIID riid,
    _Outptr_ LPVOID *ppvObject
    )
{
    RETURN_HR_IF_NULL (E_POINTER, ppvObject);
    *ppvObject = nullptr;

    if ((IID_IUnknown == riid) || (__uuidof(IMFCaptureEngineOnSampleCallback) == riid))
    {
        *ppvObject = static_cast<IMFCaptureEngineOnSampleCallback*>(this);
    }
    else
    {
        return E_NOINTERFACE;
    }

    static_cast<IUnknown*>(*ppvObject)->AddRef();

    return S_OK;
}

/// IMFCaptureEngineOnSampleCallback.
IFACEMETHODIMP
CameraCaptureEngineSampleCallback::OnSample(
    _In_ IMFSample* pSample
    )
{
    if (pSample != nullptr)
    {
        AutoLock    lock(&m_lock);

        RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(ERROR_NOT_READY), m_hSampleEvent);

        if (m_lCounter >= m_lSkipCount)
        {
            m_spSample = pSample;

            RETURN_HR_IF (HRESULT_FROM_WIN32(GetLastError()), !SetEvent(m_hSampleEvent));
        }
        m_lCounter++;
    }

    return S_OK;
}

HRESULT
CameraCaptureEngineSampleCallback::WaitForSample(
    _In_ DWORD dwTimerInMs, 
    _COM_Outptr_opt_ IMFSample** ppSample
    )
{
    DWORD   dwWait = 0;

    RETURN_HR_IF_NULL (E_POINTER, ppSample);
    *ppSample = nullptr;

    dwWait = WaitForSingleObject(m_hSampleEvent, dwTimerInMs);
    switch (dwWait)
    {
    case WAIT_OBJECT_0:
        {
            AutoLock    lock(&m_lock);

            RETURN_HR_IF_NULL (E_UNEXPECTED, m_spSample.Get());
            *ppSample = m_spSample.Detach();
            m_lCounter = 0;
        }
        break;
    case WAIT_TIMEOUT:
    case WAIT_ABANDONED:
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(dwWait));
        break;
    case WAIT_FAILED:
        RETURN_IF_FAILED (HRESULT_FROM_WIN32(GetLastError()));
        break;
    }

    return S_OK;
}

HRESULT
CameraCaptureEngineSampleCallback::InternalInitialize(
    )
{
    m_hSampleEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(GetLastError()), m_hSampleEvent);

    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////// 
/// 
HRESULT
CameraCaptureEngineCallback::CreateInstance(
    _COM_Outptr_ CameraCaptureEngineCallback** ppCallback
    )
{
    HRESULT                         hr = S_OK;
    CameraCaptureEngineCallback*    pCallback = nullptr;

    RETURN_HR_IF_NULL (E_POINTER, ppCallback);
    *ppCallback = nullptr;
    
    // This should throw if we're OOM.
    pCallback = new CameraCaptureEngineCallback();
    hr = pCallback->InternalInitialize();
    if (FAILED(hr))
    {
        delete pCallback;
        RETURN_IF_FAILED (hr);
    }
    *ppCallback = pCallback;

    return S_OK;
}

CameraCaptureEngineCallback::CameraCaptureEngineCallback(
    )
:   m_lRef(1)
,   m_hCEEvent(nullptr)
{

}

CameraCaptureEngineCallback::~CameraCaptureEngineCallback(
    )
{
    if (nullptr != m_hCEEvent)
    {
        CloseHandle(m_hCEEvent);
        m_hCEEvent = nullptr;
    }

    m_listEvents.clear();
}

/// IUnknown
IFACEMETHODIMP_(ULONG)
CameraCaptureEngineCallback::AddRef(
    )
{
    return InterlockedIncrement(&m_lRef);
}

IFACEMETHODIMP_(ULONG)
CameraCaptureEngineCallback::Release(
    )
{
    LONG cRef = InterlockedDecrement(&m_lRef);

    if (0 == cRef)
    {
        delete this;
    }

    return(cRef);
}

IFACEMETHODIMP
CameraCaptureEngineCallback::QueryInterface(
    _In_ REFIID riid,
    _Outptr_ LPVOID *ppvObject
    )
{
    RETURN_HR_IF_NULL (E_POINTER, ppvObject);
    *ppvObject = nullptr;

    if ((IID_IUnknown == riid) || (__uuidof(IMFCaptureEngineOnEventCallback) == riid))
    {
        *ppvObject = static_cast<IMFCaptureEngineOnEventCallback*>(this);
    }
    else
    {
        return E_NOINTERFACE;
    }

    static_cast<IUnknown*>(*ppvObject)->AddRef();

    return S_OK;
}

/// IMFCaptureEngineOnEventCallback.
HRESULT
CameraCaptureEngineCallback::OnEvent(
    _In_ IMFMediaEvent* pEvent
    )
{

    if (nullptr != pEvent)
    {
        AutoLock    lock(&m_lock);

        RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(ERROR_NOT_READY), m_hCEEvent);

        m_listEvents.push_back(ComPtr<IMFMediaEvent>(pEvent));

        RETURN_HR_IF (HRESULT_FROM_WIN32(GetLastError()), !SetEvent(m_hCEEvent));
    }

    return S_OK;
}

HRESULT
CameraCaptureEngineCallback::WaitForEvent(
    _In_ REFGUID guid, 
    _In_ DWORD dwTimerInMs 
    )
{
    DWORD   dwWait = 0;
    bool    fLoop = true;

    do
    {
        dwWait = WaitForSingleObject(m_hCEEvent, dwTimerInMs);
        switch (dwWait)
        {
        case WAIT_OBJECT_0:
            {
                std::list<ComPtr<IMFMediaEvent>>    listEvents;
                GUID                                guidExtendedType = GUID_NULL;

                {
                    AutoLock    lock(&m_lock);

                    m_listEvents.swap(listEvents);
                }

                for (MediaEventListItr itr = listEvents.begin(); itr != listEvents.end(); itr++)
                {
                    RETURN_IF_FAILED ((*itr)->GetExtendedType(&guidExtendedType));
                    if (guidExtendedType == guid)
                    {
                        fLoop = false;
                        break;
                    }
                }
            }
            break;
        case WAIT_TIMEOUT:
        case WAIT_ABANDONED:
            RETURN_IF_FAILED (HRESULT_FROM_WIN32(dwWait));
            break;
        case WAIT_FAILED:
            RETURN_IF_FAILED (HRESULT_FROM_WIN32(GetLastError()));
            break;
        }
    }
    while(fLoop);

    return S_OK;
}

HRESULT
CameraCaptureEngineCallback::InternalInitialize(
    )
{
    m_hCEEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    RETURN_HR_IF_NULL (HRESULT_FROM_WIN32(GetLastError()), m_hCEEvent);

    return S_OK;
}


