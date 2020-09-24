// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include <chrono>
#include "CameraMediaCapture.h"
#include <MemoryBuffer.h>
#include <windows.media.mediaproperties.h>
#include <windows.graphics.imaging.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Graphics::Imaging;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Capture;
using namespace ABI::Windows::Media::Capture::Frames;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Storage::Streams;

CameraMediaCapture::CameraMediaCapture() :
    m_initialize(RO_INIT_MULTITHREADED)
{

}

HRESULT CameraMediaCapture::InitMediaCapture(std::wstring& deviceId) 
{
    HRESULT hr = S_OK;
    ComPtr<IMediaCaptureInitializationSettings>  spMediaCaptureInitializationSettings;
    ComPtr<IMediaCaptureInitializationSettings5> spMediaCaptureInitializationSettings5;
    ComPtr<IAsyncAction>                         spInitializeWithSettingsOp;

    ComPtr<IMediaCapture5> spMediaCapture5;
    ComPtr<IMapView<HSTRING, MediaFrameSource*>> fsList;
    ComPtr<IIterable<IKeyValuePair<HSTRING, MediaFrameSource*>*>> spIterable;
    ComPtr<IIterator<IKeyValuePair<HSTRING, MediaFrameSource*>*>> spIterator;
    ComPtr<IMediaFrameReader> spFrameReader;
    ComPtr<IVectorView<MediaFrameFormat*>> spMediaFrameFormats;
    ComPtr<IMediaFrameFormat> spMediaFrameFormat;
    ComPtr<IAsyncAction> spSetMediaFormatOp;

    RETURN_IF_FAILED(ActivateInstance(HStringReference(RuntimeClass_Windows_Media_Capture_MediaCapture).Get(), &m_spMediaCapture));

    RETURN_IF_FAILED(ActivateInstance(HStringReference(RuntimeClass_Windows_Media_Capture_MediaCaptureInitializationSettings).Get(), &spMediaCaptureInitializationSettings));
    RETURN_IF_FAILED(spMediaCaptureInitializationSettings->put_VideoDeviceId(HStringReference(deviceId.c_str()).Get()));
    RETURN_IF_FAILED(spMediaCaptureInitializationSettings->put_StreamingCaptureMode(StreamingCaptureMode::StreamingCaptureMode_Video));
    RETURN_IF_FAILED(spMediaCaptureInitializationSettings.As(&spMediaCaptureInitializationSettings5));
    RETURN_IF_FAILED(spMediaCaptureInitializationSettings5->put_SharingMode(MediaCaptureSharingMode::MediaCaptureSharingMode_SharedReadOnly));
    RETURN_IF_FAILED(spMediaCaptureInitializationSettings5->put_MemoryPreference(MediaCaptureMemoryPreference::MediaCaptureMemoryPreference_Cpu));
    
    RETURN_IF_FAILED(m_spMediaCapture->InitializeWithSettingsAsync(spMediaCaptureInitializationSettings.Get(), &spInitializeWithSettingsOp));
    hr = AwaitAction(spInitializeWithSettingsOp);
    RETURN_IF_FAILED(hr);
    
    return hr;
}

