//
// AudioMixer.cpp -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Description:
//
//  Implementation of audio mixing functions
//

#include <atlbase.h>
#include <atlcom.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <mmreg.h>

#include "AudioInjectorAPO.h"
#include "AudioFileReader.h"

#pragma AVRT_CODE_BEGIN
void ProcessAudioMix(
    _Out_writes_(u32ValidFrameCount * u32SamplesPerFrame)
        FLOAT32 *pf32OutputFrames,
    _In_reads_(u32ValidFrameCount * u32SamplesPerFrame)
        const FLOAT32 *pf32InputFrames,
    UINT32       u32ValidFrameCount,
    UINT32       u32SamplesPerFrame,
    _In_reads_opt_(u32FileFrameCount * u32SamplesPerFrame)
        const FLOAT32 *pf32FileBuffer,
    UINT32       u32FileFrameCount,
    _Inout_
        UINT32  *pu32FileIndex,
    FLOAT32     fMixRatio)
{
    ASSERT_REALTIME();
    ATLASSERT(IS_VALID_TYPED_READ_POINTER(pf32InputFrames));
    ATLASSERT(IS_VALID_TYPED_WRITE_POINTER(pf32OutputFrames));

    // If no file buffer or 0 mix ratio, just copy the input to output
    if (pf32FileBuffer == nullptr || u32FileFrameCount == 0 || fMixRatio <= 0.0f)
    {
        CopyFrames(pf32OutputFrames,
                pf32InputFrames,
                u32ValidFrameCount,
                u32SamplesPerFrame);
        return;
    }

    // Ensure mix ratio is valid
    if (fMixRatio > 1.0f)
    {
        fMixRatio = 1.0f;
    }

    // Calculate mix weights
    const FLOAT32 fInputWeight = 1.0f - fMixRatio;
    const FLOAT32 fFileWeight = fMixRatio;

    // Mix the audio streams
    for (UINT32 i = 0; i < u32ValidFrameCount; i++)
    {
        for (UINT32 j = 0; j < u32SamplesPerFrame; j++)
        {
            // Index for the current sample
            UINT32 sampleIndex = i * u32SamplesPerFrame + j;

            // Get the file sample, using the current file position
            FLOAT32 fileSample = 0.0f;
            if (u32FileFrameCount > 0)
            {
                UINT32 filePos = ((*pu32FileIndex) + i) % u32FileFrameCount;
                fileSample = pf32FileBuffer[filePos * u32SamplesPerFrame + j];
            }

            // Mix the streams with the appropriate weights
            pf32OutputFrames[sampleIndex] =
                (pf32InputFrames[sampleIndex] * fInputWeight) +
                (fileSample * fFileWeight);
        }
    }

    // Update the file position index for next time
    *pu32FileIndex = (*pu32FileIndex + u32ValidFrameCount) % u32FileFrameCount;
}
#pragma AVRT_CODE_END

#pragma AVRT_CODE_BEGIN
void WriteSilence(
    _Out_writes_(u32FrameCount * u32SamplesPerFrame)
        FLOAT32 *pf32Frames,
    UINT32 u32FrameCount,
    UINT32 u32SamplesPerFrame )
{
    ZeroMemory(pf32Frames, sizeof(FLOAT32) * u32FrameCount * u32SamplesPerFrame);
}
#pragma AVRT_CODE_END

#pragma AVRT_CODE_BEGIN
void CopyFrames(
    _Out_writes_(u32FrameCount * u32SamplesPerFrame)
        FLOAT32 *pf32OutFrames,
    _In_reads_(u32FrameCount * u32SamplesPerFrame)
        const FLOAT32 *pf32InFrames,
    UINT32 u32FrameCount,
    UINT32 u32SamplesPerFrame )
{
    CopyMemory(pf32OutFrames, pf32InFrames, sizeof(FLOAT32) * u32FrameCount * u32SamplesPerFrame);
}
#pragma AVRT_CODE_END
