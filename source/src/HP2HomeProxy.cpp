#include <windows.h>
#include <iterator>

namespace {
using FolderPathW = HRESULT (WINAPI*)(HWND, int, HANDLE, DWORD, LPWSTR);
using FolderPathA = HRESULT (WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR);
constexpr int PersonalFolder = 5; // CSIDL_PERSONAL
constexpr int FolderFlagMask = 0xFF00;

HMODULE realModule() {
    static HMODULE module = [] {
        wchar_t system[MAX_PATH]{};
        if (!GetSystemDirectoryW(system, MAX_PATH)) return static_cast<HMODULE>(nullptr);
        wcscat_s(system, L"\\shfolder.dll");
        return LoadLibraryW(system);
    }();
    return module;
}

bool redirected(int folder, wchar_t* destination) {
    if ((folder & ~FolderFlagMask) != PersonalFolder) return false;
    wchar_t home[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"HP2VR_HOME", home, static_cast<DWORD>(std::size(home)));
    if (!length || length >= std::size(home)) return false;
    wcscpy_s(destination, MAX_PATH, home);
    return true;
}
}

extern "C" __declspec(dllexport) HRESULT WINAPI SHGetFolderPathW(
    HWND owner, int folder, HANDLE token, DWORD flags, LPWSTR destination) {
    if (!destination) return E_INVALIDARG;
    if (redirected(folder, destination)) return S_OK;
    HMODULE module = realModule();
    auto original = module ? reinterpret_cast<FolderPathW>(GetProcAddress(module, "SHGetFolderPathW")) : nullptr;
    return original ? original(owner, folder, token, flags, destination) : E_FAIL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI SHGetFolderPathA(
    HWND owner, int folder, HANDLE token, DWORD flags, LPSTR destination) {
    if (!destination) return E_INVALIDARG;
    wchar_t wide[MAX_PATH]{};
    if (redirected(folder, wide)) {
        return WideCharToMultiByte(CP_ACP, 0, wide, -1, destination, MAX_PATH, nullptr, nullptr) ? S_OK : E_FAIL;
    }
    HMODULE module = realModule();
    auto original = module ? reinterpret_cast<FolderPathA>(GetProcAddress(module, "SHGetFolderPathA")) : nullptr;
    return original ? original(owner, folder, token, flags, destination) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }

