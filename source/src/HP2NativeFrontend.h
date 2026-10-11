#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "HP2VRBridge.h"

// HP2's startup dialogs exist before UGameEngine. Keep their pixels and input
// inside the companion, and accept windows only from this private Game.exe.
struct HP2NativeFrontend {
    std::wstring executable;
    HWND window{}, pressedWindow{};
    bool triggerHeld{};
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    void* bits{};
    unsigned width{}, height{};
    DWORD lastCapture{};
    DWORD lastDump{};
    std::vector<unsigned char> pixels;

    HP2NativeFrontend() {
        wchar_t root[32768]{};
        if (GetEnvironmentVariableW(L"HP2VR_RUNTIME", root, 32768))
            executable = std::wstring(root) + L"\\System\\Game.exe";
    }
    ~HP2NativeFrontend() { release(); }
    void release() {
        if (dc && previous) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        dc = nullptr; bitmap = nullptr; previous = nullptr; bits = nullptr;
    }
    bool owned(HWND candidate) const {
        if (executable.empty()) return false;
        DWORD pid{}; GetWindowThreadProcessId(candidate, &pid);
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return false;
        wchar_t path[32768]{}; DWORD length = 32768;
        const bool match = QueryFullProcessImageNameW(process, 0, path, &length) &&
            _wcsicmp(path, executable.c_str()) == 0;
        CloseHandle(process);
        return match;
    }
    static BOOL CALLBACK find(HWND candidate, LPARAM data) {
        auto& self = *reinterpret_cast<HP2NativeFrontend*>(data);
        if (!IsWindowVisible(candidate) || !IsWindowEnabled(candidate)) return TRUE;
        wchar_t type[64]{}; GetClassNameW(candidate, type, 64);
        if (wcscmp(type, L"#32770") || !self.owned(candidate)) return TRUE;
        RECT rect{}; GetClientRect(candidate, &rect);
        if (rect.right <= 0 || rect.bottom <= 0) return TRUE;
        self.window = candidate;
        return FALSE;
    }
    bool refresh() {
        HWND old = window;
        window = nullptr;
        EnumWindows(find, reinterpret_cast<LPARAM>(this));
        if (window != old) { pressedWindow = nullptr; lastCapture = 0; }
        return window != nullptr;
    }
    bool capture() {
        if (!window) return false;
        RECT rect{}; GetClientRect(window, &rect);
        const unsigned w = rect.right, h = rect.bottom;
        if (!w || !h || w > 4096 || h > 4096) return false;
        if (dc && w == width && h == height && lastCapture &&
            GetTickCount() - lastCapture < 33) return true;
        if (!dc || w != width || h != height) {
            release();
            dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -static_cast<LONG>(h);
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!dc || !bitmap || !bits) { release(); return false; }
            previous = SelectObject(dc, bitmap);
            width = w; height = h; pixels.resize(size_t(w) * h * 4);
        }
        // HP2's owner-drawn dialogs do not implement WM_PRINT consistently.
        // Read their displayed client surface, using window-local coordinates.
        HDC source = GetDC(window);
        const BOOL copied = source && BitBlt(dc, 0, 0, w, h, source, 0, 0, SRCCOPY);
        if (source) ReleaseDC(window, source);
        if (!copied) return false;
        GdiFlush();
        const auto* bgra = static_cast<const unsigned char*>(bits);
        for (size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i] = bgra[i]; pixels[i + 1] = bgra[i + 1];
            pixels[i + 2] = bgra[i + 2]; pixels[i + 3] = 255;
        }
        lastCapture = GetTickCount();
        if (GetEnvironmentVariableW(L"HP2VR_PANEL_DUMP", nullptr, 0) &&
            (!lastDump || lastCapture - lastDump > 1000)) {
            FILE* file{};
            if (fopen_s(&file, "HP2NativeFrontend.bmp", "wb") == 0) {
                BITMAPFILEHEADER header{};
                BITMAPINFOHEADER info{};
                info.biSize = sizeof(info); info.biWidth = width; info.biHeight = -static_cast<LONG>(height);
                info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
                header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info);
                header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
                fwrite(&header, sizeof(header), 1, file); fwrite(&info, sizeof(info), 1, file);
                fwrite(bits, 1, pixels.size(), file); fclose(file);
                lastDump = lastCapture;
            }
        }
        return true;
    }
    void input(const HP2VR::InputPayload& input) {
        const bool down = input.trigger[1] > 0.65f;
        if (!window || !input.focusOwned || !input.inputAvailable || !input.pointerValid) {
            pressedWindow = nullptr; triggerHeld = down; return;
        }
        POINT point{static_cast<LONG>(std::clamp(input.pointerUv[0], 0.f, 1.f) * (width - 1)),
                    static_cast<LONG>(std::clamp(input.pointerUv[1], 0.f, 1.f) * (height - 1))};
        HWND target = window;
        for (int depth = 0; depth < 16; ++depth) {
            HWND child = ChildWindowFromPointEx(target, point, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
            if (!child || child == target) break;
            MapWindowPoints(target, child, &point, 1); target = child;
        }
        const LPARAM position = MAKELPARAM(point.x, point.y);
        PostMessageW(target, WM_MOUSEMOVE, down ? MK_LBUTTON : 0, position);
        if (down && !triggerHeld) pressedWindow = target;
        if (!down && triggerHeld && pressedWindow == target && IsWindowEnabled(target)) {
            wchar_t type[64]{}; GetClassNameW(target, type, 64);
            if (_wcsicmp(type, L"Button") == 0) PostMessageW(target, BM_CLICK, 0, 0);
            else {
                PostMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, position);
                PostMessageW(target, WM_LBUTTONUP, 0, position);
            }
        }
        if (!down) pressedWindow = nullptr;
        triggerHeld = down;
    }
};