HRESULT CameraMediaCapture::TakePhoto(ABI::Windows::Storage::IStorageFile** ppStorageFile)
{
    HRESULT hr = S_OK;
    FILETIME    ft = { };
    SYSTEMTIME  st = { };
    wchar_t     szFile[MAX_PATH] = { };

    ComPtr<IStorageLibraryStatics>           spStorageLibraryStatics;
    ComPtr<IAsyncOperation<StorageLibrary*>> spGetLibraryOp;
    ComPtr<IStorageLibrary>                  spStorageLibrary;
    ComPtr<IStorageFolder>                   spStorageFolder;
    ComPtr<IStorageFile>                     spFile;
    ComPtr<IAsyncOperation<StorageFile*>>    spFileOp;
    ComPtr<IRandomAccessStream>              spMemoryStream;
    ComPtr<IImageEncodingPropertiesStatics>  spImageEncodingPropertiesStatics;
    ComPtr<IImageEncodingProperties>         spJpegEncodingProperties;
    ComPtr<IAsyncOperation<IRandomAccessStream*>> spFileStreamOpenOp;
    ComPtr<IRandomAccessStream>              spFileStream;
    ComPtr<IAsyncAction>                     spCapturePhotoOp;

    RETURN_IF_FAILED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_StorageLibrary).Get(), &spStorageLibraryStatics));
    RETURN_IF_FAILED(spStorageLibraryStatics->GetLibraryAsync(KnownLibraryId::KnownLibraryId_Pictures, &spGetLibraryOp));
    hr = AwaitTypedResult(spGetLibraryOp, StorageLibrary*, spStorageLibrary);
    RETURN_IF_FAILED(hr);
    RETURN_IF_FAILED(spStorageLibrary->get_SaveFolder(&spStorageFolder));
    
    GetSystemTimePreciseAsFileTime(&ft);
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !FileTimeToSystemTime(&ft, &st));
    RETURN_IF_FAILED(StringCchPrintfW(szFile, ARRAYSIZE(szFile), L"photo_%04d-%02d-%02d-%02d-%02d-%02d.%03d.jpg", 
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds));

    RETURN_IF_FAILED(spStorageFolder->CreateFileAsync(HStringReference(szFile).Get(), CreationCollisionOption::CreationCollisionOption_GenerateUniqueName, &spFileOp));
    hr = AwaitTypedResult(spFileOp, StorageFile*, spFile);
    RETURN_IF_FAILED(hr);

    RETURN_IF_FAILED(ActivateInstance(HStringReference(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream).Get(), &spMemoryStream));
    RETURN_IF_FAILED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Media_MediaProperties_ImageEncodingProperties).Get(), &spImageEncodingPropertiesStatics));
    RETURN_IF_FAILED(spImageEncodingPropertiesStatics->CreateJpeg(&spJpegEncodingProperties));
    RETURN_IF_FAILED(m_spMediaCapture->CapturePhotoToStorageFileAsync(spJpegEncodingProperties.Get(), spFile.Get(), &spCapturePhotoOp));
    hr = AwaitAction(spCapturePhotoOp);
    RETURN_IF_FAILED(hr);

    LogInfo("Took photo %ws", szFile);
    if (ppStorageFile)
    {
        *ppStorageFile = spFile.Detach();
    }

    return hr;
}

// TODO: possible leak in here. 
HRESULT CameraMediaCapture::TakeVideo(DWORD dwMilliseconds, IStorageFile** ppStorageFile)
{
    HRESULT hr = S_OK;
    FILETIME    ft = { };
    SYSTEMTIME  st = { };
    wchar_t     szFile[MAX_PATH] = { };

    ComPtr<IStorageLibraryStatics>           spStorageLibraryStatics;
    ComPtr<IAsyncOperation<StorageLibrary*>> spGetLibraryOp;
    ComPtr<IStorageLibrary>                  spStorageLibrary;
    ComPtr<IStorageFolder>                   spStorageFolder;
    ComPtr<IStorageFile>                     spFile;
    ComPtr<IAsyncOperation<StorageFile*>>    spFileOp;

    ComPtr<IMediaCapture2>                         spMediaCapture2;
    ComPtr<IMediaEncodingProfileStatics>           spMediaEncodingProfileStatics;
    ComPtr<IMediaEncodingProfile>                  spEncodingProfile;
    ComPtr<ILowLagMediaRecording>                  spLowLagMediaRecording;
    ComPtr<IAsyncOperation<LowLagMediaRecording*>> spLowLagRecordPrepareOp;
    ComPtr<IAsyncAction>                           spLowLagRecordAction;

    *ppStorageFile = nullptr;

    RETURN_IF_FAILED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_StorageLibrary).Get(), &spStorageLibraryStatics));
    RETURN_IF_FAILED(spStorageLibraryStatics->GetLibraryAsync(KnownLibraryId::KnownLibraryId_Videos, &spGetLibraryOp));
    hr = AwaitTypedResult(spGetLibraryOp, StorageLibrary*, spStorageLibrary);
    RETURN_IF_FAILED(hr);
    RETURN_IF_FAILED(spStorageLibrary->get_SaveFolder(&spStorageFolder));

    GetSystemTimePreciseAsFileTime(&ft);
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !FileTimeToSystemTime(&ft, &st));
    RETURN_IF_FAILED(StringCchPrintfW(szFile, ARRAYSIZE(szFile), L"video_%04d-%02d-%02d-%02d-%02d-%02d.%03d.mp4",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds));

    RETURN_IF_FAILED(spStorageFolder->CreateFileAsync(HStringReference(szFile).Get(), CreationCollisionOption::CreationCollisionOption_GenerateUniqueName, &spFileOp));
    hr = AwaitTypedResult(spFileOp, StorageFile*, spFile);
    RETURN_IF_FAILED(hr);

    RETURN_IF_FAILED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Media_MediaProperties_MediaEncodingProfile).Get(), &spMediaEncodingProfileStatics));
    RETURN_IF_FAILED(spMediaEncodingProfileStatics->CreateMp4(MediaProperties::VideoEncodingQuality_HD720p, &spEncodingProfile));

    RETURN_IF_FAILED(m_spMediaCapture.As(&spMediaCapture2));
    RETURN_IF_FAILED(spMediaCapture2->PrepareLowLagRecordToStorageFileAsync(spEncodingProfile.Get(), spFile.Get(), &spLowLagRecordPrepareOp));
    hr = AwaitTypedResult(spLowLagRecordPrepareOp, LowLagMediaRecording*, spLowLagMediaRecording);
    RETURN_IF_FAILED(hr);

    RETURN_IF_FAILED(spLowLagMediaRecording->StartAsync(&spLowLagRecordAction));
    hr = AwaitAction(spLowLagRecordAction);
    RETURN_IF_FAILED(hr);

    Sleep(dwMilliseconds);

    RETURN_IF_FAILED(spLowLagMediaRecording->StopAsync(&spLowLagRecordAction));
    hr = AwaitAction(spLowLagRecordAction);
    RETURN_IF_FAILED(hr);

    RETURN_IF_FAILED(spLowLagMediaRecording->FinishAsync(&spLowLagRecordAction));
    hr = AwaitAction(spLowLagRecordAction);
    RETURN_IF_FAILED(hr);

    LogInfo("Finished recording video file %ws", szFile);
    
    if (ppStorageFile)
    {
        *ppStorageFile = spFile.Detach();
    }

    return hr;
}

