// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdexcept>
#include <azure_c_shared_utility/xlogging.h>

#include "InterfaceDescriptor.h"

#pragma region InterfaceDescriptor

// static
InterfaceDescriptorMap InterfaceDescriptor::MakeDescriptorsFromJson(
    const JSON_Array* interfaceDescriptorJsonObjs)
{
    InterfaceDescriptorMap interfaceDescriptorMap;

    for (size_t i = 0; i < json_array_get_count(interfaceDescriptorJsonObjs); i++)
    {
        auto interfaceDescriptorObj = json_array_get_object(interfaceDescriptorJsonObjs, i);
        if (!interfaceDescriptorObj)
        {
            throw std::invalid_argument("Unable to get interface descriptor.");
        }

        auto newInterfaceDescriptor = InterfaceDescriptor::MakeSharedFromJson(interfaceDescriptorObj);
        auto emplaceResult = interfaceDescriptorMap.emplace(newInterfaceDescriptor->GetInterfaceId(),
            newInterfaceDescriptor);

        if (!emplaceResult.second)
        {
            throw std::invalid_argument("Config.txt contained a duplicate descriptor for " +
                newInterfaceDescriptor->GetInterfaceId());
        }
    }

    return interfaceDescriptorMap;
}

// static
std::shared_ptr<InterfaceDescriptor> InterfaceDescriptor::MakeSharedFromJson(
    const JSON_Object* interfaceDescriptor)
{
    auto newInterfaceDescriptor = std::make_shared<InterfaceDescriptor>();
    newInterfaceDescriptor->Initialize(interfaceDescriptor);
    return newInterfaceDescriptor;
}

void InterfaceDescriptor::Initialize(const JSON_Object* interfaceDescriptor)
{
    auto interfaceId = json_object_dotget_string(interfaceDescriptor, s_interfaceIdDescriptorName);
    if (!interfaceId)
    {
        throw std::invalid_argument(
            std::string("Unable to extract ") + s_interfaceIdDescriptorName + " from descriptor.");
    }
    m_interfaceId = interfaceId;

    auto companyId = json_object_dotget_string(interfaceDescriptor, s_companyIdDescriptorName);
    if (!companyId)
    {
        throw std::invalid_argument(
            std::string("Unable to extract ") + s_companyIdDescriptorName + " from descriptor.");
    }
    m_companyId = static_cast<uint16_t>(std::stol(companyId, 0, 16));

    std::string endianness = json_object_dotget_string(interfaceDescriptor, s_endianDescriptorName);
    if (endianness.empty())
    {
        throw std::invalid_argument(
            std::string("Unable to extract ") + s_endianDescriptorName + " from descriptor.");
    }

    if (!endianness.compare(s_endianLittleValue))
    {
        m_isLittleEndian = true;
    }
    else if (!endianness.compare(s_endianBigValue))
    {
        m_isLittleEndian = false;
    }
    else
    {
        throw std::invalid_argument(
            std::string("endianness value must either be ") + s_endianLittleValue + " or " + s_endianBigValue);
    }

    auto telemetryDescriptorObjs = json_object_dotget_array(interfaceDescriptor, s_telemetryDescriptorName);
    if (!telemetryDescriptorObjs)
    {
        throw std::invalid_argument(
            std::string("Unable to extract ") + s_telemetryDescriptorName + " from descriptor.");
    }
    m_payloadParser = PayloadParser::MakeSharedFromJson(telemetryDescriptorObjs, m_isLittleEndian);
}

std::string InterfaceDescriptor::GetInterfaceId()
{
    return m_interfaceId;
}

uint16_t InterfaceDescriptor::GetCompanyId()
{
    return m_companyId;
}

bool InterfaceDescriptor::GetIsLittleEndian()
{
    return m_isLittleEndian;
}

std::shared_ptr<PayloadParser> InterfaceDescriptor::GetPayloadParser()
{
    return m_payloadParser;
}

#pragma endregion // InterfaceDescriptor

#pragma region PayloadParser

// static
std::shared_ptr<PayloadParser> PayloadParser::MakeSharedFromJson(
    const JSON_Array* telemetryDescriptors,
    bool isLittleEndian)
{
    auto newPayloadParser = std::make_shared<PayloadParser>(isLittleEndian);
    newPayloadParser->Initialize(telemetryDescriptors);
    return newPayloadParser;
}

PayloadParser::PayloadParser(bool isLittleEndian) :
    m_isLittleEndian{ isLittleEndian }
{
}

