//
// AudioInjectorAPOSFX.cpp -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Description:
//
//  Implementation of CAudioInjectorAPOSFX
//

#include <atlbase.h>
#include <atlcom.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <mmreg.h>

#include <audioenginebaseapo.h>
#include <baseaudioprocessingobject.h>
#include <resource.h>

#include <float.h>

#include "AudioInjectorAPO.h"
//#include <devicetopology.h>
#include <CustomPropKeys.h>

#include "APOLogger.h"
#include "AudioFileReader.h"


// Static declaration of the APO_REG_PROPERTIES structure
// associated with this APO.  The number in <> brackets is the
// number of IIDs supported by this APO.  If more than one, then additional
// IIDs are added at the end
#pragma warning (disable : 4815)
const AVRT_DATA CRegAPOProperties<1> CAudioInjectorAPOSFX::sm_RegProperties(
    __uuidof(AudioInjectorAPOSFX),                           // clsid of this APO
    L"CAudioInjectorAPOSFX",                                 // friendly name of this APO
    L"Copyright (c) 2025 Maxim [maxirmx] Samsonov",         // copyright info
    1,                                              // major version #
    0,                                              // minor version #
    __uuidof(IAudioInjectorAPOSFX)                           // iid of primary interface

//
// If you need to change any of these attributes, uncomment everything up to
// the point that you need to change something.  If you need to add IIDs, uncomment
// everything and add additional IIDs at the end.
//
//  , DEFAULT_APOREG_FLAGS
//  , DEFAULT_APOREG_MININPUTCONNECTIONS
//  , DEFAULT_APOREG_MAXINPUTCONNECTIONS
//  , DEFAULT_APOREG_MINOUTPUTCONNECTIONS
//  , DEFAULT_APOREG_MAXOUTPUTCONNECTIONS
//  , DEFAULT_APOREG_MAXINSTANCES
//
    );

