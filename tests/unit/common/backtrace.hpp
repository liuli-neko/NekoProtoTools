#pragma once

#include "cpptrace/basic.hpp"

#include <cpptrace/cpptrace.hpp>
#include <cstdlib>
#include <csignal>

#ifdef _MSC_VER
#include <windows.h>
#endif

namespace cpptrace {

inline void dump() { cpptrace::generate_trace().print(); }

void signal_handler(int sig) {
    // 1. 立即解除注册，防止后续代码再次触发同类型信号导致死循环
    static thread_local bool kIsHandlingCrash = false;
    if (kIsHandlingCrash) {
        signal(sig, SIG_DFL);
        raise(sig); // 让系统生成 core dump 或直接结束
        return;
    }
    kIsHandlingCrash = true;

    const char* name = "Unknown";
    switch (sig) {
    case SIGABRT:
        name = "SIGABRT";
        break;
    case SIGSEGV:
        name = "SIGSEGV";
        break;
    case SIGFPE:
        name = "SIGFPE";
        break;
    case SIGILL:
        name = "SIGILL";
        break;
    }

    fprintf(stderr, "Encountered a fatal error: %s (%d)\n", name, sig);

    // 打印堆栈
    dump();

    fflush(stderr);
    fflush(stdout);

    signal(sig, SIG_DFL);
    raise(sig);
}

#if defined(_MSC_VER)
// Windows SEH 处理函数
LONG WINAPI WindowsExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    // 防止递归崩溃
    static std::atomic_flag handling = ATOMIC_FLAG_INIT;
    if (handling.test_and_set()) {
        return EXCEPTION_EXECUTE_HANDLER; // 或者直接 TerminateProcess
    }

    fprintf(stderr, "Encountered a Windows Structured Exception (0x%x)\n",
            ExceptionInfo->ExceptionRecord->ExceptionCode);

    // cpptrace 在 Windows 上通常能自动处理上下文，或者你可以手动传递 context
    dump();

    // 返回 EXCEPTION_EXECUTE_HANDLER 表示“我已经处理了，程序可以退出了”
    // 通常会导致进程终止。
    // 如果你想让系统生成 Windows Error Reporting (WerFault)，这里返回值会有所不同。
    return EXCEPTION_EXECUTE_HANDLER;
}
#elif defined(__GNUC__)
#endif
inline void init() {
    signal(SIGABRT, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGILL, signal_handler);
#if defined(_MSC_VER)
    // 2. 注册 Windows 特有的未处理异常过滤器 (关键)
    // 这能捕获 Access Violation (0xC0000005) 等硬件异常
    SetUnhandledExceptionFilter(WindowsExceptionHandler);

    // 3. (可选) 防止 Windows 弹出 "Debug / Close program" 对话框，直接处理
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}
} // namespace cpptrace