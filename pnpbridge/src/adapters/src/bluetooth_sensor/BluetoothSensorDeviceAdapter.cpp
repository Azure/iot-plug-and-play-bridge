// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <azure_c_shared_utility/xlogging.h>

#include "Common.h"
#include "BluetoothSensorDeviceAdapter.h"
#include "BluetoothSensorDiscoveryAdapter.h"

// static
std::unique_ptr<BluetoothSensorDeviceAdapter> BluetoothSensorDeviceAdapter::MakeUnique(
    PNPADAPTER_CONTEXT adapterHandle,
    PNPMESSAGE payload,
    std::shared_ptr<InterfaceDescriptor> interfaceDescriptor)
{
    auto newPnpDevice = std::make_unique<BluetoothSensorDeviceAdapter>();
    newPnpDevice->Initialize(adapterHandle, payload, interfaceDescriptor);
    return newPnpDevice;
}

void BluetoothSensorDeviceAdapter::Initialize(
    PNPADAPTER_CONTEXT adapterHandle,
    PNPMESSAGE payload,
    std::shared_ptr<InterfaceDescriptor> interfaceDescriptor)
{
    m_interfaceDescriptor = interfaceDescriptor;

    auto pnpMsgProps = PnpMessage_AccessProperties(payload);
    if (!pnpMsgProps || !(pnpMsgProps->Context))
    {
        throw std::invalid_argument("Invalid arguments given while initializing PnP device.");
    }

    JsonValuePtr pnpMessageJson(json_parse_string(PnpMessage_GetMessage(payload)), DestroyJsonValue);
    if (!pnpMessageJson)
    {
        throw std::runtime_error("Failed to get PnP message as JSON.");
    }

    JSON_Object* pnpMessageObj = json_value_get_object(pnpMessageJson.get());
    if (!pnpMessageObj)
    {
        throw std::runtime_error("Failed to parse the PnP message as a JSON object.");
    }

    const char* interfaceId = PnpMessage_GetInterfaceId(payload);
    PNPMESSAGE_PROPERTIES* props = PnpMessage_AccessProperties(payload);

    JSON_Object* jMatchParametersObj = json_object_get_object(pnpMessageObj, "match_parameters");
    if (!jMatchParametersObj)
    {
        throw std::runtime_error("Failed to get match_parameters from JSON object.");
    }

    m_bluetoothAddress = std::stoull(json_object_get_string(jMatchParametersObj, "bluetooth_address"));
    LogInfo("Binding %llu", m_bluetoothAddress);

    DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient{};
    DIGITALTWIN_CLIENT_RESULT dtRes = DigitalTwin_InterfaceClient_Create(
        interfaceId,
        props->ComponentName,
        OnDigitalTwinInterfaceClientCreate,
        this,
        &pnpInterfaceClient);
    if (dtRes != DIGITALTWIN_CLIENT_OK)
    {
        throw DigitalTwinException("Failed to create digital twin interface", dtRes);
    }

    dtRes = DigitalTwin_InterfaceClient_SetCommandsCallback(
        pnpInterfaceClient,
        BluetoothSensorPnpCallback_ProcessCommandUpdate,
        (void*)this);
    if (dtRes != DIGITALTWIN_CLIENT_OK)
    {
        throw DigitalTwinException("Failed to set digital twin commands callback", dtRes);
    }

    // Create PnpAdapter Interface
    PNPADPATER_INTERFACE_PARAMS interfaceParams;
    PNPADPATER_INTERFACE_PARAMS_INIT(&interfaceParams, adapterHandle, pnpInterfaceClient);
    interfaceParams.ReleaseInterface = OnBluetoothSensorInterfaceRelease;
    interfaceParams.StartInterface = OnBluetoothSensorInterfaceStart;
    interfaceParams.InterfaceId = const_cast<char*>(interfaceId);

    PNPADAPTER_INTERFACE_HANDLE adapterInterface;
    int pnpRes = PnpAdapterInterface_Create(&interfaceParams, &adapterInterface);
    if (pnpRes != DIGITALTWIN_CLIENT_OK)
    {
        throw AzurePnpException("Failed to create PnP adapter interface", pnpRes);
    }

    m_pnpClientInterface = PnpAdapterInterface_GetPnpInterfaceClient(adapterInterface);

    auto bluetoothAddressMemory = std::make_unique<uint64_t>(GetBluetoothAddress());
    pnpRes = PnpAdapterInterface_SetContext(adapterInterface, bluetoothAddressMemory.release());
    if (pnpRes != DIGITALTWIN_CLIENT_OK)
    {
        throw AzurePnpException("Failed to set PnP adapter context", pnpRes);
    }
}

