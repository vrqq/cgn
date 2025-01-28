#ifdef _WIN32
#include <iostream>
#include <vector>
#include <cstdint>

#include <windows.h> 
#include <dbghelp.h> 
#pragma comment(lib, "dbghelp.lib")
 
void PrintStackTrace(CONTEXT* context) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    // 初始化调试帮助库
    SymInitialize(process, nullptr, TRUE);

    // 设置堆栈帧
    STACKFRAME64 stackFrame = {};
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

#if defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_X64)
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

    while (StackWalk64(machineType, process, thread, &stackFrame, context, nullptr,
                       SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
        DWORD64 address = stackFrame.AddrPC.Offset;

        if (address == 0) {
            break;
        }

        // 获取符号信息
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {};
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol)) {
            std::cout << "Function: " << symbol->Name << " - Address: 0x" << std::hex << symbol->Address << std::endl;
        } else {
            std::cout << "Address: 0x" << std::hex << address << " - Symbol not found" << std::endl;
        }
    }

    SymCleanup(process);
}

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    std::cerr << "\n"
                 "=============================\n"
                 "Unhandled exception occurred!" << std::endl;
    PrintStackTrace(exceptionInfo->ContextRecord);
    return EXCEPTION_EXECUTE_HANDLER;
}

void init_win_exception_handler() {
    SetUnhandledExceptionFilter(UnhandledExceptionHandler); 
}
#else

void init_win_exception_handler() {}
#endif