#pragma AVRT_CODE_BEGIN
//-------------------------------------------------------------------------
// Description:
//
//  Do the actual processing of data.
//
// Parameters:
//
//      u32NumInputConnections      - [in] number of input connections
//      ppInputConnections          - [in] pointer to list of input APO_CONNECTION_PROPERTY pointers
//      u32NumOutputConnections      - [in] number of output connections
//      ppOutputConnections         - [in] pointer to list of output APO_CONNECTION_PROPERTY pointers
//
// Return values:
//
//      void
//
// Remarks:
//
//  This function processes data in a manner dependent on the implementing
//  object.  This routine can not fail and can not block, or call any other
//  routine that blocks, or touch pagable memory.
//
STDMETHODIMP_(void) CAudioInjectorAPOSFX::APOProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_PROPERTY** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_PROPERTY** ppOutputConnections)
{
    UNREFERENCED_PARAMETER(u32NumInputConnections);
    UNREFERENCED_PARAMETER(u32NumOutputConnections);

    // APO_LOG_TRACE_F("APOProcess");

    FLOAT32 *pf32InputFrames, *pf32OutputFrames;

    ATLASSERT(m_bIsLocked);

    // assert that the number of input and output connectins fits our registration properties
    ATLASSERT(m_pRegProperties->u32MinInputConnections <= u32NumInputConnections);
    ATLASSERT(m_pRegProperties->u32MaxInputConnections >= u32NumInputConnections);
    ATLASSERT(m_pRegProperties->u32MinOutputConnections <= u32NumOutputConnections);
    ATLASSERT(m_pRegProperties->u32MaxOutputConnections >= u32NumOutputConnections);

    // check APO_BUFFER_FLAGS.
    switch( ppInputConnections[0]->u32BufferFlags )
    {
        case BUFFER_INVALID:
        {
            ATLASSERT(false);  // invalid flag - should never occur.  don't do anything.
            break;
        }
        case BUFFER_VALID:
        case BUFFER_SILENT:
        {
            // get input pointer to connection buffer
            pf32InputFrames = reinterpret_cast<FLOAT32*>(ppInputConnections[0]->pBuffer);
            ATLASSERT( IS_VALID_TYPED_READ_POINTER(pf32InputFrames) );            // get output pointer to connection buffer
            pf32OutputFrames = reinterpret_cast<FLOAT32*>(ppOutputConnections[0]->pBuffer);
            ATLASSERT( IS_VALID_TYPED_WRITE_POINTER(pf32OutputFrames) );

            if (BUFFER_SILENT == ppInputConnections[0]->u32BufferFlags)
            {
                WriteSilence( pf32InputFrames,
                              ppInputConnections[0]->u32ValidFrameCount,
                              GetSamplesPerFrame() );
            }            // Process with audio mixing if enabled
            if (
                !IsEqualGUID(m_AudioProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW) &&
                m_bEnableAudioMix &&
                m_pAudioFileReader &&
                m_pAudioFileReader->IsValid()
            )
            {
                // Save the current file position
                UINT32 previousFileIndex = m_fileIndex;

                // Mix the audio file with the input stream
                ProcessAudioMix(
                    pf32OutputFrames,
                    pf32InputFrames,
                    ppInputConnections[0]->u32ValidFrameCount,
                    GetSamplesPerFrame(),
                    m_pAudioFileReader->GetAudioData(),
                    m_pAudioFileReader->GetFrameCount(),
                    &m_fileIndex,
                    m_mixRatio);

                // we don't try to remember silence
                ppOutputConnections[0]->u32BufferFlags = BUFFER_VALID;

                // Check if we've played the whole file and need to auto-stop
                if (m_autoStopOnFileEnd && previousFileIndex > m_fileIndex)
                {
                    // File has looped back to the beginning, disable mixing
                    m_bEnableAudioMix = FALSE;

                    // Signal that effects have changed
                    m_EffectsLock.Enter();
                    if (m_hEffectsChangedEvent != NULL)
                    {
                        SetEvent(m_hEffectsChangedEvent);
                    }
                    m_EffectsLock.Leave();
                }
            }
            else
            {
                // copy the memory only if there is an output connection, and input/output pointers are unequal
                if ( (0 != u32NumOutputConnections) &&
                      (ppOutputConnections[0]->pBuffer != ppInputConnections[0]->pBuffer) )
                {
                    CopyFrames( pf32OutputFrames, pf32InputFrames,
                                ppInputConnections[0]->u32ValidFrameCount,
                                GetSamplesPerFrame() );
                }

                // pass along buffer flags
                ppOutputConnections[0]->u32BufferFlags = ppInputConnections[0]->u32BufferFlags;
            }

            // Set the valid frame count.
            ppOutputConnections[0]->u32ValidFrameCount = ppInputConnections[0]->u32ValidFrameCount;

            break;
        }
        default:
        {
            ATLASSERT(false);  // invalid flag - should never occur
            break;
        }
    } // switch

} // APOProcess
#pragma AVRT_CODE_END

//-------------------------------------------------------------------------
// Description:
//
//  Report delay added by the APO between samples given on input
//  and samples given on output.
//
// Parameters:
//
//      pTime                       - [out] hundreds-of-nanoseconds of delay added
//
// Return values:
//
//      S_OK on success, a failure code on failure
STDMETHODIMP CAudioInjectorAPOSFX::GetLatency(HNSTIME* pTime)
{
    ASSERT_NONREALTIME();

    if (NULL == pTime)
    {
        return E_POINTER;
    }

    // No delay is added when mixing audio
    *pTime = 0;
    return S_OK;
}

