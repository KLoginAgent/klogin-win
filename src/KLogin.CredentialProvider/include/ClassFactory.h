#pragma once

#include <unknwn.h>

class ClassFactory : public IClassFactory {
public:
    explicit ClassFactory(REFCLSID clsid);
    virtual ~ClassFactory();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    IFACEMETHODIMP LockServer(BOOL fLock) override;

private:
    long _cRef = 1;
    CLSID _clsid{};
};
