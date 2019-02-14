#include "pch.h"
#include "JsonWrapper.h"

#define MAX_32BIT_DECIMAL_DIGIT     11          // maximum number of digits possible for a 32-bit value in decimal (plus null terminator).
#define MAX_64BIT_DECIMAL_DIGIT     21
#define MAX_32BIT_HEX_DIGIT         11          // Max hex digit is 8, but we need "0x" and null terminator.

//////////////////////////////////////////////////////////////////////////////////////////// 
/// 
JsonWrapper::JsonWrapper(
    )
    : m_value(nullptr)
    , m_object(nullptr)
{

}

JsonWrapper::~JsonWrapper(
    )
{
    m_object = nullptr;
    if (nullptr != m_value)
    {
        json_value_free(m_value);
        m_value = nullptr;
    }
}

HRESULT
JsonWrapper::Initialize(
    )
{
    m_object = nullptr;
    if (nullptr != m_value)
    {
        json_value_free(m_value);
        m_value = nullptr;
    }

    m_value = json_value_init_object();
    RETURN_HR_IF_NULL (E_OUTOFMEMORY, m_value);
    m_object = json_value_get_object(m_value);
    RETURN_HR_IF_NULL (E_UNEXPECTED, m_object);

    return S_OK;
}

HRESULT
JsonWrapper::AddValueAsString(
    _In_z_ LPCSTR name,
    _In_ UINT32 val
    )
{
    char sz[MAX_32BIT_DECIMAL_DIGIT] = { };

    RETURN_HR_IF_NULL (E_INVALIDARG, m_object);
RETURN_HR_IF_NULL(E_INVALIDARG, name);

RETURN_IF_FAILED(StringCchPrintfA(sz, _countof(sz), "%u", val));

// json_object_set_string returns 0 on success and -1 on
// failure.  Failure for this can be either via invalid
// arg (i.e., JSON_Object is invalid) or we ran out of 
// memory.  We'll assume, since this is only called by our 
// object and the methods are protected, that the parameters
// are valid.  So only failure would be E_OUTOFMEMORY.
RETURN_HR_IF(E_OUTOFMEMORY, JSONFailure == json_object_set_string(m_object, name, sz));

return S_OK;
}

HRESULT
JsonWrapper::AddValueAsString(
    _In_z_ LPCSTR name,
    _In_ LONGLONG val
)
{
    char sz[MAX_64BIT_DECIMAL_DIGIT] = { };

    RETURN_HR_IF_NULL(E_INVALIDARG, m_object);
    RETURN_HR_IF_NULL(E_INVALIDARG, name);

    RETURN_IF_FAILED(StringCchPrintfA(sz, _countof(sz), "%I64d", val));

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF(E_OUTOFMEMORY, JSONFailure == json_object_set_string(m_object, name, sz));

    return S_OK;
}

HRESULT
JsonWrapper::AddValueAsString(
    _In_z_ LPCSTR name,
    _In_ double val
)
{
    HRESULT hr = S_OK;
    char    sz[MAX_64BIT_DECIMAL_DIGIT] = { };

    RETURN_HR_IF_NULL(E_INVALIDARG, m_object);
    RETURN_HR_IF_NULL(E_INVALIDARG, name);

    hr = StringCchPrintfA(sz, _countof(sz), "%.3f", val);
    if (hr == S_OK || hr == STRSAFE_E_INSUFFICIENT_BUFFER)
    {
        // If we successfully converted the string or if
        // we truncated the string, assume we're fine.
        hr = S_OK;
    }
    RETURN_IF_FAILED(hr);

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF(E_OUTOFMEMORY, JSONFailure == json_object_set_string(m_object, name, sz));

    return S_OK;
}

HRESULT
JsonWrapper::AddHresultAsString(
    _In_z_ LPCSTR name,
    _In_ HRESULT hr
)
{
    char sz[MAX_32BIT_HEX_DIGIT] = { };

    RETURN_HR_IF_NULL(E_INVALIDARG, m_object);
    RETURN_HR_IF_NULL(E_INVALIDARG, name);

    RETURN_IF_FAILED(StringCchPrintfA(sz, _countof(sz), "0x%08X", hr));

    // json_object_set_string returns 0 on success and -1 on
    // failure.  Failure for this can be either via invalid
    // arg (i.e., JSON_Object is invalid) or we ran out of 
    // memory.  We'll assume, since this is only called by our 
    // object and the methods are protected, that the parameters
    // are valid.  So only failure would be E_OUTOFMEMORY.
    RETURN_HR_IF(E_OUTOFMEMORY, JSONFailure == json_object_set_string(m_object, name, sz));

    return S_OK;
}

HRESULT
JsonWrapper::AddObject(
    _In_z_ LPCSTR name,
    _In_z_ LPCSTR object
    )
{
    RETURN_HR_IF_NULL(E_INVALIDARG, m_object);
    RETURN_HR_IF_NULL(E_INVALIDARG, name);
    RETURN_HR_IF_NULL(E_INVALIDARG, object);

    JSON_Value* value = json_parse_string(object);
    RETURN_HR_IF(E_OUTOFMEMORY, JSONFailure == json_object_set_value(m_object, name, value));

    return S_OK;
}

HRESULT
JsonWrapper::AddFormatString(
    _In_z_ LPCSTR name, 
    _In_z_ LPCSTR format, 
    ...
    )
{
    HRESULT                 hr = S_OK;
    va_list                 vargs;
    size_t                  cch = 0;
    std::unique_ptr<char[]> psz;

    RETURN_HR_IF_NULL (E_INVALIDARG, m_object);
    RETURN_HR_IF_NULL (E_INVALIDARG, name);

    va_start(vargs, format);
    cch = _vscprintf(format, vargs);
    psz = std::make_unique<char[]>(cch + 1);
    hr = StringCchVPrintfA(psz.get(), cch + 1, format, vargs);
    va_end(vargs);
    RETURN_IF_FAILED (hr);

    RETURN_HR_IF (E_OUTOFMEMORY, JSONFailure == json_object_set_string(m_object, name, psz.get()));

    return S_OK;
}

size_t
JsonWrapper::GetSize(
    )
{
    if (nullptr == m_object || nullptr == m_value)
    {
        return 0;
    }

    return json_serialization_size(m_value);
}

const char*
JsonWrapper::GetMessage(
    )
{
    if (nullptr == m_object || nullptr == m_value)
    {
        return nullptr;
    }

    return json_serialize_to_string(m_value);
}

JSON_Object*
JsonWrapper::GetObject(
    _Out_opt_ JSON_Value** ppvalue
    )
{
    if (nullptr != ppvalue)
    {
        *ppvalue = m_value;
    }
    return m_object;
}

// NOTE:  This should only be used when the ownership of
// the JSON object is taken by another object, otherwise
// this will leak.
void
JsonWrapper::Detach(
    )
{
    m_object = nullptr;
    m_value = nullptr;
}
