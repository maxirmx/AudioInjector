// Microsoft Windows
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once

// header files for imported files
#include "propidl.h"

#ifdef DEFINE_PROPERTYKEY
#undef DEFINE_PROPERTYKEY
#endif

#ifdef INITGUID
#define DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const PROPERTYKEY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const PROPERTYKEY name
#endif // INITGUID

// ----------------------------------------------------------------------
//
// PKEY_Endpoint_Enable_Audio_Inject_SFX: When value is 0x00000001, Audio Inject local effect is enabled
// {2EC931EF-5377-4944-AE15-53789A9629C7},4
// vartype = VT_UI4
DEFINE_PROPERTYKEY(PKEY_Endpoint_Enable_Audio_Inject_SFX, 0x2EC931ef, 0x5377, 0x4944, 0xae, 0x15, 0x53, 0x78, 0x9a, 0x96, 0x29, 0xc7, 4);

// PKEY_Endpoint_Enable_Audio_Inject_MFX: When value is 0x00000001, Audio Inject global effect is enabled
// {2EC931EF-5377-4944-AE15-53789A9629C7},5
// vartype = VT_UI4
DEFINE_PROPERTYKEY(PKEY_Endpoint_Enable_Audio_Inject_MFX, 0xa2EC91ef, 0x5377, 0x4944, 0xae, 0x15, 0x53, 0x78, 0x9a, 0x96, 0x29, 0xc7, 5);

// Define custom property keys for audio mixing
// PKEY_AudioMix_FilePath: Audio file path for mixing
// {2EC9CC99-23EA-4997-9D60-F5E22C1FD845},0
// vartype = VT_LPWSTR
DEFINE_PROPERTYKEY(PKEY_AudioMix_FilePath, 0x2EC9cc99, 0x23ea, 0x4997, 0x9d, 0x60, 0xf5, 0xe2, 0x2c, 0x1f, 0xd8, 0x45, 0);

// PKEY_AudioMix_Ratio: Audio mix ratio for mixing (0.0-1.0)
// {2EC9CC99-23EA-4997-9D60-F5E22C1FD845},1
// vartype = VT_R4
DEFINE_PROPERTYKEY(PKEY_AudioMix_Ratio, 0x2EC9cc99, 0x23ea, 0x4997, 0x9d, 0x60, 0xf5, 0xe2, 0x2c, 0x1f, 0xd8, 0x45, 1);

// PKEY_AudioMix_DeviceName: Audio device name for input (null means default)
// {2EC9CC99-23EA-4997-9D60-F5E22C1FD845},2
// vartype = VT_LPWSTR
DEFINE_PROPERTYKEY(PKEY_AudioMix_DeviceName, 0x2EC9cc99, 0x23ea, 0x4997, 0x9d, 0x60, 0xf5, 0xe2, 0x2c, 0x1f, 0xd8, 0x45, 2);
