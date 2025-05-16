//
// AudioInjectorAPOMFX.cpp -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Description:
//
//  Implementation of CAudioInjectorAPOMFX
//

#include <atlbase.h>
#include <atlcom.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <mmreg.h>

#include <initguid.h>
#include <audioenginebaseapo.h>
#include <baseaudioprocessingobject.h>
#include <resource.h>

#include <float.h>
#include "AudioInjectorAPO.h"
#include <CustomPropKeys.h>
#include "AudioFileReader.h"

//{FD7F2B29 - 24D0 - 4B5C - B177 - 592C39F9CA10}



// Static declaration of the APO_REG_PROPERTIES structure
// associated with this APO.  The number in <> brackets is the
// number of IIDs supported by this APO.  If more than one, then additional
// IIDs are added at the end
#pragma warning (disable : 4815)
const AVRT_DATA CRegAPOProperties<1> CAudioInjectorAPOMFX::sm_RegProperties(
    __uuidof(AudioInjectorAPOMFX),                           // clsid of this APO
    L"CAudioInjectorAPOMFX",                                 // friendly name of this APO
    L"2025 Maxim [maxirmx] Samsonov",         // copyright info
    1,                                              // major version #
    0,                                              // minor version #
    __uuidof(IAudioInjectorAPOMFX)                           // iid of primary interface
//
// If you need to change any of these attributes, uncomment everything up to
// the point that you need to change something.  If you need to add IIDs, uncomment
// everything and add additional IIDs at the end.
//
//   Enable inplace processing for this APO.
//  , DEFAULT_APOREG_FLAGS
//  , DEFAULT_APOREG_MININPUTCONNECTIONS
//  , DEFAULT_APOREG_MAXINPUTCONNECTIONS
//  , DEFAULT_APOREG_MINOUTPUTCONNECTIONS
//  , DEFAULT_APOREG_MAXOUTPUTCONNECTIONS
//  , DEFAULT_APOREG_MAXINSTANCES
//
    );