//-------------------------------------------------------------------------
// Description:
//
//  Verifies that the APO is ready to process and locks its state if so.
//
// Parameters:
//
//      u32NumInputConnections - [in] number of input connections attached to this APO
//      ppInputConnections - [in] connection descriptor of each input connection attached to this APO
//      u32NumOutputConnections - [in] number of output connections attached to this APO
//      ppOutputConnections - [in] connection descriptor of each output connection attached to this APO
//
// Return values:
//
//      S_OK                                Object is locked and ready to process.
//      E_POINTER                           Invalid pointer passed to function.
//      APOERR_INVALID_CONNECTION_FORMAT    Invalid connection format.
//      APOERR_NUM_CONNECTIONS_INVALID      Number of input or output connections is not valid on
//                                          this APO.
STDMETHODIMP CAudioInjectorAPOSFX::LockForProcess(UINT32 u32NumInputConnections,
    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
    UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
    ASSERT_NONREALTIME();

    HRESULT hr = CBaseAudioProcessingObject::LockForProcess(u32NumInputConnections,
        ppInputConnections, u32NumOutputConnections, ppOutputConnections);
    if (FAILED(hr))
    {
        return hr;
    }

    if (!IsEqualGUID(m_AudioProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW) && m_bEnableAudioMix)
    {
        // Create and initialize audio file reader
        m_pAudioFileReader = std::make_unique<AudioFileReader>();

        // Initialize with the audio file path
        hr = m_pAudioFileReader->Initialize(m_audioFilePath.c_str());
        if (FAILED(hr))
        {
            // Failed to load audio file, but we'll continue without mixing
            m_pAudioFileReader.reset();
            hr = S_OK;  // Don't fail the whole APO initialization
        }
        else
        {
            // Resample audio to match the APO format if needed
            hr = m_pAudioFileReader->ResampleAudio((UINT32)GetFramesPerSecond(), GetSamplesPerFrame());
            if (FAILED(hr))
            {
                m_pAudioFileReader.reset();
                hr = S_OK;  // Don't fail the whole APO initialization
            }

            // Initialize file playback position
            m_fileIndex = 0;
        }
    }

    return hr;
}

// The method that this long comment refers to is "Initialize()"
//-------------------------------------------------------------------------
// Description:
//
//  Generic initialization routine for APOs.
//
// Parameters:
//
//     cbDataSize - [in] the size in bytes of the initialization data.
//     pbyData - [in] initialization data specific to this APO
//
// Return values:
//
//     S_OK                         Successful completion.
//     E_POINTER                    Invalid pointer passed to this function.
//     E_INVALIDARG                 Invalid argument
//     AEERR_ALREADY_INITIALIZED    APO is already initialized
//
// Remarks:
//
//  This method initializes the APO.  The data is variable length and
//  should have the form of:
//
//    struct MyAPOInitializationData
//    {
//        APOInitBaseStruct APOInit;
//        ... // add additional fields here
//    };
//
//  If the APO needs no initialization or needs no data to initialize
//  itself, it is valid to pass NULL as the pbyData parameter and 0 as
//  the cbDataSize parameter.
//
//  As part of designing an APO, decide which parameters should be
//  immutable (set once during initialization) and which mutable (changeable
//  during the lifetime of the APO instance).  Immutable parameters must
//  only be specifiable in the Initialize call; mutable parameters must be
//  settable via methods on whichever parameter control interface(s) your
//  APO provides. Mutable values should either be set in the initialize
//  method (if they are required for proper operation of the APO prior to
//  LockForProcess) or default to reasonable values upon initialize and not
//  be required to be set before LockForProcess.
//
//  Within the mutable parameters, you must also decide which can be changed
//  while the APO is locked for processing and which cannot.
//
//  All parameters should be considered immutable as a first choice, unless
//  there is a specific scenario which requires them to be mutable; similarly,
//  no mutable parameters should be changeable while the APO is locked, unless
//  a specific scenario requires them to be.  Following this guideline will
//  simplify the APO's state diagram and implementation and prevent certain
//  types of bug.
//
//  If a parameter changes the APOs latency or MaxXXXFrames values, it must be
//  immutable.
//
//  The default version of this function uses no initialization data, but does verify
//  the passed parameters and set the m_bIsInitialized member to true.
//
//  Note: This method may not be called from a real-time processing thread.
//

HRESULT CAudioInjectorAPOSFX::Initialize(UINT32 cbDataSize, BYTE* pbyData)
{
    HRESULT                     hr = S_OK;
    GUID                        processingMode;

    // Parameter validation
    if ((NULL == pbyData) && (0 != cbDataSize))
    {
        return E_INVALIDARG;
    }

    if ((NULL != pbyData) && (0 == cbDataSize))
    {
        return E_INVALIDARG;
    }

    if (cbDataSize == sizeof(APOInitSystemEffects2))
    {
        //
        // Initialize for mode-specific signal processing
        //
        APOInitSystemEffects2* papoSysFxInit2 = (APOInitSystemEffects2*)pbyData;

        // Save reference to the effects property store. This saves effects settings
        // and is the communication medium between this APO and any associated UI.
        m_spAPOSystemEffectsProperties = papoSysFxInit2->pAPOSystemEffectsProperties;

        // Windows should pass a valid collection.
        ATLASSERT(papoSysFxInit2->pDeviceCollection != nullptr);
        if (papoSysFxInit2->pDeviceCollection == nullptr)
        {
            return E_INVALIDARG;
        }

        // Save the processing mode being initialized.
        processingMode = papoSysFxInit2->AudioProcessingMode;

    }
    else if (cbDataSize == sizeof(APOInitSystemEffects))
    {
        //
        // Initialize for default signal processing
        //
        APOInitSystemEffects* papoSysFxInit = (APOInitSystemEffects*)pbyData;

        // Save reference to the effects property store. This saves effects settings
        // and is the communication medium between this APO and any associated UI.
        m_spAPOSystemEffectsProperties = papoSysFxInit->pAPOSystemEffectsProperties;

        // Assume default processing mode
        processingMode = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;
    }
    else
    {
        // Invalid initialization size
        return E_INVALIDARG;
    }

    // Validate then save the processing mode. Note an endpoint effects APO
    // does not depend on the mode. Windows sets the APOInitSystemEffects2
    // AudioProcessingMode member to GUID_NULL for an endpoint effects APO.
    if (processingMode != AUDIO_SIGNALPROCESSINGMODE_DEFAULT        &&
        processingMode != AUDIO_SIGNALPROCESSINGMODE_RAW            &&
        processingMode != AUDIO_SIGNALPROCESSINGMODE_COMMUNICATIONS &&
        processingMode != AUDIO_SIGNALPROCESSINGMODE_SPEECH         &&
        processingMode != AUDIO_SIGNALPROCESSINGMODE_MEDIA          &&
        processingMode != AUDIO_SIGNALPROCESSINGMODE_MOVIE          &&
        processingMode != AUDIO_SIGNALPROCESSINGMODE_NOTIFICATION)
    {
        return E_INVALIDARG;
    }

    m_AudioProcessingMode = processingMode;

    //
    // An APO that implements signal processing more complex than this sample
    // would configure its processing for the processingMode determined above.
    // If necessary, the APO would also use the IDeviceTopology and IConnector
    // interfaces retrieved above to communicate with its counterpart audio
    // driver to configure any additional signal processing in the driver and
    // associated hardware.
    //    //
    //  Get the current values
    //
    if (m_spAPOSystemEffectsProperties != NULL)
    {
        // Default to disabled - will be enabled only when valid parameters are set
        m_bEnableAudioMix = FALSE;

        // Try to read custom audio file path from properties (if available)
        CComPtr<IPropertyStore> spProperties = m_spAPOSystemEffectsProperties;
        if (spProperties != nullptr)
        {
            PROPVARIANT var;
            PropVariantInit(&var);

            // Check if we have a custom audio file path property
            if (SUCCEEDED(spProperties->GetValue(PKEY_AudioMix_FilePath, &var)) && var.vt == VT_LPWSTR && var.pwszVal != nullptr)
            {
                m_audioFilePath = var.pwszVal;
            }
            PropVariantClear(&var);

            // Check if we have a custom device name
            PropVariantInit(&var);
            if (SUCCEEDED(spProperties->GetValue(PKEY_AudioMix_DeviceName, &var)) && var.vt == VT_LPWSTR)
            {
                if (var.pwszVal != nullptr)
                {
                    m_audioDeviceName = var.pwszVal;
                }
                else
                {
                    m_audioDeviceName.clear(); // Use default device
                }
            }
            PropVariantClear(&var);

            // Check if we have a custom mix ratio property
            PropVariantInit(&var);
            if (SUCCEEDED(spProperties->GetValue(PKEY_AudioMix_Ratio, &var)) && var.vt == VT_R4)
            {
                m_mixRatio = var.fltVal;
                // Ensure mix ratio is between 0 and 1
                if (m_mixRatio < 0.0f) m_mixRatio = 0.0f;
                if (m_mixRatio > 1.0f) m_mixRatio = 1.0f;
            }
            PropVariantClear(&var);
        }
    }    //
    //  Register for notification of registry updates
    //
    hr = m_spEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    if (FAILED(hr))
    {
        return hr;
    }

    hr = m_spEnumerator->RegisterEndpointNotificationCallback(this);
    if (FAILED(hr))
    {
        return hr;
    }

    m_bIsInitialized = true;
    return hr;
}

