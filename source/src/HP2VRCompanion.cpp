#include <windows.h>
#include "HP2AimAngles.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <openvr.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "HP2VRBridge.h"
#include "HP2ScreenPanel.h"
#include "HP2StereoProjection.h"
#include "HP2NativeFrontend.h"

namespace {
FILE* logFile{};
constexpr unsigned EyeCapacity = 2048;

void log(const char* format, ...) {
    if (!logFile) fopen_s(&logFile, "HP2VRCompanion.log", "w");
    if (!logFile) return;
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(logFile, format, arguments);
    va_end(arguments);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}

uint64_t qpc() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return static_cast<uint64_t>(value.QuadPart);
}

struct Mappings {
    HANDLE mainHandle{};
    HANDLE gpuHandle{};
    HANDLE shutdownEvent{};
    HP2VR::SharedState* main{};
    HP2VR::GpuSharedState* gpu{};

    bool initialize() {
        mainHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            sizeof(HP2VR::SharedState), HP2VR::MappingName);
        if (!mainHandle) return false;
        const bool freshMain = GetLastError() != ERROR_ALREADY_EXISTS;
        main = static_cast<HP2VR::SharedState*>(MapViewOfFile(mainHandle,
            FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HP2VR::SharedState)));
        if (!main) return false;
        if (freshMain || !HP2VR::compatible(*main)) {
            ZeroMemory(main, sizeof(*main));
            main->magic = HP2VR::BridgeMagic;
            main->version = HP2VR::BridgeVersion;
            main->bytes = sizeof(*main);
        }
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        main->qpcFrequency = static_cast<uint64_t>(frequency.QuadPart);
        main->companionPid = GetCurrentProcessId();

        gpuHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            sizeof(HP2VR::GpuSharedState), HP2VR::GpuMappingName);
        if (!gpuHandle) return false;
        const bool freshGpu = GetLastError() != ERROR_ALREADY_EXISTS;
        gpu = static_cast<HP2VR::GpuSharedState*>(MapViewOfFile(gpuHandle,
            FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HP2VR::GpuSharedState)));
        if (!gpu) return false;
        if (freshGpu || !HP2VR::compatible(*gpu)) {
            ZeroMemory(gpu, sizeof(*gpu));
            gpu->magic = HP2VR::GpuBridgeMagic;
            gpu->version = HP2VR::GpuBridgeVersion;
        }
        shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, HP2VR::ShutdownEventName);
        if (shutdownEvent) ResetEvent(shutdownEvent);
        return shutdownEvent != nullptr;
    }

    void shutdown() {
        if (gpu) InterlockedExchange(reinterpret_cast<volatile LONG*>(&gpu->consumerReady), 0);
        if (main) {
            HP2VR::InputPayload neutral{};
            HP2VR::publishInput(*main, neutral);
            main->companionPid = 0;
            UnmapViewOfFile(main);
        }
        if (gpu) UnmapViewOfFile(gpu);
        if (mainHandle) CloseHandle(mainHandle);
        if (gpuHandle) CloseHandle(gpuHandle);
        if (shutdownEvent) CloseHandle(shutdownEvent);
        main = nullptr;
        gpu = nullptr;
    }
};

struct SharedEyes {
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    ID3D11Texture2D* texture[2]{};
    ID3D11Texture2D* staging[2]{};
    IDXGIKeyedMutex* mutex[2]{};
    LONG generation{-1};
    bool pending{};
    HP2VR::FramePayload stagedFrame{};
    unsigned readbackDeferrals{};

    void releaseTextures() {
        for (int eye = 0; eye < 2; ++eye) {
            if (mutex[eye]) { mutex[eye]->Release(); mutex[eye] = nullptr; }
            if (staging[eye]) { staging[eye]->Release(); staging[eye] = nullptr; }
            if (texture[eye]) { texture[eye]->Release(); texture[eye] = nullptr; }
        }
        generation = -1;
        pending=false;
    }

    void releaseDevice() {
        releaseTextures();
        if (context) { context->Release(); context = nullptr; }
        if (device) { device->Release(); device = nullptr; }
    }

