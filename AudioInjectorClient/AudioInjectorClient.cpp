#include <Windows.h>
#include <Propsys.h> 
#include <Propidl.h>

#include <combaseapi.h>  
#include <atlbase.h>
#include <initguid.h>

#include <Functiondiscoverykeys_devpkey.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>

#include "AudioInjectorClient.h"
#include "../inc/CustomPropKeys.h"

// Helper functions
namespace {
    // Find an audio device by name and return its property store
    HRESULT GetDevicePropertyStore(LPCWSTR deviceName, CComPtr<IMMDevice>& pIMMDevice, CComPtr<IPropertyStore>& pPropertyStore) {

        HRESULT hr = S_OK;
        CComPtr<IMMDeviceEnumerator> pEnumerator;
        CComPtr<IMMDeviceCollection> pDeviceCollection;

        pIMMDevice = nullptr;

        // Create device enumerator
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (FAILED(hr)) return hr;

        // If no device name specified, use the default render device
        if (!deviceName || wcslen(deviceName) == 0) {
            hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pIMMDevice);
            if (FAILED(hr)) return hr;
        } else {
            // Enumerate all render devices
            hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pDeviceCollection);
            if (FAILED(hr)) return hr;

            UINT count = 0;
            hr = pDeviceCollection->GetCount(&count);
            if (FAILED(hr)) return hr;

            // Find the device with matching name
            for (UINT i = 0; i < count; i++) {
                CComPtr<IMMDevice> pCurrentDevice;
                hr = pDeviceCollection->Item(i, &pCurrentDevice);
                if (FAILED(hr)) continue;

                CComPtr<IPropertyStore> pProps;
                hr = pCurrentDevice->OpenPropertyStore(STGM_READ, &pProps);
                if (FAILED(hr)) continue;

                PROPVARIANT varName;
                PropVariantInit(&varName);
                hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR && varName.pwszVal) {
                    if (wcscmp(deviceName, varName.pwszVal) == 0) {
                        pIMMDevice = pCurrentDevice;
                        PropVariantClear(&varName);
                        break;
                    }
                }
                PropVariantClear(&varName);
            }

            if (!pIMMDevice) return AUDCLNT_E_DEVICE_INVALIDATED; // Device not found
        }

        // Open the device's property store for writing
        hr = pIMMDevice->OpenPropertyStore(STGM_READWRITE, &pPropertyStore);
        return hr;
    }
}

// Start audio injection
AUDIOINJECTOR_API HRESULT StartInjection(LPCWSTR deviceName, LPCWSTR filePath, float ratio) {
    if (!filePath) return E_INVALIDARG;

    // Initialize COM if needed
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return hr;
    }

    CComPtr<IPropertyStore> pPropertyStore;
    CComPtr<IMMDevice> pIMMDevice;
    hr = GetDevicePropertyStore(deviceName, pIMMDevice, pPropertyStore);
    if (FAILED(hr)) {
        return hr;
    }

    // Validate mixing ratio
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;    // Set the audio file path property
    PROPVARIANT varFilePath;
    PropVariantInit(&varFilePath);

    // Create a copy of the file path string
    size_t len = wcslen(filePath) + 1;
    varFilePath.pwszVal = (LPWSTR)CoTaskMemAlloc(len * sizeof(wchar_t));
    if (!varFilePath.pwszVal) {
        return E_OUTOFMEMORY;
    }
    wcscpy_s(varFilePath.pwszVal, len, filePath);
    varFilePath.vt = VT_LPWSTR;

    hr = pPropertyStore->SetValue(PKEY_AudioMix_FilePath, varFilePath);
    if (FAILED(hr)) {
        PropVariantClear(&varFilePath);
        return hr;
    }    // Set the mix ratio property
    PROPVARIANT varRatio;
    PropVariantInit(&varRatio);
    varRatio.vt = VT_R4;
    varRatio.fltVal = ratio;
    hr = pPropertyStore->SetValue(PKEY_AudioMix_Ratio, varRatio);
    if (FAILED(hr)) {
        PropVariantClear(&varFilePath);
        PropVariantClear(&varRatio);
        return hr;
    }    // Set the device name property (even if it's null/empty)
    if (deviceName && wcslen(deviceName) > 0) {
        PROPVARIANT varDevName;
        PropVariantInit(&varDevName);

        // Create a copy of the device name string
        size_t len = wcslen(deviceName) + 1;
        varDevName.pwszVal = (LPWSTR)CoTaskMemAlloc(len * sizeof(wchar_t));
        if (!varDevName.pwszVal) {
            PropVariantClear(&varFilePath);
            PropVariantClear(&varRatio);
            return E_OUTOFMEMORY;
        }
        wcscpy_s(varDevName.pwszVal, len, deviceName);
        varDevName.vt = VT_LPWSTR;

        hr = pPropertyStore->SetValue(PKEY_AudioMix_DeviceName, varDevName);
        if (FAILED(hr)) {
            PropVariantClear(&varFilePath);
            PropVariantClear(&varRatio);
            PropVariantClear(&varDevName);
            return hr;
        }
        PropVariantClear(&varDevName);
    }    // Enable the SFX audio injection
    PROPVARIANT varEnable;
    PropVariantInit(&varEnable);
    varEnable.vt = VT_UI4;
    varEnable.ulVal = 1; // Enable
    hr = pPropertyStore->SetValue(PKEY_Endpoint_Enable_Audio_Inject_SFX, varEnable);

    // Clean up PROPVARIANT resources
    PropVariantClear(&varFilePath);
    PropVariantClear(&varRatio);
    PropVariantClear(&varEnable);

    PropVariantClear(&varEnable);
    return hr;
}

// Cancel audio injection
AUDIOINJECTOR_API HRESULT CancelInjection() {
    // Initialize COM if needed
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return hr;
    }

    // Get the default playback device
    CComPtr<IPropertyStore> pPropertyStore;
    CComPtr<IMMDevice> pIMMDevice;
    hr = GetDevicePropertyStore(nullptr, pIMMDevice, pPropertyStore);
    if (FAILED(hr)) {
        return hr;
    }    // Disable the SFX audio injection
    PROPVARIANT varDisable;
    PropVariantInit(&varDisable);
    varDisable.vt = VT_UI4;
    varDisable.ulVal = 0; // Disable
    hr = pPropertyStore->SetValue(PKEY_Endpoint_Enable_Audio_Inject_SFX, varDisable);

    PropVariantClear(&varDisable);
    return hr;
}
