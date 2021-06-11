#include "impinj_reader_r700.h"
#include "helpers/restapi.h"
#include "helpers/pnp_utils.h"
#include "helpers/pnp_property.h"
#include "helpers/pnp_command.h"
#include "helpers/pnp_telemetry.h"
#include "helpers/led.h"

MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(R700_REST_VERSION, R700_REST_VERSION_VALUES);

/****************************************************************
A callback for Property Update (DEVICE_TWIN_UPDATE_COMPLETE)
****************************************************************/
bool ImpinjReader_OnPropertyCompleteCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    JSON_Value* Payload,
    void* UserContextCallback)
{
    LogInfo("R700 : %s() enter", __FUNCTION__);
    return OnPropertyCompleteCallback(PnpComponentHandle, Payload, UserContextCallback);
}

/****************************************************************
A callback for Property Update (DEVICE_TWIN_UPDATE_PARTIAL)
****************************************************************/
void ImpinjReader_OnPropertyPatchCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int Version,
    void* ClientHandle)
{
    LogInfo("R700 : %s() enter", __FUNCTION__);
    OnPropertyPatchCallback(PnpComponentHandle, PropertyName, PropertyValue, Version, ClientHandle);
    return;
}

/****************************************************************
A callback for Direct Method
****************************************************************/
int ImpinjReader_OnCommandCallback(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    int ret;
    LogInfo("R700 : %s() enter", __FUNCTION__);

    ret = OnCommandCallback(PnpComponentHandle, CommandName, CommandValue, CommandResponse, CommandResponseSize);

    LogInfo("R700 : %s() exit", __FUNCTION__);
    return ret;
}

/****************************************************************
PnP Bridge
****************************************************************/
IOTHUB_CLIENT_RESULT ImpinjReader_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    // Store client handle before starting Pnp component
    device->ClientHandle = PnpComponentHandleGetClientHandle(PnpComponentHandle);

    // Set shutdown state
    device->ShuttingDown = false;
    LogInfo("Impinj Reader: Starting Pnp Component");

    PnpComponentHandleSetContext(PnpComponentHandle, device);

    curlStreamSpawnReaderThread(device->curl_stream_session);

    // Create a thread to periodically publish telemetry
    if (ThreadAPI_Create(&device->WorkerHandle, ImpinjReader_TelemetryWorker, PnpComponentHandle) != THREADAPI_OK)
    {
        LogError("ThreadAPI_Create failed");
        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);

    if (device)
    {
        device->ShuttingDown = true;
        curlStreamStopThread(device->curl_stream_session);
        ThreadAPI_Join(device->WorkerHandle, NULL);
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PIMPINJ_READER device = PnpComponentHandleGetContext(PnpComponentHandle);
    if (device != NULL)
    {
        if (device->SensorState != NULL)
        {
            if (device->SensorState->customerName != NULL)
            {
                if (device->curl_static_session != NULL)
                {
                    curlStreamCleanup(device->curl_stream_session);
                    curlStaticCleanup(device->curl_static_session);
                    curlStaticCleanup(device->curl_polling_session);
                    free(device->curl_static_session);
                }
                free(device->SensorState->customerName);
            }
            free(device->SensorState);
        }
        free(device);

        PnpComponentHandleSetContext(PnpComponentHandle, NULL);
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);

    writeLed(SYSTEM_GREEN, LED_OFF);
    curl_global_cleanup();   // cleanup cURL globally

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT ImpinjReader_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterGlobalConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);

    curl_global_init(CURL_GLOBAL_DEFAULT);   // initialize cURL globally

    // Enable HTTPS on reader RShell
    char * enableRShellCmd = "rshell -c \"config network https enable\"";
    system(enableRShellCmd);
    ThreadAPI_Sleep(1000);

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT
ImpinjReader_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterComponentConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    PIMPINJ_READER device       = NULL;

    /* print component creation message */
    char compCreateStr[128] = {0};

    char* compHostname;
    char* http_user;
    char* http_pass;
    char* http_basepath;

    compHostname = (char*)json_object_dotget_string(AdapterComponentConfig, "hostname");
    http_user    = (char*)json_object_dotget_string(AdapterComponentConfig, "username");
    http_pass    = (char*)json_object_dotget_string(AdapterComponentConfig, "password");

    strcat(compCreateStr, "Creating Impinj Reader component: ");
    strcat(compCreateStr, ComponentName);
    strcat(compCreateStr, "\n       Hostname: ");
    strcat(compCreateStr, compHostname);

    LogInfo("%s", compCreateStr);

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        BridgeComponentHandle = NULL;
        result                = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    /* initialize base HTTP strings */

    char str_http[]     = "https://";
    char str_basepath[] = "/api/v1";

    char build_str_url_always[100] = "";
    strcat(build_str_url_always, str_http);
    strcat(build_str_url_always, compHostname);
    strcat(build_str_url_always, str_basepath);

    mallocAndStrcpy_s(&http_basepath, build_str_url_always);

    /* initialize cURL sessions */
    CURL_Static_Session_Data* curl_polling_session = curlStaticInit(http_user, http_pass, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);
    CURL_Static_Session_Data* curl_static_session  = curlStaticInit(http_user, http_pass, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);
    CURL_Stream_Session_Data* curl_stream_session  = curlStreamInit(http_user, http_pass, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

    device = calloc(1, sizeof(IMPINJ_READER));
    if (NULL == device)
    {

        LogError("Unable to allocate memory for Impinj Reader component.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    device->SensorState = calloc(1, sizeof(IMPINJ_READER_STATE));
    if (NULL == device)
    {
        LogError("Unable to allocate memory for Impinj Reader component state.");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    mallocAndStrcpy_s(&device->SensorState->componentName, ComponentName);

    device->curl_polling_session = curl_polling_session;
    device->curl_static_session  = curl_static_session;
    device->curl_stream_session  = curl_stream_session;
    device->ComponentName        = ComponentName;

    GetFirmwareVersion(device);

    PnpComponentHandleSetContext(BridgeComponentHandle, device);
    PnpComponentHandleSetPropertyPatchCallback(BridgeComponentHandle, ImpinjReader_OnPropertyPatchCallback);
    PnpComponentHandleSetPropertyCompleteCallback(BridgeComponentHandle, ImpinjReader_OnPropertyCompleteCallback);
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, ImpinjReader_OnCommandCallback);

exit:
    return result;
}

PNP_ADAPTER ImpinjReaderR700 = {
    .identity            = "impinj-reader-r700",
    .createAdapter       = ImpinjReader_CreatePnpAdapter,
    .createPnpComponent  = ImpinjReader_CreatePnpComponent,
    .startPnpComponent   = ImpinjReader_StartPnpComponent,
    .stopPnpComponent    = ImpinjReader_StopPnpComponent,
    .destroyPnpComponent = ImpinjReader_DestroyPnpComponent,
    .destroyAdapter      = ImpinjReader_DestroyPnpAdapter};
