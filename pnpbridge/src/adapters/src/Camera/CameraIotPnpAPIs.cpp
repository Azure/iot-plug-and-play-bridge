// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "pch.h"
#include "CameraIotPnpAPIs.h"

DISCOVERY_ADAPTER CameraPnpAdapter = {
    "camera-discovery-adapter",
    CameraPnpStartDiscovery,
    CameraPnpStopDiscovery
};

PNP_ADAPTER CameraPnpInterface = {
    "camera-pnp-adapter",
    CameraPnpInterfaceInitialize,
    CameraPnpInterfaceBind,
    CameraPnpInterfaceShutdown
};

void 
CameraPnpCallback_ProcessCommandUpdate(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* dtCommandRequest,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* dtCommandResponse,
    void* userInterfaceContext
    ) 
{
    if (strcmp(dtCommandRequest->commandName, "TakePhoto") == 0) 
    {
        CameraPnpCallback_TakePhoto(dtCommandRequest, dtCommandResponse, userInterfaceContext);
    }
    else if (strcmp(dtCommandRequest->commandName, "TakeVideo") == 0)
    {
        CameraPnpCallback_TakeVideo(dtCommandRequest, dtCommandResponse, userInterfaceContext);
    }
    else if (strcmp(dtCommandRequest->commandName, "GetURI") == 0)
    {
        CameraPnpCallback_GetURI(dtCommandRequest, dtCommandResponse, userInterfaceContext);
    }
    //else if (strcmp(dtCommandRequest->commandName, "StartDetection") == 0) 
    //{
    //    CameraPnpCallback_StartDetection(dtCommandRequest, dtCommandResponse, userInterfaceContext);
    //}
    else 
    {
        LogError("Unknown command request");
    }
}

// Forward decls for callback functions:
void CameraPnpCallback_TakePhoto(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext, 
    void* userContextCallback)
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)userContextCallback;
    std::string         strResponse;

    UNREFERENCED_PARAMETER (pnpClientCommandContext);

    if (nullptr == p)
    {
        // Nothing we can do, we need the context!
        pnpClientCommandResponseContext->version            = DIGITALTWIN_CLIENT_COMMAND_REQUEST_VERSION_1;
        pnpClientCommandResponseContext->status             = 500;
        pnpClientCommandResponseContext->responseDataLen    = 0;
    }
    else
    {
        pnpClientCommandResponseContext->version            = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
        pnpClientCommandResponseContext->status             = (S_OK == p->TakePhotoOp(strResponse) ? 200 : 500);
        char* responseBuf = NULL;
        mallocAndStrcpy_s(&responseBuf, strResponse.c_str());
        pnpClientCommandResponseContext->responseData = (unsigned char*)responseBuf;
        pnpClientCommandResponseContext->responseDataLen = strlen(responseBuf);
    }
}

void CameraPnpCallback_TakeVideo(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    void* userContextCallback)
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)userContextCallback;
    std::string         strResponse;

    UNREFERENCED_PARAMETER(pnpClientCommandContext);

    if (nullptr == p)
    {
        // Nothing we can do, we need the context!
        pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_REQUEST_VERSION_1;
        pnpClientCommandResponseContext->status = 500;
        pnpClientCommandResponseContext->responseDataLen = 0;
    }
    else
    {
        pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
        pnpClientCommandResponseContext->status = (S_OK == p->TakeVideoOp(10000, strResponse) ? 200 : 500);
        char* responseBuf = NULL;
        mallocAndStrcpy_s(&responseBuf, strResponse.c_str());
        pnpClientCommandResponseContext->responseData = (unsigned char*)responseBuf;
        pnpClientCommandResponseContext->responseDataLen = strlen(responseBuf);
    }
}

