//
// AudioInjectorAPO.h -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Description:
//
//   Declaration of the CAudioInjectorAPO class.
//

#pragma once

#include <audioenginebaseapo.h>
#include <BaseAudioProcessingObject.h>
#include <AudioInjectorAPOInterface.h>
#include <AudioInjectorAPODll.h>
#include <resource.h>

#include <commonmacros.h>
#include <devicetopology.h>
#include <memory>
#include <string>
#include "AudioFileReader.h"

_Analysis_mode_(_Analysis_code_type_user_driver_)

#define PK_EQUAL(x, y)  ((x.fmtid == y.fmtid) && (x.pid == y.pid))

//
// Define a GUID identifying the type of this APO's custom effect.
//
// APOs generally should not define new GUIDs for types of effects and instead
// should use predefined effect types. Only define a new GUID if the effect is
// truly very different from all predefined types of effects.
//

// {DE5E2F27-3A3E-486D-B038-AC4401A774D7}
DEFINE_GUID(InjectEffectId,     0x2EC92F27, 0x3A3E, 0x486D, 0xB0, 0x38, 0xAC, 0x44, 0x01, 0xA7, 0x74, 0xD7);

// Default audio mix ratio (50%)
#define DEFAULT_MIX_RATIO 0.5f

// Default audio file path
#define DEFAULT_AUDIO_FILE_PATH L"C:\\Windows\\Media\\notify.wav"

LONG GetCurrentEffectsSetting(IPropertyStore* properties, PROPERTYKEY pkeyEnable, GUID processingMode);

#pragma AVRT_VTABLES_BEGIN
// Delay APO class - MFX
class CAudioInjectorAPOMFX :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CAudioInjectorAPOMFX, &CLSID_AudioInjectorAPOMFX>,
    public CBaseAudioProcessingObject,
    public IMMNotificationClient,
    public IAudioSystemEffects2,
    // IAudioSystemEffectsCustomFormats may be optionally supported
    // by APOs that attach directly to the connector in the DEFAULT mode streaming graph
    public IAudioSystemEffectsCustomFormats,
    public IAudioInjectorAPOMFX
{
public:    // constructor
    CAudioInjectorAPOMFX()
    :   CBaseAudioProcessingObject(sm_RegProperties)
    ,   m_hEffectsChangedEvent(NULL)
    ,   m_AudioProcessingMode(AUDIO_SIGNALPROCESSINGMODE_DEFAULT)
    ,   m_fEnableAudioMix(FALSE)
    ,   m_fileIndex(0)
    ,   m_mixRatio(DEFAULT_MIX_RATIO)
    ,   m_audioFilePath(DEFAULT_AUDIO_FILE_PATH)
    {
        m_pf32Coefficients = NULL;
    }

    virtual ~CAudioInjectorAPOMFX();    // destructor

DECLARE_REGISTRY_RESOURCEID(IDR_AUDIOINJECTORAPOMFX)

BEGIN_COM_MAP(CAudioInjectorAPOMFX)
    COM_INTERFACE_ENTRY(IAudioInjectorAPOMFX)
    COM_INTERFACE_ENTRY(IAudioSystemEffects)
    COM_INTERFACE_ENTRY(IAudioSystemEffects2)
    // IAudioSystemEffectsCustomFormats may be optionally supported
    // by APOs that attach directly to the connector in the DEFAULT mode streaming graph
    COM_INTERFACE_ENTRY(IAudioSystemEffectsCustomFormats)
    COM_INTERFACE_ENTRY(IMMNotificationClient)
    COM_INTERFACE_ENTRY(IAudioProcessingObjectRT)
    COM_INTERFACE_ENTRY(IAudioProcessingObject)
    COM_INTERFACE_ENTRY(IAudioProcessingObjectConfiguration)
END_COM_MAP()

DECLARE_PROTECT_FINAL_CONSTRUCT()

public:
    STDMETHOD_(void, APOProcess)(UINT32 u32NumInputConnections,
        APO_CONNECTION_PROPERTY** ppInputConnections, UINT32 u32NumOutputConnections,
        APO_CONNECTION_PROPERTY** ppOutputConnections);

    STDMETHOD(GetLatency)(HNSTIME* pTime);

    STDMETHOD(LockForProcess)(UINT32 u32NumInputConnections,
        APO_CONNECTION_DESCRIPTOR** ppInputConnections,
        UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections);

    STDMETHOD(Initialize)(UINT32 cbDataSize, BYTE* pbyData);

    // IAudioSystemEffects2
    STDMETHOD(GetEffectsList)(_Outptr_result_buffer_maybenull_(*pcEffects)  LPGUID *ppEffectsIds, _Out_ UINT *pcEffects, _In_ HANDLE Event);

    virtual HRESULT ValidateAndCacheConnectionInfo(
                                    UINT32 u32NumInputConnections,
                                    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                    UINT32 u32NumOutputConnections,
                                    APO_CONNECTION_DESCRIPTOR** ppOutputConnections);

    // IMMNotificationClient
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
    {
        UNREFERENCED_PARAMETER(pwstrDeviceId);
        UNREFERENCED_PARAMETER(dwNewState);
        return S_OK;
    }
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId)
    {
        UNREFERENCED_PARAMETER(pwstrDeviceId);
        return S_OK;
    }
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId)
    {
        UNREFERENCED_PARAMETER(pwstrDeviceId);
        return S_OK;
    }
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
    {
        UNREFERENCED_PARAMETER(flow);
        UNREFERENCED_PARAMETER(role);
        UNREFERENCED_PARAMETER(pwstrDefaultDeviceId);
        return S_OK;
    }
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key);

    // IAudioSystemEffectsCustomFormats
    // This interface may be optionally supported by APOs that attach directly to the connector in the DEFAULT mode streaming graph
    STDMETHODIMP GetFormatCount(UINT* pcFormats);
    STDMETHODIMP GetFormat(UINT nFormat, IAudioMediaType** ppFormat);
    STDMETHODIMP GetFormatRepresentation(UINT nFormat, _Outptr_ LPWSTR* ppwstrFormatRep);

    // IAudioProcessingObject
    STDMETHODIMP IsOutputFormatSupported(IAudioMediaType *pOppositeFormat, IAudioMediaType *pRequestedOutputFormat, IAudioMediaType **ppSupportedOutputFormat);    STDMETHODIMP CheckCustomFormats(IAudioMediaType *pRequestedFormat);

