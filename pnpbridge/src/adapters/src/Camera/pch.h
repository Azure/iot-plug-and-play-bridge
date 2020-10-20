// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PCH_H
#define PCH_H

#include <Windows.h>
#include <ks.h>
#include <ksmedia.h>
#include <evntcons.h>
#include <evntrace.h>
#include <strsafe.h>
#include <memory>
#include <list>
#include <mutex>
#include <vector>
#include <string>
#include <cfgmgr32.h>
#include <initguid.h>
#include <guiddef.h>
#include <devpkey.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfcaptureengine.h>
#include <functional>

// Need ComPtr.
#include <wrl\client.h>
#include <wrl\event.h>
#include <wrl\wrappers\corewrappers.h>
#include <roapi.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

// PnpBridge headers.
#include <pnpadapter_api.h>

// IOT Hub Headers
#include <iothub.h>
#include <iothub_device_client.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>

// IOT PnP Headers
#include <azure_c_shared_utility/lock.h>
#include "azure_c_shared_utility/condition.h"
#include "azure_c_shared_utility/xlogging.h"

#include <parson.h>


#define RETURN_IF_FAILED(hr)                                                                            \
    {                                                                                                   \
        HRESULT __hrRet = hr;                                                                           \
        if (FAILED(__hrRet))                                                                            \
        {                                                                                               \
            LogError("[ERROR]%s:%d - hr=0x%08X\n", __FUNCTION__, __LINE__, hr);                         \
            return __hrRet;                                                                             \
        }                                                                                               \
    }                                                                                                   \

#define RETURN_HR_IF(hr, condition)                                                                     \
    {                                                                                                   \
        if ((condition))                                                                                \
        {                                                                                               \
            LogError("[ERROR]%s:%d - hr=0x%08X\n", __FUNCTION__, __LINE__, hr);                         \
            return hr;                                                                                  \
        }                                                                                               \
    }                                                                                                   \

#define RETURN_HR_IF_NULL(hr, ptr)                                                                      \
    {                                                                                                   \
        if ((ptr) == nullptr)                                                                           \
        {                                                                                               \
            LogError("[ERROR]%s:%d - hr=0x%08X\n", __FUNCTION__, __LINE__, hr);                         \
            return hr;                                                                                  \
        }                                                                                               \
    }                                                                                                   \

////
//// helpers for waiting on async calls
////
#define AwaitTypedResult(op,type,result) [&op, &result]() -> HRESULT                                                            \
{                                                                                                                               \
    HRESULT hr;                                                                                                                 \
    Event threadCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));          \
    ComPtr<IAsyncOperationCompletedHandler<type>> cb                                                                            \
    = Callback<Implements<RuntimeClassFlags<ClassicCom>, IAsyncOperationCompletedHandler<type>, FtmBase>>(                      \
        [&threadCompleted](IAsyncOperation<type>* asyncOperation, AsyncStatus status)->HRESULT                                  \
    {                                                                                                                           \
        UNREFERENCED_PARAMETER(asyncOperation);                                                                                 \
        UNREFERENCED_PARAMETER(status);                                                                                         \
        SetEvent(threadCompleted.Get());                                                                                        \
        return S_OK;                                                                                                            \
    });                                                                                                                         \
    op->put_Completed(cb.Get());                                                                                                \
    WaitForSingleObject(threadCompleted.Get(), INFINITE);                                                                       \
    hr = op->GetResults(&result);                                                                                               \
    return hr;                                                                                                                  \
} ();

#define Await(op,result) [&op, &result]() -> HRESULT                                                                            \
{                                                                                                                               \
    HRESULT hr;                                                                                                                 \
    Event threadCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));          \
    ComPtr<IAsyncOperationCompletedHandler<decltype(result.Get())>> cb                                                          \
    = Callback<Implements<RuntimeClassFlags<ClassicCom>, IAsyncOperationCompletedHandler<decltype(result.Get())>, FtmBase>>(    \
        [&threadCompleted](IAsyncOperation<decltype(result.Get())>* asyncOperation, AsyncStatus status)->HRESULT                \
    {                                                                                                                           \
        UNREFERENCED_PARAMETER(asyncOperation);                                                                                 \
        UNREFERENCED_PARAMETER(status);                                                                                         \
        SetEvent(threadCompleted.Get());                                                                                        \
        return S_OK;                                                                                                            \
    });                                                                                                                         \
    op->put_Completed(cb.Get());                                                                                                \
    WaitForSingleObject(threadCompleted.Get(), INFINITE);                                                                       \
    hr = op->GetResults(&result);                                                                                               \
    return hr;                                                                                                                  \
} ();

#define AwaitAction(op) [&op]() -> HRESULT                                                                                      \
{                                                                                                                               \
    HRESULT hr;                                                                                                                 \
    Event threadCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));          \
    ComPtr<IAsyncActionCompletedHandler> cb                                                                                     \
    = Callback<Implements<RuntimeClassFlags<ClassicCom>, IAsyncActionCompletedHandler, FtmBase>>(                               \
        [&threadCompleted](decltype(op.Get()) asyncAction, AsyncStatus status)->HRESULT                                         \
    {                                                                                                                           \
        UNREFERENCED_PARAMETER(asyncAction);                                                                                    \
        UNREFERENCED_PARAMETER(status);                                                                                         \
        SetEvent(threadCompleted.Get());                                                                                        \
        return S_OK;                                                                                                            \
    });                                                                                                                         \
    op->put_Completed(cb.Get());                                                                                                \
    WaitForSingleObject(threadCompleted.Get(), INFINITE);                                                                       \
    hr = op->GetResults();                                                                                                      \
    return hr;                                                                                                                  \
} ();


