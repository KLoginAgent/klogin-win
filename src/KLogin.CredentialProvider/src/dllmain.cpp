#include <credentialprovider.h>
#include <new>
#include <windows.h>

#include "ClassFactory.h"
#define INITGUID
#include "guid.h"

HINSTANCE g_hInstance = nullptr;

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_hInstance = module;
            DisableThreadLibraryCalls(module);
            break;
        default:
            break;
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() {
    return S_OK;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    if (!IsEqualCLSID(rclsid, CLSID_KLoginCredentialProvider) &&
        !IsEqualCLSID(rclsid, CLSID_KLoginCredentialProviderFilter)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) ClassFactory(rclsid);
    if (!factory) {
        return E_OUTOFMEMORY;
    }

    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}
