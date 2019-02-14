//*@@@+++@@@@*******************************************************************
//
// Microsoft Windows Media
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@*******************************************************************
#pragma once

#include "pch.h"

class JsonWrapper
{
public:
    JsonWrapper();
    virtual ~JsonWrapper();

    HRESULT                 Initialize();
    HRESULT                 AddValueAsString(_In_z_ LPCSTR name, _In_ UINT32 val);
    HRESULT                 AddValueAsString(_In_z_ LPCSTR name, _In_ LONGLONG val);
    HRESULT                 AddValueAsString(_In_z_ LPCSTR name, _In_ double val);
    HRESULT                 AddHresultAsString(_In_z_ LPCSTR name, _In_ HRESULT hr);
    HRESULT                 AddObject(_In_z_ LPCSTR name, _In_z_ LPCSTR object);
    HRESULT                 AddFormatString(_In_z_ LPCSTR name, _In_z_ LPCSTR format, ...);
    size_t                  GetSize();
    const char*             GetMessage();
    JSON_Object*            GetObject(_Out_opt_ JSON_Value** ppvalue = nullptr);
    void                    Detach();

protected:
    JSON_Value*             m_value;
    JSON_Object*            m_object;
};
