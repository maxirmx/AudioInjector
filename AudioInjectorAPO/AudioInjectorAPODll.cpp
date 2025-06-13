//
// AudioInjectorAPODll.cpp -- Copyright (c) 2025 Maxim [maxirmx] Samsonov. All rights reserved.
//
// Author:
//
// Description:
//
// AudioInjectorAPODll.cpp : Implementation of DLL Exports.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <mmreg.h>

#include "resource.h"
#include "AudioInjectorAPODll.h"
#include <APOLogger.h>
#include <AudioInjectorAPO.h>

#include <AudioInjectorAPODll_i.c>

const LPCWSTR FX_Clsid = L"{D04E05A6-594B-4fb6-A80D-01AF5EED7D1D},7";

//-------------------------------------------------------------------------
// Array of APO_REG_PROPERTIES structures implemented in this module.
// Each new APO implementation will be added to this array.
//
APO_REG_PROPERTIES const *gCoreAPOs[] =
{
    &CAudioInjectorAPOMFX::sm_RegProperties.m_Properties,
    &CAudioInjectorAPOSFX::sm_RegProperties.m_Properties
};

static HRESULT AddApoPerCaptureDevice(void);
static HRESULT RemoveApoFromCaptureDevices(void);

static bool AddToRegMultiSz(HKEY hKey, LPCWSTR valueName, LPCWSTR newValue);
static bool RemoveFromRegMultiSz(HKEY hKey, LPCWSTR valueName, LPCWSTR valueToRemove);


static std::wstring ExtractRealDeviceId(const std::wstring& deviceId);
static bool IsProcessElevated(void);


// {secret}
class CAudioInjectorAPODllModule : public CAtlDllModuleT< CAudioInjectorAPODllModule >
{
public :
    DECLARE_LIBID(LIBID_AudioInjectorAPODlllib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_AUDIOINJECTORAPODLL, "{2EC9D954-674A-4C09-806E-DB4FBE8F199C}")
};

// {secret}
CAudioInjectorAPODllModule _AtlModule;


BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) 
{
    UNREFERENCED_PARAMETER(hModule);
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        // Get appropriate log directory using environment variables
        // wchar_t logPath[MAX_PATH] = {0};
        wchar_t logFileName[MAX_PATH] = { 0 };

        std::time_t t = std::time(nullptr);
        std::tm tm;
        localtime_s(&tm, &t);

        // Format time as YYYY-MM-DD-HH-mm  
        StringCchPrintfW(logFileName, MAX_PATH, L"R:\\AudioInjectorAPO-%04d-%02d-%02d-%02d-%02d-%02d.log",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

        // Convert to UTF-8 for logger
        char logPathA[MAX_PATH] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, logFileName, -1, logPathA, MAX_PATH, NULL, NULL);
        APOLogger::GetInstance().Initialize(logPathA);

        APO_LOG_TRACE_F("Opening AudioInjectorAPO log file at %ls", logFileName);

        /*
         // Try to use %PROGRAMDATA% environment variable first
         DWORD result = GetEnvironmentVariableW(L"PROGRAMDATA", logPath, MAX_PATH);
        if (result > 0 && result < MAX_PATH) {
            // Append company and app name to create folder structure
            StringCchCatW(logPath, MAX_PATH, L"\\AudioMixer");

            // Create directory if it doesn't exist
            CreateDirectoryW(logPath, NULL);
            StringCchCatW(logPath, MAX_PATH, logFileName);

            // Convert to UTF-8 for logger
            char logPathA[MAX_PATH] = {0};
            WideCharToMultiByte(CP_UTF8, 0, logPath, -1, logPathA, MAX_PATH, NULL, NULL);

            APOLogger::GetInstance().Initialize(logPathA);
            APOLogger::GetInstance().Log(LogLevel::INFO, "AudioMixerAPO DllMain - Logging to ProgramData");
        }
        // Fallback to temp directory if we can't access ProgramData
        else {
            char tempPath[MAX_PATH] = {0};
            DWORD pathLen = GetTempPathA(MAX_PATH, tempPath);
            if (pathLen > 0 && pathLen < MAX_PATH) {
                StringCchCatA(tempPath, MAX_PATH, "AudioMixer_APO.log");
                APOLogger::GetInstance().Initialize(tempPath);
                APOLogger::GetInstance().Log(LogLevel::INFO, "AudioMixerAPO DllMain - Logging to temp folder");
            }
            else {
                // Last resort - just log to the current directory
                APOLogger::GetInstance().Initialize("audiomixer_apo.log");
                APOLogger::GetInstance().Log(LogLevel::INFO, "AudioMixerAPO DllMain - Logging to current directory");
            }
        }
        */
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        // Clean up the logger on process exit
        APO_LOG_TRACE_F("Closing AudioInjectorAPO log file.");
        APOLogger::GetInstance().Shutdown();
    }

    return _AtlModule.DllMain(fdwReason, lpReserved);
}

