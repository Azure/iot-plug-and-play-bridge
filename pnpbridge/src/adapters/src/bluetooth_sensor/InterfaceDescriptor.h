// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <parson.h>

// Forward declarations
class PayloadParser;
class InterfaceDescriptor;

// Maps sensor data name to its stringified value.
using ParsedSensorData = std::unordered_map<std::string, std::string>;
// Maps interface identity (i.e. `blesensor_identity` in config.json) to its corresponding descriptor.
using InterfaceDescriptorMap = std::unordered_map<std::string, std::shared_ptr<InterfaceDescriptor>>;

// Represents an interface identity. It contains properties, such as company ID, and how to parse the
// interface's data payload.
class InterfaceDescriptor
{
public:
    static constexpr char s_companyIdDescriptorName[] = "company_id";
    static constexpr char s_endianDescriptorName[] = "endianness";
    static constexpr char s_telemetryDescriptorName[] = "telemetry_descriptor";

    static constexpr char s_endianLittleValue[] = "little";
    static constexpr char s_endianBigValue[] = "big";

    // Parses the adapter's global config JSON into the corresponding InterfaceDescriptor instances.
    static InterfaceDescriptorMap MakeDescriptorsFromJson(
        _In_ const JSON_Object* adapterGlobalConfigJson);

    // Parses a single global config interface JSON into an InterfaceDescriptor instance.
    static std::shared_ptr<InterfaceDescriptor> MakeSharedFromJson(
        const std::string& interfaceIdentity,
        _In_ const JSON_Object* interfaceDescriptor);

    InterfaceDescriptor(const std::string& interfaceIdentity);

    void Initialize(_In_ const JSON_Object* interfaceDescriptor);

    std::string GetInterfaceId();

    uint16_t GetCompanyId();

    bool GetIsLittleEndian();

    std::shared_ptr<PayloadParser> GetPayloadParser();

private:
    const std::string m_interfaceId;
    uint16_t m_companyId;
    bool m_isLittleEndian;
    std::shared_ptr<PayloadParser> m_payloadParser;
};

// Helper class which parses a BLE advertisement manufacturer payload based on an interface descriptor.
class PayloadParser
{
public:
    enum SupportedParseType
    {
        uint8,
        uint16,
        uint32,
        uint64,
        int8,
        int16,
        int32,
        int64,
        float32,
        float64
    };

    // Contains all of the data necessary to parse one data field.
    struct DataDescriptor
    {
        std::string telemetryName;
        SupportedParseType dataParseType;
        unsigned int dataOffset;
        double conversionCoefficient;
        double conversionBias;
    };

    static constexpr char s_telemetryNameDescriptorName[] = "telemetry_name";
    static constexpr char s_dataParseTypeDescriptorName[] = "data_parse_type";
    static constexpr char s_dataOffsetDescriptorName[] = "data_offset";
    static constexpr char s_conversionCoefficientDescriptorName[] = "conversion_coefficient";
    static constexpr char s_conversionBias[] = "conversion_bias";

    // Parses a `telemetry_descriptor` JSON into its corresponding PayloadParser instance.
    static std::shared_ptr<PayloadParser> MakeSharedFromJson(
        _In_ const JSON_Array* dataDescriptors,
        bool isLittleEndian);

    PayloadParser(bool isLittleEndian);

    void Initialize(_In_ const JSON_Array* dataDescriptors);

    // Parses a manufacturer payload. Returns an empty ParsedSensorData if the buffer could not be
    // parsed properly.
    ParsedSensorData ParsePayload(const std::vector<unsigned char>& buffer);

private:
    // Parses a single data field in the manufacturer payload. This simply returns the raw parsed
    // value, the conversion coefficient and bias are not applied.
    double PayloadParser::ParseDataSection(
        std::vector<unsigned char> buffer,
        SupportedParseType parseType,
        int dataOffset);

    // Gets the size in bytes of the data parse type.
    unsigned int GetSupportedParseTypeLength(SupportedParseType supportedParseType);

    // Converts a stringified version of SupportedParseType into the enum type.
    SupportedParseType StringToSupportedParseType(_In_z_ const char* str);

    std::vector<DataDescriptor> m_payloadDescriptors;
    const bool m_isLittleEndian;
};
