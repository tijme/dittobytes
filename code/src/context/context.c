#include "../../inc/context/context.h"

PIMAGE_NT_HEADERS get_nt_headers(PVOID module_addr) {
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module_addr;
    LONG nt_header_rva = dos_header->e_lfanew;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)module_addr + nt_header_rva);

    return ntHeaders;
}

PIMAGE_OPTIONAL_HEADER get_optional_header(PVOID module_addr) {
    PIMAGE_NT_HEADERS ntHeaders = get_nt_headers(module_addr);
    PIMAGE_OPTIONAL_HEADER optionalHeader = &ntHeaders->OptionalHeader;

    return optionalHeader;
}

PIMAGE_DATA_DIRECTORY get_data_directory(PVOID dllAddr, SIZE_T directoryEntry) {
    PIMAGE_OPTIONAL_HEADER optionalHeader = get_optional_header(dllAddr);
    PIMAGE_DATA_DIRECTORY dataDirectory = &(optionalHeader->DataDirectory[directoryEntry]);

    return dataDirectory;
}

PIMAGE_EXPORT_DIRECTORY get_export_directory(PVOID dllAddr) {
    PIMAGE_DATA_DIRECTORY dataDirectory = get_data_directory(dllAddr, IMAGE_DIRECTORY_ENTRY_EXPORT);
    DWORD virtualAddr = dataDirectory->VirtualAddress;
    PIMAGE_EXPORT_DIRECTORY exportDirectory = (PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)dllAddr + virtualAddr);

    return exportDirectory;
}

PPEB get_peb() {
    PPEB peb = NULL;

#if defined(_WIN64)
    peb = (PPEB)(__readgsqword(0x60));
#elif defined(_WIN32)
    peb = (PPEB)(__readfsdword(0x30));
#endif

    return peb;
}

PVOID get_module_addr(PWCHAR module_name) {
    PPEB peb = get_peb();
    if(peb == NULL) return NULL;
    
    PLIST_ENTRY modules_list = &peb->Ldr->InMemoryOrderModuleList;

    for (PLIST_ENTRY entry = modules_list->Flink; entry != modules_list; entry = entry->Flink) {
        C_PLDR_DATA_TABLE_ENTRY ldr_entry = CONTAINING_RECORD(
            entry,
            C_LDR_DATA_TABLE_ENTRY,
            InMemoryOrderLinks
        );

        UNICODE_STRING current_module = ldr_entry->BaseDllName;
        PWSTR current_name = current_module.Buffer;
        UINT32 name_length = current_module.Length / sizeof(WCHAR);

        BOOL names_matched = TRUE;

        for (SIZE_T i = 0; i <= name_length; ++i) {
            WCHAR char1 = module_name[i];
            WCHAR char2 = current_name[i];

            if (char1 >= L'a' && char1 <= L'z') char1 -= 0x20;
            if (char2 >= L'a' && char2 <= L'z') char2 -= 0x20;

            if (char1 != char2) {
                names_matched = FALSE;
                break;
            }

            if (char1 == L'\0') break;
        }

        if (names_matched) {
            return ldr_entry->DllBase;
        }
    }

    return NULL;
}

PVOID get_function_from_module(PCHAR function_name, PVOID module_addr) {
    PIMAGE_EXPORT_DIRECTORY export_directory = get_export_directory(module_addr);

    DWORD number_of_names = export_directory->NumberOfNames;

    PDWORD address_of_functions = (PDWORD)((ULONG_PTR)module_addr + export_directory->AddressOfFunctions);
    PWORD address_of_name_ordinals = (PWORD)((ULONG_PTR)module_addr + export_directory->AddressOfNameOrdinals);
    PDWORD address_of_names = (PDWORD)((ULONG_PTR)module_addr + export_directory->AddressOfNames);

    for (UINT32 i = 0; i < number_of_names; i++) {
        LPSTR current_name = (LPSTR)((ULONG_PTR)module_addr + address_of_names[i]);
        UINT8 charCount = 0;

        while (TRUE) {
            CHAR char1 = function_name[charCount];
            CHAR char2 = current_name[charCount];

            if (char1 != char2) {
                break;
            }

            if (char1 == 0 && char2 == 0) {
                WORD ordinal_value = address_of_name_ordinals[i];
                DWORD relative_function_addr = address_of_functions[ordinal_value];
                PVOID function_addr = (PVOID)((ULONG_PTR)module_addr + relative_function_addr);

                return function_addr;
            }

            charCount++;
        }
    }
    
    return 0;
}

VOID cleanup_context(PCTX context) {
    if (context->modules.user32) {
        context->functions.FreeLibrary(context->modules.user32);
    }
    if (context->modules.crypt32) {
        context->functions.FreeLibrary(context->modules.crypt32);
    }
    if (context->modules.advapi32) {
        context->functions.FreeLibrary(context->modules.advapi32);
    }
    if (context->modules.wininet) {
        context->functions.FreeLibrary(context->modules.wininet);
    }
}

BOOL init_functions_from_wininet(PCTX context) {
    HMODULE wininet_addr = context->modules.wininet;
    
    RESOLVE_OR_FAIL(context, wininet_addr, InternetOpenW, INTERNET_OPEN_W);
    RESOLVE_OR_FAIL(context, wininet_addr, InternetConnectW, INTERNET_CONNECT_W);
    RESOLVE_OR_FAIL(context, wininet_addr, InternetCloseHandle, INTERNET_CLOSE_HANDLE);
    RESOLVE_OR_FAIL(context, wininet_addr, HttpOpenRequestW, HTTP_OPEN_REQUEST_W);
    RESOLVE_OR_FAIL(context, wininet_addr, HttpSendRequestW, HTTP_SEND_REQUEST_W);
    RESOLVE_OR_FAIL(context, wininet_addr, InternetReadFile, INTERNET_READ_FILE);
    RESOLVE_OR_FAIL(context, wininet_addr, HttpQueryInfoW, HTTP_QUERY_INFO_W);
    
    return TRUE;
}

