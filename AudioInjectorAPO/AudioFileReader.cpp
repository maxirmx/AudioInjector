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

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

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
    HRESULT hr = S_OK;
    IMFSourceReader* pSourceReader = nullptr;
    IMFMediaType* pMediaType = nullptr;
    IMFMediaType* pAudioType = nullptr;
    WAVEFORMATEX* pWaveFormat = nullptr;
    PROPVARIANT propVariant;
    PropVariantInit(&propVariant);

    // Clean up any previous data
    Cleanup();

    // Create source reader
    hr = MFCreateSourceReaderFromURL(filePath, nullptr, &pSourceReader);
    if (FAILED(hr)) goto done;

    // Configure the source reader to give us PCM audio
    hr = MFCreateMediaType(&pAudioType);
    if (FAILED(hr)) goto done;

    hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) goto done;

    hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    if (FAILED(hr)) goto done;

    hr = pSourceReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pAudioType);
    if (FAILED(hr)) goto done;

    // Get the complete media type
    hr = pSourceReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pMediaType);
    if (FAILED(hr)) goto done;

    // Get the audio format details
    UINT32 waveFormatSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(pMediaType, &pWaveFormat, &waveFormatSize);
    if (FAILED(hr)) goto done;

    m_channelCount = pWaveFormat->nChannels;
    m_sampleRate = pWaveFormat->nSamplesPerSec;

    // Get duration to pre-allocate buffer
    hr = pSourceReader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
                                                MF_PD_DURATION, &propVariant);
    if (FAILED(hr)) goto done;

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
        hr = E_OUTOFMEMORY;
        goto done;
    }

    // Read all audio data
    DWORD flags = 0;
    DWORD actualStreamIndex = 0;
    LONGLONG timestamp = 0;
    UINT32 currentFrame = 0;
    IMFSample* pSample = nullptr;

    while (currentFrame < m_frameCount)
    {
        hr = pSourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                                       0, &actualStreamIndex, &flags, &timestamp, &pSample);
        if (FAILED(hr)) goto done;

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;

        if (!pSample)
            continue;

        IMFMediaBuffer* pBuffer = nullptr;
        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
        if (FAILED(hr)) {
            SafeRelease(&pSample);
            goto done;
        }

        BYTE* pAudioData = nullptr;
        DWORD cbBuffer = 0;

        hr = pBuffer->Lock(&pAudioData, nullptr, &cbBuffer);
        if (FAILED(hr)) {
            SafeRelease(&pBuffer);
            SafeRelease(&pSample);
            goto done;
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
    hr = S_OK;

done:
    PropVariantClear(&propVariant);
    CoTaskMemFree(pWaveFormat);
    SafeRelease(&pAudioType);
    SafeRelease(&pMediaType);
    SafeRelease(&pSourceReader);

    return hr;
}

HRESULT AudioFileReader::ResampleAudio(UINT32 targetSampleRate, UINT32 targetChannelCount)
{
    // If already matching the target format, no need to resample
    if (m_sampleRate == targetSampleRate && m_channelCount == targetChannelCount)
        return S_OK;

    // TODO: Implement resampling using Media Foundation or another resampler
    // For this initial implementation, we'll just do a basic channel conversion
    // without sample rate conversion for simplicity

    if (m_channelCount != targetChannelCount) {
        // Simple channel conversion (this is a very basic implementation)
        std::unique_ptr<FLOAT32[]> newBuffer = std::make_unique<FLOAT32[]>(static_cast<UINT64>(m_frameCount) * targetChannelCount);

        for (UINT64 i = 0; i < m_frameCount; i++) {
            if (m_channelCount == 1 && targetChannelCount > 1) {
                // Mono to multi-channel - duplicate the mono channel
                float sample = m_pAudioData[i];
                for (UINT32 ch = 0; ch < targetChannelCount; ch++) {
                    newBuffer[i * targetChannelCount + ch] = sample;
                }
            }
            else if (m_channelCount > 1 && targetChannelCount == 1) {
                // Multi-channel to mono - average all channels
                float sum = 0.0f;
                for (UINT32 ch = 0; ch < m_channelCount; ch++) {
                    sum += m_pAudioData[i * m_channelCount + ch];
                }
                newBuffer[i] = sum / m_channelCount;
            }
            else {
                // Generic case - take the first targetChannelCount channels
                // or duplicate/pad with zeros as needed
                for (UINT32 ch = 0; ch < targetChannelCount; ch++) {
                    if (ch < m_channelCount) {
                        newBuffer[i * targetChannelCount + ch] = m_pAudioData[i * m_channelCount + ch];
                    }
                    else {
                        newBuffer[i * targetChannelCount + ch] = 0.0f;
                    }
                }
            }
        }

        m_pAudioData = std::move(newBuffer);
        m_channelCount = targetChannelCount;
    }

    // Note: Sample rate conversion is not implemented in this simple version
    // A real implementation would use a resampler

    return S_OK;
}

void AudioFileReader::Cleanup()
{
    m_pAudioData.reset();
    m_frameCount = 0;
    m_channelCount = 0;
    m_sampleRate = 0;
    m_isInitialized = false;
}
