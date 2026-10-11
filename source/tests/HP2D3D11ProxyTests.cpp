#include <windows.h>
#include <cassert>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    HMODULE module = LoadLibraryW(argv[1]);
    assert(module != nullptr);
    assert(GetProcAddress(module, "D3D11CreateDevice") != nullptr);
    assert(GetProcAddress(module, "HP2VR_D3D11CaptureEye") != nullptr);
    assert(GetProcAddress(module, "HP2VR_D3D11CaptureSequence") != nullptr);
    assert(GetProcAddress(module, "HP2VR_D3D11ConsumerReady") != nullptr);
    assert(GetProcAddress(module, "HP2VR_D3D11ReadPixels") != nullptr);
    assert(GetProcAddress(module, "HP2VR_D3D11UploadPanel") != nullptr);
    assert(GetProcAddress(module, "HPVR_D3D11CaptureEye") == nullptr);
    FreeLibrary(module);
    std::cout << "HP2D3D11ProxyTests passed\n";
}
