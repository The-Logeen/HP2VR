#pragma once

#include <array>
#include <string_view>

namespace HP2BuildIdentity {

inline constexpr std::wstring_view VersionLine = L"HPCos_021009_1205-1";

struct FileIdentity {
    std::wstring_view relativePath;
    std::wstring_view sha256;
};

// Observed in the selected HP2 installation on 2026-09-28. These values gate
// native work; an unknown build must be inspected, not silently accepted.
inline constexpr std::array<FileIdentity, 9> RequiredFiles{{
    {L"System\\Game.exe",      L"EBA64695B352B103611DC8E827E6BC44E8376E31D8CF3DA0B7E11D09EDC1A694"},
    {L"System\\Core.dll",      L"F6AD0A5FB348D868CB7499FDD727D3F6034ACF0D8BA3795AE78809893B4D5249"},
    {L"System\\Engine.dll",    L"EFA56C753FE306C2AD2744C82E20258DA39DF9CE51B3B415D476F8282AE1A189"},
    {L"System\\Render.dll",    L"EB71BC45DAD3B744B9155F37E1BA59119A31743FC74A7FB8CB4DAFA93797755A"},
    {L"System\\WinDrv.dll",    L"C886ECD1A0F17E25D0FA54E57F41485EF2AA61BA65231EB3C5ACED9C60AAD302"},
    {L"System\\hgame.u",       L"5DE5F3EFE11F1E882C93EFF82D6EA35380E3428D5B80B49FCC2D311B49809576"},
    {L"System\\Engine.u",      L"E6E8F3C3E6E346BD53008E89F1706D7A4719317A34DDFB90D73040ABCF5FC29C"},
    {L"System\\Core.u",        L"C480E9A6D7AFB8F300C581E79459E28D8611685B8327848898246B056D333D9B"},
    {L"System\\UWindow.u",     L"7E5878D0FE73BEFCB1BBCBC19784B7F115D4365B620614A334D420873A44D8BF"},
}};

inline constexpr FileIdentity SupportedRenderer{
    L"System\\d3d11drv.dll",
    L"75E7284CA6D5EF22F3FA8ABFD7C7031C703F0120ED6D9B06165DA298C712EF4E"
};

inline bool equalsInsensitive(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size()) return false;
    for (size_t i = 0; i < left.size(); ++i) {
        wchar_t a = left[i], b = right[i];
        if (a >= L'a' && a <= L'z') a -= L'a' - L'A';
        if (b >= L'a' && b <= L'z') b -= L'a' - L'A';
        if (a != b) return false;
    }
    return true;
}

} // namespace HP2BuildIdentity

