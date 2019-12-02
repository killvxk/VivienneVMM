/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

Module Name:

    breakpoint_manager.h

Abstract:

    This header defines the hardware breakpoint manager interface.

Author:

    changeofpace

Environment:

    Kernel mode only.

--*/

#pragma once

#include <fltKernel.h>

#include "breakpoint_callback.h"

#include "..\common\arch_x64.h"
#include "..\common\driver_io_types.h"

#include "HyperPlatform\HyperPlatform\ia32_type.h"

//=============================================================================
// Types
//=============================================================================
typedef struct _HARDWARE_BREAKPOINT {
    HANDLE ProcessId;
    ULONG Index;
    ULONG_PTR Address;
    HWBP_TYPE Type;
    HWBP_SIZE Size;
} HARDWARE_BREAKPOINT, *PHARDWARE_BREAKPOINT;

//=============================================================================
// Meta Interface
//=============================================================================
_Check_return_
NTSTATUS
BpmDriverEntry();

VOID
BpmDriverUnload();

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
NTSTATUS
BpmQuerySystemDebugState(
    _Out_ PSYSTEM_DEBUG_STATE pSystemDebugState,
    _In_ ULONG cbSystemDebugState
);

_Check_return_
NTSTATUS
BpmInitializeBreakpoint(
    _In_ ULONG_PTR ProcessId,
    _In_ ULONG Index,
    _In_ ULONG_PTR Address,
    _In_ HWBP_TYPE Type,
    _In_ HWBP_SIZE Size,
    _Out_ PHARDWARE_BREAKPOINT pBreakpoint
);

//
// TODO Add support for automatically setting host CR3 to the directory table
//  of the target process. The 'set breakpoint' functions should accept a
//  boolean parameter which indicates if the host CR3 should be updated before
//  the callback is invoked.
//
// TODO Research the side effects of modifying host CR3.
//  See: 4.10.4.1 Operations that Invalidate TLBs and Paging-Structure Caches
//
_Check_return_
NTSTATUS
BpmSetHardwareBreakpoint(
    _In_ PHARDWARE_BREAKPOINT pBreakpoint,
    _In_ FPBREAKPOINT_CALLBACK pCallbackFn,
    _In_opt_ PVOID pCallbackCtx
);

_Check_return_
NTSTATUS
BpmSetHardwareBreakpoint(
    _In_ ULONG_PTR ProcessId,
    _In_ ULONG Index,
    _In_ ULONG_PTR Address,
    _In_ HWBP_TYPE Type,
    _In_ HWBP_SIZE Size,
    _In_ FPBREAKPOINT_CALLBACK pCallbackFn,
    _In_opt_ PVOID pCallbackCtx
);

_Check_return_
NTSTATUS
BpmClearHardwareBreakpoint(
    _In_ ULONG Index
);

_Check_return_
NTSTATUS
BpmCleanupBreakpoints();

//=============================================================================
// Vmx Interface
//=============================================================================
_IRQL_requires_(HIGH_LEVEL)
_Check_return_
NTSTATUS
BpmVmxSetHardwareBreakpoint(
    _In_ PVOID pVmxContext
);

_IRQL_requires_(HIGH_LEVEL)
_Check_return_
NTSTATUS
BpmVmxProcessDebugExceptionEvent(
    _Inout_ GpRegisters* pGuestRegisters,
    _Inout_ FlagRegister* pGuestFlags,
    _In_ PULONG_PTR pGuestIp
);
