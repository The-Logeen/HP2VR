#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "HP2BuildIdentity.h"
#include "Sha256.h"

namespace fs = std::filesystem;

static bool isX86Pe(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    stream.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!stream || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) return false;
    stream.seekg(dos.e_lfanew);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    stream.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    return stream && signature == IMAGE_NT_SIGNATURE && header.Machine == IMAGE_FILE_MACHINE_I386;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::wcerr << L"Usage: HP2VRInspector.exe <HP2 game root>\n";
        return 1;
    }
    std::error_code error;
    const fs::path root = fs::weakly_canonical(argv[1], error);
    if (error || !fs::is_directory(root / L"System")) {
        std::wcerr << L"Not an HP2 game root: " << argv[1] << L"\n";
        return 2;
    }

    bool supported = true;
    std::wcout << L"HP2 VR read-only identity check\nRoot: " << root.c_str() << L"\n";
    for (const auto& expected : HP2BuildIdentity::RequiredFiles) {
        const fs::path path = root / expected.relativePath;
        const std::wstring actual = HP2Hash::sha256File(path.c_str());
        const bool match = HP2BuildIdentity::equalsInsensitive(actual, expected.sha256);
        std::wcout << (match ? L"MATCH " : L"MISMATCH ") << expected.relativePath << L" " << actual << L"\n";
        supported &= match;
    }

    const fs::path game = root / L"System" / L"Game.exe";
    const bool x86 = isX86Pe(game);
    std::wcout << L"PE machine: " << (x86 ? L"x86" : L"unsupported/unknown") << L"\n";
    supported &= x86;

    const fs::path renderer = root / HP2BuildIdentity::SupportedRenderer.relativePath;
    const std::wstring rendererHash = HP2Hash::sha256File(renderer.c_str());
    const bool rendererMatch = HP2BuildIdentity::equalsInsensitive(rendererHash, HP2BuildIdentity::SupportedRenderer.sha256);
    std::wcout << (rendererMatch ? L"MATCH " : L"NOTE unknown ")
               << HP2BuildIdentity::SupportedRenderer.relativePath << L" " << rendererHash << L"\n";
    std::wcout << (supported ? L"SUPPORTED CORE BUILD\n" : L"UNSUPPORTED CORE BUILD; no native hooks may be installed\n");
    return supported ? 0 : 3;
}

