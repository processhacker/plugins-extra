#ifndef EXPLORER_H
#define EXPLORER_H

#pragma comment(lib, "Samlib.lib")

#include <phdk.h>
#include <secedit.h>

#include "resource.h"

#ifndef SX_MAIN_PRIVATE
extern PPH_PLUGIN PluginInstance;
#endif

VOID SxShowExplorer();

_Callback_ NTSTATUS SxStdGetObjectSecurity(
    _Out_ PSECURITY_DESCRIPTOR *SecurityDescriptor,
    _In_ SECURITY_INFORMATION SecurityInformation,
    _In_opt_ PVOID Context
    );

_Callback_ NTSTATUS SxStdSetObjectSecurity(
    _In_ PSECURITY_DESCRIPTOR SecurityDescriptor,
    _In_ SECURITY_INFORMATION SecurityInformation,
    _In_opt_ PVOID Context
    );

#endif
