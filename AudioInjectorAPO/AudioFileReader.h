//
// AudioFileReader.h -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Description:
//
//  Implementation of AudioFileReader class
//

#pragma once

#include <atlbase.h>
#include <mmreg.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <atlcoll.h>
#include <memory>
#include <AudioAPOTypes.h>

template <class T>
void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

class AudioFileReader
{
public:
    AudioFileReader();
    ~AudioFileReader();

    // Initialize the reader with a file path
    HRESULT Initialize(LPCWSTR filePath);

    // Get the loaded audio data
    const FLOAT32* GetAudioData() const { return m_pAudioData.get(); }

    // Get the number of frames in the audio file
    UINT32 GetFrameCount() const { return m_frameCount; }

    // Get the number of channels in the audio file
    UINT32 GetChannelCount() const { return m_channelCount; }

    // Get the sample rate of the audio file
    UINT32 GetSampleRate() const { return m_sampleRate; }

    // Check if reader is initialized and valid
    bool IsValid() const { return m_isInitialized; }

    // Resample the audio data to match the target sample rate and channel count
    HRESULT ResampleAudio(UINT32 targetSampleRate, UINT32 targetChannelCount);

private:
    // Clean up and release resources
    void Cleanup();

private:
    std::unique_ptr<FLOAT32[]> m_pAudioData;
    UINT32 m_frameCount;
    UINT32 m_channelCount;
    UINT32 m_sampleRate;
    bool m_isInitialized;
};
