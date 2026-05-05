/**
 * GNU General Public License, version 2.0.
 *
 * Copyright (c) 2025 Tijme Gommers (@tijme).
 *
 * This source code file is part of Dittobytes. Dittobytes is 
 * licensed under GNU General Public License, version 2.0, and 
 * you are free to use, modify, and distribute this file under 
 * its terms. However, any modified versions of this file must 
 * include this same license and copyright notice.
 */

/**
 * This `calc.exe` pop example only works on Windows.
 */
#if !defined(__WINDOWS__)
#error "This `calc.exe` pop example only works on Windows AMD64 & ARM64. Build with `make beacon-all-win-all`."
#endif

/**
 * Booleans.
 * 
 * Defines boolean types.
 * https://pubs.opengroup.org/onlinepubs/007904975/basedefs/stdbool.h.html
 */
#include <stdbool.h>

/**
 * Integers.
 * 
 * Defines macros that specify limits of integer types corresponding to types defined in other standard headers.
 * https://pubs.opengroup.org/onlinepubs/009696899/basedefs/stdint.h.html
 */
#include <stdint.h>

/**
 * Windows API.
 * 
 * Contains declarations for all of the functions, macro's & data types in the Windows API.
 * https://docs.microsoft.com/en-us/previous-versions//aa383749(v=vs.85)?redirectedfrom=MSDN
 */
#include <windows.h>

/**
 * Internal NT API's and data structures.
 * 
 * Helper library that contains NT API's and data structures for system services, security and identity.
 * https://docs.microsoft.com/en-us/windows/win32/api/winternl/
 */
#include <winternl.h>

/**
 * Windows API.
 * 
 * Contains declarations for all of the functions, macro's & data types in the Windows API.
 * https://docs.microsoft.com/en-us/previous-versions//aa383749(v=vs.85)?redirectedfrom=MSDN
 */
#include <windows.h>

/**
 * A struct of module definitions we are loading.
 */
struct ModuleTable {
    HMODULE hKernel32;
};

/**
 * A struct of function definitions we are loading.
 */
struct FunctionTable {
    // Must always be present
    HMODULE (*LoadLibraryA)(LPCSTR lpLibFileName);
    FARPROC (*GetProcAddress)(HMODULE hModule, LPCSTR lpProcName);

    // Any other functions we use in this verification
    UINT (*WinExec)(LPCSTR, UINT);
};

/**
 * The main struct that holds our modules & functions to be used.
 */
struct Relocatable {
    struct ModuleTable modules;
    struct FunctionTable functions;
};

/**
 * Helper Macro Functions
 */
#define DEFINE_STRING(name, value) char name[] = value;

/**
 * Get current Process Environment Block.
 *
 * @return PEB* The current PEB.
 */
void* RelocatableNtGetPeb();

/**
 * Retrieve the LDR_DATA_TABLE_ENTRY from a given LIST_ENTRY pointer.
 * 
 * @param ptr The LIST_ENTRY pointer to retrieve the data table entry from.
 * @return LDR_DATA_TABLE_ENTRY* The corresponding LDR_DATA_TABLE_ENTRY pointer.
 */
LDR_DATA_TABLE_ENTRY *RelocatableGetDataTableEntry(const LIST_ENTRY *ptr);

/**
 * Compare two null-terminated strings.
 * 
 * @param a First string.
 * @param b Second string.
 * @return true if the strings are equal, false otherwise.
 */
bool RelocatableStrCmp(const char *a, const char *b);

/**
 * Retrieve the address of a function from a module in memory by matching
 * both the module name and the function name.
 * 
 * This function and its dependencies are inspired on ShellcodeStdio 
 * from @jackullrich: https://github.com/jackullrich/ShellcodeStdio/tree/master
 * 
 * @param moduleName The name of the module to search for.
 * @param functionName The name of the function to search for.
 * @return void* The address of the function if found, NULL otherwise.
 */
void* RelocatablePreliminaryGetProcAddress(const char *moduleName, const char *functionName);

/**
 * Initialize Relocatable by resolving the two main Windows APIs it depends on.
 * 
 * @param struct Relocatable* context A 'global' variable capturing Relocatable's entire context (loaded modules & functions)
 */
void InitializeRelocatable(struct Relocatable* context);

/**
 * Populate the context tables with modules & functions you would like to use.
 * 
 * @param struct Relocatable* context A 'global' variable capturing Relocatable's entire context (loaded modules & functions)
 */
void PopulateTables(struct Relocatable* context);

/**
 * The main function to pop `calc.exe`.
 */
uint32_t EntryFunction() {
    struct Relocatable context;

    // Populate module & function tables
    InitializeRelocatable(&context);
    PopulateTables(&context);

    // Run WinExec and return its return value
    DEFINE_STRING(CalculatorBinary, "calc.exe");
    return (uint32_t) context.functions.WinExec(CalculatorBinary, SW_SHOW) > 31;
}

/**
 * Get current Process Environment Block.
 *
 * @return PEB* The current PEB.
 */