STDAPI DllRegisterServer(void)
{
    if (!IsProcessElevated())
    {
        MessageBoxW(NULL, L"Please run this program as an administrator.", L"Error", MB_OK | MB_ICONERROR);
        return SELFREG_E_CLASS;
    }
    
	HRESULT hr = _AtlModule.DllRegisterServer(false);
	if (SUCCEEDED(hr))
	{
        APO_LOG_TRACE("Successfully registered COM objects.");
        for (int i = 0; i < ARRAYSIZE(gCoreAPOs); i++)
		{
            wchar_t clsidStr[64]{ 0 };
            (void)StringFromGUID2(gCoreAPOs[i]->clsid, clsidStr, ARRAYSIZE(clsidStr));

            HRESULT hr0 = RegisterAPO(gCoreAPOs[i]);
			if (FAILED(hr))
            {
                APO_LOG_ERROR_F("Failed to register APO with CLSID: %ls", clsidStr);
                hr = hr0;
            }
            else
            {
                APO_LOG_TRACE_F("Successfully registered APO with CLSID: %ls", clsidStr);
            }
        }
    }
    else
    {
        APO_LOG_ERROR_F("Failed to register COM objects. Error: 0x%x", hr);
    }

	// Add APO to all active capture devices
    if (SUCCEEDED(hr))
    {
        hr = AddApoPerCaptureDevice();
        if (FAILED(hr))
        {
            APO_LOG_ERROR_F("Failed to add APO to capture devices. Error: 0x%x", hr);
        }
    }
	return hr;
}

STDAPI DllUnregisterServer(void)
{
    if (!IsProcessElevated())
    {
        MessageBoxW(NULL, L"Please run this program as an administrator.", L"Error", MB_OK | MB_ICONERROR);
        return SELFREG_E_CLASS;
    }
    
    // Unregister all the APOs in this module
	HRESULT hr = _AtlModule.DllUnregisterServer(false);
	HRESULT hr0 = S_OK;
	if (SUCCEEDED(hr))
	{
		APO_LOG_TRACE("Successfully unregistered COM objects.");
		for (int i = 0; i < ARRAYSIZE(gCoreAPOs); i++)
		{
            wchar_t clsidStr[64]{ 0 };
            (void)StringFromGUID2(gCoreAPOs[i]->clsid, clsidStr, ARRAYSIZE(clsidStr));

            hr0 = UnregisterAPO(gCoreAPOs[i]->clsid);
			if (FAILED(hr))
			{
				APO_LOG_ERROR_F("Failed to unregister APO with CLSID: %ls", clsidStr);
				hr = hr0;
			}
			else
			{
				APO_LOG_TRACE_F("Successfully unregistered APO with CLSID: %ls", clsidStr);
			}
		}
	}
    else
    {
		APO_LOG_ERROR_F("Failed to unregister COM objects. Error: 0x%x", hr);
    }
    // Remove APO from all active capture devices
    hr0 = RemoveApoFromCaptureDevices();
    if (FAILED(hr0))
    {
        APO_LOG_ERROR_F("Failed to remove APO from capture devices. Error: 0x%x", hr0);
		hr = hr0;
    }
    return hr;
}

// {secret}
STDAPI DllCanUnloadNow(void)
{
    HRESULT hr = _AtlModule.DllCanUnloadNow();
    APO_LOG_TRACE_F("DllCanUnloadNow returning 0x%x", hr);
    return hr;
}


// {secret}
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
    HRESULT hr = _AtlModule.DllGetClassObject(rclsid, riid, ppv);
    APO_LOG_TRACE_F("DllGetClassObject returning 0x%x", hr);
    return hr;
}

static HRESULT AddApoPerCaptureDevice(void)
{
    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        return hr;
    }

    IMMDeviceCollection* pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        pEnumerator->Release();
        return hr;
    }

    UINT count = 0;
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; i++)
    {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pCollection->Item(i, &pDevice)))
        {
            LPWSTR deviceId = nullptr;
            if (SUCCEEDED(pDevice->GetId(&deviceId)))
            {
                // Create registry path under each device's FxProperties
                wchar_t regPath[512];
                swprintf_s(regPath, 
                           ARRAYSIZE(regPath),
                           L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Capture\\%s\\FxProperties", 
                           ExtractRealDeviceId(deviceId).c_str());

                HKEY hKey;
                HRESULT hr0 = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ | KEY_WRITE, &hKey);
                if (hr0 == ERROR_SUCCESS)
                {
                    wchar_t clsidStr[64]{ 0 };
                    int result = StringFromGUID2(gCoreAPOs[1]->clsid, clsidStr, ARRAYSIZE(clsidStr));
                    if (result == 0)
                    {
                        APO_LOG_ERROR("StringFromGUID2 [AddApoPerCaptureDevice] failed to convert CLSID to string.");
                        hr = E_FAIL;
                    }

                    if (AddToRegMultiSz(hKey, FX_Clsid, clsidStr)) 
                    {
                        APO_LOG_TRACE_F("Attached APO CLSID %ls to %ls", clsidStr, regPath);
                    }
                    else
					{
						APO_LOG_ERROR_F("Failed to attach APO CLSID %ls to %ls", clsidStr, regPath);
						hr = E_FAIL;
					}

                    RegCloseKey(hKey);
				}
				else
				{
					APO_LOG_ERROR_F("Failed to open registry key %ls. Error: %d", regPath, hr0);
                    hr = hr0;
				}

                CoTaskMemFree(deviceId);
            }
            pDevice->Release();
        }
    }

    pCollection->Release();
    pEnumerator->Release();

    return S_OK;
}

