// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

#ifndef PCH_H
#define PCH_H

// TODO: add headers that you want to pre-compile here
#include <Windows.h>
#include <ks.h>
#include <ksmedia.h>
#include <evntcons.h>
#include <evntrace.h>
#include <strsafe.h>
#include <memory>
#include <list>
#include <vector>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <mfidl.h>

// PnpBridge headers.
#include <DiscoveryAdapterInterface.h>

// IOT Hub Headers here...
#include <iothub.h>
#include <iothub_device_client.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>

// IOT PnP Headers here...
#include <pnp_device_client.h>
#include <pnp_interface_client.h>
#include <azure_c_shared_utility/lock.h>

// JSON stuff
#include <parson.h>

// Internal stuff???
#include <internal/pnp_client_core.h>

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

// GUID for the frame server event provider.  This can be hardcoded since
// the provider GUID cannot change over Windows iteration.
extern GUID g_FrameServerEventProvider;

// To avoid including any MF headers.
#ifndef MF_E_SHUTDOWN
#define MF_E_SHUTDOWN                    _HRESULT_TYPEDEF_(0xC00D3E85L)
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

__inline HRESULT HResultFromPnpClient(_In_ PNP_CLIENT_RESULT r)
{
    switch (r)
    {
    case PNP_CLIENT_OK:
        return S_OK;
    case PNP_CLIENT_ERROR_INVALID_ARG:
        return E_INVALIDARG;
    case PNP_CLIENT_ERROR_OUT_OF_MEMORY:
        return E_OUTOFMEMORY;
    case PNP_CLIENT_ERROR_NOT_REGISTERED:
        return HRESULT_FROM_WIN32(ERROR_ADDRESS_NOT_ASSOCIATED);
    case PNP_CLIENT_ERROR_REGISTRATION_PENDING:
        return HRESULT_FROM_WIN32(ERROR_CONNECTION_ACTIVE);
    case PNP_CLIENT_ERROR_INTERFACE_ALREADY_REGISTERED:
        return HRESULT_FROM_WIN32(ERROR_ADDRESS_ALREADY_ASSOCIATED);
    case PNP_CLIENT_ERROR_COMMAND_NOT_PRESENT:
    case PNP_CLIENT_ERROR_INTERFACE_NOT_PRESENT:
        return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
    case PNP_CLIENT_ERROR_SHUTTING_DOWN:
        return SEC_E_SHUTDOWN_IN_PROGRESS;
    case PNP_CLIENT_ERROR_TIMEOUT:
        return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    case PNP_CLIENT_ERROR:
    default:
        return E_UNEXPECTED;
    }
}

__inline HRESULT HResultFromPnpInterfaceStatus(_In_ PNP_REPORTED_INTERFACES_STATUS r)
{
    switch (r)
    {
    case PNP_REPORTED_INTERFACES_OK:
        return S_OK;
    case PNP_REPORTED_INTERFACES_ERROR_HANDLE_DESTROYED:
        return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
    case PNP_REPORTED_INTERFACES_ERROR_OUT_OF_MEMORY:
        return E_OUTOFMEMORY;
    case PNP_REPORTED_INTERFACES_ERROR_TIMEOUT:
        return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    case PNP_REPORTED_INTERFACES_ERROR:
    default:
        return E_UNEXPECTED;
    }
}

// Return the QPC in 100ns units.
LONGLONG
GetQPCInHns(
    );

#include "CameraStatConsumer.h"
#include "CameraIotPnpDevice.h"
#include "CameraPnpDiscovery.h"

#endif //PCH_H
