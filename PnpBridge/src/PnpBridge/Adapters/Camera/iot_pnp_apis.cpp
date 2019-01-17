#include "pch.h"
#include "iot_pnp_apis.h"

// Camera discovery API entry points.
CameraPnpDiscovery*  g_pPnpDiscovery = nullptr;

int
CameraPnpStartDiscovery(
    _In_ PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback,
    _In_ JSON_Object* deviceArgs,
    _In_ JSON_Object* adapterArgs
    )
{
    // Avoid using unique_ptr here, we want to play some pointer
    // swapping games...
    CameraPnpDiscovery* p = new CameraPnpDiscovery();

    if (nullptr != InterlockedCompareExchangePointer((PVOID*)&g_pPnpDiscovery, (PVOID)p, nullptr))
    {
        delete p;
        p = g_pPnpDiscovery;
    }

    RETURN_IF_FAILED (p->InitializePnpDiscovery(DeviceChangeCallback, (void*)deviceArgs));

    return 0;
}

int 
CameraPnpStopDiscovery(
    )
{
    CameraPnpDiscovery* p = (CameraPnpDiscovery*) InterlockedExchangePointer((PVOID*)&g_pPnpDiscovery, nullptr);

    if (nullptr != p)
    {
        p->Shutdown();
        delete p;
        p = nullptr;
    }

    return 0;
}

int
CameraGetFilterFormatIds(
    _Out_ char*** filterFormatIds,
    _Out_ int* numberOfFormats
    )
{
    if (nullptr == filterFormatIds || nullptr == numberOfFormats)
    {
        return -1;
    }

    // $$WARNING - We cannot use malloc/new for allocation
    // of format IDs.  Either we have the numberOfFormats be
    // an IN/OUT or we add a new API to get the number of
    // format IDs along with their lengths.
    *filterFormatIds = (char**) malloc(sizeof(char*) * 1);
    (*filterFormatIds)[0] = (char*) "camera-pnp-module";
    *numberOfFormats = 1;

    return 0;
}

// Interface bind functions.
__inline
PNPBRIDGE_RESULT PnpBridgeFromHresult(
    _In_ HRESULT hr
    )
{
    switch (hr)
    {
    case S_OK:
        return PNPBRIDGE_OK;
    case HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER):
    case E_OUTOFMEMORY:
        return PNPBRIDGE_INSUFFICIENT_MEMORY;
    case E_INVALIDARG:
        return PNPBRIDGE_INVALID_ARGS;
    case HRESULT_FROM_WIN32(ERROR_GEN_FAILURE):
        return PNPBRIDGE_CONFIG_READ_FAILED;
    default:
        return PNPBRIDGE_FAILED;
    }
}

PNPBRIDGE_RESULT
CameraPnpInterfaceInitialize(
    _In_ JSON_Object* adapterArgs
    )
{
    return PnpBridgeFromHresult(S_OK);
}

PNPBRIDGE_RESULT
CameraPnpInterfaceShutdown(
    )
{
    return PnpBridgeFromHresult(S_OK);
}

PNPBRIDGE_RESULT
CameraPnpInterfaceBind(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface, 
    _In_ PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, 
    _In_ PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload
    )
{
    CameraPnpDiscovery*                 p = nullptr;
    std::wstring                        cameraName;
    std::unique_ptr<CameraIotPnpDevice> pIotPnp;

    if (nullptr == Interface || nullptr == payload || nullptr == payload->Context)
    {
        return E_INVALIDARG;
    }

    p = (CameraPnpDiscovery*)payload->Context;
    RETURN_IF_FAILED (p->GetFirstCamera(cameraName));

    pIotPnp = std::make_unique<CameraIotPnpDevice>();
    RETURN_IF_FAILED (pIotPnp->Initialize(PnpAdapter_GetPnpInterface(Interface), cameraName.c_str()));
    RETURN_IF_FAILED (pIotPnp->StartTelemetryWorker());

    if (0 == PnpAdapter_SetContext(Interface, (void*)pIotPnp.get()))
    {
        // Our interface context now owns the object.
        pIotPnp.release();
        return 0;
    }
    else
    {
        RETURN_IF_FAILED (E_UNEXPECTED);
    }

    return PnpBridgeFromHresult(S_OK);
}

PNPBRIDGE_RESULT
CameraPnpInterfaceRelease(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface
    )
{
    return PnpBridgeFromHresult(S_OK);
}

int
CameraBindPnpInterface(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface,
    _In_ PDEVICE_CHANGE_PAYLOAD payload
    )
{
    CameraPnpDiscovery*                 p = nullptr;
    std::wstring                        cameraName;
    std::unique_ptr<CameraIotPnpDevice> pIotPnp;

    if (nullptr == Interface || nullptr == payload || nullptr == payload->Context)
    {
        return E_INVALIDARG;
    }

    p = (CameraPnpDiscovery*)payload->Context;
    RETURN_IF_FAILED (p->GetFirstCamera(cameraName));

    pIotPnp = std::make_unique<CameraIotPnpDevice>();
    RETURN_IF_FAILED (pIotPnp->Initialize(PnpAdapter_GetPnpInterface(Interface), cameraName.c_str()));
    RETURN_IF_FAILED (pIotPnp->StartTelemetryWorker());

    if (0 == PnpAdapter_SetContext(Interface, (void*)pIotPnp.get()))
    {
        // Our interface context now owns the object.
        pIotPnp.release();
        return 0;
    }
    else
    {
        RETURN_IF_FAILED (E_UNEXPECTED);
    }
}

int
CameraReleasePnpInterface(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface
    )
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)PnpAdapter_GetContext(Interface);

    if (p != nullptr)
    {
        // This will block until the worker is stopped.
        (VOID) p->StopTelemetryWorker();
        p->Shutdown();

        delete p;
    }

    // Clear our context to make sure we don't keep a stale
    // object around.
    (void) PnpAdapter_SetContext(Interface, nullptr);

    return 0;
}

int
CameraGetPnpFilterFormatIds(
    _Out_ char*** filterFormatIds,
    _Out_ int* numberOfFormats
    )
{
    return CameraGetFilterFormatIds(filterFormatIds, numberOfFormats);
}

int
CameraPnpInterfaceModuleSupportsInterfaceId(
    _In_z_ char* interfaceId,
    _Out_ bool* supported
    )
{
    return 0;

}
