#include "HP2ProcessSession.h"
#include <cassert>
#include <string>
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    if (argc > 1 && std::wstring(argv[1]) == L"leaf") { Sleep(1500); return 0; }
    wchar_t path[32768]{};
    assert(GetModuleFileNameW(nullptr, path, 32768));
    const bool frontend = argc > 1;
    std::wstring command = L"\"" + std::wstring(path) + L"\" " + (frontend ? L"leaf" : L"frontend");
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    assert(CreateProcessW(path, command.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi));
    if (frontend) {
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return 0;
    }
    HP2ProcessSession session;
    assert(session.attach(pi.hProcess));
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    assert(WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0);
    assert(session.wait(0) == WAIT_TIMEOUT); // Menu exited; gameplay still alive.
    assert(!session.contains(GetCurrentProcessId()));
    HANDLE quit = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    assert(quit);
    assert(session.wait(5000, quit) == WAIT_OBJECT_0 + 1);
    CloseHandle(quit);
    assert(session.wait(5000) == WAIT_OBJECT_0);
    CloseHandle(pi.hProcess);
    puts("PASS: frontend handoff retained; quit wakes session; unrelated parent excluded; full session exit detected");
}