CameraMediaCaptureFrameReader::CameraMediaCaptureFrameReader()
    : CameraMediaCapture()
{
    m_frameReaderLock = Lock_Init();
}

CameraMediaCaptureFrameReader::~CameraMediaCaptureFrameReader()
{
}

HRESULT CameraMediaCaptureFrameReader::InitMediaCapture(std::wstring& deviceId)
{
    HRESULT hr = S_OK;
    ComPtr<IMediaCapture5>                                        spMediaCapture5;
    ComPtr<IMapView<HSTRING, MediaFrameSource*>>                  spFSList;
    ComPtr<IIterable<IKeyValuePair<HSTRING, MediaFrameSource*>*>> spIterable;
    ComPtr<IIterator<IKeyValuePair<HSTRING, MediaFrameSource*>*>> spIterator;
    ComPtr<IVectorView<MediaFrameFormat*>>                        spMediaFrameFormats;
    ComPtr<IMediaFrameFormat>                                     spMediaFrameFormat;
    ComPtr<IAsyncAction>                                          spSetMediaFormatOp;

    // call base method init first, then set up frame reader
    CameraMediaCapture::InitMediaCapture(deviceId);
    RETURN_IF_FAILED(m_spMediaCapture.As(&spMediaCapture5));
    RETURN_IF_FAILED(spMediaCapture5->get_FrameSources(&spFSList));
    RETURN_IF_FAILED(spFSList.As(&spIterable));
    RETURN_IF_FAILED(spIterable->First(&spIterator));
    boolean bHasCurrent;
    RETURN_IF_FAILED(spIterator->get_HasCurrent(&bHasCurrent));
    if (bHasCurrent)
    {
        ComPtr<IKeyValuePair<HSTRING, MediaFrameSource*>> spKeyValue;
        ComPtr<IMediaFrameSource>                         spFrameSource;
        ComPtr<IAsyncOperation<MediaFrameReader*>>        spCreateFrameReaderrOp;
        ComPtr<IMediaFrameSourceInfo>                     spInfo;
        ComPtr<IMediaFrameReader>                         spFrameReader;
        MediaStreamType                                   streamType;
        MediaFrameSourceKind                              sourceKind;

        RETURN_IF_FAILED(spIterator->get_Current(&spKeyValue));
        RETURN_IF_FAILED(spKeyValue->get_Value(&spFrameSource));
        do
        {
            if (!bHasCurrent)
            {
                // No valid video frame sources were found with source type color
                RETURN_IF_FAILED(MF_E_INVALIDMEDIATYPE);
            }

            RETURN_IF_FAILED(spIterator->get_Current(spKeyValue.ReleaseAndGetAddressOf()));
            RETURN_IF_FAILED(spKeyValue->get_Value(spFrameSource.ReleaseAndGetAddressOf()));
            RETURN_IF_FAILED(spFrameSource->get_Info(spInfo.ReleaseAndGetAddressOf()));
            RETURN_IF_FAILED(spInfo->get_MediaStreamType(&streamType));
            RETURN_IF_FAILED(spInfo->get_SourceKind(&sourceKind));

            RETURN_IF_FAILED(spIterator->MoveNext(&bHasCurrent));
        } while (((streamType != MediaStreamType::MediaStreamType_VideoPreview && (streamType != MediaStreamType::MediaStreamType_VideoRecord))
            || (sourceKind != MediaFrameSourceKind::MediaFrameSourceKind_Color)));

        // Create FrameReader with the FrameSource that we selected in the loop above.
        RETURN_IF_FAILED(spMediaCapture5->CreateFrameReaderWithSubtypeAsync(spFrameSource.Get(), HStringReference(L"Bgra8").Get(), &spCreateFrameReaderrOp));
        //RETURN_IF_FAILED(spMediaCapture5->CreateFrameReaderAsync(spFrameSource.Get(), &spCreateFrameReaderrOp));
        hr = AwaitTypedResult(spCreateFrameReaderrOp, MediaFrameReader*, spFrameReader);
        RETURN_IF_FAILED(hr);
        m_spFrameReader = spFrameReader;

        // Set up a delegate to handle the frames when they are ready
        m_spFrameArrivedHandlerDelegate = Callback<FrameArrivedEvent>(
            [&](IMediaFrameReader * pFrameReader, IMediaFrameArrivedEventArgs* eventArgs)
            {
                return FrameArrivedHandler(pFrameReader, eventArgs);
            }
        );

        RETURN_IF_FAILED(m_spFrameReader->add_FrameArrived(m_spFrameArrivedHandlerDelegate.Get(), &m_token));
    }

    return hr;
}