    bool tryAdapter(IDXGIAdapter1* adapter, const uint64_t handles[2]) {
        ID3D11Device* candidate{};
        ID3D11DeviceContext* candidateContext{};
        const HRESULT created = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            &candidate, nullptr, &candidateContext);
        if (FAILED(created)) return false;
        ID3D11Texture2D* candidateTexture[2]{};
        IDXGIKeyedMutex* candidateMutex[2]{};
        bool success = true;
        for (int eye = 0; eye < 2 && success; ++eye) {
            HRESULT hr = candidate->OpenSharedResource(
                reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handles[eye])),
                __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&candidateTexture[eye]));
            if (SUCCEEDED(hr)) hr = candidateTexture[eye]->QueryInterface(
                __uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&candidateMutex[eye]));
            success = SUCCEEDED(hr);
        }
        if (!success) {
            for (int eye = 0; eye < 2; ++eye) {
                if (candidateMutex[eye]) candidateMutex[eye]->Release();
                if (candidateTexture[eye]) candidateTexture[eye]->Release();
            }
            candidateContext->Release();
            candidate->Release();
            return false;
        }
        releaseDevice();
        device = candidate;
        context = candidateContext;
        for (int eye = 0; eye < 2; ++eye) {
            texture[eye] = candidateTexture[eye];
            mutex[eye] = candidateMutex[eye];
        }
        return true;
    }

    bool refresh(HP2VR::GpuSharedState& state) {
        const LONG wanted = InterlockedCompareExchange(
            reinterpret_cast<volatile LONG*>(&state.generation), 0, 0);
        if (wanted == generation && texture[0] && texture[1]) return state.consumerError == 0;
        const uint64_t handles[2]{state.sharedHandle[0], state.sharedHandle[1]};
        if (wanted <= 0 || !handles[0] || !handles[1]) return false;

        IDXGIFactory1* factory{};
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
            return false;
        bool opened = false;
        for (UINT index = 0; !opened; ++index) {
            IDXGIAdapter1* adapter{};
            if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 description{};
            adapter->GetDesc1(&description);
            opened = tryAdapter(adapter, handles);
            if (opened) log("Opened shared HP2 eyes on adapter %u: %ls", index, description.Description);
            adapter->Release();
        }
        factory->Release();
        if (!opened) {
            log("No local D3D11 adapter could open both HP2 shared eyes");
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&state.consumerError), 1);
            return false;
        }
        generation = wanted;
        bool held[2]{};
        for (int eye = 0; eye < 2; ++eye) {
            const HRESULT hr = mutex[eye]->AcquireSync(1, 0);
            if (hr != S_OK) {
                log("Shared eye bootstrap failed eye=%d hr=0x%08lx", eye,
                    static_cast<unsigned long>(hr));
                for (int prior = 0; prior < eye; ++prior)
                    if (held[prior]) mutex[prior]->ReleaseSync(1);
                releaseDevice();
                if(hr!=WAIT_TIMEOUT)InterlockedExchange(reinterpret_cast<volatile LONG*>(&state.consumerError), 2);
                return false;
            }
            held[eye] = true;
        }
        // Release both only after both key-1 textures are held.  Releasing the
        // left eye early lets the producer advance it while the right eye is
        // still being opened, permanently splitting the initial pair.
        for (int eye = 0; eye < 2; ++eye) mutex[eye]->ReleaseSync(0);
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&state.consumerError), 0);
        MemoryBarrier();
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&state.consumerReady), 1);
        log("Shared eye generation %ld ready: %ux%u format=%u", generation,
            state.width, state.height, state.format);
        return true;
    }

    bool acquire() {
        bool held[2]{};
        for (int eye = 0; eye < 2; ++eye) {
            const HRESULT hr = mutex[eye] ? mutex[eye]->AcquireSync(1, 0) : E_POINTER;
            if (hr != S_OK) {
                for (int prior = 0; prior < eye; ++prior) if (held[prior]) mutex[prior]->ReleaseSync(1);
                return false;
            }
            held[eye] = true;
        }
        return true;
    }

    void release() {
        for (auto* value : mutex) if (value) value->ReleaseSync(0);
    }

    bool stage(const HP2VR::FramePayload& frame) {
        if(pending || !device || !context)return false;
        for (int eye = 0; eye < 2; ++eye) {
            if (!staging[eye]) {
                D3D11_TEXTURE2D_DESC desc{};
                texture[eye]->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                desc.MiscFlags = 0;
                if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging[eye]))) {
                    log("Cross-adapter staging allocation failed eye=%d", eye);
                    return false;
                }
            }
            context->CopyResource(staging[eye], texture[eye]);
        }
        context->Flush();
        stagedFrame=frame;pending=true;return true;
    }

    bool uploadTo(ID3D11DeviceContext* destinationContext,
                  ID3D11Texture2D* const destination[2]) {
        if (!pending || !destinationContext || !destination[0] || !destination[1])return false;
        D3D11_MAPPED_SUBRESOURCE mapped[2]{};
        // Never wait for the Intel readback on SteamVR's submission thread.
        // Map BOTH eyes before changing either persistent output texture.
        for(int eye=0;eye<2;++eye) {
            const HRESULT hr=context->Map(staging[eye],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped[eye]);
            if(FAILED(hr)) {
                for(int prior=0;prior<eye;++prior)context->Unmap(staging[prior],0);
                if(hr==DXGI_ERROR_WAS_STILL_DRAWING)++readbackDeferrals;
                else {pending=false;log("Cross-adapter readback failed hr=0x%08lx",static_cast<unsigned long>(hr));}
                return false;
            }
        }
        for(int eye=0;eye<2;++eye) {
            D3D11_TEXTURE2D_DESC description{};
            texture[eye]->GetDesc(&description);
            const D3D11_BOX region{0, 0, 0, description.Width, description.Height, 1};
            if (description.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                description.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                static std::vector<unsigned char> bgra;
                bgra.resize(size_t(description.Width) * description.Height * 4);
                for (unsigned y = 0; y < description.Height; ++y) {
                    const auto* src = static_cast<const unsigned char*>(mapped[eye].pData) + size_t(y) * mapped[eye].RowPitch;
                    auto* dst = bgra.data() + size_t(y) * description.Width * 4;
                    for (unsigned x = 0; x < description.Width * 4; x += 4) {
                        dst[x] = src[x + 2]; dst[x + 1] = src[x + 1];
                        dst[x + 2] = src[x]; dst[x + 3] = 255;
                    }
                }
                destinationContext->UpdateSubresource(destination[eye], 0, &region,
                    bgra.data(), description.Width * 4, 0);
            } else {
                destinationContext->UpdateSubresource(destination[eye], 0, &region,
                    mapped[eye].pData, mapped[eye].RowPitch, 0);
            }
            context->Unmap(staging[eye], 0);
        }
        pending=false;return true;
    }
};

// SteamVR rejects DirectX textures created on a different physical adapter.
// HP2 renders on the Intel adapter on this hybrid laptop while SteamVR owns the
// discrete adapter. Keep the producer device for keyed shared capture, then
// upload each coherent pair into persistent textures created on SteamVR's DXGI
// adapter before Submit or overlay registration.
struct CompositorDevice {
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    int32_t adapterIndex{-1};

    bool initialize(vr::IVRSystem& system) {
        system.GetDXGIOutputInfo(&adapterIndex);
        if (adapterIndex < 0) {
            log("SteamVR did not report a DXGI output adapter");
            return false;
        }
        return initializeAdapter(adapterIndex);
    }
    bool initializeAdapter(int index) {
        adapterIndex=index;
        IDXGIFactory1* factory{};
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                reinterpret_cast<void**>(&factory)))) return false;
        IDXGIAdapter1* adapter{};
        const HRESULT enumerated = factory->EnumAdapters1(
            static_cast<UINT>(adapterIndex), &adapter);
        factory->Release();
        if (FAILED(enumerated) || !adapter) {
            log("SteamVR DXGI adapter index %d was not found", adapterIndex);
            return false;
        }
        DXGI_ADAPTER_DESC1 description{};
        adapter->GetDesc1(&description);
        const HRESULT created = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN,
            nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            &device, nullptr, &context);
        adapter->Release();
        if (FAILED(created)) {
            log("SteamVR adapter D3D11 device creation failed index=%d hr=0x%08lx",
                adapterIndex, static_cast<unsigned long>(created));
            return false;
        }
        log("SteamVR submit device adapter %d: %ls", adapterIndex,
            description.Description);
        return true;
    }

    void shutdown() {
        if (context) { context->Release(); context = nullptr; }
        if (device) { device->Release(); device = nullptr; }
    }
};

struct PanelInteraction {
    HP2ScreenPanel::Panel panel;
    bool wasVisible{};
    uint32_t trackingGeneration{};
    bool pointerValid{};
    float head[12]{};
    float controller[12]{};
    float hit[3]{};

    void update(HP2VR::InputPayload& input, const HP2VR::FramePayload* frame) {
        const bool visible = frame &&
            frame->presentation == HP2VR::Presentation::StationaryPanel;
        pointerValid = false;
        input.pointerValid = input.pointerPressed = 0;
        input.pointerUv[0] = input.pointerUv[1] = 0.0f;
        if (!visible || !HP2VR::finitePose(input.head)) {
            wasVisible = visible;
            return;
        }
        const float aspect = frame->height
            ? static_cast<float>(frame->width) / static_cast<float>(frame->height)
            : 4.0f / 3.0f;
        const bool steamVrReset = trackingGeneration != input.head.trackingGeneration;
        if (!wasVisible || steamVrReset || !panel.ready) {
            panel.place(input.head.m, 1.8f, aspect);
            trackingGeneration = input.head.trackingGeneration;
            log("Stationary HP2 panel placed %.3f x %.3f m at 1.8 m",
                panel.halfWidth * 2.0f, panel.halfHeight * 2.0f);
        }
        wasVisible = true;
        // Keep every dialog within the same comfortable envelope. Preserving
        // the previous width made tall Load/Options dialogs spill out of view.
        if (panel.ready && aspect > 0.0f) {
            panel.halfHeight = (std::min)(0.6f, 0.8f / aspect);
            panel.halfWidth = panel.halfHeight * aspect;
        }
        std::memcpy(head, input.head.m, sizeof(head));
        if (!panel.ready || !HP2VR::finitePose(input.hand[1])) return;
        std::memcpy(controller, input.hand[1].m, sizeof(controller));
        float origin[3]{}, direction[3]{};
        HP2ScreenPanel::controllerRay(controller, origin, direction);
        // Read only our sibling runtime settings; keep shared hand poses unchanged.
        static ULONGLONG checked{};static float pitch{},yaw{};
        const auto now=GetTickCount64();
        if(!checked||now-checked>=100){
            wchar_t settings[MAX_PATH]{};GetModuleFileNameW(nullptr,settings,MAX_PATH);
            if(auto* slash=wcsrchr(settings,L'\\')){*slash=0;wcscat_s(settings,L"\\..\\System\\HP2VRSettings.ini");
                pitch=std::clamp(float(static_cast<int>(GetPrivateProfileIntW(L"VR",L"PointerPitchDegrees",0,settings))),-90.f,90.f);
                yaw=std::clamp(float(static_cast<int>(GetPrivateProfileIntW(L"VR",L"PointerYawDegrees",0,settings))),-90.f,90.f);
            }checked=now;
        }
        float adjustedUp[3];HP2AimAngles::axes(controller,pitch,yaw,direction,adjustedUp);
        float u{}, v{};
        pointerValid = panel.pick(origin, direction, u, v, hit);
        if (!pointerValid) return;
        input.pointerUv[0] = u;
        input.pointerUv[1] = v;
        input.pointerValid = 1;
        input.pointerPressed = input.trigger[1] > 0.65f ? 1u : 0u;
    }
};

bool placePointerOverlay(vr::IVROverlay* overlays, vr::VROverlayHandle_t handle,
                         const PanelInteraction& interaction) {
    if (!overlays || handle == vr::k_ulOverlayHandleInvalid || !interaction.pointerValid)
        return false;
    float start[3]{interaction.controller[3], interaction.controller[7],
                   interaction.controller[11]};
    float axisX[3]{interaction.hit[0] - start[0], interaction.hit[1] - start[1],
                   interaction.hit[2] - start[2]};
    float length = std::sqrt(axisX[0] * axisX[0] + axisX[1] * axisX[1] +
                             axisX[2] * axisX[2]);
    if (length < 0.06f || length > 8.0f) return false;
    for (float& value : axisX) value /= length;
    for (int index = 0; index < 3; ++index) start[index] += axisX[index] * 0.035f;
    length -= 0.035f;
    float center[3]{};
    for (int index = 0; index < 3; ++index)
        center[index] = (start[index] + interaction.hit[index]) * 0.5f;
    float axisZ[3]{interaction.head[3] - center[0], interaction.head[7] - center[1],
                   interaction.head[11] - center[2]};
    float along = axisZ[0] * axisX[0] + axisZ[1] * axisX[1] + axisZ[2] * axisX[2];
    for (int index = 0; index < 3; ++index) axisZ[index] -= axisX[index] * along;
    float zLength = std::sqrt(axisZ[0] * axisZ[0] + axisZ[1] * axisZ[1] +
                              axisZ[2] * axisZ[2]);
    if (zLength < 0.001f) return false;
    for (float& value : axisZ) value /= zLength;
    float axisY[3]{axisZ[1] * axisX[2] - axisZ[2] * axisX[1],
                   axisZ[2] * axisX[0] - axisZ[0] * axisX[2],
                   axisZ[0] * axisX[1] - axisZ[1] * axisX[0]};
    vr::HmdMatrix34_t transform{
        axisX[0], axisY[0], axisZ[0], center[0],
        axisX[1], axisY[1], axisZ[1], center[1],
        axisX[2], axisY[2], axisZ[2], center[2]};
    return overlays->SetOverlayWidthInMeters(handle, length) == vr::VROverlayError_None &&
        overlays->SetOverlayTransformAbsolute(handle, vr::TrackingUniverseStanding,
            &transform) == vr::VROverlayError_None &&
        overlays->ShowOverlay(handle) == vr::VROverlayError_None &&
        overlays->IsOverlayVisible(handle);
}