// GUID for the frame server event provider.  This can be hardcoded since
// the provider GUID cannot change over Windows iteration.
extern GUID g_FrameServerEventProvider;

// To avoid including any MF headers.
#ifndef MF_E_SHUTDOWN
#define MF_E_SHUTDOWN                    _HRESULT_TYPEDEF_(0xC00D3E85L)
#endif

#ifndef MF_E_INVALIDMEDIATYPE
#define MF_E_INVALIDMEDIATYPE            _HRESULT_TYPEDEF_(0xC00D36B4L)
#endif

typedef enum
{
    FSCapStatSource_Start = 0,
    FSCapStatSource_KsPin = FSCapStatSource_Start,              // 0
    FSCapStatSource_DMFT,                                       // 1
    FSCapStatSource_MFT0,                                       // 2
    FSCapStatSource_DevStream,                                  // 3
    FSCapStatSource_MJPG,                                       // 4
    FSCapStatSource_XVP,                                        // 5
    FSCapStatSource_FSStream,                                   // 6
    FSCapStatSource_End,                                        // 7
} FSCapStatSource_e;

#pragma pack(4)
typedef struct
{
    UINT32          m_streamid;
    UINT32          m_statsource;
    UINT32          m_flags;
    LONGLONG        m_request;
    LONGLONG        m_input;
    LONGLONG        m_output;
    LONGLONG        m_dropped;
    double          m_delayrmsacc;
    UINT32          m_delayrmscounter;
    UINT32          m_expectedframedelay;
    HRESULT         m_hrstatus;
    UINT32          m_padding;
} FSStatisticsEntry, *PFSStatisticsEntry;
#pragma pack()

interface DECLSPEC_UUID("15D0A3C7-B53E-412E-A19E-A579F50FED60") DECLSPEC_NOVTABLE
    IFSStatCallback : public IUnknown
{
public:
    IFACEMETHOD(OnEventRecord)(_In_ FSStatisticsEntry* pEntry, _In_z_ LPCWSTR SymbolicName, _In_opt_ PVOID pvContext) = 0;
};

class CritSec
{
private:
    CRITICAL_SECTION m_criticalSection;
public:
    CritSec() { InitializeCriticalSection(&m_criticalSection); }
    ~CritSec() { DeleteCriticalSection(&m_criticalSection); }
    _Requires_lock_not_held_(m_criticalSection) _Acquires_lock_(m_criticalSection)
        void Lock() { EnterCriticalSection(&m_criticalSection); }
    _Requires_lock_held_(m_criticalSection) _Releases_lock_(m_criticalSection)
        void Unlock() { LeaveCriticalSection(&m_criticalSection); }
};

class AutoLock
{
protected:
    CritSec *m_pCriticalSection;
public:
    _Acquires_lock_(this->m_pCriticalSection->m_criticalSection)
        AutoLock(CritSec& crit)
    {
        m_pCriticalSection = &crit;
        m_pCriticalSection->Lock();
    }
    _Acquires_lock_(this->m_pCriticalSection->m_criticalSection)
        AutoLock(CritSec* crit)
    {
        m_pCriticalSection = crit;
        m_pCriticalSection->Lock();
    }
    _Releases_lock_(this->m_pCriticalSection->m_criticalSection)
        ~AutoLock()
    {
        m_pCriticalSection->Unlock();
    }
};

__inline HRESULT HResultFromIotHub(_In_ IOTHUB_CLIENT_RESULT r)
{
    switch (r)
    {
    case IOTHUB_CLIENT_OK:
        return S_OK;
    case IOTHUB_CLIENT_INVALID_ARG:
        return E_INVALIDARG;
    case IOTHUB_CLIENT_ERROR:
        return E_UNEXPECTED;
    case IOTHUB_CLIENT_INVALID_SIZE:
        return HRESULT_FROM_WIN32(ERROR_INVALID_BLOCK);
    case IOTHUB_CLIENT_INDEFINITE_TIME:
        return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    default:
        return E_UNEXPECTED;
    }
}

__inline HRESULT HResultFromPnpClient(_In_ IOTHUB_CLIENT_RESULT r)
{
    switch (r)
    {
    case IOTHUB_CLIENT_OK:
        return S_OK;
    case IOTHUB_CLIENT_INVALID_ARG:
        return E_INVALIDARG;
    case IOTHUB_CLIENT_INDEFINITE_TIME:
        return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    case IOTHUB_CLIENT_ERROR:
    default:
        return E_UNEXPECTED;
    }
}

// Return the QPC in 100ns units.
LONGLONG
GetQPCInHns(
    );

#include "CameraCaptureEngineCallbacks.h"
#include "CameraCaptureEngine.h"
#include "JsonWrapper.h"
#include "CameraStatConsumer.h"
#include "CameraIotPnpDevice.h"
#include "CameraPnpDiscovery.h"
#include "NetworkCameraIotPnpDevice.h"

#endif //PCH_H