HRESULT CameraMediaCaptureFrameReader::StartFrameReader()
{
    HRESULT hr = S_OK;
    ComPtr<IAsyncOperation<MediaFrameReaderStartStatus>> spStartFrameReaderOp;
    MediaFrameReaderStartStatus Stat;

    RETURN_IF_FAILED(m_spFrameReader->StartAsync(&spStartFrameReaderOp));
    hr = AwaitTypedResult(spStartFrameReaderOp, MediaFrameReaderStartStatus, Stat);
    RETURN_IF_FAILED(hr);

    if (hr != MediaFrameReaderStartStatus::MediaFrameReaderStartStatus_Success)
    {
        return E_FAIL;
    }
    return hr;
}

HRESULT CameraMediaCaptureFrameReader::FrameArrivedHandler(IMediaFrameReader* pFrameReader, IMediaFrameArrivedEventArgs*)
{
    HRESULT hr = S_OK;

    ComPtr<IMediaFrameReference> spFrameRef;
    ComPtr<IVideoMediaFrame>     spVideoMediaFrame;
    ComPtr<IVideoFrame>          spVideoFrame;
    ComPtr<ISoftwareBitmap>      spSoftwareBitmap;

    // Lock context so multiple overlapping events from FrameReader do not race for the resources.
    Lock(m_frameReaderLock);

    // Try to get the actual Video Frame from the FrameReader
    RETURN_IF_FAILED(pFrameReader->TryAcquireLatestFrame(&spFrameRef));
    if (spFrameRef == nullptr)
    {
        hr = S_OK; goto exit;
    }

    RETURN_IF_FAILED(spFrameRef->get_VideoMediaFrame(&spVideoMediaFrame));
    if (spVideoMediaFrame == nullptr)
    {
        hr = S_OK; goto exit;
    }

    RETURN_IF_FAILED(spVideoMediaFrame->GetVideoFrame(&spVideoFrame));

    /***********************************************************
    Here is where developers can, for example, take the VideoFrame 
    and perform some function on it (CV, ML, etc), and optionally 
    interact with IoT Hub, like send telemtry based on the CV or 
    ML operation. 
    ***********************************************************/
exit:
    Unlock(m_frameReaderLock);

    return hr;
}