//-------------------------------------------------------------------------
//
// GetEffectsList
//
//  Retrieves the list of signal processing effects currently active and
//  stores an event to be signaled if the list changes.
//
// Parameters
//
//  ppEffectsIds - returns a pointer to a list of GUIDs each identifying a
//      class of effect. The caller is responsible for freeing this memory by
//      calling CoTaskMemFree.
//
//  pcEffects - returns a count of GUIDs in the list.
//
//  Event - passes an event handle. The APO signals this event when the list
//      of effects changes from the list returned from this function. The APO
//      uses this event until either this function is called again or the APO
//      is destroyed. The passed handle may be NULL. In this case, the APO
//      stops using any previous handle and does not signal an event.
//
// Remarks
//
//  An APO imlements this method to allow Windows to discover the current
//  effects applied by the APO. The list of effects may depend on what signal
//  processing mode the APO initialized (see AudioProcessingMode in the
//  APOInitSystemEffects2 structure) as well as any end user configuration.
//
//  If there are no effects then the function still succeeds, ppEffectsIds
//  returns a NULL pointer, and pcEffects returns a count of 0.
//
STDMETHODIMP CAudioInjectorAPOSFX::GetEffectsList(_Outptr_result_buffer_maybenull_(*pcEffects) LPGUID *ppEffectsIds, _Out_ UINT *pcEffects, _In_ HANDLE Event)
{
    HRESULT hr = S_OK;
    BOOL effectsLocked = FALSE;
    UINT cEffects = 0;

    // Parameter validation
    if (ppEffectsIds == NULL)
    {
        return E_POINTER;
    }

    if (pcEffects == NULL)
    {
        return E_POINTER;
    }

    // Synchronize access to the effects list and effects changed event
    m_EffectsLock.Enter();
    effectsLocked = TRUE;

    // Always close existing effects change event handle
    if (m_hEffectsChangedEvent != NULL)
    {
        CloseHandle(m_hEffectsChangedEvent);
        m_hEffectsChangedEvent = NULL;
    }

    // If an event handle was specified, save it here (duplicated to control lifetime)
    if (Event != NULL)
    {
        if (!DuplicateHandle(GetCurrentProcess(), Event, GetCurrentProcess(), &m_hEffectsChangedEvent, EVENT_MODIFY_STATE, FALSE, 0))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            // Clean up and return error
            m_EffectsLock.Leave();
            return hr;
        }
    }

    // naked scope to force the initialization of list[] to be after we enter the critical section
    {
        struct EffectControl
        {
            GUID effect;
            BOOL control;
        };
          EffectControl list[] =
        {
            { InjectEffectId, m_bEnableAudioMix },
        };

        if (!IsEqualGUID(m_AudioProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW))
        {
            // count the active effects
            for (UINT i = 0; i < ARRAYSIZE(list); i++)
            {
                if (list[i].control)
                {
                    cEffects++;
                }
            }
        }

        if (0 == cEffects)
        {
            *ppEffectsIds = NULL;
            *pcEffects = 0;
        }
        else
        {            GUID *pEffectsIds = (LPGUID)CoTaskMemAlloc(sizeof(GUID) * cEffects);
            if (pEffectsIds == nullptr)
            {
                // Clean up and return out of memory error
                m_EffectsLock.Leave();
                return E_OUTOFMEMORY;
            }

            // pick up the active effects
            UINT j = 0;
            for (UINT i = 0; i < ARRAYSIZE(list); i++)
            {
                if (list[i].control)
                {
                    pEffectsIds[j++] = list[i].effect;
                }
            }

            *ppEffectsIds = pEffectsIds;
            *pcEffects = cEffects;
        }

        hr = S_OK;
    }

    // Always release the lock before returning
    if (effectsLocked)
    {
        m_EffectsLock.Leave();
    }
    return hr;
}