static HRESULT RemoveApoFromCaptureDevices(void)
{
    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        return hr;
    }

    IMMDeviceCollection* pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        pEnumerator->Release();
        return hr;
    }

    UINT count = 0;
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; i++)
    {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pCollection->Item(i, &pDevice)))
        {
            LPWSTR deviceId = nullptr;
            if (SUCCEEDED(pDevice->GetId(&deviceId)))
            {
                // Open FxProperties key
                wchar_t regPath[512];
                swprintf_s(regPath, 
                           ARRAYSIZE(regPath),
                           L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Capture\\%s\\FxProperties", 
                           ExtractRealDeviceId(deviceId).c_str());

                HKEY hKey;
                HRESULT hr0 = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ | KEY_WRITE, &hKey);
                if (hr0 == ERROR_SUCCESS)
                {
                    wchar_t clsidStr[64] { 0 };
                    int result = StringFromGUID2(gCoreAPOs[1]->clsid, clsidStr, ARRAYSIZE(clsidStr));
                    if (result == 0)
                    {
                        APO_LOG_ERROR("StringFromGUID2 [RemoveApoFromCaptureDevices] failed to convert CLSID to string.");
                        hr = E_FAIL;
                    }


                    if (RemoveFromRegMultiSz(hKey, FX_Clsid, clsidStr))
					{
						APO_LOG_TRACE_F("Detached APO CLSID %ls from %ls", clsidStr, regPath);
					}
					else
					{
						APO_LOG_ERROR_F("Failed to detach APO CLSID %ls from %ls", clsidStr, regPath);
						hr = E_FAIL;
					}

                    RegCloseKey(hKey);
                }
				else
				{
					APO_LOG_ERROR_F("Failed to open registry key %ls. Error: %d", regPath, hr0);
					hr = hr0;
				}

                CoTaskMemFree(deviceId);
            }
            pDevice->Release();
        }
    }

    pCollection->Release();
    pEnumerator->Release();

    return S_OK;
}

static std::wstring ExtractRealDeviceId(const std::wstring& deviceId)
{
    // Find the index of "}.{"
    size_t index = deviceId.find(L"}.{");
    if (index != std::wstring::npos && deviceId.length() > index + 2)
    {
        // Return the substring after "}.{"
        return deviceId.substr(index + 2);
    }
    // Return the original string if "}.{"
    // is not found or the string is too short
    return deviceId;
}

static bool IsProcessElevated(void)
{
    BOOL fIsElevated = FALSE;
    HANDLE hToken = nullptr;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION elevation{};
        DWORD dwSize = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize))
        {
            fIsElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fIsElevated;
}