// Persistent output eyes avoid changing submitted resource identity when a
// menu opens. This preserves V31's stable-eye fix and prevents world/menu
// format churn from reviving shimmer or SteamVR close-transition crashes.
struct StablePresentation {
    vr::IVROverlay* overlays{};
    vr::VROverlayHandle_t panelOverlay{vr::k_ulOverlayHandleInvalid};
    vr::VROverlayHandle_t pointerOverlay{vr::k_ulOverlayHandleInvalid};
    ID3D11Texture2D* output[2]{};
    ID3D11RenderTargetView* outputView[2]{};
    ID3D11Texture2D* uiTexture{};
    ID3D11Texture2D* pointerTexture{};
    UINT width{}, height{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    bool pointerTextureRegistered{};

    bool initialize() {
        overlays = vr::VROverlay();
        if (!overlays) return false;
        auto panelError = overlays->CreateOverlay("hp2vr.native.menu.panel",
            "HP2 VR native menu panel", &panelOverlay);
        if (panelError == vr::VROverlayError_KeyInUse)
            panelError = overlays->FindOverlay("hp2vr.native.menu.panel", &panelOverlay);
        if (panelError != vr::VROverlayError_None) {
            panelOverlay = vr::k_ulOverlayHandleInvalid;
            log("HP2 menu overlay creation failed error=%d", panelError);
            return false;
        }
        overlays->SetOverlayTexelAspect(panelOverlay, 1.0f);
        overlays->SetOverlayAlpha(panelOverlay, 1.0f);
        overlays->SetOverlayFlag(panelOverlay, vr::VROverlayFlags_IgnoreTextureAlpha, true);

        auto pointerError = overlays->CreateOverlay("hp2vr.native.menu.pointer",
            "HP2 VR native menu pointer", &pointerOverlay);
        if (pointerError == vr::VROverlayError_KeyInUse)
            pointerError = overlays->FindOverlay("hp2vr.native.menu.pointer", &pointerOverlay);
        if (pointerError == vr::VROverlayError_None) {
            overlays->SetOverlayTexelAspect(pointerOverlay, 1.0f);
            overlays->SetOverlayAlpha(pointerOverlay, 1.0f);
            overlays->SetOverlaySortOrder(pointerOverlay, 10);
            overlays->SetOverlayFlag(pointerOverlay,
                vr::VROverlayFlags_IgnoreTextureAlpha, true);
        } else {
            pointerOverlay = vr::k_ulOverlayHandleInvalid;
            log("HP2 pointer overlay creation failed error=%d", pointerError);
        }
        return true;
    }

    void releaseResources() {
        for (int eye = 0; eye < 2; ++eye) {
            if (outputView[eye]) { outputView[eye]->Release(); outputView[eye] = nullptr; }
            if (output[eye]) { output[eye]->Release(); output[eye] = nullptr; }
        }
        if (uiTexture) { uiTexture->Release(); uiTexture = nullptr; }
        if (pointerTexture) { pointerTexture->Release(); pointerTexture = nullptr; }
        width = height = 0;
        format = DXGI_FORMAT_UNKNOWN;
        pointerTextureRegistered = false;
    }

    bool ensureResources(const D3D11_TEXTURE2D_DESC& source, CompositorDevice& compositor) {
        if (!compositor.device || source.Width > EyeCapacity || source.Height > EyeCapacity) return false;
        if (output[0] && width == source.Width && height == source.Height &&
            format == source.Format) return true;
        // V31 stable-eye policy: input/menu resizing must never retire an eye
        // texture already submitted to the Oculus/SteamVR runtime.
        if (uiTexture) { uiTexture->Release(); uiTexture = nullptr; }
        D3D11_TEXTURE2D_DESC target = source;
        target.Width = target.Height = EyeCapacity;
        target.MipLevels = target.ArraySize = target.SampleDesc.Count = 1;
        target.SampleDesc.Quality = 0;
        target.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        target.Usage = D3D11_USAGE_DEFAULT;
        target.CPUAccessFlags = 0;
        target.MiscFlags = 0;
        target.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        for (int eye = 0; eye < 2; ++eye) {
            if (output[eye]) continue;
            if (FAILED(compositor.device->CreateTexture2D(&target, nullptr, &output[eye])) ||
                FAILED(compositor.device->CreateRenderTargetView(output[eye], nullptr,
                    &outputView[eye]))) {
                log("Stable HP2 eye allocation failed");
                releaseResources();
                return false;
            }
            const float black[4]{0, 0, 0, 1};
            compositor.context->ClearRenderTargetView(outputView[eye], black);
            log("Persistent RGBA eye %d allocated %ux%u pointer=%p", eye, EyeCapacity, EyeCapacity, output[eye]);
        }
        target.Width = source.Width; target.Height = source.Height;
        target.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(compositor.device->CreateTexture2D(&target, nullptr, &uiTexture))) {
            log("Persistent HP2 menu texture allocation failed");
            releaseResources();
            return false;
        }
        if (!pointerTexture && pointerOverlay != vr::k_ulOverlayHandleInvalid) {
            uint8_t pixels[256 * 2 * 4]{};
            for (size_t offset = 0; offset < sizeof(pixels); offset += 4) {
                pixels[offset] = 255; pixels[offset + 1] = 140;
                pixels[offset + 2] = 30; pixels[offset + 3] = 255;
            }
            D3D11_TEXTURE2D_DESC ray{};
            ray.Width = 256; ray.Height = 2; ray.MipLevels = ray.ArraySize = 1;
            ray.Format = DXGI_FORMAT_B8G8R8A8_UNORM; ray.SampleDesc.Count = 1;
            ray.Usage = D3D11_USAGE_DEFAULT; ray.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ray.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
            D3D11_SUBRESOURCE_DATA data{pixels, 256 * 4, 0};
            if (SUCCEEDED(compositor.device->CreateTexture2D(&ray, &data, &pointerTexture))) {
                vr::Texture_t texture{pointerTexture, vr::TextureType_DirectX,
                                      vr::ColorSpace_Gamma};
                pointerTextureRegistered = overlays->SetOverlayTexture(pointerOverlay,
                    &texture) == vr::VROverlayError_None;
            }
        }
        width = source.Width;
        height = source.Height;
        format = source.Format;
        log("Stable HP2 presentation eyes ready %ux%u format=%u", width, height,
            static_cast<unsigned>(format));
        return true;
    }

    bool prepare(SharedEyes& eyes, CompositorDevice& compositor,
                 const PanelInteraction& interaction,
                 const HP2VR::FramePayload& frame) {
        uploaded=false;
        D3D11_TEXTURE2D_DESC source{};
        eyes.texture[0]->GetDesc(&source);
        if (!ensureResources(source, compositor)) return false;
        if (!eyes.uploadTo(compositor.context, output)) return false;
        uploaded=true;
        return present(compositor, interaction, frame);
    }
    bool uploaded{};

    bool prepareNative(HP2NativeFrontend& frontend, CompositorDevice& compositor,
                       const PanelInteraction& interaction, const HP2VR::FramePayload& frame) {
        D3D11_TEXTURE2D_DESC source{};
        source.Width = frontend.width; source.Height = frontend.height;
        source.MipLevels = source.ArraySize = source.SampleDesc.Count = 1;
        source.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (!ensureResources(source, compositor)) return false;
        static std::vector<unsigned char> rgba;
        rgba = frontend.pixels;
        for (size_t i = 0; i < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);
        const D3D11_BOX region{0, 0, 0, frontend.width, frontend.height, 1};
        compositor.context->UpdateSubresource(output[1], 0, &region,
            rgba.data(), frontend.width * 4, 0);
        return present(compositor, interaction, frame);
    }

    bool present(CompositorDevice& compositor, const PanelInteraction& interaction,
                 const HP2VR::FramePayload& frame) {
        const bool wantsPanel = frame.presentation == HP2VR::Presentation::StationaryPanel &&
            interaction.panel.ready && panelOverlay != vr::k_ulOverlayHandleInvalid;
        bool overlayReady = false;
        if (wantsPanel) {
            const D3D11_BOX region{0, 0, 0, width, height, 1};
            compositor.context->CopySubresourceRegion(uiTexture, 0, 0, 0, 0, output[1], 0, &region);
            vr::Texture_t texture{uiTexture, vr::TextureType_DirectX, vr::ColorSpace_Gamma};
            const auto textureError = overlays->SetOverlayTexture(panelOverlay, &texture);
            const auto& panel = interaction.panel;
            vr::HmdMatrix34_t transform{
                panel.right[0], 0, panel.normal[0], panel.center[0],
                0, 1, 0, panel.center[1],
                panel.right[2], 0, panel.normal[2], panel.center[2]};
            const auto transformError = overlays->SetOverlayTransformAbsolute(panelOverlay,
                vr::TrackingUniverseStanding, &transform);
            const auto widthError = overlays->SetOverlayWidthInMeters(panelOverlay,
                panel.halfWidth * 2.0f);
            const auto showError = overlays->ShowOverlay(panelOverlay);
            overlayReady = textureError == vr::VROverlayError_None &&
                transformError == vr::VROverlayError_None &&
                widthError == vr::VROverlayError_None &&
                showError == vr::VROverlayError_None && overlays->IsOverlayVisible(panelOverlay);
            if (overlayReady && interaction.pointerValid && pointerTextureRegistered)
                placePointerOverlay(overlays, pointerOverlay, interaction);
            else if (pointerOverlay != vr::k_ulOverlayHandleInvalid)
                overlays->HideOverlay(pointerOverlay);
        } else {
            if (panelOverlay != vr::k_ulOverlayHandleInvalid) overlays->HideOverlay(panelOverlay);
            if (pointerOverlay != vr::k_ulOverlayHandleInvalid) overlays->HideOverlay(pointerOverlay);
        }

        const float black[4]{0.01f, 0.01f, 0.01f, 1.0f};
        for (int eye = 0; eye < 2; ++eye) {
            if (overlayReady) compositor.context->ClearRenderTargetView(outputView[eye], black);
        }
        return overlayReady;
    }

    ID3D11Texture2D* eye(int index, ID3D11Texture2D* fallback) const {
        return output[index] ? output[index] : fallback;
    }

