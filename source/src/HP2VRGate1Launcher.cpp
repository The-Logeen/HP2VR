#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "HP2BuildIdentity.h"
#include "Sha256.h"
#include "HP2VRBridge.h"
#include "HP2ProcessSession.h"

namespace fs = std::filesystem;

static bool supportedPrivateRuntime(const fs::path& root) {
    if (!fs::is_regular_file(root / L".hp2vr-private-runtime")) {
        std::wcerr << L"Refusing to run: .hp2vr-private-runtime is missing.\n";
        return false;
    }
    for (const auto& expected : HP2BuildIdentity::RequiredFiles) {
        const fs::path path = root / expected.relativePath;
        const std::wstring actual = HP2Hash::sha256File(path.c_str());
        if (!HP2BuildIdentity::equalsInsensitive(actual, expected.sha256)) {
            std::wcerr << L"Refusing to run: unsupported or changed file " << expected.relativePath << L"\n";
            return false;
        }
    }
    return true;
}

static bool hasIniLine(const fs::path& path, const wchar_t* expected) {
    std::wifstream input(path);
    if (!input) return false;
    std::wstring line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (_wcsicmp(line.c_str(), expected) == 0) return true;
    }
    return false;
}

struct CloseWindowsContext { DWORD processId{}; unsigned posted{}; std::wofstream* log{}; };

struct MainMenuContext { DWORD processId{}; HWND dialog{}; HWND newGame{}; };

static BOOL CALLBACK findMainMenu(HWND window, LPARAM value) {
    auto* context = reinterpret_cast<MainMenuContext*>(value);
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner != context->processId || !IsWindowVisible(window)) return TRUE;
    wchar_t title[64]{}, className[64]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    if (wcscmp(className, L"#32770") || wcscmp(title, L"Main Menu")) return TRUE;
    HWND button = GetDlgItem(window, 3021);
    wchar_t buttonTitle[64]{}, buttonClass[64]{};
    if (button) {
        GetWindowTextW(button, buttonTitle, static_cast<int>(std::size(buttonTitle)));
        GetClassNameW(button, buttonClass, static_cast<int>(std::size(buttonClass)));
    }
    if (button && !wcscmp(buttonClass, L"Button") && !wcscmp(buttonTitle, L"New Game") &&
        IsWindowVisible(button) && IsWindowEnabled(button)) {
        context->dialog = window;
        context->newGame = button;
        return FALSE;
    }
    return TRUE;
}

static bool startNewGame(DWORD processId, HANDLE process, std::wofstream& log) {
    // This is HP2's native launcher dialog, not an Unreal world.  Entering it
    // automatically makes the private verification path deterministic.
    for (unsigned attempt = 0; attempt < 300; ++attempt) {
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) return false;
        MainMenuContext context{processId};
        EnumWindows(findMainMenu, reinterpret_cast<LPARAM>(&context));
        if (context.newGame) {
            DWORD_PTR ignored = 0;
            const LRESULT sent = SendMessageTimeoutW(context.newGame, BM_CLICK, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &ignored);
            log << L"Native Main Menu New Game id=3021 click result=" << sent << L"\n";
            log.flush();
            return sent != 0;
        }
        Sleep(100);
    }
    log << L"Native Main Menu New Game button was not found within 30 seconds.\n";
    log.flush();
    return false;
}

static BOOL CALLBACK logChildWindow(HWND window, LPARAM value) {
    auto* log = reinterpret_cast<std::wofstream*>(value);
    if (!log) return TRUE;
    wchar_t title[512]{}, className[256]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    *log << L"  child id=" << GetDlgCtrlID(window) << L" class=" << className
         << L" title=" << title << L" visible=" << (IsWindowVisible(window) ? 1 : 0)
         << L" enabled=" << (IsWindowEnabled(window) ? 1 : 0) << L"\n";
    return TRUE;
}

