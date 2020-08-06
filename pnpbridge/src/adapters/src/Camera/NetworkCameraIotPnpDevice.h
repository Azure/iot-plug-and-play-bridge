// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include "pch.h"
#include <windows.foundation.h>
#include <windows.storage.h>
#include <windows.storage.fileproperties.h>
#include "CameraMediaCapture.h"
#include "CameraIotPnpDevice.h"

class NetworkCameraIotPnpDevice : public CameraIotPnpDevice
{
public:
    NetworkCameraIotPnpDevice(std::wstring& uniqueId, std::string& componentName);
    virtual ~NetworkCameraIotPnpDevice();

    virtual HRESULT    Initialize();
    virtual HRESULT    GetURIOp(_Out_ std::string& strResponse);
protected:
    std::wstring  m_URI;
};