void CameraPnpCallback_GetURI(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    void* userContextCallback)
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)userContextCallback;
    std::string         strResponse;

    UNREFERENCED_PARAMETER(pnpClientCommandContext);

    if (nullptr == p)
    {
        // Nothing we can do, we need the context!
        pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_REQUEST_VERSION_1;
        pnpClientCommandResponseContext->status = 500;
        pnpClientCommandResponseContext->responseDataLen = 0;
    }
    else
    {
        pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
        pnpClientCommandResponseContext->status = (S_OK == p->GetURIOp(strResponse) ? 200 : 500);
        char* responseBuf = NULL;
        mallocAndStrcpy_s(&responseBuf, strResponse.c_str());
        pnpClientCommandResponseContext->responseData = (unsigned char*)responseBuf;
        pnpClientCommandResponseContext->responseDataLen = strlen(responseBuf);
    }
}

/*
void
CameraPnpCallback_StartDetection(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* pnpClientCommandContext,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* pnpClientCommandResponseContext,
    void* userContextCallback
)
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)userContextCallback;
    std::string         strResponse;

    UNREFERENCED_PARAMETER(pnpClientCommandContext);

    if (nullptr == p)
    {
        // Nothing we can do, we need the context!
        pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_REQUEST_VERSION_1;
        pnpClientCommandResponseContext->status = 500;
        pnpClientCommandResponseContext->responseDataLen = 0;
    }
    else
    {
        pnpClientCommandResponseContext->version = DIGITALTWIN_CLIENT_COMMAND_RESPONSE_VERSION_1;
        pnpClientCommandResponseContext->status = (S_OK == p->StartDetection() ? 200 : 500);
        pnpClientCommandResponseContext->responseDataLen = 0;
    }
}
*/

// Camera discovery API entry points.
CameraPnpDiscovery*  g_pPnpDiscovery = nullptr;

int CameraPnpStartDiscovery(
    _In_ PNPMEMORY deviceArgs,
    _In_ PNPMEMORY adapterArgs)
{
    // Avoid using unique_ptr here, we want to play some pointer
    // swapping games...
    CameraPnpDiscovery* p = new CameraPnpDiscovery();

    if (nullptr != InterlockedCompareExchangePointer((PVOID*)&g_pPnpDiscovery, (PVOID)p, nullptr))
    {
        delete p;
        p = g_pPnpDiscovery;
    }

    RETURN_IF_FAILED (p->InitializePnpDiscovery(DiscoveryAdapter_ReportDevice, deviceArgs, adapterArgs));

    return DIGITALTWIN_CLIENT_OK;
}

int CameraPnpStopDiscovery()
{
    CameraPnpDiscovery* p = (CameraPnpDiscovery*) InterlockedExchangePointer((PVOID*)&g_pPnpDiscovery, nullptr);

    if (nullptr != p)
    {
        p->Shutdown();
        delete p;
        p = nullptr;
    }

    return DIGITALTWIN_CLIENT_OK;
}

// Interface bind functions.
int CameraPnpInterfaceInitialize(_In_ const char* adapterArgs)
{
    UNREFERENCED_PARAMETER(adapterArgs);
    return DIGITALTWIN_CLIENT_OK;
}

int
CameraPnpInterfaceShutdown()
{
    return DIGITALTWIN_CLIENT_OK;
}

