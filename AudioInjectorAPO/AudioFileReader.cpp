//
// AudioFileReader.cpp -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Description:
//
//  Implementation of AudioFileReader class
//

#include "AudioFileReader.h"
#include <mfapi.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>
#include <mftransform.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

AudioFileReader::AudioFileReader()
    : m_frameCount(0)
    , m_channelCount(0)
    , m_sampleRate(0)
    , m_isInitialized(false)
{
    // Initialize MF platform
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
}

AudioFileReader::~AudioFileReader()
{
    Cleanup();
    MFShutdown();
}

HRESULT AudioFileReader::Initialize(LPCWSTR filePath)
{
    // Clean up any previous data
    Cleanup();

    // Define all resources
    IMFSourceReader* pSourceReader = nullptr;
    IMFMediaType* pMediaType = nullptr;
    IMFMediaType* pAudioType = nullptr;
    WAVEFORMATEX* pWaveFormat = nullptr;
    PROPVARIANT propVariant;
    PropVariantInit(&propVariant);
    HRESULT hr = S_OK;

    // Resource cleanup helper using RAII
    struct ResourceGuard {
        IMFSourceReader*& sourceReader;
        IMFMediaType*& mediaType;
        IMFMediaType*& audioType;
        WAVEFORMATEX*& waveFormat;
        PROPVARIANT& variant;

        ResourceGuard(IMFSourceReader*& sr, IMFMediaType*& mt, IMFMediaType*& at,
                     WAVEFORMATEX*& wf, PROPVARIANT& pv)
            : sourceReader(sr), mediaType(mt), audioType(at), waveFormat(wf), variant(pv) {}

        ~ResourceGuard() {
            PropVariantClear(&variant);
            CoTaskMemFree(waveFormat);
            SafeRelease(&audioType);
            SafeRelease(&mediaType);
            SafeRelease(&sourceReader);
        }
    };

    // Create the guard object to automatically clean up resources
    ResourceGuard guard(pSourceReader, pMediaType, pAudioType, pWaveFormat, propVariant);

    // Create source reader
    hr = MFCreateSourceReaderFromURL(filePath, nullptr, &pSourceReader);
    if (FAILED(hr)) return hr;

    // Configure the source reader to give us PCM audio
    hr = MFCreateMediaType(&pAudioType);
    if (FAILED(hr)) return hr;

    hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) return hr;

    hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    if (FAILED(hr)) return hr;

    hr = pSourceReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pAudioType);
    if (FAILED(hr)) return hr;

    // Get the complete media type
    hr = pSourceReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pMediaType);
    if (FAILED(hr)) return hr;

    // Get the audio format details
    UINT32 waveFormatSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(pMediaType, &pWaveFormat, &waveFormatSize);
    if (FAILED(hr)) return hr;

    m_channelCount = pWaveFormat->nChannels;
    m_sampleRate = pWaveFormat->nSamplesPerSec;

    // Get duration to pre-allocate buffer
    hr = pSourceReader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
                                                MF_PD_DURATION, &propVariant);
    if (FAILED(hr)) return hr;

    // Duration is in 100-nanosecond units
    LONGLONG duration = propVariant.uhVal.QuadPart;

    // Calculate number of frames based on duration and sample rate
    UINT64 totalFrames = (duration * m_sampleRate) / 10000000;
    if (totalFrames > UINT32_MAX) {
        // File too large, limit to something reasonable
        totalFrames = UINT32_MAX;
    }

    m_frameCount = static_cast<UINT32>(totalFrames);

    // Allocate buffer for audio data
    try {
        m_pAudioData = std::make_unique<FLOAT32[]>(static_cast<UINT64>(m_frameCount) * m_channelCount);
    }
    catch (std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }

    // Read all audio data
    DWORD flags = 0;
    DWORD actualStreamIndex = 0;
    LONGLONG timestamp = 0;
    UINT32 currentFrame = 0;

    while (currentFrame < m_frameCount)
    {
        IMFSample* pSample = nullptr;

        hr = pSourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                                     0, &actualStreamIndex, &flags, &timestamp, &pSample);
        if (FAILED(hr)) {
            SafeRelease(&pSample);
            return hr;
        }

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            SafeRelease(&pSample);
            break;
        }

        if (!pSample) {
            continue;
        }

        IMFMediaBuffer* pBuffer = nullptr;
        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
        if (FAILED(hr)) {
            SafeRelease(&pSample);
            return hr;
        }

        BYTE* pAudioData = nullptr;
        DWORD cbBuffer = 0;

        hr = pBuffer->Lock(&pAudioData, nullptr, &cbBuffer);
        if (FAILED(hr)) {
            SafeRelease(&pBuffer);
            SafeRelease(&pSample);
            return hr;
        }

        // Copy audio data to our buffer
        UINT32 framesToCopy = cbBuffer / (sizeof(FLOAT32) * m_channelCount);
        if (currentFrame + framesToCopy > m_frameCount) {
            framesToCopy = m_frameCount - currentFrame;
        }

        memcpy(&m_pAudioData[static_cast<UINT64>(currentFrame) * m_channelCount],
             pAudioData,
             static_cast<UINT64>(framesToCopy) * m_channelCount * sizeof(FLOAT32));

        currentFrame += framesToCopy;

        pBuffer->Unlock();
        SafeRelease(&pBuffer);
        SafeRelease(&pSample);
    }

    // Update actual frame count
    m_frameCount = currentFrame;
    m_isInitialized = true;
    return S_OK;
}

