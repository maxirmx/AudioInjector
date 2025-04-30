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
#include <AudioInjectorAPO.h>

#include <AudioInjectorAPODll_i.c>

//-------------------------------------------------------------------------
// Array of APO_REG_PROPERTIES structures implemented in this module.
// Each new APO implementation will be added to this array.
//
APO_REG_PROPERTIES const *gCoreAPOs[] =
{
    &CAudioInjectorAPOMFX::sm_RegProperties.m_Properties,
    &CAudioInjectorAPOSFX::sm_RegProperties.m_Properties
};

// {secret}
class CAudioInjectorAPODllModule : public CAtlDllModuleT< CAudioInjectorAPODllModule >
{
public :
    DECLARE_LIBID(LIBID_AudioInjectorAPODlllib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_AUDIOINJECTORAPODLL, "{2EC9D954-674A-4C09-806E-DB4FBE8F199C}")
};

// {secret}
CAudioInjectorAPODllModule _AtlModule;


// {secret}
extern "C" BOOL WINAPI DllMain(HINSTANCE /* hInstance */, DWORD dwReason, LPVOID lpReserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
    {
    }
    // do necessary cleanup only if the DLL is being unloaded dynamically
    else if ((DLL_PROCESS_DETACH == dwReason) && (NULL == lpReserved))
    {
    }

    return _AtlModule.DllMain(dwReason, lpReserved);
}


// {secret}
STDAPI DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}


// {secret}
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