static BOOL CALLBACK closeProcessWindow(HWND window, LPARAM value) {
    auto* context = reinterpret_cast<CloseWindowsContext*>(value);
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner == context->processId && GetWindow(window, GW_OWNER) == nullptr && IsWindowVisible(window)) {
        wchar_t title[512]{}, className[256]{};
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        GetClassNameW(window, className, static_cast<int>(std::size(className)));
        if (context->log) *context->log << L"Owned window class=" << className << L" title=" << title << L"\n";
        if (context->log) EnumChildWindows(window, logChildWindow, reinterpret_cast<LPARAM>(context->log));
        PostMessageW(window, WM_SYSCOMMAND, SC_CLOSE, 0);
        if (PostMessageW(window, WM_CLOSE, 0, 0)) ++context->posted;
    }
    return TRUE;
}

static unsigned postQuitToProcessThreads(DWORD processId) {
    unsigned posted = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 thread{}; thread.dwSize = sizeof(thread);
    if (Thread32First(snapshot, &thread)) {
        do {
            if (thread.th32OwnerProcessID == processId && PostThreadMessageW(thread.th32ThreadID, WM_QUIT, 0, 0)) ++posted;
        } while (Thread32Next(snapshot, &thread));
    }
    CloseHandle(snapshot);
    return posted;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::wcerr << L"Usage: HP2VRHost.exe <private-runtime-root> [UNREAL_URL]\n";
        return 1;
    }
    std::error_code error;
    const fs::path runtime = fs::weakly_canonical(argv[1], error);
    if (error || !supportedPrivateRuntime(runtime)) return 2;

    const fs::path game = runtime / L"System" / L"Game.exe";
    if (!fs::is_regular_file(game)) {
        std::wcerr << L"Game executable is missing: " << game.c_str() << L"\n";
        return 3;
    }
    const fs::path gameIni = runtime / L"System" / L"Game.ini";
    if (!hasIniLine(gameIni, L"FirstRun=433") ||
        !hasIniLine(gameIni, L"GameRenderDevice=D3D11Drv.D3D11RenderDevice")) {
        std::wcerr << L"Private Game.ini is not pinned to HP2 D3D11/FirstRun=433. "
                      L"Run tools\\Install-Gate1.ps1 after closing HP2.\n";
        return 3;
    }
    wchar_t launcherPath[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, launcherPath, MAX_PATH)) return 3;
    const fs::path builtProxy = fs::path(launcherPath).parent_path() / L"shfolder.dll";
    const fs::path installedProxy = runtime / L"System" / L"shfolder.dll";
    const std::wstring builtProxyHash = HP2Hash::sha256File(builtProxy.c_str());
    const std::wstring installedProxyHash = HP2Hash::sha256File(installedProxy.c_str());
    if (builtProxyHash.empty() || !HP2BuildIdentity::equalsInsensitive(builtProxyHash, installedProxyHash)) {
        std::wcerr << L"Private home redirection is not installed or is from another build. Run tools\\Install-Gate1.ps1.\n";
        return 3;
    }
    const fs::path home = runtime / L"Home";
    fs::create_directories(home / L"Harry Potter II", error);
    if (error || !SetEnvironmentVariableW(L"HP2VR_HOME", home.c_str())) {
        std::wcerr << L"Could not establish the private HP2 home folder.\n";
        return 3;
    }

    std::wofstream log(runtime / L"HP2VRGate1Launcher.log", std::ios::trunc);
    log << L"HP2VR Gate 1 launcher\nRuntime=" << runtime.c_str() << L"\n";
    std::wstring command = L"\"" + game.native() + L"\" ";
    command += argc == 3 ? argv[2] : L"Startup.unr -windowed INI=Game.ini USERINI=User.ini LOG=HP2VRGate1.log";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    HP2ProcessSession session;
    const fs::path system = runtime / L"System";
    if (!CreateProcessW(game.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                        nullptr, system.c_str(), &startup, &process)) {
        log << L"CreateProcess failed: " << GetLastError() << L"\n";
        std::wcerr << L"Could not start private HP2. Windows error " << GetLastError() << L".\n";
        return 4;
    }
    // Assign before any game code runs, otherwise a fast frontend could spawn
    // its gameplay child before we establish ownership.
    if (!session.attach(process.hProcess)) {
        log << L"Session assignment failed: " << GetLastError() << L"\n";
        TerminateProcess(process.hProcess, 4);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        return 4;
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
        TerminateJobObject(session.job, 4);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        return 4;
    }
    CloseHandle(process.hThread);
    log << L"Started Game.exe pid=" << process.dwProcessId << L"\n";

    wchar_t manualMenu[8]{};
    const bool keepMenu = GetEnvironmentVariableW(L"HP2VR_MANUAL_MENU", manualMenu,
        static_cast<DWORD>(std::size(manualMenu))) && _wtoi(manualMenu) != 0;
    const bool noFrontend = command.find(L"NOFRONTEND") != std::wstring::npos ||
        command.find(L"nofrontend") != std::wstring::npos;
    if (noFrontend) log << L"Explicit NOFRONTEND diagnostic requested.\n";
    else log << L"Native Main Menu left for the user; automatic New Game disabled.\n";

    DWORD runSeconds = 20;
    wchar_t untilExitValue[8]{};
    const bool runUntilExit = GetEnvironmentVariableW(L"HP2VR_RUN_UNTIL_EXIT", untilExitValue,
        static_cast<DWORD>(std::size(untilExitValue))) && _wtoi(untilExitValue) != 0;
    wchar_t duration[16]{};
    if (GetEnvironmentVariableW(L"HP2VR_RUN_SECONDS", duration, static_cast<DWORD>(std::size(duration)))) {
        const int parsed = _wtoi(duration);
        if (parsed >= 3 && parsed <= 300) runSeconds = static_cast<DWORD>(parsed);
    }
    if (runUntilExit) {
        log << L"Interactive VR launch; waiting for the entire HP2 process session, including frontend handoffs.\n";
        log.flush();
    }
    HANDLE shutdownRequest = OpenEventW(SYNCHRONIZE, FALSE, HP2VR::ShutdownEventName);
    const DWORD observed = session.wait(runUntilExit ? INFINITE : runSeconds * 1000, shutdownRequest);
    if (shutdownRequest) CloseHandle(shutdownRequest);
    if (observed == WAIT_FAILED) {
        log << L"Session query failed: " << GetLastError() << L"\n";
        CloseHandle(process.hProcess);
        return 5;
    }
    bool forced = false;
    if (observed == WAIT_TIMEOUT || observed == WAIT_OBJECT_0 + 1) {
        log << (observed == WAIT_TIMEOUT ? L"Diagnostic interval ended" : L"VR/game shutdown requested")
            << L"; closing every owned HP2 process window.\n";
        // Job membership includes gameplay descendants after the launcher menu
        // exits; never select targets by a generic Game.exe process name.
        EnumWindows([](HWND window, LPARAM value) -> BOOL {
            auto* owned = reinterpret_cast<HP2ProcessSession*>(value);
            DWORD pid{}; GetWindowThreadProcessId(window, &pid);
            if (owned->contains(pid) && GetWindow(window, GW_OWNER) == nullptr)
                PostMessageW(window, WM_CLOSE, 0, 0);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&session));
        if (session.wait(10000) == WAIT_TIMEOUT) {
        log << L"Graceful exit timed out; terminating only remaining owned HP2 descendants.\n";
        TerminateJobObject(session.job, 7);
        session.wait(5000);
        forced = true;
        }
    }
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    // Only release VR once the whole session ends, not when the menu process
    // exits to launch gameplay.
    if (HANDLE shutdown = OpenEventW(EVENT_MODIFY_STATE, FALSE, HP2VR::ShutdownEventName)) {
        SetEvent(shutdown);
        CloseHandle(shutdown);
    }
    log << L"Game.exe exit=" << exitCode << L" forced=" << (forced ? 1 : 0) << L"\n";
    log.flush();
    std::wcout << L"Private HP2 flat run exit=" << exitCode << (forced ? L" (forced cleanup)" : L"") << L"\n";
    return forced ? 7 : static_cast<int>(exitCode);
}

