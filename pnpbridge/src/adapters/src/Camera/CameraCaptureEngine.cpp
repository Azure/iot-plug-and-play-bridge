// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "pch.h"

#define SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS  20000

CameraCaptureEngine::CameraCaptureEngine()
{
}


CameraCaptureEngine::~CameraCaptureEngine()
{
}


HRESULT
CameraCaptureEngine::TakePhoto(
    _In_opt_z_ LPCWSTR DeviceName,
    _COM_Outptr_ IMFSample** ppSample
    )
{
    ComPtr<IMFSensorGroup>                      spSensorGroup;
    ComPtr<IMFSensorDevice>                     spSensorDevice;
    ComPtr<IMFMediaSource>                      spMediaSource;
    ComPtr<IMFAttributes>                       spAttributes;
    ComPtr<IMFCaptureEngineClassFactory>        spCEFactory;
    ComPtr<IMFCaptureEngine>                    spCaptureEngine;
    ComPtr<IMFCaptureSource>                    spCaptureSource;
    ComPtr<IMFCapturePhotoSink>                 spPhotoSink;
    ComPtr<IMFCapturePreviewSink>               spPreviewSink;
    ComPtr<CameraCaptureEngineCallback>         spCECallback;
    ComPtr<CameraCaptureEngineSampleCallback>   spPreviewCallback;
    ComPtr<CameraCaptureEngineSampleCallback>   spPhotoCallback;
    std::wstring                                cameraName;
    DWORD                                       cCount = 0;
    DWORD                                       dwStreamIndex = 0;
    ComPtr<IMFSample>                           spPreviewSample;

    if (DeviceName == nullptr)
    {
        // Enumerate the first KSCATEGORY_VIDEO_CAMERA available.
        RETURN_IF_FAILED (GetDefaultCamera(cameraName));
    }
    else
    {
        cameraName = DeviceName;
    }

    RETURN_IF_FAILED (MFCreateSensorGroup(cameraName.c_str(), &spSensorGroup));
    // There should always be ust one device in this sensor group.
    RETURN_IF_FAILED (spSensorGroup->GetSensorDeviceCount(&cCount));
    if (cCount != 1)
    {
        RETURN_IF_FAILED (E_UNEXPECTED);
    }
    RETURN_IF_FAILED (spSensorGroup->GetSensorDevice(0, &spSensorDevice));

    // Set the device to be in shared mode.
    RETURN_IF_FAILED (spSensorDevice->SetSensorDeviceMode(MFSensorDeviceMode_Shared));
    RETURN_IF_FAILED (spSensorGroup->CreateMediaSource(&spMediaSource));

    // We need to create three callbacks:
    // 1. Capture Engine on event callback, this is to handle the state transition
    //    for Capture Engine to make sure the operations are completed.
    // 2. Preview sample callback.  Most cameras are designed so their 3A
    //    algorithm requires a minimum number of frames for the 3A to 
    //    converge.  In some edge cases, if the camera doesn't have sufficient
    //    3A "training", photo operations may fail.
    // 3. Photo sample callback.  We could technically use the preview sample
    //    callback, but Photo sink gives us a nice already encoded JPEG
    //    snapshot of the video stream.
    RETURN_IF_FAILED (CameraCaptureEngineCallback::CreateInstance(&spCECallback));
    RETURN_IF_FAILED (CameraCaptureEngineSampleCallback::CreateInstance(5, &spPreviewCallback)); // Skip 5 frames...
    RETURN_IF_FAILED (CameraCaptureEngineSampleCallback::CreateInstance(0, &spPhotoCallback));

    // Now we need to create our capture engine.  We need at a minimum
    // one attribute to let capture engine know to not attempt to use
    // an audio device.  Then we initialize it and wait for the initialization
    // to complete.
    RETURN_IF_FAILED (MFCreateAttributes(&spAttributes, 1));
    RETURN_IF_FAILED (spAttributes->SetUINT32(MF_CAPTURE_ENGINE_USE_VIDEO_DEVICE_ONLY, TRUE));
    RETURN_IF_FAILED (CoCreateInstance(CLSID_MFCaptureEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spCEFactory)));
    RETURN_IF_FAILED (spCEFactory->CreateInstance(CLSID_MFCaptureEngine, IID_PPV_ARGS(&spCaptureEngine)));
    RETURN_IF_FAILED (spCaptureEngine->Initialize(spCECallback.Get(), spAttributes.Get(), nullptr, spMediaSource.Get()));
    RETURN_IF_FAILED (spCECallback->WaitForEvent(MF_CAPTURE_ENGINE_INITIALIZED, SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS));


    // Since we created our media source in shared mode, we can't change
    // the source media type, but we do need to know the current media
    // type on source.  This is so we can configure out sink with the
    // correct target media type.  For photo, that would be a JPEG with
    // the same resolution, but for the preview, we need to check if
    // the source type is MJPG.  Capture engine doesn't allow us to
    // obtain MJPG, it has to be configured with an uncompressed target
    // type or the pipeline will fail.
    RETURN_IF_FAILED (spCaptureEngine->GetSource(&spCaptureSource));

    // Preview sink - just adding a scope here to make the code easier
    // to read.
    {
        ComPtr<IMFMediaType>                        spSourceMediaType;
        ComPtr<IMFMediaType>                        spTargetMediaType;
        ComPtr<IMFCaptureSink>                      spSink;
        ComPtr<IMFCaptureEngineOnSampleCallback>    spSampleCallback;

        RETURN_IF_FAILED (spCaptureSource->GetCurrentDeviceMediaType((DWORD) MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW, &spSourceMediaType));
        RETURN_IF_FAILED (TranslateSourceToTarget(spSourceMediaType.Get(), &spTargetMediaType));
        RETURN_IF_FAILED (spCaptureEngine->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW, &spSink));
        RETURN_IF_FAILED (spSink.As(&spPreviewSink));
        RETURN_IF_FAILED (spPreviewSink->RemoveAllStreams());
        RETURN_IF_FAILED (spPreviewSink->AddStream((DWORD) MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW, spTargetMediaType.Get(), nullptr, &dwStreamIndex));
        RETURN_IF_FAILED (spPreviewCallback.As(&spSampleCallback));
        RETURN_IF_FAILED (spPreviewSink->SetSampleCallback(dwStreamIndex, spSampleCallback.Get()));
    }

    // Now the photo sink.  Again, new scope to make things easier to
    // read.
    {
        ComPtr<IMFMediaType>                        spSourceMediaType;
        ComPtr<IMFMediaType>                        spJpegMediaType;
        ComPtr<IMFCaptureSink>                      spSink;
        ComPtr<IMFCaptureEngineOnSampleCallback>    spSampleCallback;

        RETURN_IF_FAILED (spCaptureSource->GetCurrentDeviceMediaType((DWORD) MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO, &spSourceMediaType));
        RETURN_IF_FAILED (CreateJpegMediaTypeFromSourceMediaType(spSourceMediaType.Get(), &spJpegMediaType));
        RETURN_IF_FAILED (spCaptureEngine->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, &spSink));
        RETURN_IF_FAILED (spSink.As(&spPhotoSink));
        RETURN_IF_FAILED (spPhotoSink->RemoveAllStreams());
        RETURN_IF_FAILED (spPhotoSink->AddStream((DWORD) MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO, spJpegMediaType.Get(), nullptr, &dwStreamIndex));
        RETURN_IF_FAILED (spPhotoCallback.As(&spSampleCallback));
        RETURN_IF_FAILED (spPhotoSink->SetSampleCallback(spSampleCallback.Get()));
    }

    // Start the preivew and wait for the number of "skipped" frames
    // on the preview sink.
    RETURN_IF_FAILED (spCaptureEngine->StartPreview());
    RETURN_IF_FAILED (spCECallback->WaitForEvent(MF_CAPTURE_ENGINE_PREVIEW_STARTED, SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS));
    RETURN_IF_FAILED (spPreviewCallback->WaitForSample(SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS, &spPreviewSample));

    // Now take a photo and get that from the callback.
    RETURN_IF_FAILED (spCaptureEngine->TakePhoto());
    RETURN_IF_FAILED (spCECallback->WaitForEvent(MF_CAPTURE_ENGINE_PHOTO_TAKEN, SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS));
    RETURN_IF_FAILED (spPhotoCallback->WaitForSample(SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS, ppSample));

    // Stop our preview and clean up out capture engine.
    RETURN_IF_FAILED (spCaptureEngine->StopPreview());
    RETURN_IF_FAILED (spCECallback->WaitForEvent(MF_CAPTURE_ENGINE_PREVIEW_STOPPED, SHAREDCAMERA_DEFAULT_OPERATION_TIMEOUT_INMS));
    RETURN_IF_FAILED (spPreviewSink->RemoveAllStreams());
    RETURN_IF_FAILED (spPhotoSink->RemoveAllStreams());

    spCaptureEngine = nullptr;
    spCECallback = nullptr;

    return S_OK;
}

