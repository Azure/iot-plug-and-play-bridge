// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "pch.h"
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.media.capture.h>
#include <windows.media.capture.frames.h>

typedef ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::Capture::Frames::MediaFrameReader*, 
    ABI::Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs*> FrameArrivedEvent;

class CameraMediaCapture
{
public:
    CameraMediaCapture();
    virtual HRESULT InitMediaCapture(std::wstring& deviceId);
    HRESULT TakePhoto(ABI::Windows::Storage::IStorageFile** ppStorageFile);
    HRESULT TakeVideo(DWORD dwMillisecond, ABI::Windows::Storage::IStorageFile** ppStorageFile);
private:
    Microsoft::WRL::Wrappers::RoInitializeWrapper m_initialize;

protected:
    ComPtr<ABI::Windows::Media::Capture::IMediaCapture> m_spMediaCapture; 
};

// Derived mediacapture class with framereader capabilities, meant to be used with VisionSkills
class CameraMediaCaptureFrameReader : public CameraMediaCapture
{
public:
    CameraMediaCaptureFrameReader();
    ~CameraMediaCaptureFrameReader();
    virtual HRESULT InitMediaCapture(std::wstring& deviceId);
    HRESULT StartFrameReader();

private:
    LOCK_HANDLE            m_frameReaderLock;
    EventRegistrationToken m_token;

    ComPtr<FrameArrivedEvent>                                       m_spFrameArrivedHandlerDelegate;
    ComPtr<ABI::Windows::Media::Capture::Frames::IMediaFrameReader> m_spFrameReader;
    
    HRESULT FrameArrivedHandler(ABI::Windows::Media::Capture::Frames::IMediaFrameReader* pFrameReader, 
        ABI::Windows::Media::Capture::Frames::IMediaFrameArrivedEventArgs*);
};
