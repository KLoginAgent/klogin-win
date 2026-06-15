#include "ClassFactory.h"

#include "KLoginProvider.h"

ClassFactory::ClassFactory() = default;
ClassFactory::~ClassFactory() = default;

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef() { return InterlockedIncrement(&_cRef); }

IFACEMETHODIMP_(ULONG) ClassFactory::Release() {
    const LONG cRef = InterlockedDecrement(&_cRef);
    if (!cRef) {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) {
    if (!ppvObject) {
        return E_POINTER;
    }
    if (pUnkOuter) {
        return CLASS_E_NOAGGREGATION;
    }

    auto* provider = new (std::nothrow) KLoginProvider();
    if (!provider) {
        return E_OUTOFMEMORY;
    }

    const HRESULT hr = provider->QueryInterface(riid, ppvObject);
    provider->Release();
    return hr;
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL) { return S_OK; }
