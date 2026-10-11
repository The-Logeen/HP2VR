#include <windows.h>
#include <shlobj.h>
#include <cassert>
#include <cstring>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    const wchar_t* expected = L"C:\\HP2VR-Isolation-Test";
    SetEnvironmentVariableW(L"HP2VR_HOME", expected);
    HMODULE module = LoadLibraryW(argv[1]);
    assert(module);
    using W = HRESULT (WINAPI*)(HWND, int, HANDLE, DWORD, LPWSTR);
    using A = HRESULT (WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR);
    auto wide = reinterpret_cast<W>(GetProcAddress(module, "SHGetFolderPathW"));
    auto ansi = reinterpret_cast<A>(GetProcAddress(module, "SHGetFolderPathA"));
    assert(wide && ansi);
    wchar_t widePath[MAX_PATH]{};
    char ansiPath[MAX_PATH]{};
    assert(SUCCEEDED(wide(nullptr, CSIDL_PERSONAL, nullptr, 0, widePath)));
    assert(SUCCEEDED(ansi(nullptr, CSIDL_PERSONAL, nullptr, 0, ansiPath)));
    assert(wcscmp(widePath, expected) == 0);
    assert(std::strcmp(ansiPath, "C:\\HP2VR-Isolation-Test") == 0);
    FreeLibrary(module);
    SetEnvironmentVariableW(L"HP2VR_HOME", nullptr);
    std::cout << "HP2HomeProxyTests passed\n";
}

