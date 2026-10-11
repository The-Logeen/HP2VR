// Exercise the real companion implementation without starting SteamVR or HP2.
#define wmain HP2CompanionEntryForTest
#include "../src/HP2VRCompanion.cpp"
#undef wmain
#include <cassert>

int main() {
    SetProcessDPIAware();
    PanelInteraction geometry;
    HP2VR::InputPayload tracked{};
    tracked.head.valid = 1;
    tracked.head.m[0] = tracked.head.m[5] = tracked.head.m[10] = 1;
    tracked.head.m[7] = 1.7f;
    HP2VR::FramePayload menu{};
    menu.presentation = HP2VR::Presentation::StationaryPanel;
    for (const auto size : {std::pair{640u, 480u}, std::pair{480u, 570u}, std::pair{1920u, 600u}}) {
        menu.width = size.first; menu.height = size.second;
        geometry.update(tracked, &menu);
        assert(geometry.panel.halfWidth <= 0.801f && geometry.panel.halfHeight <= 0.601f);
        assert(std::fabs(geometry.panel.halfWidth / geometry.panel.halfHeight - float(menu.width) / menu.height) < 0.001f);
        assert(std::fabs(geometry.panel.center[2] + 1.8f) < 0.001f);
    }
    puts("PASS: portrait/wide menu fit, preserved aspect and stationary anchor");
    CompositorDevice device;
    assert(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, nullptr, 0, D3D11_SDK_VERSION, &device.device, nullptr, &device.context)));
    StablePresentation presentation;
    ID3D11Texture2D* original[2]{};
    for (const auto dimensions : {std::pair{720u, 855u}, std::pair{1280u, 960u}, std::pair{640u, 480u}}) {
        HP2NativeFrontend frontend;
        frontend.width = dimensions.first; frontend.height = dimensions.second;
        frontend.pixels.resize(size_t(frontend.width) * frontend.height * 4);
        for (size_t i = 0; i < frontend.pixels.size(); i += 4) {
            frontend.pixels[i] = 30; frontend.pixels[i + 1] = 70;
            frontend.pixels[i + 2] = 190; frontend.pixels[i + 3] = 255;
        }
        PanelInteraction interaction;
        HP2VR::FramePayload frame{};
        presentation.prepareNative(frontend, device, interaction, frame);
        for (int eye = 0; eye < 2; ++eye) {
            assert(presentation.output[eye]);
            if (original[eye]) assert(presentation.output[eye] == original[eye]);
            else original[eye] = presentation.output[eye];
        }
        D3D11_TEXTURE2D_DESC desc{};
        presentation.output[1]->GetDesc(&desc);
        assert(desc.Width == EyeCapacity && desc.Height == EyeCapacity && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
        desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.BindFlags = desc.MiscFlags = 0;
        ID3D11Texture2D* readback{};
        assert(SUCCEEDED(device.device->CreateTexture2D(&desc, nullptr, &readback)));
        device.context->CopyResource(readback, presentation.output[1]);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        assert(SUCCEEDED(device.context->Map(readback, 0, D3D11_MAP_READ, 0, &mapped)));
        const auto* bytes = static_cast<const unsigned char*>(mapped.pData);
        const auto* last = bytes + (frontend.height - 1) * mapped.RowPitch + (frontend.width - 1) * 4;
        assert(bytes[0] == 190 && bytes[1] == 70 && bytes[2] == 30 && bytes[3] == 255);
        assert(last[0] == 190 && last[1] == 70 && last[2] == 30 && last[3] == 255);
        device.context->Unmap(readback, 0); readback->Release();
    }
    presentation.shutdown(); device.shutdown();
    puts("PASS: persistent eye identity across menu/world sizes, RGBA conversion, bounded GPU uploads");

    HWND root = CreateWindowExW(0, L"STATIC", L"HP2VR test fixture", WS_POPUP | WS_VISIBLE,
        -30000, 0, 400, 300, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(root);
    HWND nested = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        40, 30, 300, 220, root, nullptr, nullptr, nullptr);
    HWND button = CreateWindowExW(0, L"BUTTON", L"Fixture selection", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 20, 180, 50, nested, nullptr, nullptr, nullptr);
    assert(nested && button);
    HP2NativeFrontend frontend;
    frontend.window = root; frontend.width = 400; frontend.height = 300;
    HP2VR::InputPayload input{};
    input.focusOwned = input.inputAvailable = input.pointerValid = 1;
    input.pointerUv[0] = 100.f / 399; input.pointerUv[1] = 75.f / 299;
    auto pump = [] { MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } };
    input.trigger[1] = 1; frontend.input(input); pump();
    assert(SendMessageW(button, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    input.trigger[1] = 0; frontend.input(input); pump();
    assert(SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED);
    frontend.input(input); pump();
    assert(SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED);
    input.trigger[1] = 1; input.pointerValid = 0; frontend.input(input);
    input.pointerValid = 1; frontend.input(input);
    input.trigger[1] = 0; frontend.input(input); pump();
    assert(SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED);
    input.trigger[1] = 1; frontend.input(input);
    input.focusOwned = 0; input.trigger[1] = 0; frontend.input(input); pump();
    assert(SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED);
    DestroyWindow(root);
    puts("PASS: nested native controls, trigger release click, no repeat, pointer reentry and focus-loss cancellation");
    if (logFile) { fclose(logFile); logFile = nullptr; }
}
