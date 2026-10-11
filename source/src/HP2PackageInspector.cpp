#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class Reader {
public:
    explicit Reader(const fs::path& path) : input(path, std::ios::binary) {
        if (!input) throw std::runtime_error("could not open package");
    }
    template<class T> T fixed() {
        T value{};
        input.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (!input) throw std::runtime_error("unexpected end of package");
        return value;
    }
    void seek(uint32_t offset) {
        input.seekg(offset, std::ios::beg);
        if (!input) throw std::runtime_error("invalid package offset");
    }
    int32_t compact() {
        const uint8_t first = fixed<uint8_t>();
        uint32_t value = first & 0x3fu;
        if (first & 0x40u) {
            uint8_t next = fixed<uint8_t>();
            value |= uint32_t(next & 0x7fu) << 6;
            if (next & 0x80u) {
                next = fixed<uint8_t>();
                value |= uint32_t(next & 0x7fu) << 13;
                if (next & 0x80u) {
                    next = fixed<uint8_t>();
                    value |= uint32_t(next & 0x7fu) << 20;
                    if (next & 0x80u) {
                        next = fixed<uint8_t>();
                        value |= uint32_t(next & 0x1fu) << 27;
                    }
                }
            }
        }
        const int32_t signedValue = static_cast<int32_t>(value);
        return (first & 0x80u) ? -signedValue : signedValue;
    }
    std::string ansi(size_t bytes) {
        std::string value(bytes, '\0');
        input.read(value.data(), static_cast<std::streamsize>(bytes));
        if (!input) throw std::runtime_error("truncated package string");
        if (!value.empty() && value.back() == '\0') value.pop_back();
        return value;
    }
    std::string wide(size_t units) {
        std::string value;
        value.reserve(units);
        for (size_t i = 0; i < units; ++i) {
            const uint16_t ch = fixed<uint16_t>();
            if (!ch) continue;
            value.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
        }
        return value;
    }
private:
    std::ifstream input;
};

struct Name { std::string text; uint32_t flags{}; };
struct Import { int32_t classPackage{}, className{}, outer{}, objectName{}; };
struct Export {
    int32_t classIndex{}, superIndex{}, outer{}, objectName{};
    uint32_t flags{};
    int32_t serialSize{}, serialOffset{};
};

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: HP2PackageInspector.exe <package.u>\n";
            return 2;
        }
        Reader reader(argv[1]);
        const uint32_t tag = reader.fixed<uint32_t>();
        if (tag != 0x9e2a83c1u) throw std::runtime_error("not an Unreal package");
        const uint32_t combinedVersion = reader.fixed<uint32_t>();
        const uint16_t version = static_cast<uint16_t>(combinedVersion);
        const uint16_t licensee = static_cast<uint16_t>(combinedVersion >> 16);
        const uint32_t packageFlags = reader.fixed<uint32_t>();
        const uint32_t nameCount = reader.fixed<uint32_t>();
        const uint32_t nameOffset = reader.fixed<uint32_t>();
        const uint32_t exportCount = reader.fixed<uint32_t>();
        const uint32_t exportOffset = reader.fixed<uint32_t>();
        const uint32_t importCount = reader.fixed<uint32_t>();
        const uint32_t importOffset = reader.fixed<uint32_t>();
        if (nameCount > 1000000 || importCount > 1000000 || exportCount > 1000000)
            throw std::runtime_error("implausible package table counts");

        std::vector<Name> names;
        names.reserve(nameCount);
        reader.seek(nameOffset);
        for (uint32_t i = 0; i < nameCount; ++i) {
            const int32_t length = reader.compact();
            if (length == 0 || length > 1'000'000 || length < -1'000'000)
                throw std::runtime_error("invalid name length");
            Name name;
            name.text = length > 0 ? reader.ansi(static_cast<size_t>(length))
                                   : reader.wide(static_cast<size_t>(-length));
            name.flags = reader.fixed<uint32_t>();
            names.push_back(std::move(name));
        }
        auto nameOf = [&](int32_t index) -> const std::string& {
            static const std::string invalid = "<bad-name>";
            return index >= 0 && static_cast<size_t>(index) < names.size() ? names[index].text : invalid;
        };

        std::vector<Import> imports;
        imports.reserve(importCount);
        reader.seek(importOffset);
        for (uint32_t i = 0; i < importCount; ++i) {
            Import item;
            item.classPackage = reader.compact();
            item.className = reader.compact();
            item.outer = reader.fixed<int32_t>();
            item.objectName = reader.compact();
            imports.push_back(item);
        }

        std::vector<Export> exports;
        exports.reserve(exportCount);
        reader.seek(exportOffset);
        for (uint32_t i = 0; i < exportCount; ++i) {
            Export item;
            item.classIndex = reader.compact();
            item.superIndex = reader.compact();
            item.outer = reader.fixed<int32_t>();
            item.objectName = reader.compact();
            item.flags = reader.fixed<uint32_t>();
            item.serialSize = reader.compact();
            if (item.serialSize > 0) item.serialOffset = reader.compact();
            exports.push_back(item);
        }

        auto objectName = [&](int32_t objectIndex) -> std::string {
            if (objectIndex == 0) return "None";
            if (objectIndex < 0) {
                const size_t index = static_cast<size_t>(-objectIndex - 1);
                return index < imports.size() ? nameOf(imports[index].objectName) : "<bad-import>";
            }
            const size_t index = static_cast<size_t>(objectIndex - 1);
            return index < exports.size() ? nameOf(exports[index].objectName) : "<bad-export>";
        };

        std::cout << "PACKAGE version=" << version << " licensee=" << licensee
                  << " flags=0x" << std::hex << packageFlags << std::dec
                  << " names=" << nameCount << " imports=" << importCount
                  << " exports=" << exportCount << "\n";
        for (uint32_t i = 0; i < nameCount; ++i)
            std::cout << "NAME " << i << " " << names[i].text << " flags=0x"
                      << std::hex << names[i].flags << std::dec << "\n";
        for (uint32_t i = 0; i < importCount; ++i) {
            const auto& item = imports[i];
            std::cout << "IMPORT -" << (i + 1) << " class=" << nameOf(item.classPackage)
                      << "." << nameOf(item.className) << " outer=" << objectName(item.outer)
                      << " name=" << nameOf(item.objectName) << "\n";
        }
        for (uint32_t i = 0; i < exportCount; ++i) {
            const auto& item = exports[i];
            std::cout << "EXPORT " << (i + 1) << " class=" << objectName(item.classIndex)
                      << " super=" << objectName(item.superIndex) << " outer=" << objectName(item.outer)
                      << " name=" << nameOf(item.objectName) << " flags=0x" << std::hex << item.flags
                      << std::dec << " size=" << item.serialSize << " offset=" << item.serialOffset << "\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "HP2 package inspection failed: " << error.what() << "\n";
        return 1;
    }
}