public:
    LONG                                    m_fEnableAudioMix;
    GUID                                    m_AudioProcessingMode;
    CComPtr<IPropertyStore>                 m_spAPOSystemEffectsProperties;
    CComPtr<IMMDeviceEnumerator>            m_spEnumerator;
    static const CRegAPOProperties<1>       sm_RegProperties;   // registration properties

    // Locked memory
    FLOAT32                                 *m_pf32Coefficients;

    // Audio file mixing properties
    std::unique_ptr<AudioFileReader>        m_pAudioFileReader;
    FLOAT32                                 m_mixRatio;
    UINT32                                  m_fileIndex;
    std::wstring                            m_audioFilePath;

private:
    CCriticalSection                        m_EffectsLock;
    HANDLE                                  m_hEffectsChangedEvent;

    HRESULT ProprietaryCommunicationWithDriver(APOInitSystemEffects2 *_pAPOSysFxInit2);

};
#pragma AVRT_VTABLES_END


#pragma AVRT_VTABLES_BEGIN
// Delay APO class - SFX
class CAudioInjectorAPOSFX :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CAudioInjectorAPOSFX, &CLSID_AudioInjectorAPOSFX>,
    public CBaseAudioProcessingObject,
    public IMMNotificationClient,
    public IAudioSystemEffects2,
    public IAudioInjectorAPOSFX
{
public:    // constructor
    CAudioInjectorAPOSFX()
    :   CBaseAudioProcessingObject(sm_RegProperties)
    ,   m_hEffectsChangedEvent(NULL)
    ,   m_AudioProcessingMode(AUDIO_SIGNALPROCESSINGMODE_DEFAULT)
    ,   m_fEnableAudioMix(FALSE)
    ,   m_fileIndex(0)
    ,   m_mixRatio(DEFAULT_MIX_RATIO)
    ,   m_audioFilePath(DEFAULT_AUDIO_FILE_PATH)
    {
    }

    virtual ~CAudioInjectorAPOSFX();    // destructor

DECLARE_REGISTRY_RESOURCEID(IDR_AUDIOINJECTORAPOSFX)

BEGIN_COM_MAP(CAudioInjectorAPOSFX)
    COM_INTERFACE_ENTRY(IAudioInjectorAPOSFX)
    COM_INTERFACE_ENTRY(IAudioSystemEffects)
    COM_INTERFACE_ENTRY(IAudioSystemEffects2)
    COM_INTERFACE_ENTRY(IMMNotificationClient)
    COM_INTERFACE_ENTRY(IAudioProcessingObjectRT)
    COM_INTERFACE_ENTRY(IAudioProcessingObject)
    COM_INTERFACE_ENTRY(IAudioProcessingObjectConfiguration)
END_COM_MAP()

DECLARE_PROTECT_FINAL_CONSTRUCT()

public:
    STDMETHOD_(void, APOProcess)(UINT32 u32NumInputConnections,
        APO_CONNECTION_PROPERTY** ppInputConnections, UINT32 u32NumOutputConnections,
        APO_CONNECTION_PROPERTY** ppOutputConnections);

    STDMETHOD(GetLatency)(HNSTIME* pTime);

    STDMETHOD(LockForProcess)(UINT32 u32NumInputConnections,
        APO_CONNECTION_DESCRIPTOR** ppInputConnections,
        UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections);

    STDMETHOD(Initialize)(UINT32 cbDataSize, BYTE* pbyData);

    // IAudioSystemEffects2
    STDMETHOD(GetEffectsList)(_Outptr_result_buffer_maybenull_(*pcEffects)  LPGUID *ppEffectsIds, _Out_ UINT *pcEffects, _In_ HANDLE Event);

    // IMMNotificationClient
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
    {
        UNREFERENCED_PARAMETER(pwstrDeviceId);
        UNREFERENCED_PARAMETER(dwNewState);
        return S_OK;
    }
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId)
    {
        UNREFERENCED_PARAMETER(pwstrDeviceId);
        return S_OK;
    }
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId)
    {
        UNREFERENCED_PARAMETER(pwstrDeviceId);
        return S_OK;
    }
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
    {
        UNREFERENCED_PARAMETER(flow);
        UNREFERENCED_PARAMETER(role);
        UNREFERENCED_PARAMETER(pwstrDefaultDeviceId);
        return S_OK;
    }
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key);