void* RelocatableNtGetPeb() {
    void* lpTEB;
    void* lpPEB;

    #if defined(__AMD64__)
        lpPEB = (void*) __readgsqword(0x60);
    #elif defined(__ARM64__)
        __asm__("mov %0, x18" : "=r" (lpTEB));
        lpPEB = * (void**) ((char*) lpTEB + 0x60);
    #else
        #error "This architecture is currently unsupported"
    #endif

    return lpPEB;
}

/**
 * Retrieve the LDR_DATA_TABLE_ENTRY from a given LIST_ENTRY pointer.
 * 
 * @param ptr The LIST_ENTRY pointer to retrieve the data table entry from.
 * @return LDR_DATA_TABLE_ENTRY* The corresponding LDR_DATA_TABLE_ENTRY pointer.
 */
LDR_DATA_TABLE_ENTRY *RelocatableGetDataTableEntry(const LIST_ENTRY *ptr) {
    LDR_DATA_TABLE_ENTRY* entry = (LDR_DATA_TABLE_ENTRY *) 0;
    size_t offset = (size_t) &entry->InMemoryOrderLinks;
    return (LDR_DATA_TABLE_ENTRY *)((uint8_t *) ptr - offset);
}

/**
 * Compare two null-terminated strings.
 * 
 * @param a First string.
 * @param b Second string.
 * @return true if the strings are equal, false otherwise.
 */
bool RelocatableStrCmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++, b++;
    }
    
    return *a == *b;
}

/**
 * Retrieve the address of a function from a module in memory by matching
 * both the module name and the function name.
 * 
 * This function and its dependencies are inspired on ShellcodeStdio 
 * from @jackullrich: https://github.com/jackullrich/ShellcodeStdio/tree/master
 * 
 * @param moduleName The name of the module to search for.
 * @param functionName The name of the function to search for.
 * @return void* The address of the function if found, NULL otherwise.
 */
void* RelocatablePreliminaryGetProcAddress(const char *moduleName, const char *functionName) {
    PEB *peb = RelocatableNtGetPeb();
    LIST_ENTRY *first = peb->Ldr->InMemoryOrderModuleList.Flink;
    LIST_ENTRY *ptr = first;

    do {
        LDR_DATA_TABLE_ENTRY *dte = RelocatableGetDataTableEntry(ptr);
        ptr = ptr->Flink;

        uint8_t *base = (uint8_t *)dte->DllBase;
        if (!base) continue;

        IMAGE_DOS_HEADER *dosHdr = (IMAGE_DOS_HEADER *)base;
        IMAGE_NT_HEADERS *ntHdrs = (IMAGE_NT_HEADERS *)(base + dosHdr->e_lfanew);
        DWORD expDirRVA = ntHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        if (!expDirRVA) continue;

        IMAGE_EXPORT_DIRECTORY *expDir = (IMAGE_EXPORT_DIRECTORY *)(base + expDirRVA);
        if (!RelocatableStrCmp(moduleName, (char *)(base + expDir->Name))) continue;

        DWORD *nameRVAs = (DWORD *)(base + expDir->AddressOfNames);
        WORD *ordinals = (WORD *)(base + expDir->AddressOfNameOrdinals);
        DWORD *funcRVAs = (DWORD *)(base + expDir->AddressOfFunctions);

        for (DWORD i = 0; i < expDir->NumberOfNames; i++) {
            if (RelocatableStrCmp(functionName, (char *)(base + nameRVAs[i]))) {
                return (void *)(base + funcRVAs[ordinals[i]]);
            }
        }
    } while (ptr != first);
 
    return NULL;
}

/**
 * Initialize Relocatable by resolving the two main Windows APIs it depends on.
 * 
 * @param struct Relocatable* context A 'global' variable capturing Relocatable's entire context (loaded modules & functions)
 */
void InitializeRelocatable(struct Relocatable* context) {
    // Resolve LoadLibraryA and GetProcAddress (assuming `KERNEL32.dll` is loaded)
    DEFINE_STRING(Kernel32ModuleName, "KERNEL32.dll");
    DEFINE_STRING(LoadLibraryAFunctionName, "LoadLibraryA");
    DEFINE_STRING(GetProcAddressFunctionName, "GetProcAddress");

    context->functions.LoadLibraryA = (HMODULE (*)(LPCSTR lpLibFileName)) RelocatablePreliminaryGetProcAddress(Kernel32ModuleName, LoadLibraryAFunctionName);
    context->functions.GetProcAddress = (FARPROC (*)(HMODULE hModule, LPCSTR lpProcName)) RelocatablePreliminaryGetProcAddress(Kernel32ModuleName, GetProcAddressFunctionName);
}

/**
 * Populate the context tables with modules & functions you would like to use.
 * 
 * @param struct Relocatable* context A 'global' variable capturing Relocatable's entire context (loaded modules & functions)
 */
void PopulateTables(struct Relocatable* context) {
    // Define modules
    DEFINE_STRING(Kernel32ModuleName, "KERNEL32.dll");

    // Load modules
    context->modules.hKernel32 = context->functions.LoadLibraryA(Kernel32ModuleName);

    // Define functions
    DEFINE_STRING(WinExecFunctionName, "WinExec");

    // Load functions
    context->functions.WinExec = (UINT (*)(LPCSTR, UINT)) context->functions.GetProcAddress(context->modules.hKernel32, WinExecFunctionName);
}
