#include "test_util.h"

#include <Psapi.h>

#include <cstdio>
#include <iostream>
#include <intrin.h>
#include <limits.h>
#include <time.h>

#include "..\common\arch_x64.h"
#include "..\common\time_util.h"

#include "..\VivienneCL\debug.h"
#include "..\VivienneCL\driver_io.h"
#include "..\VivienneCL\ntdll.h"


//=============================================================================
// Constants
//=============================================================================
#define PROMPT_DBG_FLUSH_DURATION_MS       (SECONDS_TO_MILLISECONDS(2))


//=============================================================================
// Module Globals
//=============================================================================
static ULONG g_RngSeed = 0;


//=============================================================================
// Meta Interface
//=============================================================================

//
// InitializeRngSeed
//
_Use_decl_annotations_
BOOL
InitializeRngSeed()
{
    g_RngSeed = (ULONG)(time(NULL));

    return TRUE;
}


//=============================================================================
// Debugger Configuration
//=============================================================================

//
// PromptDebuggerExceptionConfiguration
//
// Print a message to instruct the user to configure their debugger's exception
//  handling settings.
//
_Use_decl_annotations_
VOID
PromptDebuggerExceptionConfiguration(
    PCSTR pszPromptMessage,
    PCSTR pszCommandMessage
)
{
    printf(pszPromptMessage);
    printf("    %s\n", pszCommandMessage);
    std::cout.flush();
    Sleep(PROMPT_DBG_FLUSH_DURATION_MS);
    DEBUG_BREAK();
}


//=============================================================================
// Memory
//=============================================================================

//
// IsVirtualAddressValid
//
// Determine if the PTE for the specified address is valid.
//
_Use_decl_annotations_
BOOL
IsVirtualAddressValid(
    PVOID pAddress,
    PBOOL pValid
)
{
    PSAPI_WORKING_SET_EX_INFORMATION WorkingSetInfo = {};
    BOOL status = TRUE;

    // Zero out parameters.
    *pValid = FALSE;

    //
    // Initialize the target virtual address for the working set query.
    //
    WorkingSetInfo.VirtualAddress = pAddress;

    status = QueryWorkingSetEx(
        GetCurrentProcess(),
        &WorkingSetInfo,
        sizeof(WorkingSetInfo));
    if (!status)
    {
        printf("QueryWorkingSetEx failed: %u\n", GetLastError());
        goto exit;
    }

    // Set out parameters.
    if (WorkingSetInfo.VirtualAttributes.Valid)
    {
        *pValid = TRUE;
    }

exit:
    return status;
}


//=============================================================================
// Exception Handlers
//=============================================================================

