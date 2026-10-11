#include "HP2BuildIdentity.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace HP2BuildIdentity;
    assert(RequiredFiles.size() >= 8);
    assert(VersionLine == L"HPCos_021009_1205-1");
    for (const auto& file : RequiredFiles) {
        assert(file.relativePath.starts_with(L"System\\"));
        assert(file.sha256.size() == 64);
    }
    assert(equalsInsensitive(L"abcdef", L"ABCDEF"));
    assert(!equalsInsensitive(L"HP1", L"HP2"));
    std::cout << "HP2BuildIdentityTests passed\n";
}

