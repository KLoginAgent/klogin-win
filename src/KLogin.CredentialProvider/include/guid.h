#pragma once

#include <windows.h>
#include <initguid.h>

// {8f3e2a10-4b5c-4d6e-9f01-23456789abcd}
DEFINE_GUID(CLSID_KLoginCredentialProvider,
    0x8f3e2a10, 0x4b5c, 0x4d6e, 0x9f, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd);

// {8f3e2a10-4b5c-4d6e-9f01-23456789abce}
DEFINE_GUID(CLSID_KLoginCredentialProviderFilter,
    0x8f3e2a10, 0x4b5c, 0x4d6e, 0x9f, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xce);

// Microsoft field-type GUIDs (from credentialprovider / shlguid; not always exposed as macros).
// {2d837775-f6cd-464e-a745-482fd0b47493}
DEFINE_GUID(KLOGIN_CPFG_CREDENTIAL_PROVIDER_LOGO,
    0x2d837775, 0xf6cd, 0x464e, 0xa7, 0x45, 0x48, 0x2f, 0xd0, 0xb4, 0x74, 0x93);
