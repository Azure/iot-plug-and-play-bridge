//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#pragma once

class CameraCaptureEngine
{
public:
    CameraCaptureEngine();
    virtual ~CameraCaptureEngine();

    // These methods are synchronous, they will block and wait for the
    // underlying capture engine to complete the operation (or return
    // an error--note:  each operation is expected to take no more than
    // 20 seconds.  FrameServer internally has a watchdog which will
    // fire in that time frame).
    virtual HRESULT                             TakePhoto(_In_z_ LPCWSTR DeviceName, _COM_Outptr_ IMFSample** ppSample);

protected:
    static HRESULT                              CopyAttribute(_In_ IMFAttributes* pSrc, _In_ IMFAttributes* pDst, _In_ REFGUID guidAttributeId);
    static HRESULT                              CreateJpegMediaTypeFromSourceMediaType(_In_ IMFMediaType* pMediaType, _COM_Outptr_ IMFMediaType** ppMediaType);
    static HRESULT                              GetDefaultCamera(std::wstring& defaultCamera);
    static HRESULT                              TranslateSourceToTarget(_In_ IMFMediaType* pSourceMT, _COM_Outptr_ IMFMediaType** ppTargetMT);
};

