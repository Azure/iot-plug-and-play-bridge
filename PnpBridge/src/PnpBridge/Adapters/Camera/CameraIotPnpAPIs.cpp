#include "pch.h"
#include "CameraIotPnpAPIs.h"

DISCOVERY_ADAPTER CameraPnpAdapter = {
    "camera-health-monitor",
    CameraPnpStartDiscovery,
    CameraPnpStopDiscovery
};

PNP_ADAPTER CameraPnpInterface = {
    "camera-health-monitor",
    CameraPnpInterfaceInitialize,
    CameraPnpInterfaceBind,
    CameraPnpInterfaceRelease,
    CameraPnpInterfaceShutdown
};

// Forward decls for callback functions:
void 
CameraPnpCallback_TakePhoto(
    const PNP_CLIENT_COMMAND_REQUEST* pnpClientCommandContext, 
    PNP_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, 
    void* userContextCallback
    )
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)userContextCallback;
    std::string         strResponse;

    UNREFERENCED_PARAMETER (pnpClientCommandContext);

    if (nullptr == p)
    {
        // Nothing we can do, we need the context!
        pnpClientCommandResponseContext->version            = PNP_CLIENT_COMMAND_RESPONSE_VERSION_1;
        pnpClientCommandResponseContext->status             = 500;
        pnpClientCommandResponseContext->responseDataLen    = 0;
    }
    else
    {
        pnpClientCommandResponseContext->version            = PNP_CLIENT_COMMAND_RESPONSE_VERSION_1;
        pnpClientCommandResponseContext->status             = (S_OK == p->TakePhotoOp(strResponse) ? 200 : 500);
        pnpClientCommandResponseContext->responseDataLen    = 0;
    }
}

static const char* s_CameraPnpCommandNames[] = {
    "takephoto"
};

static const PNP_COMMAND_EXECUTE_CALLBACK s_CameraPnpCommandCallbacks[] = {
    CameraPnpCallback_TakePhoto
};

static const PNP_CLIENT_COMMAND_CALLBACK_TABLE s_CameraPnpCommandTable = 
{
    PNP_CLIENT_COMMAND_CALLBACK_VERSION_1, // version of structure
    sizeof(s_CameraPnpCommandNames) / sizeof(s_CameraPnpCommandNames[0]),
    (const char**)s_CameraPnpCommandNames,
    (const PNP_COMMAND_EXECUTE_CALLBACK*)s_CameraPnpCommandCallbacks
};


// Camera discovery API entry points.
CameraPnpDiscovery*  g_pPnpDiscovery = nullptr;

int
CameraPnpStartDiscovery(
    _In_ PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback,
    _In_ const char* deviceArgs,
    _In_ const char* adapterArgs
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

    RETURN_IF_FAILED (p->InitializePnpDiscovery(DeviceChangeCallback, deviceArgs, adapterArgs));

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
    _In_ const char* adapterArgs
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
    PNP_INTERFACE_CLIENT_HANDLE         pnpInterfaceClient;
    const char*                         interfaceId;
    JSON_Value*                         jmsg;
    JSON_Object*                        jobj;


    if (nullptr == Interface || nullptr == payload || nullptr == payload->Context)
    {
        return E_INVALIDARG;
    }

    p = (CameraPnpDiscovery*)payload->Context;
    RETURN_IF_FAILED (p->GetFirstCamera(cameraName));

    // Make the camera IoT device early so we can pass it to the
    // interface client create call.
    pIotPnp = std::make_unique<CameraIotPnpDevice>();

    jmsg =  json_parse_string(payload->Message);
    jobj = json_value_get_object(jmsg);

    interfaceId = "http://windows.com/camera_health_monitor/1.0.0"; //json_object_get_string(jobj, "InterfaceId");
    pnpInterfaceClient = PnP_InterfaceClient_Create(pnpDeviceClientHandle, interfaceId, nullptr, &s_CameraPnpCommandTable, pIotPnp.get());
    RETURN_HR_IF_NULL (E_UNEXPECTED, pnpInterfaceClient);

    PnpAdapter_SetPnpInterfaceClient(Interface, pnpInterfaceClient);

    RETURN_IF_FAILED (pIotPnp->Initialize(PnpAdapter_GetPnpInterfaceClient(Interface), pnpDeviceClientHandle, nullptr /* cameraName.c_str() */));
    RETURN_IF_FAILED (pIotPnp->StartTelemetryWorker());

    RETURN_HR_IF (E_UNEXPECTED, 0 != PnpAdapter_SetContext(Interface, (void*)pIotPnp.get()));

    // Our interface context now owns the object.
    pIotPnp.release();

    return S_OK;
}

int
CameraPnpInterfaceRelease(
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

