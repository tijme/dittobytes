#ifndef CONTEXT_H
#define CONTEXT_H

#include <windows.h>
#include <wininet.h>
#include <winternl.h>

// kernel32.dll
typedef HMODULE(WINAPI *LOAD_LIBRARY_A)(LPCSTR);
typedef FARPROC(WINAPI *GET_PROC_ADDRESS)(HMODULE, LPCSTR);
typedef LPVOID(WINAPI *VIRTUAL_ALLOC)(LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL(WINAPI *VIRTUAL_FREE)(LPVOID, SIZE_T, DWORD);
typedef int (WINAPI *MESSAGE_BOX_W)(HWND, LPCWSTR, LPCWSTR, UINT);
typedef int (WINAPI *MESSAGE_BOX_A)(HWND, LPCSTR, LPCSTR, UINT);
typedef HANDLE(WINAPI *GET_PROCESS_HEAP)(VOID);
typedef int (WINAPI *WSPRINTF_A)(LPSTR, LPCSTR, ...);
typedef int (WINAPI *WSPRINTF_W)(LPWSTR, LPCWSTR, ...);
typedef VOID (WINAPI *OUTPUT_DEBUG_STRING_A)(LPCSTR);
typedef VOID (WINAPI *OUTPUT_DEBUG_STRING_W)(LPCWSTR);
typedef VOID (WINAPI *SLEEP)(DWORD);
typedef DWORD (WINAPI *GET_TICK_COUNT)(VOID);
typedef VOID (WINAPI *EXIT_PROCESS)(UINT);
typedef BOOL (WINAPI *FREE_LIBRARY)(HMODULE);
typedef DWORD (WINAPI *GET_LAST_ERROR)(VOID);

// ntdll.dll
typedef LPVOID(NTAPI *RTL_HEAP_ALLOCX)(HANDLE, DWORD, SIZE_T);
typedef LPVOID(NTAPI *RTL_HEAP_REALLOC)(HANDLE, DWORD, LPVOID, SIZE_T);
typedef BOOL(NTAPI *RTL_HEAP_FREE)(HANDLE, DWORD, LPVOID);
typedef ULONG(NTAPI *RTL_RANDOM_EX)(PULONG);

// crypt32.dll
typedef BOOL(WINAPI *CRYPT_BINARY_TO_STRING_A)(CONST PBYTE, DWORD, DWORD, LPSTR, PDWORD);
typedef BOOL(WINAPI *CRYPT_STRING_TO_BINARY_A)(LPCSTR, DWORD, DWORD, PBYTE, PDWORD, PDWORD, PDWORD);

// advapi32.dll
typedef BOOL(WINAPI *GET_USER_NAME_W)(LPWSTR, LPDWORD);

// wininet.dll
typedef HINTERNET(WINAPI *INTERNET_OPEN_W)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef HINTERNET(WINAPI *INTERNET_CONNECT_W)(HINTERNET, LPCWSTR, INTERNET_PORT, LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD_PTR);
typedef HINTERNET(WINAPI *HTTP_OPEN_REQUEST_W)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD, DWORD_PTR);
typedef BOOL(WINAPI* INTERNET_CLOSE_HANDLE)(HINTERNET);
typedef BOOL(WINAPI *HTTP_SEND_REQUEST_W)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD);
typedef BOOL(WINAPI *INTERNET_READ_FILE)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI *HTTP_QUERY_INFO_W)(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);

typedef struct _MODULE_TABLE {
    PVOID kernel32;
    PVOID ntdll;
    PVOID user32;
    PVOID crypt32;
    PVOID advapi32;
    PVOID wininet;
} MODULE_TABLE, *PMODULE_TABLE;

typedef struct _FUNCTION_TABLE {
	// kernel32.dll
    LOAD_LIBRARY_A LoadLibraryA;
    GET_PROC_ADDRESS GetProcAddress;
	VIRTUAL_ALLOC VirtualAlloc;
	VIRTUAL_FREE VirtualFree;
	GET_PROCESS_HEAP GetProcessHeap;
	SLEEP Sleep;
	GET_TICK_COUNT GetTickCount;
	EXIT_PROCESS ExitProcess;
	FREE_LIBRARY FreeLibrary;
	OUTPUT_DEBUG_STRING_A OutputDebugStringA;
	OUTPUT_DEBUG_STRING_W OutputDebugStringW;
	GET_LAST_ERROR GetLastError;

	// user32.dll
	MESSAGE_BOX_W MessageBoxW;
	MESSAGE_BOX_A MessageBoxA;
	WSPRINTF_A wsprintfA;
	WSPRINTF_W wsprintfW;

	// ntdll.dll
	RTL_HEAP_ALLOCX RtlAllocateHeapX;
	RTL_HEAP_REALLOC RtlReAllocateHeap;
	RTL_HEAP_FREE RtlFreeHeap;
	RTL_RANDOM_EX RtlRandomEx;

	// crypt32.dll
	CRYPT_BINARY_TO_STRING_A CryptBinaryToStringA;
	CRYPT_STRING_TO_BINARY_A CryptStringToBinaryA;

	// advapi32.dll
	GET_USER_NAME_W GetUserNameW;

	// wininet.dll
	INTERNET_OPEN_W InternetOpenW;
	INTERNET_CONNECT_W InternetConnectW;
	HTTP_OPEN_REQUEST_W HttpOpenRequestW;
	INTERNET_CLOSE_HANDLE InternetCloseHandle;
	HTTP_SEND_REQUEST_W HttpSendRequestW;
	INTERNET_READ_FILE InternetReadFile;
	HTTP_QUERY_INFO_W HttpQueryInfoW;
} FUNCTION_TABLE;