//-------------------------------------------------------------------------
// Description:
//
//
//
// Parameters:
//
//
//
// Return values:
//
//
//
// Remarks:
//
//
HRESULT CAudioInjectorAPOSFX::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
    HRESULT     hr = S_OK;
    APO_LOG_TRACE_F("OnPropertyValueChanged %ls", pwstrDeviceId);
    UNREFERENCED_PARAMETER(pwstrDeviceId);

    if (!m_spAPOSystemEffectsProperties)
    {
        return hr;
    }    // If either the master disable or our APO's enable properties changed...
    if (PK_EQUAL(key, PKEY_Endpoint_Enable_Audio_Inject_SFX) ||
        PK_EQUAL(key, PKEY_AudioEndpoint_Disable_SysFx))
    {
        LONG nChanges = 0;

        // Synchronize access to the effects list and effects changed event
        m_EffectsLock.Enter();

        struct KeyControl
        {
            PROPERTYKEY key;
            LONG *value;
        };        KeyControl controls[] =
        {
            { PKEY_Endpoint_Enable_Audio_Inject_SFX,        &m_bEnableAudioMix },
        };

        for (int i = 0; i < ARRAYSIZE(controls); i++)
        {
            LONG fOldValue;
            LONG fNewValue = true;

            // Get the state of whether audio mixing is enabled or not
            fNewValue = GetCurrentEffectsSetting(m_spAPOSystemEffectsProperties, controls[i].key, m_AudioProcessingMode);

            // Delay in the new setting
            fOldValue = InterlockedExchange(controls[i].value, fNewValue);

            if (fNewValue != fOldValue)
            {
                nChanges++;
            }
        }

        // If anything changed and a change event handle exists
        if ((nChanges > 0) && (m_hEffectsChangedEvent != NULL))
        {
            SetEvent(m_hEffectsChangedEvent);
        }        m_EffectsLock.Leave();
    }

    // If any of our relevant properties change, reevaluate whether we should enable mixing
    if (PK_EQUAL(key, PKEY_AudioMix_FilePath) ||
        PK_EQUAL(key, PKEY_AudioMix_DeviceName) ||
        PK_EQUAL(key, PKEY_Endpoint_Enable_Audio_Inject_SFX) ||
        PK_EQUAL(key, PKEY_AudioEndpoint_Disable_SysFx))
    {
        // Update file path if it changed
        if (PK_EQUAL(key, PKEY_AudioMix_FilePath))
        {
            PROPVARIANT var;
            PropVariantInit(&var);

            if (SUCCEEDED(m_spAPOSystemEffectsProperties->GetValue(PKEY_AudioMix_FilePath, &var)) && var.vt == VT_LPWSTR)
            {
                // Store the new file path
                if (var.pwszVal != nullptr)
                {
                    m_audioFilePath = var.pwszVal;
                }
                else
                {
                    m_audioFilePath.clear();
                }
            }
            PropVariantClear(&var);
        }

        // Update device name if it changed
        if (PK_EQUAL(key, PKEY_AudioMix_DeviceName))
        {
            PROPVARIANT var;
            PropVariantInit(&var);

            if (SUCCEEDED(m_spAPOSystemEffectsProperties->GetValue(PKEY_AudioMix_DeviceName, &var)) && var.vt == VT_LPWSTR)
            {
                // Store the new device name
                if (var.pwszVal != nullptr)
                {
                    m_audioDeviceName = var.pwszVal;
                }
                else
                {
                    m_audioDeviceName.clear(); // Use default device
                }
            }
            PropVariantClear(&var);
        }

        // Determine if we should enable mixing
        // Only enable if we have a valid file path
        LONG masterEnableState = GetCurrentEffectsSetting(
            m_spAPOSystemEffectsProperties,
            PKEY_Endpoint_Enable_Audio_Inject_SFX,
            m_AudioProcessingMode);

        LONG oldEnableState = m_bEnableAudioMix;

        // Only enable when the master switch is on AND we have a valid file path
        m_bEnableAudioMix = masterEnableState && !m_audioFilePath.empty();

        // If the enable state changed, notify via the effects changed event
        if (oldEnableState != m_bEnableAudioMix)
        {
            m_EffectsLock.Enter();
            if (m_hEffectsChangedEvent != NULL)
            {
                SetEvent(m_hEffectsChangedEvent);
            }
            m_EffectsLock.Leave();

            // If newly enabled and we're locked, initialize the audio file reader
            if (m_bEnableAudioMix && m_bIsLocked)
            {
                // Create a new reader with the updated path
                std::unique_ptr<AudioFileReader> newReader = std::make_unique<AudioFileReader>();
                if (SUCCEEDED(newReader->Initialize(m_audioFilePath.c_str())) &&
                    SUCCEEDED(newReader->ResampleAudio((UINT32)GetFramesPerSecond(), GetSamplesPerFrame())))
                {
                    // Swap in the new reader
                    m_pAudioFileReader = std::move(newReader);
                    m_fileIndex = 0;
                }
                else
                {
                    // Failed to load audio file, disable mixing
                    m_bEnableAudioMix = FALSE;

                    if (m_hEffectsChangedEvent != NULL)
                    {
                        SetEvent(m_hEffectsChangedEvent);
                    }
                }
            }
        }
    }
    else if (PK_EQUAL(key, PKEY_AudioMix_Ratio) && m_spAPOSystemEffectsProperties)
    {
        // Mix ratio has changed
        PROPVARIANT var;
        PropVariantInit(&var);

        if (SUCCEEDED(m_spAPOSystemEffectsProperties->GetValue(PKEY_AudioMix_Ratio, &var)) &&
            var.vt == VT_R4)
        {
            // Update the mix ratio
            m_mixRatio = var.fltVal;

            // Ensure mix ratio is between 0 and 1
            if (m_mixRatio < 0.0f) m_mixRatio = 0.0f;
            if (m_mixRatio > 1.0f) m_mixRatio = 1.0f;
        }

        PropVariantClear(&var);
    }

    return hr;
}


//-------------------------------------------------------------------------
// Description:
//
//  Destructor.
//
// Parameters:
//
//     void
//
// Return values:
//
//      void
//
// Remarks:
//
//      This method deletes whatever was allocated.
//
//      This method may not be called from a real-time processing thread.
//
CAudioInjectorAPOSFX::~CAudioInjectorAPOSFX(void)
{
    // Release the audio file reader
    m_pAudioFileReader.reset();

    //
    // unregister for callbacks
    //
    if (m_bIsInitialized)
    {
        m_spEnumerator->UnregisterEndpointNotificationCallback(this);
    }

    if (m_hEffectsChangedEvent != NULL)
    {
        CloseHandle(m_hEffectsChangedEvent);
    }
} // ~CAudioInjectorAPOSFX
