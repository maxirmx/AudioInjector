#pragma once

#include <Windows.h>
#include <Objbase.h>
#include <Propidl.h>
#include <Propsys.h>
#include <string>

// Define the exported functions
#ifdef AUDIOINJECTOR_EXPORTS
#define AUDIOINJECTOR_API __declspec(dllexport)
#else
#define AUDIOINJECTOR_API __declspec(dllimport)
#endif

/**
 * @brief Starts audio injection for a specified audio device.
 *
 * @param deviceName The name of the audio device (null or empty for default device)
 * @param filePath Full path to the audio file to inject
 * @param ratio Mixing ratio between 0.0 and 1.0, where 1.0 means full volume
 * @return HRESULT S_OK on success, error code on failure
 */
extern "C" AUDIOINJECTOR_API HRESULT StartInjection(
    LPCWSTR deviceName,
    LPCWSTR filePath,
    float ratio
);

/**
 * @brief Cancels any active audio injections.
 *
 * @return HRESULT S_OK on success, error code on failure
 */
extern "C" AUDIOINJECTOR_API HRESULT CancelInjection();