HRESULT
CameraCaptureEngine::CopyAttribute(
    _In_ IMFAttributes* pSrc,
    _In_ IMFAttributes* pDst, 
    _In_ REFGUID guidAttributeId
    )
{
    HRESULT     hr = S_OK;
    PROPVARIANT var;

    PropVariantInit(&var);
    RETURN_HR_IF_NULL (E_INVALIDARG, pSrc);
    RETURN_HR_IF_NULL (E_INVALIDARG, pDst);

    if (SUCCEEDED(pSrc->GetItem(guidAttributeId, &var)))
    {
        hr = pDst->SetItem(guidAttributeId, var);
        PropVariantClear(&var);
    }
    RETURN_IF_FAILED (hr);

    return S_OK;
}

HRESULT
CameraCaptureEngine::CreateJpegMediaTypeFromSourceMediaType(
    _In_ IMFMediaType* pMediaType,
    _COM_Outptr_ IMFMediaType** ppMediaType
    )
{
    ComPtr<IMFMediaType>    spJpegMediaType;

    RETURN_HR_IF_NULL (E_INVALIDARG, pMediaType);
    RETURN_HR_IF_NULL (E_POINTER, ppMediaType);
    *ppMediaType = nullptr;
    
    RETURN_IF_FAILED (MFCreateMediaType(&spJpegMediaType));
    RETURN_IF_FAILED (spJpegMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Image));
    RETURN_IF_FAILED (spJpegMediaType->SetGUID(MF_MT_SUBTYPE, MFImageFormat_JPEG));
    RETURN_IF_FAILED (CopyAttribute(pMediaType, spJpegMediaType.Get(), MF_MT_FRAME_SIZE));

    *ppMediaType = spJpegMediaType.Detach();

    return S_OK;
}