public:
    LONG                                    m_fEnableAudioMix;
    GUID                                    m_AudioProcessingMode;
    CComPtr<IPropertyStore>                 m_spAPOSystemEffectsProperties;
    CComPtr<IMMDeviceEnumerator>            m_spEnumerator;
    static const CRegAPOProperties<1>       sm_RegProperties;   // registration properties

    CCriticalSection                        m_EffectsLock;
    HANDLE                                  m_hEffectsChangedEvent;

    // Audio file mixing properties
    std::unique_ptr<AudioFileReader>        m_pAudioFileReader;
    FLOAT32                                 m_mixRatio;
    UINT32                                  m_fileIndex;
    std::wstring                            m_audioFilePath;
};
#pragma AVRT_VTABLES_END

OBJECT_ENTRY_AUTO(__uuidof(AudioInjectorAPOMFX), CAudioInjectorAPOMFX)
OBJECT_ENTRY_AUTO(__uuidof(AudioInjectorAPOSFX), CAudioInjectorAPOSFX)

//
//   Declaration of the ProcessAudioMix routine.
//
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
    FLOAT32     fMixRatio);


//
//   Convenience methods
//

void WriteSilence(
    _Out_writes_(u32FrameCount * u32SamplesPerFrame)
        FLOAT32 *pf32Frames,
    UINT32 u32FrameCount,
    UINT32 u32SamplesPerFrame );

void CopyFrames(
    _Out_writes_(u32FrameCount * u32SamplesPerFrame)
        FLOAT32 *pf32OutFrames,
    _In_reads_(u32FrameCount * u32SamplesPerFrame)
        const FLOAT32 *pf32InFrames,
    UINT32 u32FrameCount,
    UINT32 u32SamplesPerFrame );