HRESULT AudioFileReader::ResampleAudio(UINT32 targetSampleRate, UINT32 targetChannelCount)
{
    // If already matching the target format, no need to resample
    if (m_sampleRate == targetSampleRate && m_channelCount == targetChannelCount)
        return S_OK;

    // Validate input
    if (!m_isInitialized || m_frameCount == 0 || !m_pAudioData)
        return E_FAIL;

    // Create media foundation resources for resampling
    IMFMediaBuffer* pInputBuffer = nullptr;
    IMFSample* pInputSample = nullptr;
    IMFMediaType* pInputType = nullptr;
    IMFMediaType* pOutputType = nullptr;
    IMFTransform* pResampler = nullptr;
    IMFMediaBuffer* pOutputBuffer = nullptr;
    IMFSample* pOutputSample = nullptr;
    HRESULT hr = S_OK;

    // Resource cleanup helper using RAII
    struct ResourceGuard {
        IMFMediaBuffer*& inputBuffer;
        IMFSample*& inputSample;
        IMFMediaType*& inputType;
        IMFMediaType*& outputType;
        IMFTransform*& resampler;
        IMFMediaBuffer*& outputBuffer;
        IMFSample*& outputSample;

        ResourceGuard(IMFMediaBuffer*& ib, IMFSample*& is, IMFMediaType*& it, IMFMediaType*& ot,
                     IMFTransform*& r, IMFMediaBuffer*& ob, IMFSample*& os)
            : inputBuffer(ib), inputSample(is), inputType(it), outputType(ot),
              resampler(r), outputBuffer(ob), outputSample(os) {}

        ~ResourceGuard() {
            SafeRelease(&inputBuffer);
            SafeRelease(&inputSample);
            SafeRelease(&inputType);
            SafeRelease(&outputType);
            SafeRelease(&resampler);
            SafeRelease(&outputBuffer);
            SafeRelease(&outputSample);
        }
    };

    // Create the guard object to automatically clean up resources
    ResourceGuard guard(pInputBuffer, pInputSample, pInputType, pOutputType,
                       pResampler, pOutputBuffer, pOutputSample);

    // Calculate input buffer size in bytes
    UINT32 inputBufferSize = (UINT64)m_frameCount * m_channelCount * sizeof(FLOAT32);

    try {
        // 1. Create a Media Foundation resampler transform
        hr = CoCreateInstance(CLSID_CResamplerMediaObject, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IMFTransform, (void**)&pResampler);
        if (FAILED(hr)) return hr;

        // 2. Create and configure input media type
        hr = MFCreateMediaType(&pInputType);
        if (FAILED(hr)) return hr;

        hr = pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (FAILED(hr)) return hr;

        hr = pInputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        if (FAILED(hr)) return hr;

        hr = pInputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, m_channelCount);
        if (FAILED(hr)) return hr;

        hr = pInputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, m_sampleRate);
        if (FAILED(hr)) return hr;

        hr = pInputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, m_channelCount * sizeof(FLOAT32));
        if (FAILED(hr)) return hr;

        hr = pInputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT64)m_sampleRate * m_channelCount * sizeof(FLOAT32));
        if (FAILED(hr)) return hr;

        hr = pInputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32); // 32-bit float
        if (FAILED(hr)) return hr;

        hr = pInputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        if (FAILED(hr)) return hr;

        // 3. Create and configure output media type
        hr = MFCreateMediaType(&pOutputType);
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, targetChannelCount);
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, targetSampleRate);
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, targetChannelCount * sizeof(FLOAT32));
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT64)targetSampleRate * targetChannelCount * sizeof(FLOAT32));
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32); // 32-bit float
        if (FAILED(hr)) return hr;

        hr = pOutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        if (FAILED(hr)) return hr;

        // 4. Set input and output types on the resampler transform
        hr = pResampler->SetInputType(0, pInputType, 0);
        if (FAILED(hr)) return hr;

        hr = pResampler->SetOutputType(0, pOutputType, 0);
        if (FAILED(hr)) return hr;

        // 5. Process the audio data through the resampler

        // Create input sample
        hr = MFCreateSample(&pInputSample);
        if (FAILED(hr)) return hr;

        // Create input buffer
        hr = MFCreateMemoryBuffer(inputBufferSize, &pInputBuffer);
        if (FAILED(hr)) return hr;

        // Copy audio data to input buffer
        BYTE* pInputData = nullptr;
        hr = pInputBuffer->Lock(&pInputData, nullptr, nullptr);
        if (FAILED(hr)) return hr;

        memcpy(pInputData, m_pAudioData.get(), inputBufferSize);

        hr = pInputBuffer->Unlock();
        if (FAILED(hr)) return hr;

        hr = pInputBuffer->SetCurrentLength(inputBufferSize);
        if (FAILED(hr)) return hr;

        hr = pInputSample->AddBuffer(pInputBuffer);
        if (FAILED(hr)) return hr;

        // Set sample timestamp and duration
        LONGLONG sampleDuration = static_cast<LONGLONG>(m_frameCount) * 10000000 / m_sampleRate;
        hr = pInputSample->SetSampleTime(0);
        if (FAILED(hr)) return hr;

        hr = pInputSample->SetSampleDuration(sampleDuration);
        if (FAILED(hr)) return hr;

        // Process input
        hr = pResampler->ProcessInput(0, pInputSample, 0);
        if (FAILED(hr)) return hr;

        // 6. Get the output from the transform
        MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
        DWORD dwStatus = 0;

        // Create output sample
        hr = MFCreateSample(&pOutputSample);
        if (FAILED(hr)) return hr;

        // Calculate expected output buffer size
        UINT32 expectedFrames = (UINT32)((UINT64)m_frameCount * targetSampleRate / m_sampleRate);
        UINT32 outputBufferSize = (UINT64)expectedFrames * targetChannelCount * sizeof(FLOAT32);

        // Create output buffer
        hr = MFCreateMemoryBuffer(outputBufferSize, &pOutputBuffer);
        if (FAILED(hr)) return hr;

        hr = pOutputSample->AddBuffer(pOutputBuffer);
        if (FAILED(hr)) return hr;

        outputDataBuffer.pSample = pOutputSample;

        // Process output
        hr = pResampler->ProcessOutput(0, 1, &outputDataBuffer, &dwStatus);
        if (FAILED(hr)) return hr;

        // 7. Copy resampled data from output buffer to our member variable

        // Get output buffer details
        IMFMediaBuffer* pActualOutputBuffer = nullptr;
        hr = pOutputSample->GetBufferByIndex(0, &pActualOutputBuffer);
        if (FAILED(hr)) return hr;

        BYTE* pOutputData = nullptr;
        DWORD cbOutputLength = 0;

        // Use a scope to ensure proper cleanup of the output buffer
        {
            hr = pActualOutputBuffer->Lock(&pOutputData, nullptr, &cbOutputLength);
            if (FAILED(hr)) {
                SafeRelease(&pActualOutputBuffer);
                return hr;
            }

            // Calculate actual frame count from output buffer size
            UINT32 actualFrames = cbOutputLength / (targetChannelCount * sizeof(FLOAT32));

            // Allocate new buffer for the resampled audio
            std::unique_ptr<FLOAT32[]> newAudioData = std::make_unique<FLOAT32[]>(static_cast<UINT64>(actualFrames) * targetChannelCount);

            // Copy resampled data
            memcpy(newAudioData.get(), pOutputData, cbOutputLength);

            // Unlock buffer before updating our member variables
            pActualOutputBuffer->Unlock();

            // 8. Update member variables with new audio data
            m_pAudioData = std::move(newAudioData);
            m_frameCount = actualFrames;
            m_sampleRate = targetSampleRate;
            m_channelCount = targetChannelCount;
        }

        SafeRelease(&pActualOutputBuffer);
        return S_OK;

    } catch (std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
}


void AudioFileReader::Cleanup()
{
    m_pAudioData.reset();
    m_frameCount = 0;
    m_channelCount = 0;
    m_sampleRate = 0;
    m_isInitialized = false;
}