    void shutdown() {
        releaseResources();
        if (overlays && pointerOverlay != vr::k_ulOverlayHandleInvalid)
            overlays->DestroyOverlay(pointerOverlay);
        if (overlays && panelOverlay != vr::k_ulOverlayHandleInvalid)
            overlays->DestroyOverlay(panelOverlay);
        pointerOverlay = panelOverlay = vr::k_ulOverlayHandleInvalid;
        overlays = nullptr;
    }
};

struct SteamActions {
    bool initialized{};
    bool armed{};
    vr::VRActionSetHandle_t actionSet{};
    vr::VRActionHandle_t action[10]{};
    vr::VRActionHandle_t haptic[2]{};

    bool initialize() {
        wchar_t manifest[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, manifest, MAX_PATH);
        wchar_t* slash = length ? wcsrchr(manifest, L'\\') : nullptr;
        if (!slash) return false;
        wcscpy_s(slash + 1, MAX_PATH - static_cast<size_t>(slash + 1 - manifest),
            L"actions_hp2_v1.json");
        char utf8[MAX_PATH * 3]{};
        if (!WideCharToMultiByte(CP_UTF8, 0, manifest, -1, utf8,
                static_cast<int>(sizeof(utf8)), nullptr, nullptr)) return false;
        if (vr::VRInput()->SetActionManifestPath(utf8) != vr::VRInputError_None ||
            vr::VRInput()->GetActionSetHandle("/actions/main", &actionSet) !=
                vr::VRInputError_None) return false;
        const char* names[]{"left_pose", "right_pose", "move", "turn", "cast",
                            "jump", "menu", "left_grip", "right_grip", "skip_dialog"};
        for (int index = 0; index < 10; ++index) {
            char name[128]{};
            sprintf_s(name, "/actions/main/in/%s", names[index]);
            if (vr::VRInput()->GetActionHandle(name, &action[index]) !=
                vr::VRInputError_None) return false;
        }
        vr::VRInput()->GetActionHandle("/actions/main/out/left_haptic", &haptic[0]);
        vr::VRInput()->GetActionHandle("/actions/main/out/right_haptic", &haptic[1]);
        initialized = true;
        return true;
    }