HRESULT
CameraCaptureEngine::GetDefaultCamera(
    _Out_ std::wstring& defaultCamera
    )
{
    CONFIGRET                   cr = CR_SUCCESS;
    std::unique_ptr<WCHAR[]>    pwzInterfaceList;
    ULONG                       cchInterfaceList = 0;
    GUID                        guidCategory = KSCATEGORY_VIDEO_CAMERA;

    // Do this in a loop, there's a very small chance that after the CM_Get_Device_Interface_List_Size
    // call but before we call CM_Get_Device_Interface_List, we can get a new SG published.
    do
    {
        cchInterfaceList = 0;
        cr = CM_Get_Device_Interface_List_Size(&cchInterfaceList, (GUID*)&guidCategory, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (cr != CR_SUCCESS || cchInterfaceList == 0)
        {
            // No Sensor Groups available, this is not an error, it just makes this
            // test a no-op.  Leave.
            return HRESULT_FROM_WIN32(ERROR_DEV_NOT_EXIST);
        }

        pwzInterfaceList = std::make_unique<WCHAR[]>(cchInterfaceList);
        cr = CM_Get_Device_Interface_ListW((GUID*)&guidCategory, nullptr, pwzInterfaceList.get(), cchInterfaceList, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (cr != CR_SUCCESS)
        {
            pwzInterfaceList = nullptr;
        }
    }
    while (cr == CR_BUFFER_SMALL);

    RETURN_HR_IF_NULL (E_UNEXPECTED, pwzInterfaceList.get());
    if (cchInterfaceList <= 1)
    {
        // This is empty...leave.
        return HRESULT_FROM_WIN32(ERROR_DEV_NOT_EXIST);
    }

    defaultCamera = (LPCWSTR) pwzInterfaceList.get();

    return S_OK;
}

HRESULT
CameraCaptureEngine::TranslateSourceToTarget(
    _In_ IMFMediaType* pSourceMT, 
    _COM_Outptr_ IMFMediaType** ppTargetMT
    )
{
    GUID                    guidSourceSubtype;
    ComPtr<IMFMediaType>    spTargetMT;

    RETURN_HR_IF_NULL (E_INVALIDARG, pSourceMT);
    RETURN_HR_IF_NULL (E_POINTER, ppTargetMT);
    *ppTargetMT = nullptr;

    RETURN_IF_FAILED (pSourceMT->GetGUID(MF_MT_SUBTYPE, &guidSourceSubtype));
    if (guidSourceSubtype != MFVideoFormat_MJPG)
    {
        *ppTargetMT = pSourceMT;
        (*ppTargetMT)->AddRef();

        return S_OK;
    }

    // We need a partial NV12 media type.
    RETURN_IF_FAILED (MFCreateMediaType(&spTargetMT));
    RETURN_IF_FAILED (spTargetMT->SetGUID(MF_MT_MAJOR_TYPE, MFVideoFormat_NV12));
    RETURN_IF_FAILED (CopyAttribute(pSourceMT, spTargetMT.Get(), MF_MT_FRAME_SIZE));
    RETURN_IF_FAILED (CopyAttribute(pSourceMT, spTargetMT.Get(), MF_MT_FRAME_RATE));

    *ppTargetMT = spTargetMT.Detach();

    return S_OK;
}

