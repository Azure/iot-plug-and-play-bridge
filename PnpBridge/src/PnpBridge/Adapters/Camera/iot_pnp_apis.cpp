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

// Interface bind functions.
int
CameraPnpInterfaceInitialize(
    _In_ JSON_Object* adapterArgs
    )
{
    return S_OK;
}

int
CameraPnpInterfaceShutdown(
    )
{
    return S_OK;
}

int
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

    return S_OK;
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

    return S_OK;
}