    bool update(HP2VR::InputPayload& input, bool focusOwned, uint32_t generation) {
        if (!initialized) return false;
        vr::VRActiveActionSet_t active{};
        active.ulActionSet = actionSet;
        if (vr::VRInput()->UpdateActionState(&active, sizeof(active), 1) !=
                vr::VRInputError_None) {
            armed = false;
            return false;
        }
        for (int hand = 0; hand < 2; ++hand) {
            vr::InputPoseActionData_t pose{};
            if (vr::VRInput()->GetPoseActionDataForNextFrame(action[hand],
                    vr::TrackingUniverseStanding, &pose, sizeof(pose),
                    vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
                pose.bActive && pose.pose.bPoseIsValid) {
                std::memcpy(input.hand[hand].m,
                    pose.pose.mDeviceToAbsoluteTracking.m, sizeof(input.hand[hand].m));
                input.hand[hand].sampleQpc = input.sampleQpc;
                input.hand[hand].valid = 1;
                input.hand[hand].trackingGeneration = generation;
            }
        }
        if (!focusOwned) {
            armed = false;
            return false;
        }
        bool actionsValid = true;
        auto analog = [&](int index, float& x, float& y) {
            vr::InputAnalogActionData_t value{};
            const auto error = vr::VRInput()->GetAnalogActionData(action[index], &value,
                sizeof(value), vr::k_ulInvalidInputValueHandle);
            if (error != vr::VRInputError_None) actionsValid = false;
            if (error == vr::VRInputError_None && value.bActive) {
                x = value.x;
                y = value.y;
            }
        };
        analog(2, input.move[0], input.move[1]);
        analog(3, input.turn[0], input.turn[1]);
        float ignored{};
        analog(4, input.trigger[1], ignored);
        ignored = 0.0f;
        analog(5, input.trigger[0], ignored);
        auto pressed = [&](int index) {
            vr::InputDigitalActionData_t value{};
            const auto error = vr::VRInput()->GetDigitalActionData(action[index], &value,
                sizeof(value), vr::k_ulInvalidInputValueHandle);
            if (error != vr::VRInputError_None) actionsValid = false;
            return error == vr::VRInputError_None && value.bActive && value.bState;
        };
        if (input.trigger[0] > 0.65f) input.buttons |= HP2VR::Jump;
        if (pressed(6)) input.buttons |= HP2VR::Menu;
        if (pressed(7)) input.buttons |= HP2VR::LeftGrip;
        if (pressed(8)) input.buttons |= HP2VR::RightGrip;
        if (pressed(9)) input.buttons |= HP2VR::SkipDialogue;
        if (!actionsValid) {
            armed = false;
        } else if (!armed) {
            bool neutral = input.buttons == 0 && input.trigger[0] < 0.15f &&
                input.trigger[1] < 0.15f;
            for (float value : input.move) if (std::fabs(value) > 0.25f) neutral = false;
            for (float value : input.turn) if (std::fabs(value) > 0.25f) neutral = false;
            armed = neutral;
        }
        if (!armed) {
            input.move[0] = input.move[1] = 0.0f;
            input.turn[0] = input.turn[1] = 0.0f;
            input.trigger[0] = input.trigger[1] = 0.0f;
            input.buttons = 0;
            return false;
        }
        return false;
    }
};

HP2VR::InputPayload publishTracking(HP2VR::SharedState& state, vr::IVRSystem& system,
                     const vr::TrackedDevicePose_t* poses, bool focusOwned,
                     uint32_t& generation, SteamActions* actions,
                     PanelInteraction& panel, const HP2VR::FramePayload* nativeFrame = nullptr) {
    HP2VR::InputPayload input{};
    input.sampleQpc = qpc();
    input.focusOwned = focusOwned ? 1u : 0u;
    const auto& head = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (head.bPoseIsValid && head.bDeviceIsConnected) {
        std::memcpy(input.head.m, head.mDeviceToAbsoluteTracking.m, sizeof(input.head.m));
        input.head.sampleQpc = input.sampleQpc;
        input.head.valid = 1;
        input.head.trackingGeneration = generation;
        input.inputAvailable = focusOwned ? 1u : 0u;
    }
    bool stereo = true;
    for (int eye = 0; eye < 2; ++eye) {
        const auto vrEye = static_cast<vr::EVREye>(eye);
        const auto transform = system.GetEyeToHeadTransform(vrEye);
        std::memcpy(input.eyeToHead[eye].m, transform.m, sizeof(input.eyeToHead[eye].m));
        input.eyeToHead[eye].sampleQpc = input.sampleQpc;
        input.eyeToHead[eye].valid = 1;
        input.eyeToHead[eye].trackingGeneration = generation;
        system.GetProjectionRaw(vrEye, &input.eyeTangents[eye][0], &input.eyeTangents[eye][1],
            &input.eyeTangents[eye][2], &input.eyeTangents[eye][3]);
        stereo = stereo && HP2VR::finitePose(input.eyeToHead[eye]) &&
            HP2VR::finiteTangents(input.eyeTangents[eye]);
    }
    input.stereoAvailable = stereo ? 1u : 0u;
    if (actions) actions->update(input, focusOwned, generation);
    HP2VR::FramePayload lastFrame{};
    const bool haveFrame = HP2VR::readFrame(state, lastFrame);
    panel.update(input, nativeFrame ? nativeFrame : (haveFrame ? &lastFrame : nullptr));
    HP2VR::publishInput(state, input);
    state.companionHeartbeatQpc = input.sampleQpc;
    return input;
}

void publishSimulatedTracking(HP2VR::SharedState& state, uint32_t generation) {
    HP2VR::InputPayload input{};
    input.sampleQpc = qpc();
    input.focusOwned = input.inputAvailable = input.stereoAvailable = 1;
    input.head.valid = 1;
    input.head.sampleQpc = input.sampleQpc;
    input.head.trackingGeneration = generation;
    input.head.m[0] = input.head.m[5] = input.head.m[10] = 1.0f;
    input.head.m[7] = 1.70f;
    for (int eye = 0; eye < 2; ++eye) {
        auto& pose = input.eyeToHead[eye];
        pose.valid = 1;
        pose.sampleQpc = input.sampleQpc;
        pose.trackingGeneration = generation;
        pose.m[0] = pose.m[5] = pose.m[10] = 1.0f;
        pose.m[3] = eye ? 0.032f : -0.032f;
        input.eyeTangents[eye][0] = -1.10f;
        input.eyeTangents[eye][1] = 1.10f;
        input.eyeTangents[eye][2] = -1.10f;
        input.eyeTangents[eye][3] = 1.10f;
    }
    // Explicit diagnostic only; never enabled by the normal headset launcher.
    if (GetEnvironmentVariableW(L"HP2VR_SIMULATED_CONTROLS", nullptr, 0)) {
        static DWORD began{};
        if (state.gameHeartbeatQpc && !began) began = GetTickCount();
        const DWORD elapsed = began ? GetTickCount() - began : 0;
        wchar_t mode[32]{};
        GetEnvironmentVariableW(L"HP2VR_SIMULATED_CONTROLS", mode, 32);
        if (!wcscmp(mode, L"wand") || !wcscmp(mode,L"avatar")) {
            auto& hand = input.hand[1];
            hand = input.head;
            hand.m[3] = .25f; hand.m[7] = .95f; hand.m[11] = .05f;
            if (elapsed >= 4000 && elapsed < 16500) input.buttons |= HP2VR::RightGrip;
            if (elapsed >= 8000) { hand.m[7] = 1.4f; hand.m[11] = -.35f; }
            if (elapsed >= 8500 && elapsed < 9500) input.move[0] = .8f;
            if (elapsed >= 9500 && elapsed < 10500) input.move[0] = -.8f;
            if (elapsed >= 10500 && elapsed < 11500) input.turn[0] = .5f;
            if (elapsed >= 10000 && elapsed < 12000) input.trigger[1] = 1.f;
            if (elapsed >= 12500 && elapsed < 12700) input.buttons |= HP2VR::Jump;
            if(!wcscmp(mode,L"avatar")) {
                input.move[0]=0;input.turn[0]=0;input.buttons&=~HP2VR::Jump;
                if(elapsed>=8000){input.head.m[7]=1.25f;input.head.m[5]=.5f;input.head.m[6]=.8660254f;input.head.m[9]=-.8660254f;input.head.m[10]=.5f;hand.m[7]=1.0f;}
            }
        } else if (!wcscmp(mode,L"broom")) {
            const DWORD phase=(elapsed/2500)%4;
            input.move[1]=phase==0?.8f:phase==1?-.8f:0.f;
            input.move[0]=phase==2?.8f:0.f;
            // Diagnostic only: look down toward the mounted broom and request
            // ordinary guarded intro skipping. Never bypass director states.
            if(elapsed>=4000){input.head.m[5]=.8660254f;input.head.m[6]=.5f;input.head.m[9]=-.5f;input.head.m[10]=.8660254f;}
            if(elapsed>=6000 && elapsed%6000<200)input.buttons|=HP2VR::SkipDialogue;
        } else if (!wcscmp(mode,L"vendor")) {
            input.hand[1]=input.head;
            input.hand[1].m[3]=0.f;input.hand[1].m[7]=1.5f;input.hand[1].m[11]=-.25f;
            wchar_t vendorTest[20]{};GetEnvironmentVariableW(L"HP2VR_VENDOR_TEST",vendorTest,20);
            if(!wcscmp(vendorTest,L"walkaway")&&elapsed>=16000)input.move[1]=-1.f;
        } else if (!wcscmp(mode,L"skip")) {
            if(elapsed>=6000 && elapsed<6200)input.buttons|=HP2VR::SkipDialogue;
        } else if (!wcscmp(mode,L"settings")) {
            if(elapsed>=5000 && elapsed<5300)input.buttons|=HP2VR::RightGrip|HP2VR::Menu;
            if(elapsed>=7000){input.pointerValid=1;input.pointerUv[0]=.8f;input.pointerUv[1]=.55f;}
            if(elapsed>=8000 && elapsed<8300)input.pointerPressed=1;
            if(elapsed>=10000 && elapsed<10300)input.buttons|=HP2VR::Menu;
        } else {
            if (elapsed >= 2000 && elapsed < 5000) input.move[1] = 0.8f;
            if (elapsed >= 6000 && elapsed < 6250) input.buttons |= HP2VR::Menu;
        }
    }
    HP2VR::publishInput(state, input);
    state.companionHeartbeatQpc = input.sampleQpc;
}

bool matchingCaptureSequences(HP2VR::GpuSharedState& gpu,
                              const HP2VR::FramePayload& frame) {
    for (int eye = 0; eye < 2; ++eye) {
        const LONG value = InterlockedCompareExchange(
            reinterpret_cast<volatile LONG*>(&gpu.captureSequence[eye]), 0, 0);
        if (value != frame.captureSequence[eye]) return false;
    }
    return true;
}

bool readLockedFrame(HP2VR::SharedState& state, HP2VR::GpuSharedState& gpu,
                     HP2VR::FramePayload& frame) {
    // Capture publishes the keyed texture before returning to the engine; the
    // engine then publishes FramePayload.  Holding both mutexes keeps pixels
    // stable while this short wait closes that intentional publication gap.
    for (int attempt = 0; attempt < 12; ++attempt) {
        if (HP2VR::readFrame(state, frame) && frame.pairNumber &&
            frame.width == gpu.width && frame.height == gpu.height &&
            matchingCaptureSequences(gpu, frame)) return true;
        Sleep(1);
    }
    return false;
}
}