typedef struct _CTX {
    MODULE_TABLE modules;
    FUNCTION_TABLE functions;

	// Various variables.
	HANDLE process_heap;

    // Error tracking (set by RESOLVE_OR_FAIL on failure).
    DWORD  last_error;
    LPCSTR failed_func;
} CTX, *PCTX;

typedef struct C_LDR_DATA_TABLE_ENTRY {
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID      DllBase;
	PVOID      EntryPoint;
	ULONG      SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG      Flags;
	USHORT     LoadCount;
	USHORT     TlsIndex;
	LIST_ENTRY HashLinks;
	PVOID      SectionPointer;
	ULONG      CheckSum;
	ULONG      TimeDateStamp;
	PVOID      LoadedImports;
	PVOID      EntryPointActivationContext;
	PVOID      PatchInformation;
} C_LDR_DATA_TABLE_ENTRY, * C_PLDR_DATA_TABLE_ENTRY;

#define RESOLVE_OR_FAIL(ctx, module, func_name, func_type)                          \
    do {                                                                             \
        ctx->functions.func_name =                                                   \
            (func_type)ctx->functions.GetProcAddress(module, #func_name);            \
        if (!ctx->functions.func_name) {                                             \
            ctx->last_error  = ctx->functions.GetLastError();                        \
            ctx->failed_func = #func_name;                                           \
            if (ctx->functions.OutputDebugStringA)                                   \
                ctx->functions.OutputDebugStringA("RESOLVE_OR_FAIL: " #func_name);  \
            return FALSE;                                                            \
        }                                                                            \
    } while(0)

#define M_VIRTUAL_ALLOC(context, size) \
    (context)->functions.virtual_alloc(NULL, size, MEM_COMMIT, PAGE_READWRITE)

#define M_VIRTUAL_FREE(context, ptr) \
    (context)->functions.virtual_free(ptr, 0, MEM_RELEASE)

_Ret_notnull_
PIMAGE_NT_HEADERS get_nt_headers(_In_ PVOID dllAddr);

_Ret_notnull_
PIMAGE_OPTIONAL_HEADER get_optional_header(_In_ PVOID dllAddr);

_Ret_notnull_
PIMAGE_DATA_DIRECTORY get_data_directory(_In_ PVOID dllAddr, _In_ SIZE_T directoryEntry);

_Ret_notnull_
PIMAGE_EXPORT_DIRECTORY get_export_directory(_In_ PVOID dllAddr);

_Ret_maybenull_
PPEB get_peb(VOID);

_Ret_maybenull_
PVOID get_module_addr(_In_z_ PWCHAR module_name);

_Ret_maybenull_
PVOID get_function_from_module(_In_z_ PCHAR function_name, _In_ PVOID module_addr);

VOID cleanup_context(_In_ PCTX context);

_Must_inspect_result_ BOOL init_functions_from_wininet(_Inout_ PCTX context);
_Must_inspect_result_ BOOL init_functions_from_crypt32(_Inout_ PCTX context);
_Must_inspect_result_ BOOL init_functions_from_advapi32(_Inout_ PCTX context);
_Must_inspect_result_ BOOL init_functions_from_user32(_Inout_ PCTX context);

_Must_inspect_result_ BOOL init_functions_from_kernel32(_Inout_ PCTX context);
_Must_inspect_result_ BOOL init_functions_from_ntdll(_Inout_ PCTX context);

_Must_inspect_result_ BOOL init_function_table(_Inout_ PCTX context);
_Must_inspect_result_ BOOL init_kernel_ntdll_module(_Inout_ PCTX context);

_Must_inspect_result_ BOOL init_context(_Out_ PCTX context);

#endif // CONTEXT_H