static bool AddToRegMultiSz(HKEY hKey, LPCWSTR valueName, LPCWSTR newValue)
{
    DWORD dataType = 0;
    DWORD dataSize = 0;

    // Query the existing value to determine its size and type
    if (RegQueryValueExW(hKey, valueName, nullptr, &dataType, nullptr, &dataSize) != ERROR_SUCCESS) {
        // If the value does not exist, initialize it as REG_MULTI_SZ with double null termination
        std::vector<wchar_t> newData(wcslen(newValue) + 2); // +2 for double null termination
        wcscpy_s(newData.data(), newData.size(), newValue);
        newData[wcslen(newValue) + 1] = L'\0'; // Add second null terminator

        if (RegSetValueExW(hKey, valueName, 0, REG_MULTI_SZ,
            (const BYTE*)newData.data(),
            (DWORD)(newData.size() * sizeof(wchar_t))) == ERROR_SUCCESS) {
            return true;
        }
        return false;
    }

    // Handle REG_SZ type by converting it to REG_MULTI_SZ
    if (dataType == REG_SZ) {
        // Allocate a buffer to read the existing REG_SZ value
        std::vector<wchar_t> buffer(dataSize / sizeof(wchar_t));
        if (RegQueryValueExW(hKey, valueName, nullptr, nullptr, (LPBYTE)buffer.data(), &dataSize) != ERROR_SUCCESS) {
            return false;
        }

        // Create a new REG_MULTI_SZ buffer with the existing value and the new value
        size_t existingLength = wcslen(buffer.data());
        std::vector<wchar_t> newBuffer(existingLength + wcslen(newValue) + 3); // +3 for null terminators
        wcscpy_s(newBuffer.data(), newBuffer.size(), buffer.data());
        newBuffer[existingLength + 1] = L'\0'; // Add null terminator for REG_MULTI_SZ
        wcscpy_s(newBuffer.data() + existingLength + 2, wcslen(newValue) + 1, newValue);
        newBuffer[existingLength + wcslen(newValue) + 2] = L'\0'; // Ensure double null termination

        // Write the updated data back to the registry as REG_MULTI_SZ
        if (RegSetValueExW(hKey, valueName, 0, REG_MULTI_SZ,
            (const BYTE*)newBuffer.data(),
            (DWORD)(newBuffer.size() * sizeof(wchar_t))) != ERROR_SUCCESS) {
            return false;
        }
        return true;
    }

    // Handle REG_MULTI_SZ type
    if (dataType == REG_MULTI_SZ) {
        // Allocate a buffer to read the existing data
        std::vector<wchar_t> buffer(dataSize / sizeof(wchar_t));
        if (RegQueryValueExW(hKey, valueName, nullptr, nullptr, (LPBYTE)buffer.data(), &dataSize) != ERROR_SUCCESS) {
            return false;
        }

        // Check if the value already exists
        const wchar_t* current = buffer.data();
        while (*current) {
            if (wcscmp(current, newValue) == 0) {
                // Value already exists, no need to add
                return true;
            }
            current += wcslen(current) + 1;
        }

        // Calculate position of the final null terminator (before the second null in the double-null terminator)
        size_t endPos = dataSize / sizeof(wchar_t) - 1;

        // Create new buffer with added string
        std::vector<wchar_t> newBuffer(endPos + wcslen(newValue) + 2); // +2 for null terminators

        // Copy existing strings (excluding the double null terminator)
        memcpy(newBuffer.data(), buffer.data(), endPos * sizeof(wchar_t));

        // Add the new string with its null terminator
        wcscpy_s(newBuffer.data() + endPos, wcslen(newValue) + 1, newValue);

        // Ensure double null termination
        newBuffer[endPos + wcslen(newValue) + 1] = L'\0';

        // Write the updated data back to the registry
        if (RegSetValueExW(hKey, valueName, 0, REG_MULTI_SZ,
            (const BYTE*)newBuffer.data(),
            (DWORD)(newBuffer.size() * sizeof(wchar_t))) != ERROR_SUCCESS) {
            return false;
        }

        return true;
    }

    // Unsupported data type
    return false;
}

static bool RemoveFromRegMultiSz(HKEY hKey, LPCWSTR valueName, LPCWSTR valueToRemove) 
{
    DWORD dataType = 0;
    DWORD dataSize = 0;

    // Query the existing value to determine its size
    if (RegQueryValueExW(hKey, valueName, nullptr, &dataType, nullptr, &dataSize) != ERROR_SUCCESS) {
        // Value does not exist, nothing to remove
        return false;
    }

    // Ensure the value is of type REG_MULTI_SZ otherwise there is nothing to do
    if (dataType != REG_MULTI_SZ) {
        return true;
    }

    // Allocate a buffer to read the existing data
    std::vector<wchar_t> buffer(dataSize / sizeof(wchar_t));
    if (RegQueryValueExW(hKey, valueName, nullptr, nullptr, (LPBYTE)buffer.data(), &dataSize) != ERROR_SUCCESS) {
        return false;
    }

    // Create a new buffer to store the updated data
    std::vector<wchar_t> updatedBuffer;
    const wchar_t* current = buffer.data();
    bool found = false;

    // Iterate through the existing strings
    while (*current) {
        if (wcscmp(current, valueToRemove) != 0) {
            // Copy strings that are not the one to remove
            updatedBuffer.insert(updatedBuffer.end(), current, current + wcslen(current) + 1);
        }
        else {
            found = true; // Mark that the value was found and removed
        }
        current += wcslen(current) + 1;
    }

    // If the value was not found, return true (nothing to do)
    if (!found) {
        return true;
    }

    // Add the final null terminator for REG_MULTI_SZ
    updatedBuffer.push_back(L'\0');

    // Write the updated data back to the registry
    if (RegSetValueExW(hKey, valueName, 0, REG_MULTI_SZ, (const BYTE*)updatedBuffer.data(), (DWORD)(updatedBuffer.size() * sizeof(wchar_t))) != ERROR_SUCCESS) {
        return false;
    }

    return true;
}