int wmain(int argc, wchar_t** argv) {
    // Capture pixels and controller hit-testing must use physical coordinates,
    // including when the laptop and an external monitor have different scaling.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        SetProcessDPIAware();
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (wchar_t* slash = wcsrchr(path, L'\\')) { *slash = 0; SetCurrentDirectoryW(path); }
    const bool offline = argc > 1 && wcscmp(argv[1], L"--offline") == 0;
    const bool simulate = argc > 1 && wcscmp(argv[1], L"--simulate") == 0;
    const bool submitProbe = argc > 1 && wcscmp(argv[1], L"--submit-probe") == 0;
    Mappings mappings;
    if (!mappings.initialize()) {
        log("HP2 bridge mapping initialization failed error=%lu", GetLastError());
        return 2;
    }
    log("HP2 x64 companion v3 MENU-DPI-FIX-20261011; bridge=%u bytes gpu=%u bytes", static_cast<unsigned>(sizeof(*mappings.main)),
        static_cast<unsigned>(sizeof(*mappings.gpu)));
    if (offline) {
        log("Offline bridge validation passed; SteamVR was not started");
        mappings.shutdown();
        if (logFile) std::fclose(logFile);
        return 0;
    }

    if (simulate) {
        DWORD seconds = 45;
        wchar_t duration[16]{};
        if (GetEnvironmentVariableW(L"HP2VR_SIMULATE_SECONDS", duration,
                static_cast<DWORD>(sizeof(duration) / sizeof(duration[0])))) {
            const int parsed = _wtoi(duration);
            if (parsed >= 10 && parsed <= 300) seconds = static_cast<DWORD>(parsed);
        }
        log("Simulation tracking active for %lu seconds; SteamVR was not started", seconds);
        SharedEyes eyes;
        HP2NativeFrontend frontend;
        const bool readbackTest=GetEnvironmentVariableW(L"HP2VR_READBACK_TEST",nullptr,0)!=0;
        CompositorDevice testDevice;StablePresentation testPresentation;PanelInteraction testInteraction;
        if(readbackTest) {
            // Test the real two-device upload code without opening SteamVR.
            IDXGIFactory1* factory{};bool created=false;
            if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)))) {
                for(UINT index=0;!created;++index){IDXGIAdapter1* adapter{};if(factory->EnumAdapters1(index,&adapter)!=S_OK)break;
                    DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);adapter->Release();
                    if(desc.VendorId==0x10de)created=testDevice.initializeAdapter(index);
                }factory->Release();
            }
            if(!created){log("Readback test requires NVIDIA destination adapter");mappings.shutdown();return 7;}
        }
        const DWORD started = GetTickCount();
        uint32_t lastPair{};
        unsigned observedPairs{};
        unsigned observedMenus{};
        unsigned uploadedPairs{};DWORD transferMax{};
        while (GetTickCount() - started < seconds * 1000 &&
               WaitForSingleObject(mappings.shutdownEvent, 0) != WAIT_OBJECT_0) {
            publishSimulatedTracking(*mappings.main, 1);
            if (frontend.refresh() && frontend.capture()) {
                if (!observedMenus++) log("Native Windows menu captured %ux%u", frontend.width, frontend.height);
            }
            if(readbackTest && eyes.pending) {
                const DWORD before=GetTickCount();
                testPresentation.prepare(eyes,testDevice,testInteraction,eyes.stagedFrame);
                transferMax=(std::max)(transferMax,GetTickCount()-before);
                if(testPresentation.uploaded)++uploadedPairs;
                Sleep(5);continue;
            }
            if (!eyes.refresh(*mappings.gpu) || !eyes.acquire()) {
                Sleep(5);
                continue;
            }
            HP2VR::FramePayload locked{};
            const bool coherent = readLockedFrame(*mappings.main, *mappings.gpu, locked);
            if(coherent && readbackTest)eyes.stage(locked);
            eyes.release();
            if (coherent && locked.pairNumber != lastPair) {
                lastPair = locked.pairNumber;
                ++observedPairs;
            }
            Sleep(5);
        }
        log("Simulation shutdown after %u coherent captured pairs, %u native menu captures", observedPairs, observedMenus);
        if(readbackTest)log("Cross-GPU readback test: uploaded=%u deferred=%u maxPollUploadMs=%lu",uploadedPairs,eyes.readbackDeferrals,transferMax);
        testPresentation.shutdown();testDevice.shutdown();
        eyes.releaseDevice();
        mappings.shutdown();
        if (logFile) std::fclose(logFile);
        return readbackTest ? (uploadedPairs?0:8) : (observedPairs || observedMenus ? 0 : 6);
    }

    vr::EVRInitError initError = vr::VRInitError_None;
    vr::IVRSystem* system = vr::VR_Init(&initError, vr::VRApplication_Scene);
    if (initError != vr::VRInitError_None || !system) {
        log("SteamVR initialization failed: %s", vr::VR_GetVRInitErrorAsEnglishDescription(initError));
        mappings.shutdown();
        return 3;
    }
    log("SteamVR scene compositor initialized");
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);
    CompositorDevice compositorDevice;
    if (!compositorDevice.initialize(*system)) {
        log("SteamVR submit device initialization failed");
        vr::VR_Shutdown();
        mappings.shutdown();
        if (logFile) std::fclose(logFile);
        return 4;
    }
    if (submitProbe) {
        log("Submit probe: Texture_t=%zu poseTexture=%zu deviceFeature=0x%x", sizeof(vr::Texture_t), sizeof(vr::VRTextureWithPose_t), compositorDevice.device->GetFeatureLevel());
        unsigned successes{};
        for (unsigned variant = 0; variant < 8; ++variant) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = desc.Height = 1024; desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = variant & 1 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = variant & 2 ? D3D11_RESOURCE_MISC_SHARED : 0;
            std::vector<uint32_t> pixels(1024 * 1024, 0xff101010);
            D3D11_SUBRESOURCE_DATA data{pixels.data(), 4096, 0};
            ID3D11Texture2D* testTexture{};
            HRESULT created = compositorDevice.device->CreateTexture2D(&desc, &data, &testTexture);
            if (FAILED(created)) { log("PROBE allocation failed %x", created); continue; }
            vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
            vr::VRCompositor()->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
            vr::VRTextureWithPose_t texture{};
            texture.handle = testTexture; texture.eType = vr::TextureType_DirectX; texture.eColorSpace = vr::ColorSpace_Gamma;
            texture.mDeviceToAbsoluteTracking = poses[0].mDeviceToAbsoluteTracking;
            vr::VRTextureBounds_t bounds{0, 0, 1, 1};
            const auto flags = variant & 4 ? vr::Submit_TextureWithPose : vr::Submit_Default;
            auto left = vr::VRCompositor()->Submit(vr::Eye_Left, &texture, &bounds, flags);
            auto right = vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds, flags);
            log("PROBE format=%u shared=%u pose=%u results=%d,%d", desc.Format, desc.MiscFlags, variant & 4, left, right);
            if (!left && !right) ++successes;
            compositorDevice.context->Flush(); vr::VRCompositor()->PostPresentHandoff();
            Sleep(100);
            testTexture->Release();
        }
        log("Submit probe successful variants=%u", successes);
        vr::VR_Shutdown(); compositorDevice.shutdown(); mappings.shutdown();
        if (logFile) fclose(logFile);
        return successes ? 0 : 7;
    }
    SteamActions actions;
    const bool actionsReady = actions.initialize();
    log("SteamVR input actions %s", actionsReady ? "initialized" : "unavailable; controls neutral");
    SharedEyes eyes;
    PanelInteraction panelInteraction;
    HP2NativeFrontend frontend;
    StablePresentation presentation;
    const bool overlaysReady = presentation.initialize();
    log("Stationary menu/pointer overlays %s", overlaysReady ? "initialized" : "unavailable; stereo fallback active");
    uint32_t trackingGeneration = 1;
    unsigned submitted = 0;
    DWORD lastFresh=GetTickCount();uint32_t freshPair{};
    HP2VR::FramePayload lastPresented{};bool havePresented{},lastPanelReady{};
    DWORD lastTiming=GetTickCount(),maxTransfer{},maxSubmit{};
    unsigned freshCount{},fallbackCount{},acquireMiss{},incoherent{};
    log("Automatic stale-frame blackouts disabled; asynchronous paired readback active");
    while (WaitForSingleObject(mappings.shutdownEvent, 0) != WAIT_OBJECT_0) {
        vr::VREvent_t event{};
        while (system->PollNextEvent(&event, sizeof(event))) {
            if (event.eventType == vr::VREvent_Quit) {
                log("SteamVR requested application exit; closing owned HP2 session");
                SetEvent(mappings.shutdownEvent);
                system->AcknowledgeQuit_Exiting();
            }
            if (event.eventType == vr::VREvent_SeatedZeroPoseReset ||
                event.eventType == vr::VREvent_StandingZeroPoseReset ||
                event.eventType == vr::VREvent_ChaperoneUniverseHasChanged) ++trackingGeneration;
        }
        if (WaitForSingleObject(mappings.shutdownEvent, 0) == WAIT_OBJECT_0) break;
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
        const vr::EVRCompositorError poseResult = vr::VRCompositor()->WaitGetPoses(
            poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
        const bool focus = vr::VRCompositor()->CanRenderScene();
        const bool nativeMenu = frontend.refresh() && frontend.capture();
        HP2VR::FramePayload locked{};
        if (nativeMenu) {
            locked.width = frontend.width; locked.height = frontend.height;
            locked.presentation = HP2VR::Presentation::StationaryPanel;
        }
        const auto currentInput = publishTracking(*mappings.main, *system, poses, focus, trackingGeneration,
            actionsReady ? &actions : nullptr, panelInteraction, nativeMenu ? &locked : nullptr);
        if (poseResult != vr::VRCompositorError_None || !focus) {
            Sleep(5);
            continue;
        }
        bool acquired = false;
        bool panelReady = false;
        if (nativeMenu) {
            frontend.input(currentInput);
            locked.renderedHead = currentInput.head;
            std::memcpy(locked.eyeTangents, currentInput.eyeTangents, sizeof(locked.eyeTangents));
            panelReady = presentation.prepareNative(frontend, compositorDevice, panelInteraction, locked);
        } else {
            const DWORD transferStart=GetTickCount();
            if(!eyes.pending) {
                acquired = eyes.refresh(*mappings.gpu) && eyes.acquire();
                if(acquired) {
                    if(readLockedFrame(*mappings.main,*mappings.gpu,locked))eyes.stage(locked);
                    else ++incoherent;
                    // Release producer locks after queueing the GPU copies,
                    // before CPU readback/upload and SteamVR Submit.
                    eyes.release();acquired=false;
                }else ++acquireMiss;
            }
            bool ready=false;
            if(eyes.pending) {
                locked=eyes.stagedFrame;
                panelReady=presentation.prepare(eyes,compositorDevice,panelInteraction,locked);
                ready=presentation.uploaded;
            }
            maxTransfer=(std::max)(maxTransfer,GetTickCount()-transferStart);
            if (!ready) {
                ++fallbackCount;
                // Persistent output textures belong to this process, not the
                // loading game. Keep submitting them at compositor cadence.
                // Retain the ORIGINAL rendered pose for correct reprojection.
                if(!havePresented){Sleep(1);continue;}
                locked=lastPresented;panelReady=lastPanelReady;
            } else {
            if(locked.pairNumber!=freshPair) {
                freshPair=locked.pairNumber;lastFresh=GetTickCount();++freshCount;
            }
            }
        }
        if (!presentation.output[0] || !presentation.output[1]) {
            if (acquired) eyes.release();
            continue;
        }
        static bool panelWasReady{};
        if (panelReady != panelWasReady) {
            panelWasReady = panelReady;
            log("HP2 menu presentation %s", panelReady
                ? "stationary overlay with stable black eye background"
                : "stereo world/fallback");
        }
        const float symmetricExtent = HP2Stereo::horizontalExtent(locked.eyeTangents, locked.width, locked.height);
        const DWORD submitStart=GetTickCount();
        for (int eye = 0; eye < 2; ++eye) {
            const auto centered = HP2Stereo::centeredCrop(
                locked.eyeTangents[eye], symmetricExtent, locked.height ? float(locked.width) / locked.height : 1.0f);
            vr::VRTextureBounds_t bounds{centered.uMin, centered.vMin,
                centered.uMax, centered.vMax};
            if (!panelReady) {
                if (!centered.valid) bounds = {0, 0, 1, 1};
                const float sx = float(locked.width) / EyeCapacity;
                const float sy = float(locked.height) / EyeCapacity;
                bounds.uMin *= sx; bounds.uMax *= sx;
                bounds.vMin *= sy; bounds.vMax *= sy;
            }
            vr::VRTextureWithPose_t texture{};
            texture.handle = presentation.eye(eye, eyes.texture[eye]);
            texture.eType = vr::TextureType_DirectX;
            texture.eColorSpace = vr::ColorSpace_Gamma;
            std::memcpy(texture.mDeviceToAbsoluteTracking.m, locked.renderedHead.m,
                sizeof(locked.renderedHead.m));
            const auto result = vr::VRCompositor()->Submit(static_cast<vr::EVREye>(eye),
                &texture, !panelReady ? &bounds : nullptr,
                vr::Submit_TextureWithPose);
            if (result != vr::VRCompositorError_None && submitted < 8)
                log("SteamVR submit failed eye=%d error=%d", eye, result);
        }
        if (acquired) eyes.release();
        lastPresented=locked;lastPanelReady=panelReady;havePresented=true;
        compositorDevice.context->Flush();
        vr::VRCompositor()->PostPresentHandoff();
        maxSubmit=(std::max)(maxSubmit,GetTickCount()-submitStart);
        if(GetTickCount()-lastTiming>=5000) {
            log("HP2 frame timing: fresh=%u reuse=%u acquireMiss=%u incoherent=%u readbackDeferred=%u transferMaxMs=%lu submitMaxMs=%lu frameAgeMs=%lu",freshCount,fallbackCount,acquireMiss,incoherent,eyes.readbackDeferrals,maxTransfer,maxSubmit,GetTickCount()-lastFresh);
            lastTiming=GetTickCount();freshCount=fallbackCount=acquireMiss=incoherent=eyes.readbackDeferrals=0;maxTransfer=maxSubmit=0;
        }
        ++submitted;
    }
    log("Companion shutdown after %u submitted pairs", submitted);
    presentation.shutdown();
    eyes.releaseDevice();
    compositorDevice.shutdown();
    vr::VR_Shutdown();
    mappings.shutdown();
    if (logFile) std::fclose(logFile);
    return 0;
}