void BluetoothSensorDeviceAdapter::ParseManufacturerData(const std::vector<unsigned char>& buffer)
{
    auto parsedPayload = m_interfaceDescriptor->GetPayloadParser()->ParsePayload(buffer);
    ReportSensorData(parsedPayload);
}

uint64_t BluetoothSensorDeviceAdapter::GetBluetoothAddress()
{
    return m_bluetoothAddress;
}

void BluetoothSensorDeviceAdapter::BluetoothSensorPnpCallback_ProcessCommandUpdate(
    const DIGITALTWIN_CLIENT_COMMAND_REQUEST* /* clientCommandContext */,
    DIGITALTWIN_CLIENT_COMMAND_RESPONSE* /* clientCommandResponseContext */,
    void* /* userContextCallback */)
{
    // no-op
}

int BluetoothSensorDeviceAdapter::OnBluetoothSensorInterfaceStart(
    PNPADAPTER_INTERFACE_HANDLE /* pnpInterface */)
{
    // no-op
    return DIGITALTWIN_CLIENT_OK;
}

int BluetoothSensorDeviceAdapter::OnBluetoothSensorInterfaceRelease(
    PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface)
{
    auto bluetootAddressDeleter = [pnpAdapterInterface](uint64_t* ptr){
        delete ptr;
        PnpAdapterInterface_SetContext(pnpAdapterInterface, nullptr);
        PnpAdapterInterface_Destroy(pnpAdapterInterface);
    };

    std::unique_ptr<uint64_t, decltype(bluetootAddressDeleter)> bluetoothAddressPtr(
        reinterpret_cast<uint64_t*>(PnpAdapterInterface_GetContext(pnpAdapterInterface)),
        bluetootAddressDeleter);

    auto discoveryAdapter = BluetoothSensorPnpDiscovery::GetShared();
    if (!discoveryAdapter)
    {
        discoveryAdapter->RemoveSensorDevice(*bluetoothAddressPtr);
    }

    return DIGITALTWIN_CLIENT_OK;
}

void BluetoothSensorDeviceAdapter::OnDigitalTwinInterfaceClientCreate(
    DIGITALTWIN_CLIENT_RESULT interfaceStatus,
    void* /* userInterfaceContext */)
{
    if (interfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        // Once the interface is registered, send our reported properties to the service.
        // It *IS* safe to invoke most DigitalTwin API calls from a callback thread like this, though it
        // is NOT safe to create/destroy/register interfaces now.
        LogInfo("Bluetooth Sensor Interface: Interface successfully registered.");
    }
    else if (interfaceStatus == DIGITALTWIN_CLIENT_ERROR_INTERFACE_UNREGISTERING)
    {
        // Once an interface is marked as unregistered, it cannot be used for any DigitalTwin SDK calls.
        LogInfo("Bluetooth Sensor Interface: Interface received unregistering callback.");
    }
    else
    {
        LogError("Bluetooth Sensor Interface: Interface received failed, status=<%s>.",
            MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, interfaceStatus));
    }
}

void BluetoothSensorDeviceAdapter::OnDigitalTwinInterfaceSendTelemetry(
    DIGITALTWIN_CLIENT_RESULT pnpTelemetryStatus,
    void* userContextCallback)
{
    LogInfo("pnpstatus=%d, context=0x%p", pnpTelemetryStatus, userContextCallback);
}

void BluetoothSensorDeviceAdapter::ReportTelemetry(
    const std::string& telemetryName,
    const std::string& message)
{
    char telemetryMessage[512] = { 0 };
    sprintf(telemetryMessage, "{\"%s\":%s}", telemetryName.c_str(), message.c_str());
    LogInfo("Reporting telemetry: [%s]", telemetryMessage);

    std::vector<char> telemetryNameBuffer(telemetryName.c_str(), telemetryName.c_str() + telemetryName.size() + 1);
    DIGITALTWIN_CLIENT_RESULT result = DigitalTwin_InterfaceClient_SendTelemetryAsync(m_pnpClientInterface,
                                                                                        (unsigned char*)(telemetryMessage),
                                                                                        strlen(telemetryMessage),
                                                                                        OnDigitalTwinInterfaceSendTelemetry,
                                                                                        static_cast<void*>(telemetryNameBuffer.data()));
    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("Reporting telemetry=<%s> failed, error=<%s>", telemetryName.c_str(), MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("Queued async report telemetry for %s", telemetryName.c_str());
    }
}

void BluetoothSensorDeviceAdapter::ReportSensorData(const ParsedSensorData& payload)
{
    for (const auto& payloadSection : payload)
    {
        ReportTelemetry(payloadSection.first, payloadSection.second);
    }
}
