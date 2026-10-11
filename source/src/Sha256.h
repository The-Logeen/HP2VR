#pragma once

#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <string>

namespace HP2Hash {

inline std::wstring sha256File(const wchar_t* path) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::wstring result;
    if (CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        std::array<BYTE, 64 * 1024> buffer{};
        DWORD read = 0;
        bool ok = true;
        while (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read) {
            if (!CryptHashData(hash, buffer.data(), read, 0)) { ok = false; break; }
        }
        if (GetLastError() != ERROR_SUCCESS && GetLastError() != ERROR_HANDLE_EOF) ok = false;
        std::array<BYTE, 32> digest{};
        DWORD bytes = static_cast<DWORD>(digest.size());
        if (ok && CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &bytes, 0) && bytes == digest.size()) {
            static constexpr wchar_t hex[] = L"0123456789ABCDEF";
            result.reserve(64);
            for (BYTE value : digest) { result.push_back(hex[value >> 4]); result.push_back(hex[value & 15]); }
        }
    }
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return result;
}

} // namespace HP2Hash