BOOL init_functions_from_crypt32(PCTX context) {
    HMODULE crypt32_addr = context->modules.crypt32;

    RESOLVE_OR_FAIL(context, crypt32_addr, CryptBinaryToStringA, CRYPT_BINARY_TO_STRING_A);
    RESOLVE_OR_FAIL(context, crypt32_addr, CryptStringToBinaryA, CRYPT_STRING_TO_BINARY_A);
    
    return TRUE;
}

BOOL init_functions_from_advapi32(PCTX context) {
    HMODULE advapi32_addr = context->modules.advapi32;

    RESOLVE_OR_FAIL(context, advapi32_addr, GetUserNameW, GET_USER_NAME_W);
    
    return TRUE;
}

BOOL init_functions_from_user32(PCTX context) {
    HMODULE user32_addr = context->modules.user32;

    RESOLVE_OR_FAIL(context, user32_addr, MessageBoxW, MESSAGE_BOX_W);
    RESOLVE_OR_FAIL(context, user32_addr, MessageBoxA, MESSAGE_BOX_A);
    RESOLVE_OR_FAIL(context, user32_addr, wsprintfA, WSPRINTF_A);
    RESOLVE_OR_FAIL(context, user32_addr, wsprintfW, WSPRINTF_W);
    
    return TRUE;
}

BOOL init_functions_from_kernel32(PCTX context) {
    HMODULE kernel32_addr = (HMODULE)context->modules.kernel32;

    // Manually resolving LoadLibraryA and GetProcAddress.
    context->functions.LoadLibraryA = (LOAD_LIBRARY_A)get_function_from_module(
        "LoadLibraryA", kernel32_addr
    );
    if (!context->functions.LoadLibraryA) return FALSE;
    
    context->functions.GetProcAddress = (GET_PROC_ADDRESS)get_function_from_module(
        "GetProcAddress", kernel32_addr
    );
    if (!context->functions.GetProcAddress) return FALSE;
    
    // GetProcAddress for everything else.
    RESOLVE_OR_FAIL(context, kernel32_addr, VirtualAlloc, VIRTUAL_ALLOC);
    RESOLVE_OR_FAIL(context, kernel32_addr, VirtualFree, VIRTUAL_FREE);
    RESOLVE_OR_FAIL(context, kernel32_addr, GetProcessHeap, GET_PROCESS_HEAP);
    RESOLVE_OR_FAIL(context, kernel32_addr, Sleep, SLEEP);
    RESOLVE_OR_FAIL(context, kernel32_addr, GetTickCount, GET_TICK_COUNT);
    RESOLVE_OR_FAIL(context, kernel32_addr, ExitProcess, EXIT_PROCESS);
    RESOLVE_OR_FAIL(context, kernel32_addr, FreeLibrary, FREE_LIBRARY);
    RESOLVE_OR_FAIL(context, kernel32_addr, OutputDebugStringA, OUTPUT_DEBUG_STRING_A);
    RESOLVE_OR_FAIL(context, kernel32_addr, OutputDebugStringW, OUTPUT_DEBUG_STRING_W);
    RESOLVE_OR_FAIL(context, kernel32_addr, GetLastError, GET_LAST_ERROR);
    
    return TRUE;
}

BOOL init_functions_from_ntdll(PCTX context) {
    HMODULE ntdll_addr = (HMODULE)context->modules.ntdll;

    RESOLVE_OR_FAIL(context, ntdll_addr, RtlAllocateHeapX, RTL_HEAP_ALLOCX);
    RESOLVE_OR_FAIL(context, ntdll_addr, RtlReAllocateHeap, RTL_HEAP_REALLOC);
    RESOLVE_OR_FAIL(context, ntdll_addr, RtlFreeHeap, RTL_HEAP_FREE);
    RESOLVE_OR_FAIL(context, ntdll_addr, RtlRandomEx, RTL_RANDOM_EX);
    
    return TRUE;
}

BOOL init_function_table(PCTX context) {
    if (!init_functions_from_kernel32(context)) return FALSE;
    if (!init_functions_from_ntdll(context)) return FALSE;

    context->modules.user32 = context->functions.LoadLibraryA("user32.dll");
    if (!init_functions_from_user32(context)) return FALSE;

    context->modules.crypt32 = context->functions.LoadLibraryA("crypt32.dll");
    if (!init_functions_from_crypt32(context)) return FALSE;

    context->modules.advapi32 = context->functions.LoadLibraryA("advapi32.dll");
    if (!init_functions_from_advapi32(context)) return FALSE;

    context->modules.wininet = context->functions.LoadLibraryA("wininet.dll");
    if (!init_functions_from_wininet(context)) return FALSE;

    return TRUE;
}

BOOL init_kernel_ntdll_module(PCTX context) {
    context->modules.kernel32 = get_module_addr(L"kernel32.dll");
    context->modules.ntdll    = get_module_addr(L"ntdll.dll");

    return context->modules.kernel32 && context->modules.ntdll;
}

BOOL init_context(PCTX context) {
    if (!context) return FALSE;

    if (!init_kernel_ntdll_module(context)) {
        return FALSE;
    }

    if (!init_function_table(context)) {
        return FALSE;
    }

    context->process_heap = context->functions.GetProcessHeap();

    return TRUE;
}