int CameraPnpInterfaceBind(
    _In_ PNPADAPTER_CONTEXT adapterHandle,
    _In_ PNPMESSAGE payload)
{
    CameraPnpDiscovery*                 p = nullptr;
    std::wstring                        cameraName;
    std::unique_ptr<CameraIotPnpDevice> pIotPnp;
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE         pnpInterfaceClient;
    const char*                         interfaceId;
    const char*                         camera_id;
    JSON_Value*                         jmsg;
    JSON_Object*                        jobj;
    JSON_Object*                        jMatchParametersObj;

    PNPADAPTER_INTERFACE_HANDLE         adapterInterface = nullptr;
    PNPMESSAGE_PROPERTIES*              pnpMsgProps = nullptr;
    DIGITALTWIN_CLIENT_RESULT           dtRes;
    PNPMESSAGE_PROPERTIES*              props;

    pnpMsgProps = PnpMessage_AccessProperties(payload);

    if (nullptr == payload || nullptr == pnpMsgProps || nullptr == pnpMsgProps->Context)
    {
        return E_INVALIDARG;
    }

    jmsg =  json_parse_string(PnpMessage_GetMessage(payload));
    jobj = json_value_get_object(jmsg);
    interfaceId = PnpMessage_GetInterfaceId(payload);
    props = PnpMessage_AccessProperties(payload);

    jMatchParametersObj = json_object_get_object(jobj, "match_parameters");
    camera_id = json_object_get_string(jMatchParametersObj, "camera_id");

    p = (CameraPnpDiscovery*)pnpMsgProps->Context;
    std::string cameraIdStr(camera_id);
    RETURN_IF_FAILED(p->GetUniqueIdByCameraId(cameraIdStr, cameraName));
    
    // For now, suppose IP Camera IDs will begin with "uuid", while other camera names look like "USB\VID###" (for usb cameras)
    char type[5] = {};
    strncpy(type, camera_id, 4);
    if (strcmp("uuid", type) == 0)
    {
        pIotPnp = std::make_unique<NetworkCameraIotPnpDevice>(cameraName);
    }
    else
    {
        pIotPnp = std::make_unique<CameraIotPnpDevice>(cameraName);
    }

    //s_CameraPnpCommandTable
    dtRes = DigitalTwin_InterfaceClient_Create(interfaceId, props->ComponentName,
                nullptr, pIotPnp.get(), &pnpInterfaceClient);
    RETURN_HR_IF(E_UNEXPECTED, DIGITALTWIN_CLIENT_OK != dtRes);

    dtRes = DigitalTwin_InterfaceClient_SetCommandsCallback(pnpInterfaceClient, CameraPnpCallback_ProcessCommandUpdate, (void*)pIotPnp.get());
    RETURN_HR_IF(E_UNEXPECTED, DIGITALTWIN_CLIENT_OK != dtRes);

    // Create PnpAdapter Interface
    PNPADPATER_INTERFACE_PARAMS interfaceParams = { 0 };
    PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, adapterHandle, pnpInterfaceClient);
    interfaceParams.ReleaseInterface = CameraPnpInterfaceRelease;
    interfaceParams.StartInterface = CamerePnpStartPnpInterface;
    interfaceParams.InterfaceId = (char*)interfaceId;

    RETURN_HR_IF (E_UNEXPECTED, 0 != PnpAdapterInterface_Create(&interfaceParams, &adapterInterface))

    RETURN_IF_FAILED(pIotPnp->Initialize(PnpAdapterInterface_GetPnpInterfaceClient(adapterInterface)));
    RETURN_IF_FAILED (pIotPnp->StartTelemetryWorker());

    RETURN_HR_IF (E_UNEXPECTED, 0 != PnpAdapterInterface_SetContext(adapterInterface, (void*)pIotPnp.get()));

    // Our interface context now owns the object.
    pIotPnp.release();

    return DIGITALTWIN_CLIENT_OK;
}

int
CameraPnpInterfaceRelease(
    _In_ PNPADAPTER_INTERFACE_HANDLE Interface
    )
{
    CameraIotPnpDevice* p = (CameraIotPnpDevice*)PnpAdapterInterface_GetContext(Interface);

    if (p != nullptr)
    {
        // This will block until the worker is stopped.
        (VOID) p->StopTelemetryWorker();
        p->Shutdown();

        delete p;
    }

    // Clear our context to make sure we don't keep a stale
    // object around.
    (void) PnpAdapterInterface_SetContext(Interface, nullptr);

    PnpAdapterInterface_Destroy(Interface);

    return DIGITALTWIN_CLIENT_OK;
}

int
CamerePnpStartPnpInterface(
    _In_ PNPADAPTER_INTERFACE_HANDLE PnpInterface
    )
{
    AZURE_UNREFERENCED_PARAMETER(PnpInterface);
    
    return DIGITALTWIN_CLIENT_OK;
}