//
// StealthCheckVectoredHandler
//
_Use_decl_annotations_
LONG
CALLBACK
StealthCheckVectoredHandler(
    PEXCEPTION_POINTERS pExceptionInfo
)
{
    if (STATUS_BREAKPOINT == pExceptionInfo->ExceptionRecord->ExceptionCode)
    {
        FAIL_TEST(
            "Detected breakpoint exception (veh): 0x%IX\n",
            (ULONG_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress);
    }
    else if (STATUS_SINGLE_STEP == pExceptionInfo->ExceptionRecord->ExceptionCode)
    {
        FAIL_TEST(
            "Detected single-step exception (veh): 0x%IX\n",
            (ULONG_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress);
    }

    if (pExceptionInfo->ContextRecord->Dr0 ||
        pExceptionInfo->ContextRecord->Dr1 ||
        pExceptionInfo->ContextRecord->Dr2 ||
        pExceptionInfo->ContextRecord->Dr3 ||
        pExceptionInfo->ContextRecord->Dr6 ||
        pExceptionInfo->ContextRecord->Dr7)
    {
        PrintDebugRegisterContext(pExceptionInfo->ContextRecord);
        FAIL_TEST("Detected non-zero debug register (veh).\n");
    }

    // Continue searching so that our SEH is executed.
    return EXCEPTION_CONTINUE_SEARCH;
}


//
// StealthCheckFilter
//
_Use_decl_annotations_
LONG
WINAPI
StealthCheckFilter(
    unsigned long ExceptionCode,
    PEXCEPTION_POINTERS pExceptionInfo
)
{
    UNREFERENCED_PARAMETER(ExceptionCode);

    if (STATUS_BREAKPOINT == ExceptionCode)
    {
        FAIL_TEST(
            "Detected breakpoint exception (seh): 0x%IX.\n",
            (ULONG_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress);
    }
    else if (STATUS_SINGLE_STEP == ExceptionCode)
    {
        FAIL_TEST(
            "Detected single-step exception (seh): 0x%IX.\n",
            (ULONG_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress);
    }

    if (pExceptionInfo->ContextRecord->Dr0 ||
        pExceptionInfo->ContextRecord->Dr1 ||
        pExceptionInfo->ContextRecord->Dr2 ||
        pExceptionInfo->ContextRecord->Dr3 ||
        pExceptionInfo->ContextRecord->Dr6 ||
        pExceptionInfo->ContextRecord->Dr7)
    {
        PrintDebugRegisterContext(pExceptionInfo->ContextRecord);
        FAIL_TEST("Detected non-zero debug register (seh).\n");
    }

    return EXCEPTION_EXECUTE_HANDLER;
}


//=============================================================================
// Breakpoints
//=============================================================================

//
// iSetThreadLocalHardwareBreakpoint
//
// Install a hardware breakpoint for the target hThread by modifying the debug
//  registers inside the context of the target thread.
//
_Check_return_
BOOL
iSetThreadLocalHardwareBreakpoint(
    _In_ BOOLEAN Enable,
    _In_ ULONG Index,
    _In_ HANDLE hThread,
    _In_ ULONG_PTR Address,
    _In_ HWBP_TYPE Type,
    _In_ HWBP_SIZE Size
)
{
    DR7 NewDr7 = {};
    CONTEXT Context = {};
    BOOL status = TRUE;

    // Initialize context.
    Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    status = GetThreadContext(hThread, &Context);
    if (!status)
    {
        printf("GetThreadContext failed: %u\n", GetLastError());
        goto exit;
    }

    NewDr7.All = Context.Dr7;

    switch (Index)
    {
        //
        // Dr0
        //
        case 0:
            Context.Dr0 = Address;
            NewDr7.L0 = Enable;
            NewDr7.RW0 = (UCHAR)Type;
            NewDr7.Len0 = (UCHAR)Size;
            break;

        //
        // Dr1
        //
        case 1:
            Context.Dr1 = Address;
            NewDr7.L1 = Enable;
            NewDr7.RW1 = (UCHAR)Type;
            NewDr7.Len1 = (UCHAR)Size;
            break;

        //
        // Dr2
        //
        case 2:
            Context.Dr2 = Address;
            NewDr7.L2 = Enable;
            NewDr7.RW2 = (UCHAR)Type;
            NewDr7.Len2 = (UCHAR)Size;
            break;

        //
        // Dr3
        //
        case 3:
            Context.Dr3 = Address;
            NewDr7.L3 = Enable;
            NewDr7.RW3 = (UCHAR)Type;
            NewDr7.Len3 = (UCHAR)Size;
            break;

        //
        // Invalid debug address register index.
        //
        default:
            printf("Invalid debug address register index.\n");
            status = FALSE;
            goto exit;
    }

    //NewDr7.Reserved1 = 1;

    Context.Dr7 = NewDr7.All;
    Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    status = SetThreadContext(hThread, &Context);
    if (!status)
    {
        printf("SetThreadContext failed: %u\n", GetLastError());
        goto exit;
    }

exit:
    return status;
}


//
// BreakpointStealthCheck
//
// View the debug register values for the current thread by exercising
//  exception handlers to inspect thread context.
//
// Caller must install the 'StealthCheckVectoredHandler' VEH before calling
//  this function.
//
_Use_decl_annotations_
BOOL
BreakpointStealthCheck()
{
    CONTEXT Context = {};
    BOOL status = TRUE;

    // Check if the debug registers are set by querying current thread context.
    Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    status = GetThreadContext(GetCurrentThread(), &Context);
    if (!status)
    {
        printf("GetThreadContext failed: %u\n", GetLastError());
        goto exit;
    }

    if (Context.Dr0 ||
        Context.Dr1 ||
        Context.Dr2 ||
        Context.Dr3 ||
        Context.Dr6 ||
        Context.Dr7)
    {
        printf("Detected non-zero debug register.\n");
        PrintDebugRegisterContext(&Context);
        status = FALSE;
        goto exit;
    }

    // Throw an exception to inspect thread context in our VEH and SEH filter.
    __try
    {
        __ud2();
    }
    __except (
        StealthCheckFilter(
            GetExceptionCode(),
            GetExceptionInformation())
        )
    {
        // Do nothing.
    }

exit:
    return status;
}


//
// VerifyThreadLocalBreakpointByIndex
//
_Use_decl_annotations_
BOOL
VerifyThreadLocalBreakpointByIndex(
    ULONG Index,
    HANDLE hThread,
    ULONG_PTR Address,
    HWBP_TYPE Type,
    HWBP_SIZE Size
)
{
    CONTEXT Context = {};
    DR7 ActualDr7 = {};
    BOOL status = TRUE;

    // Initialize context.
    Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    status = GetThreadContext(hThread, &Context);
    if (!status)
    {
        printf("GetThreadContext failed: %u\n", GetLastError());
        goto exit;
    }

    // Initialize actual dr7.
    ActualDr7.All = Context.Dr7;

    switch (Index)
    {
        //
        // Dr0
        //
        case 0:
            if (Address != Context.Dr0 ||
                Type != (HWBP_TYPE)ActualDr7.RW0 ||
                Size != (HWBP_SIZE)ActualDr7.Len0)
            {
                status = FALSE;
                goto exit;
            }

            break;

        //
        // Dr1
        //
        case 1:
            if (Address != Context.Dr1 ||
                Type != (HWBP_TYPE)ActualDr7.RW1 ||
                Size != (HWBP_SIZE)ActualDr7.Len1)
            {
                status = FALSE;
                goto exit;
            }

            break;

        //
        // Dr2
        //
        case 2:
            if (Address != Context.Dr2 ||
                Type != (HWBP_TYPE)ActualDr7.RW2 ||
                Size != (HWBP_SIZE)ActualDr7.Len2)
            {
                status = FALSE;
                goto exit;
            }

            break;

        //
        // Dr3
        //
        case 3:
            if (Address != Context.Dr3 ||
                Type != (HWBP_TYPE)ActualDr7.RW3 ||
                Size != (HWBP_SIZE)ActualDr7.Len3)
            {
                status = FALSE;
                goto exit;
            }

            break;

        //
        // Invalid debug address register index.
        //
        default:
            printf("Invalid index: %u\n", Index);
            status = FALSE;
            goto exit;
    }

exit:
    if (!status)
    {
        PrintDebugRegisterContext(&Context);
    }

    return status;
}


//
// AreAllHardwareBreakpointsCleared
//
// Returns TRUE if all debug registers on all processors are zero.
//
_Use_decl_annotations_
BOOL
AreAllHardwareBreakpointsCleared()
{
    PSYSTEM_DEBUG_STATE pSystemDebugState = NULL;
    ULONG cbSystemDebugState = NULL;
    ULONG RequiredSize = 0;
    PDEBUG_REGISTER_STATE pDebugRegisterState = NULL;
    BOOL status = TRUE;

    // Initial allocation size.
    cbSystemDebugState = sizeof(*pSystemDebugState);

    pSystemDebugState = (PSYSTEM_DEBUG_STATE)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        cbSystemDebugState);
    if (!pSystemDebugState)
    {
        printf("Out of memory.\n");
        status = FALSE;
        goto exit;
    }

    // Multi-processor systems will fail the initial query to obtain the
    //  required struct size.
    if (!VivienneIoQuerySystemDebugState(pSystemDebugState, cbSystemDebugState))
    {
        RequiredSize = pSystemDebugState->Size;

        status = HeapFree(GetProcessHeap(), 0, pSystemDebugState);
        if (!status)
        {
            printf("HeapFree failed: %u\n", GetLastError());
            goto exit;
        }

        pSystemDebugState = (PSYSTEM_DEBUG_STATE)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            RequiredSize);
        if (!pSystemDebugState)
        {
            printf("Out of memory.\n");
            status = FALSE;
            goto exit;
        }

        status =
            VivienneIoQuerySystemDebugState(pSystemDebugState, RequiredSize);
        if (!status)
        {
            printf(
                "VivienneIoQuerySystemDebugState failed twice: %u\n",
                GetLastError());
            goto exit;
        }
    }

    // Check for non-zero fields.
    for (ULONG i = 0; i < pSystemDebugState->NumberOfProcessors; ++i)
    {
        pDebugRegisterState =
            pSystemDebugState->Processors[i].DebugRegisters;

        for (ULONG j = 0;
            j < ARRAYSIZE(pSystemDebugState->Processors->DebugRegisters);
            ++j)
        {
            if (pDebugRegisterState[j].ProcessId ||
                pDebugRegisterState[j].Address ||
                (ULONG)pDebugRegisterState[j].Type ||
                (ULONG)pDebugRegisterState[j].Size)
            {
                printf("Detected an installed breakpoint:\n");
                printf(
                    "    proc=%u Dr%u: pid=%04Iu addr=0x%016IX type=%d size=%d\n",
                    i,
                    j,
                    pDebugRegisterState[j].ProcessId,
                    pDebugRegisterState[j].Address,
                    pDebugRegisterState[j].Type,
                    pDebugRegisterState[j].Size);
                status = FALSE;
                goto exit;
            }
        }
    }

exit:
    if (pSystemDebugState)
    {
        status = HeapFree(GetProcessHeap(), 0, pSystemDebugState);
        if (!status)
        {
            printf("HeapFree failed: %u\n", GetLastError());
        }
    }

    return status;
}


//
// SetThreadLocalHardwareBreakpoint
//
_Use_decl_annotations_
BOOL
SetThreadLocalHardwareBreakpoint(
    ULONG Index,
    HANDLE hThread,
    ULONG_PTR Address,
    HWBP_TYPE Type,
    HWBP_SIZE Size
)
{
    return iSetThreadLocalHardwareBreakpoint(
        TRUE,
        Index,
        hThread,
        Address,
        Type,
        Size);
}


//
// ClearThreadLocalHardwareBreakpoint
//
_Use_decl_annotations_
BOOL
ClearThreadLocalHardwareBreakpoint(
    ULONG Index,
    HANDLE hThread
)
{
    return iSetThreadLocalHardwareBreakpoint(
        FALSE,
        Index,
        hThread,
        0,
        (HWBP_TYPE)0,
        (HWBP_SIZE)0);
}


//=============================================================================
// Random
//=============================================================================

//
// GenerateRandomValue
//
_Use_decl_annotations_
ULONG
GenerateRandomValue()
{
    return RtlRandomEx(&g_RngSeed);
}


//
// GenerateBoundedRandomValue
//
_Use_decl_annotations_
ULONG_PTR
GenerateBoundedRandomValue(
    ULONG_PTR MinValue,
    ULONG_PTR MaxValue
)
{
    return (GenerateRandomValue() % MaxValue) + MinValue;
}


//
// GenerateUniqueRandomValues
//
// This function exists because TestCaptureRegisterValues generated a
//  duplicate value once which caused the CECR request to return one less than
//  the expected value.
//
_Use_decl_annotations_
VOID
GenerateUniqueRandomValues(
    ULONG_PTR pValues[],
    SIZE_T cValues
)
{
    for (SIZE_T i = 0; i < cValues; ++i)
    {
        ULONG_PTR RandomValue = GenerateBoundedRandomValue(0, SIZE_MAX);
        BOOLEAN Duplicate = FALSE;

        // Check that this random value is not already in the array.
        for (SIZE_T j = 0; j < cValues; ++j)
        {
            if (pValues[j] == RandomValue)
            {
                Duplicate = TRUE;
                break;
            }
        }

        // If we encountered a duplicate value then redo this iteration.
        if (Duplicate)
        {
            i--;
        }
        else
        {
            pValues[i] = RandomValue;
        }
    }
}


//=============================================================================
// Logging
//=============================================================================

//
// PrintDebugRegisterContext
//
_Use_decl_annotations_
VOID
PrintDebugRegisterContext(
    PCONTEXT pContext
)
{
    printf("Thread pContext:\n");
    printf("    Dr0: 0x%016IX\n", pContext->Dr0);
    printf("    Dr1: 0x%016IX\n", pContext->Dr1);
    printf("    Dr2: 0x%016IX\n", pContext->Dr2);
    printf("    Dr3: 0x%016IX\n", pContext->Dr3);
    printf("    Dr6: 0x%016IX\n", pContext->Dr6);
    printf("    Dr7: 0x%016IX\n", pContext->Dr7);
}


//=============================================================================
// Process Termination
//=============================================================================

//
// DisplayFailureWindowThenExit
//
#define WINDOW_MESSAGE_SIZE 512
#define WINDOW_CONTEXT_MESSAGE_SIZE (WINDOW_MESSAGE_SIZE - 100)

_Use_decl_annotations_
VOID
DisplayFailureWindowThenExit(
    const char* pszFileName,
    const char* pszFunctionName,
    int LineNumber,
    const char* pszFormat,
    ...
)
{
    va_list valist;
    CHAR pszContextMessage[WINDOW_CONTEXT_MESSAGE_SIZE] = {};
    CHAR pszWindowMessage[WINDOW_MESSAGE_SIZE] = {};

    va_start(valist, pszFormat);
    _vsnprintf_s(
        pszContextMessage + strlen(pszContextMessage),
        sizeof(pszContextMessage) - strlen(pszContextMessage),
        _TRUNCATE,
        pszFormat,
        valist);
    va_end(valist);

    (VOID)_snprintf_s(
        pszWindowMessage,
        sizeof(pszWindowMessage),
        _TRUNCATE,
        ""
            "Processor: %u\n"
            "Process ID: %u\n"
            "Thread ID: %u\n\n"
            "%s\n\n"
            "%s.%d\n\n"
            "%s"
        "",
        GetCurrentProcessorNumber(),
        GetCurrentProcessId(),
        GetCurrentThreadId(),
        pszFileName,
        pszFunctionName,
        LineNumber,
        pszContextMessage);

    printf("Test Failed:\n");
    printf("    Source:  %s %s.%d\n", pszFileName, pszFunctionName, LineNumber);
    printf("    Message: %s\n", pszContextMessage);

    if (IsDebuggerPresent())
    {
        DebugBreak();
    }
    else
    {
        MessageBoxA(NULL, pszWindowMessage, "Test Failed", MB_OK);
    }

    ExitProcess(ERROR_ARENA_TRASHED);
}