//-------------------------------------------------------------------------
// Description:
//
//  GetCurrentEffectsSetting
//      Gets the current aggregate effects-enable setting
//
// Parameters:
//
//  properties - Property store holding configurable effects settings
//
//  pkeyEnable - VT_UI4 property holding an enable/disable setting
//
//  processingMode - Audio processing mode
//
// Return values:
//  LONG - true if the effect is enabled
//
// Remarks:
//  The routine considers the value of the specified property, the well known
//  master PKEY_AudioEndpoint_Disable_SysFx property, and the specified
//  processing mode.If the processing mode is RAW then the effect is off. If
//  PKEY_AudioEndpoint_Disable_SysFx is non-zero then the effect is off.
//
LONG GetCurrentEffectsSetting(IPropertyStore* properties, PROPERTYKEY pkeyEnable, GUID processingMode)
{
    HRESULT hr;
    BOOL enabled;
    PROPVARIANT var;

    PropVariantInit(&var);

    // Get the state of whether channel swap MFX is enabled or not.

    // Check the master disable property defined by Windows
    hr = properties->GetValue(PKEY_AudioEndpoint_Disable_SysFx, &var);
    enabled = (SUCCEEDED(hr)) && !((var.vt == VT_UI4) && (var.ulVal != 0));

    PropVariantClear(&var);

    // Check the APO's enable property, defined by this APO.
    hr = properties->GetValue(pkeyEnable, &var);
    enabled = enabled && ((SUCCEEDED(hr)) && ((var.vt == VT_UI4) && (var.ulVal != 0)));

    PropVariantClear(&var);

    enabled = enabled && !IsEqualGUID(processingMode, AUDIO_SIGNALPROCESSINGMODE_RAW);

    return (LONG)enabled;
}

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
STDMETHODIMP_(void) CAudioInjectorAPOMFX::APOProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_PROPERTY** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_PROPERTY** ppOutputConnections)
{
    UNREFERENCED_PARAMETER(u32NumInputConnections);
    UNREFERENCED_PARAMETER(u32NumOutputConnections);

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

                // we don't try to remember silence
                ppOutputConnections[0]->u32BufferFlags = BUFFER_VALID;
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
STDMETHODIMP CAudioInjectorAPOMFX::GetLatency(HNSTIME* pTime)
{
    ASSERT_NONREALTIME();
    HRESULT hr = S_OK;

    IF_TRUE_ACTION_JUMP(NULL == pTime, hr = E_POINTER, Exit);
    if (IsEqualGUID(m_AudioProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW))
    {
        *pTime = 0;
    }
    else
    {
        *pTime = (m_bEnableAudioMix ? 0 : 0); // No latency for audio mixing
    }

Exit:
    return hr;
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
STDMETHODIMP CAudioInjectorAPOMFX::LockForProcess(UINT32 u32NumInputConnections,
    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
    UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
    ASSERT_NONREALTIME();
    HRESULT hr = S_OK;    hr = CBaseAudioProcessingObject::LockForProcess(u32NumInputConnections,
        ppInputConnections, u32NumOutputConnections, ppOutputConnections);
    IF_FAILED_JUMP(hr, Exit);    if (!IsEqualGUID(m_AudioProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW) && m_bEnableAudioMix)
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

Exit:
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

HRESULT CAudioInjectorAPOMFX::Initialize(UINT32 cbDataSize, BYTE* pbyData)
{
    HRESULT                     hr = S_OK;
    GUID                        processingMode;

    IF_TRUE_ACTION_JUMP( ((NULL == pbyData) && (0 != cbDataSize)), hr = E_INVALIDARG, Exit);
    IF_TRUE_ACTION_JUMP( ((NULL != pbyData) && (0 == cbDataSize)), hr = E_INVALIDARG, Exit);

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
        IF_TRUE_ACTION_JUMP(papoSysFxInit2->pDeviceCollection == nullptr, hr = E_INVALIDARG, Exit);

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
        hr = E_INVALIDARG;
        goto Exit;
    }

    // Validate then save the processing mode. Note an endpoint effects APO
    // does not depend on the mode. Windows sets the APOInitSystemEffects2
    // AudioProcessingMode member to GUID_NULL for an endpoint effects APO.
    IF_TRUE_ACTION_JUMP((processingMode != AUDIO_SIGNALPROCESSINGMODE_DEFAULT        &&
                         processingMode != AUDIO_SIGNALPROCESSINGMODE_RAW            &&
                         processingMode != AUDIO_SIGNALPROCESSINGMODE_COMMUNICATIONS &&
                         processingMode != AUDIO_SIGNALPROCESSINGMODE_SPEECH         &&
                         processingMode != AUDIO_SIGNALPROCESSINGMODE_MEDIA          &&
                         processingMode != AUDIO_SIGNALPROCESSINGMODE_MOVIE          &&
                         processingMode != AUDIO_SIGNALPROCESSINGMODE_NOTIFICATION), hr = E_INVALIDARG, Exit);
    m_AudioProcessingMode = processingMode;

    //
    // An APO that implements signal processing more complex than this sample
    // would configure its processing for the processingMode determined above.
    // If necessary, the APO would also use the IDeviceTopology and IConnector
    // interfaces retrieved above to communicate with its counterpart audio
    // driver to configure any additional signal processing in the driver and
    // associated hardware.
    //    //
    //  Get current effects settings
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
    }

    //
    //  Register for notification of registry updates
    //
    hr = m_spEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    IF_FAILED_JUMP(hr, Exit);

    hr = m_spEnumerator->RegisterEndpointNotificationCallback(this);
    IF_FAILED_JUMP(hr, Exit);

    m_bIsInitialized = true;
Exit:
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
STDMETHODIMP CAudioInjectorAPOMFX::GetEffectsList(_Outptr_result_buffer_maybenull_(*pcEffects) LPGUID *ppEffectsIds, _Out_ UINT *pcEffects, _In_ HANDLE Event)
{
    HRESULT hr;
    BOOL effectsLocked = FALSE;
    UINT cEffects = 0;

    IF_TRUE_ACTION_JUMP(ppEffectsIds == NULL, hr = E_POINTER, Exit);
    IF_TRUE_ACTION_JUMP(pcEffects == NULL, hr = E_POINTER, Exit);

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
            goto Exit;
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
        {
            GUID *pEffectsIds = (LPGUID)CoTaskMemAlloc(sizeof(GUID) * cEffects);
            if (pEffectsIds == nullptr)
            {
                hr = E_OUTOFMEMORY;
                goto Exit;
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

Exit:
    if (effectsLocked)
    {
        m_EffectsLock.Leave();
    }
    return hr;
}


//-------------------------------------------------------------------------
// Description:
//
//  Implementation of IMMNotificationClient::OnPropertyValueChanged
//
// Parameters:
//
//      pwstrDeviceId - [in] the id of the device whose property has changed
//      key - [in] the property that changed
//
// Return values:
//
//      Ignored by caller
//
// Remarks:
//
//      This method is called asynchronously.  No UI work should be done here.
//
HRESULT CAudioInjectorAPOMFX::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
    HRESULT     hr = S_OK;

    UNREFERENCED_PARAMETER(pwstrDeviceId);

    if (!m_spAPOSystemEffectsProperties)
    {
        return hr;
    }

    // If any of our relevant properties change, reevaluate whether we should enable mixing
    if (PK_EQUAL(key, PKEY_AudioMix_FilePath) ||
        PK_EQUAL(key, PKEY_AudioMix_DeviceName) ||
        PK_EQUAL(key, PKEY_Endpoint_Enable_Audio_Inject_MFX) ||
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
            PKEY_Endpoint_Enable_Audio_Inject_MFX,
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
CAudioInjectorAPOMFX::~CAudioInjectorAPOMFX(void)
{
    if (m_bIsInitialized)
    {
        //
        // unregister for callbacks
        //
        if (m_spEnumerator != NULL)
        {
            m_spEnumerator->UnregisterEndpointNotificationCallback(this);
        }
    }

    if (m_hEffectsChangedEvent != NULL)
    {
        CloseHandle(m_hEffectsChangedEvent);
    }

    // Clean up the audio file reader
    m_pAudioFileReader.reset();

    // Free locked memory allocations
    if (NULL != m_pf32Coefficients)
    {
        AERT_Free(m_pf32Coefficients);
        m_pf32Coefficients = NULL;
    }
} // ~CAudioInjectorAPOMFX


//-------------------------------------------------------------------------
// Description:
//
//  Validates input/output format pair during LockForProcess.
//
// Parameters:
//
//      u32NumInputConnections - [in] number of input connections attached to this APO
//      ppInputConnections - [in] format of each input connection attached to this APO
//      u32NumOutputConnections - [in] number of output connections attached to this APO
//      ppOutputConnections - [in] format of each output connection attached to this APO
//
// Return values:
//
//      S_OK                                Connections are valid.
//
// See Also:
//
//  CBaseAudioProcessingObject::LockForProcess
//
// Remarks:
//
//  This method is an internal call that is called by the default implementation of
//  CBaseAudioProcessingObject::LockForProcess().  This is called after the connections
//  are validated for simple conformance to the APO's registration properties.  It may be
//  used to verify that the APO is initialized properly and that the connections that are passed
//  agree with the data used for initialization.  Any failure code passed back from this
//  function will get returned by LockForProcess, and cause it to fail.
//
//  By default, this routine just ASSERTS and returns S_OK.
//
HRESULT CAudioInjectorAPOMFX::ValidateAndCacheConnectionInfo(UINT32 u32NumInputConnections,
                APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                UINT32 u32NumOutputConnections,
                APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
    ASSERT_NONREALTIME();
    HRESULT hResult;
    CComPtr<IAudioMediaType> pFormat;
    UNCOMPRESSEDAUDIOFORMAT UncompInputFormat, UncompOutputFormat;
    FLOAT32 f32InverseChannelCount;

    UNREFERENCED_PARAMETER(u32NumInputConnections);
    UNREFERENCED_PARAMETER(u32NumOutputConnections);

    _ASSERTE(!m_bIsLocked);
    _ASSERTE(((0 == u32NumInputConnections) || (NULL != ppInputConnections)) &&
              ((0 == u32NumOutputConnections) || (NULL != ppOutputConnections)));

    EnterCriticalSection(&m_CritSec);

    // get the uncompressed formats and channel masks
    hResult = ppInputConnections[0]->pFormat->GetUncompressedAudioFormat(&UncompInputFormat);
    IF_FAILED_JUMP(hResult, Exit);

    hResult = ppOutputConnections[0]->pFormat->GetUncompressedAudioFormat(&UncompOutputFormat);
    IF_FAILED_JUMP(hResult, Exit);

    // Since we haven't overridden the IsIn{Out}putFormatSupported APIs in this example, this APO should
    // always have input channel count == output channel count.  The sampling rates should also be eqaul,
    // and formats 32-bit float.
    _ASSERTE(UncompOutputFormat.fFramesPerSecond == UncompInputFormat.fFramesPerSecond);
    _ASSERTE(UncompOutputFormat. dwSamplesPerFrame == UncompInputFormat.dwSamplesPerFrame);

    // Allocate some locked memory.  We will use these as scaling coefficients during APOProcess->ProcessDelayScale
    hResult = AERT_Allocate(sizeof(FLOAT32)*m_u32SamplesPerFrame, (void**)&m_pf32Coefficients);
    IF_FAILED_JUMP(hResult, Exit);

    // Set scalars to decrease volume from 1.0 to 1.0/N where N is the number of channels
    // starting with the first channel.
    f32InverseChannelCount = 1.0f/m_u32SamplesPerFrame;
    for (UINT32 u32Index=0; u32Index<m_u32SamplesPerFrame; u32Index++)
    {
        m_pf32Coefficients[u32Index] = 1.0f - (FLOAT32)(f32InverseChannelCount)*u32Index;
    }


Exit:
    LeaveCriticalSection(&m_CritSec);
    return hResult;}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// IAudioSystemEffectsCustomFormats implementation

//
// For demonstration purposes we will add 44.1KHz, 16-bit stereo and 48KHz, 16-bit
// stereo formats.  These formats should already be available in mmsys.cpl.  We
// embellish the labels to make it obvious that these formats are coming from
// the APO.
//

//
// Note that the IAudioSystemEffectsCustomFormats interface, if present, is invoked only on APOs
// that attach directly to the connector in the 'DEFAULT' mode streaming graph. For example:
// - APOs implementing global effects
// - APOs implementing endpoint effects
// - APOs implementing DEFAULT mode effects which attach directly to a connector supporting DEFAULT processing mode

struct CUSTOM_FORMAT_ITEM
{
    WAVEFORMATEXTENSIBLE wfxFmt;
    LPCWSTR              pwszRep;
};

#define STATIC_KSDATAFORMAT_SUBTYPE_AC3\
    DEFINE_WAVEFORMATEX_GUID(WAVE_FORMAT_DOLBY_AC3_SPDIF)
DEFINE_GUIDSTRUCT("00000092-0000-0010-8000-00aa00389b71", KSDATAFORMAT_SUBTYPE_AC3);
#define KSDATAFORMAT_SUBTYPE_AC3 DEFINE_GUIDNAMED(KSDATAFORMAT_SUBTYPE_AC3)

CUSTOM_FORMAT_ITEM _rgCustomFormats[] =
{
    {{ WAVE_FORMAT_EXTENSIBLE, 2, 44100, 176400, 4, 16, sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX), 16, KSAUDIO_SPEAKER_STEREO, KSDATAFORMAT_SUBTYPE_PCM},  L"Custom #1 (really 44.1 KHz, 16-bit, stereo)"},
    {{ WAVE_FORMAT_EXTENSIBLE, 2, 48000, 192000, 4, 16, sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX), 16, KSAUDIO_SPEAKER_STEREO, KSDATAFORMAT_SUBTYPE_PCM},  L"Custom #2 (really 48 KHz, 16-bit, stereo)"}
    // The compressed AC3 format has been temporarily removed since the APO is not set up for compressed formats or EFXs yet.
    // {{ WAVE_FORMAT_EXTENSIBLE, 2, 48000, 192000, 4, 16, sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX), 16, KSAUDIO_SPEAKER_STEREO, KSDATAFORMAT_SUBTYPE_AC3},  L"Custom #3 (really 48 KHz AC-3)"}
};

#define _cCustomFormats (ARRAYSIZE(_rgCustomFormats))

//-------------------------------------------------------------------------
// Description:
//
//  Implementation of IAudioSystemEffectsCustomFormats::GetFormatCount
//
// Parameters:
//
//      pcFormats - [out] receives the number of formats to be added
//
// Return values:
//
//      S_OK        Success
//      E_POINTER   Null pointer passed
//
// Remarks:
//
STDMETHODIMP CAudioInjectorAPOMFX::GetFormatCount
(
    UINT* pcFormats
)
{
    if (pcFormats == NULL)
        return E_POINTER;

    *pcFormats = _cCustomFormats;
    return S_OK;
}

//-------------------------------------------------------------------------
// Description:
//
//  Implementation of IAudioSystemEffectsCustomFormats::GetFormat
//
// Parameters:
//
//      nFormat - [in] which format is being requested
//      IAudioMediaType - [in] address of a variable that will receive a ptr
//                             to a new IAudioMediaType object
//
// Return values:
//
//      S_OK            Success
//      E_INVALIDARG    nFormat is out of range
//      E_POINTER       Null pointer passed
//
// Remarks:
//
STDMETHODIMP CAudioInjectorAPOMFX::GetFormat
(
    UINT              nFormat,
    IAudioMediaType** ppFormat
)
{
    HRESULT hr;

    IF_TRUE_ACTION_JUMP((nFormat >= _cCustomFormats), hr = E_INVALIDARG, Exit);
    IF_TRUE_ACTION_JUMP((ppFormat == NULL), hr = E_POINTER, Exit);

    *ppFormat = NULL;

    hr = CreateAudioMediaType(  (const WAVEFORMATEX*)&_rgCustomFormats[nFormat].wfxFmt,
                                sizeof(_rgCustomFormats[nFormat].wfxFmt),
                                ppFormat);

Exit:
    return hr;
}

//-------------------------------------------------------------------------
// Description:
//
//  Implementation of IAudioSystemEffectsCustomFormats::GetFormatRepresentation
//
// Parameters:
//
//      nFormat - [in] which format is being requested
//      ppwstrFormatRep - [in] address of a variable that will receive a ptr
//                             to a new string description of the requested format
//
// Return values:
//
//      S_OK            Success
//      E_INVALIDARG    nFormat is out of range
//      E_POINTER       Null pointer passed
//
// Remarks:
//
STDMETHODIMP CAudioInjectorAPOMFX::GetFormatRepresentation
(
    UINT                nFormat,
    _Outptr_ LPWSTR* ppwstrFormatRep
)
{
    HRESULT hr;
    size_t  cbRep;
    LPWSTR  pwstrLocal;

    IF_TRUE_ACTION_JUMP((nFormat >= _cCustomFormats), hr = E_INVALIDARG, Exit);
    IF_TRUE_ACTION_JUMP((ppwstrFormatRep == NULL), hr = E_POINTER, Exit);

    cbRep = (wcslen(_rgCustomFormats[nFormat].pwszRep) + 1) * sizeof(WCHAR);

    pwstrLocal = (LPWSTR)CoTaskMemAlloc(cbRep);
    IF_TRUE_ACTION_JUMP((pwstrLocal == NULL), hr = E_OUTOFMEMORY, Exit);

    hr = StringCbCopyW(pwstrLocal, cbRep, _rgCustomFormats[nFormat].pwszRep);
    if (FAILED(hr))
    {
        CoTaskMemFree(pwstrLocal);
    }
    else
    {
        *ppwstrFormatRep = pwstrLocal;
    }

Exit:
    return hr;
}

//-------------------------------------------------------------------------
// Description:
//
//  Implementation of IAudioProcessingObject::IsOutputFormatSupported
//
// Parameters:
//
//      pInputFormat - [in] A pointer to an IAudioMediaType interface. This parameter indicates the output format. This parameter must be set to NULL to indicate that the output format can be any type
//      pRequestedOutputFormat - [in] A pointer to an IAudioMediaType interface. This parameter indicates the output format that is to be verified
//      ppSupportedOutputFormat - [in] This parameter indicates the supported output format that is closest to the format to be verified
//
// Return values:
//
//      S_OK                            Success
//      S_FALSE                         The format of Input/output format pair is not supported. The ppSupportedOutPutFormat parameter returns a suggested new format
//      APOERR_FORMAT_NOT_SUPPORTED     The format is not supported. The value of ppSupportedOutputFormat does not change.
//      E_POINTER                       Null pointer passed
//
// Remarks:
//
STDMETHODIMP CAudioInjectorAPOMFX::IsOutputFormatSupported
(
    IAudioMediaType *pInputFormat,
    IAudioMediaType *pRequestedOutputFormat,
    IAudioMediaType **ppSupportedOutputFormat
)
{
    ASSERT_NONREALTIME();
    bool formatChanged = false;
    HRESULT hResult;
    UNCOMPRESSEDAUDIOFORMAT uncompOutputFormat;
    IAudioMediaType *recommendedFormat = NULL;

    IF_TRUE_ACTION_JUMP((NULL == pRequestedOutputFormat) || (NULL == ppSupportedOutputFormat), hResult = E_POINTER, Exit);
    *ppSupportedOutputFormat = NULL;

    // Initial comparison to make sure the requested format is valid and consistent with the input
    // format. Because of the APO flags specified during creation, the samples per frame value will
    // not be validated.
    hResult = IsFormatTypeSupported(pInputFormat, pRequestedOutputFormat, &recommendedFormat, true);
    IF_FAILED_JUMP(hResult, Exit);

    // Check to see if a custom format from the APO was used.
    if (S_FALSE == hResult)
    {
        hResult = CheckCustomFormats(pRequestedOutputFormat);

        // If the output format is changed, make sure we track it for our return code.
        if (S_FALSE == hResult)
        {
            formatChanged = true;
        }
    }

    // now retrieve the format that IsFormatTypeSupported decided on, building upon that by adding
    // our channel count constraint.
    hResult = recommendedFormat->GetUncompressedAudioFormat(&uncompOutputFormat);
    IF_FAILED_JUMP(hResult, Exit);

    // If the requested format exactly matched our requirements,
    // just return it.
    if (!formatChanged)
    {
        *ppSupportedOutputFormat = pRequestedOutputFormat;
        (*ppSupportedOutputFormat)->AddRef();
        hResult = S_OK;
    }
    else // we're proposing something different, copy it and return S_FALSE
    {
        hResult = CreateAudioMediaTypeFromUncompressedAudioFormat(&uncompOutputFormat, ppSupportedOutputFormat);
        IF_FAILED_JUMP(hResult, Exit);
        hResult = S_FALSE;
    }

Exit:
    if (recommendedFormat)
    {
        recommendedFormat->Release();
    }

    return hResult;
}

HRESULT CAudioInjectorAPOMFX::CheckCustomFormats(IAudioMediaType *pRequestedFormat)
{
    HRESULT hResult = S_OK;

    for (int i = 0; i < _cCustomFormats; i++)
    {
        hResult = S_OK;
        const WAVEFORMATEX* waveFormat = pRequestedFormat->GetAudioFormat();

        if (waveFormat->wFormatTag != _rgCustomFormats[i].wfxFmt.Format.wFormatTag)
        {
            hResult = S_FALSE;
        }

        if (waveFormat->nChannels != _rgCustomFormats[i].wfxFmt.Format.nChannels)
        {
            hResult = S_FALSE;
        }

        if (waveFormat->nSamplesPerSec != _rgCustomFormats[i].wfxFmt.Format.nSamplesPerSec)
        {
            hResult = S_FALSE;
        }

        if (waveFormat->nAvgBytesPerSec != _rgCustomFormats[i].wfxFmt.Format.nAvgBytesPerSec)
        {
            hResult = S_FALSE;
        }

        if (waveFormat->nBlockAlign != _rgCustomFormats[i].wfxFmt.Format.nBlockAlign)
        {
            hResult = S_FALSE;
        }

        if (waveFormat->wBitsPerSample != _rgCustomFormats[i].wfxFmt.Format.wBitsPerSample)
        {
            hResult = S_FALSE;
        }

        if (waveFormat->cbSize != _rgCustomFormats[i].wfxFmt.Format.cbSize)
        {
            hResult = S_FALSE;
        }

        if (hResult == S_OK)
        {
            break;
        }
    }

    return hResult;
}