void PayloadParser::Initialize(const JSON_Array* telemetryDescriptors)
{
    for (size_t j = 0; j < json_array_get_count(telemetryDescriptors); j++)
    {
        DataDescriptor dataDescriptor{};
        auto telemetryDescriptorObj = json_array_get_object(telemetryDescriptors, j);
        if (!telemetryDescriptorObj)
        {
            throw std::invalid_argument("Unable to get telemetry descriptor");
        }

        auto telemetryName = json_object_dotget_string(telemetryDescriptorObj, s_telemetryNameDescriptorName);
        if (!telemetryName)
        {
            throw std::invalid_argument(
                std::string("Unable to extract ") + s_telemetryNameDescriptorName + " from descriptor.");
        }
        dataDescriptor.telemetryName = telemetryName;

        auto dataParseType = json_object_dotget_string(telemetryDescriptorObj, s_dataParseTypeDescriptorName);
        if (!dataParseType)
        {
            throw std::invalid_argument(
                std::string("Unable to extract ") + s_dataParseTypeDescriptorName + " from descriptor.");
        }
        dataDescriptor.dataParseType = StringToSupportedParseType(dataParseType);

        // An explicit json_object_dothas_value check is needed for integral values since the intended
        // value may be 0 or 0.0, which will trigger a false positive on null checks.
        if (!json_object_dothas_value(telemetryDescriptorObj, s_dataOffsetDescriptorName))
        {
            throw std::invalid_argument(
                std::string("Unable to extract ") + s_dataOffsetDescriptorName + " from descriptor.");
        }
        auto dataOffset = json_object_dotget_number(telemetryDescriptorObj, s_dataOffsetDescriptorName);
        dataDescriptor.dataOffset = static_cast<unsigned int>(dataOffset);

        if (!json_object_dothas_value(telemetryDescriptorObj, s_conversionCoefficientDescriptorName))
        {
            throw std::invalid_argument(
                std::string("Unable to extract ") + s_conversionCoefficientDescriptorName + " from descriptor.");
        }
        auto conversionCoefficient = json_object_dotget_number(telemetryDescriptorObj, s_conversionCoefficientDescriptorName);
        dataDescriptor.conversionCoefficient = conversionCoefficient;

        if (!json_object_dothas_value(telemetryDescriptorObj, s_conversionBias))
        {
            throw std::invalid_argument(
                std::string("Unable to extract ") + s_conversionBias + " from descriptor.");
        }
        auto conversionBias = json_object_dotget_number(telemetryDescriptorObj, s_conversionBias);
        dataDescriptor.conversionBias = conversionBias;

        m_payloadDescriptors.push_back(dataDescriptor);
    }
}

ParsedSensorData PayloadParser::ParsePayload(const std::vector<unsigned char>& buffer)
{
    ParsedSensorData parsedSensorData{};
    for (const auto& telemetryDescriptor : m_payloadDescriptors)
    {
        auto length = GetSupportedParseTypeLength(telemetryDescriptor.dataParseType);
        if ((telemetryDescriptor.dataOffset + length) > buffer.size())
        {
            LogError("Buffer had length of %zd, but is too short for %s which has an offset of %u and length of %u",
                buffer.size(),
                telemetryDescriptor.telemetryName.c_str(),
                telemetryDescriptor.dataOffset,
                length);
            return ParsedSensorData{};
        }

        double parsedValue = ParseDataSection(buffer, telemetryDescriptor.dataParseType, telemetryDescriptor.dataOffset);
        parsedValue *= telemetryDescriptor.conversionCoefficient;
        parsedValue += telemetryDescriptor.conversionBias;

        parsedSensorData[telemetryDescriptor.telemetryName] = std::to_string(parsedValue);
    }

    return parsedSensorData;
}

double PayloadParser::ParseDataSection(
    std::vector<unsigned char> buffer,
    SupportedParseType parseType,
    int dataOffset)
{
    // Reverse byte order if data is big endian as all modern systems are small endian
    auto length = GetSupportedParseTypeLength(parseType);
    if ((length > 1) && (!m_isLittleEndian))
    {
        std::reverse((buffer.begin() + dataOffset), (buffer.begin() + dataOffset + length));
    }

    auto rawDataStart = buffer.data() + dataOffset;
    switch (parseType)
    {
    case uint8:
    {
        uint8_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case uint16:
    {
        uint16_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case uint32:
    {
        uint32_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case uint64:
    {
        uint64_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case int8:
    {
        int8_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case int16:
    {
        int16_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case int32:
    {
        int32_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case int64:
    {
        int64_t num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case float32:
    {
        float num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    case float64:
    {
        double num{};
        memcpy(&num, rawDataStart, sizeof(num));
        return static_cast<double>(num);
    }
    default:
    {
        throw std::invalid_argument("Unknown parse type");
    }
    }
}

unsigned int PayloadParser::GetSupportedParseTypeLength(SupportedParseType supportedParseType)
{
    switch (supportedParseType)
    {
    case uint8:
        __fallthrough;
    case int8:
        return 1;
    case uint16:
        __fallthrough;
    case int16:
        return 2;
    case uint32:
        __fallthrough;
    case int32:
        __fallthrough;
    case float32:
        return 4;
    case uint64:
        __fallthrough;
    case int64:
        __fallthrough;
    case float64:
        return 8;
    default:
        throw std::invalid_argument("Unkown parse type");
    }
}

PayloadParser::SupportedParseType PayloadParser::StringToSupportedParseType(const char* str)
{
    static std::unordered_map<std::string, SupportedParseType> enumToStringMap = {
        { "uint8", uint8 },
        { "uint16", uint16 },
        { "uint32", uint32 },
        { "uint64", uint64 },
        { "int8", int8 },
        { "int16", int16 },
        { "int32", int32 },
        { "int64", int64 },
        { "float32", float32 },
        { "float64", float64 }
    };

    auto findResult = enumToStringMap.find(str);
    if (findResult == enumToStringMap.end())
    {
        throw std::invalid_argument("Unsupported data parse type: " + std::string(str));
    }

    return findResult->second;
}

#pragma endregion // PayloadParser
