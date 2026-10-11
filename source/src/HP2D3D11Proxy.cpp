#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstring>
#include "HP2VRBridge.h"

namespace {
using CreateDeviceFn = HRESULT (WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

HMODULE realModule{};
CreateDeviceFn realCreateDevice{};
ID3D11Device* device{};
ID3D11DeviceContext* context{};
ID3D11Texture2D* sharedTexture[2]{};
IDXGIKeyedMutex* sharedMutex[2]{};
ID3D11Texture2D* cpuStaging{};
ID3D11Texture2D* cpuResolve{};
D3D11_TEXTURE2D_DESC cpuDesc{};
D3D11_TEXTURE2D_DESC sharedDesc{};
bool cpuDescValid{};
bool sharedDescValid{};
bool handlesDirty{};
HANDLE mapping{};
HP2VR::GpuSharedState* bridge{};
FILE* logFile{};
HMODULE probeModule{};

using InitializeProbeFn = int (WINAPI*)();

void log(const char* message, HRESULT hr = S_OK) {
    if (!logFile) fopen_s(&logFile, "HP2VRD3D11Proxy.log", "w");
    if (!logFile) return;
    if (hr == S_OK) std::fprintf(logFile, "%s\n", message);
    else std::fprintf(logFile, "%s hr=0x%08lx\n", message, static_cast<unsigned long>(hr));
    std::fflush(logFile);
}

bool initializeReal() {
    if (realCreateDevice) return true;
    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length >= MAX_PATH - 12) return false;
    wcscat_s(path, L"\\d3d11.dll");
    // Resolve both the system DLL and its dependencies from System32 only.
    realModule = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    realCreateDevice = realModule
        ? reinterpret_cast<CreateDeviceFn>(GetProcAddress(realModule, "D3D11CreateDevice"))
        : nullptr;
    if (!realCreateDevice) log("Could not load system D3D11CreateDevice", HRESULT_FROM_WIN32(GetLastError()));
    return realCreateDevice != nullptr;
}

bool initializeBridge() {
    if (bridge) return true;
    mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(HP2VR::GpuSharedState), HP2VR::GpuMappingName);
    if (!mapping) return false;
    const bool fresh = GetLastError() != ERROR_ALREADY_EXISTS;
    bridge = static_cast<HP2VR::GpuSharedState*>(MapViewOfFile(
        mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HP2VR::GpuSharedState)));
    if (!bridge) { CloseHandle(mapping); mapping = nullptr; return false; }
    // Do not overwrite an existing mapping belonging to another bridge version.
    if (!fresh && !HP2VR::compatible(*bridge)) {
        log("Existing GPU bridge is incompatible; refusing to overwrite it");
        UnmapViewOfFile(bridge); bridge = nullptr;
        CloseHandle(mapping); mapping = nullptr;
        return false;
    }
    if (fresh) {
        ZeroMemory(bridge, sizeof(*bridge));
        bridge->magic = HP2VR::GpuBridgeMagic;
        bridge->version = HP2VR::GpuBridgeVersion;
    }
    return true;
}

void initializeProbe() {
    // ABI inspection is a development diagnostic, not a rendering dependency.
    // Normal gameplay must not load an extra executable module for logging.
    wchar_t enabled[2]{};
    if (GetEnvironmentVariableW(L"HP2VR_ABI_PROBE", enabled, 2) != 1 || enabled[0] != L'1') return;
    if (probeModule) return;
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        log("Could not resolve Game.exe path for HP2 probe", HRESULT_FROM_WIN32(GetLastError()));
        return;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || static_cast<size_t>(slash - path) + 16 >= MAX_PATH) {
        log("Could not form private HP2 probe path");
        return;
    }
    wcscpy_s(slash + 1, MAX_PATH - static_cast<size_t>(slash + 1 - path), L"HP2VRProbe.dll");
    probeModule = LoadLibraryExW(path, nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!probeModule) {
        log("HP2 live ABI probe was not loaded", HRESULT_FROM_WIN32(GetLastError()));
        return;
    }
    auto initialize = reinterpret_cast<InitializeProbeFn>(
        GetProcAddress(probeModule, "HP2VR_InitializeProbe"));
    if (!initialize) {
        log("HP2 live ABI probe export is missing", HRESULT_FROM_WIN32(GetLastError()));
        return;
    }
    const int result = initialize();
    log(result ? "HP2 live ABI probe completed" : "HP2 live ABI probe reported failure");
}

bool matching(const D3D11_TEXTURE2D_DESC& a, const D3D11_TEXTURE2D_DESC& b) {
    return a.Width == b.Width && a.Height == b.Height && a.Format == b.Format &&
        a.SampleDesc.Count == b.SampleDesc.Count && a.SampleDesc.Quality == b.SampleDesc.Quality;
}

void clearTextures() {
    if (bridge) {
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&bridge->consumerReady), 0);
        bridge->sharedHandle[0] = bridge->sharedHandle[1] = 0;
        MemoryBarrier();
    }
    for (int eye = 0; eye < 2; ++eye) {
        if (sharedMutex[eye]) { sharedMutex[eye]->Release(); sharedMutex[eye] = nullptr; }
        if (sharedTexture[eye]) { sharedTexture[eye]->Release(); sharedTexture[eye] = nullptr; }
    }
    if (cpuStaging) { cpuStaging->Release(); cpuStaging = nullptr; }
    if (cpuResolve) { cpuResolve->Release(); cpuResolve = nullptr; }
    cpuDescValid = sharedDescValid = handlesDirty = false;
}

bool makeSharedTexture(int eye, const D3D11_TEXTURE2D_DESC& source) {
    D3D11_TEXTURE2D_DESC output{};
    output.Width = source.Width;
    output.Height = source.Height;
    output.MipLevels = 1;
    output.ArraySize = 1;
    output.Format = source.Format;
    output.SampleDesc.Count = 1;
    output.Usage = D3D11_USAGE_DEFAULT;
    output.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    output.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    HRESULT hr = device->CreateTexture2D(&output, nullptr, &sharedTexture[eye]);
    if (FAILED(hr)) { log("Shared eye texture creation failed", hr); return false; }
    hr = sharedTexture[eye]->QueryInterface(__uuidof(IDXGIKeyedMutex),
        reinterpret_cast<void**>(&sharedMutex[eye]));
    if (FAILED(hr)) { log("Shared eye has no keyed mutex", hr); return false; }
    IDXGIResource* resource{};
    HANDLE handle{};
    hr = sharedTexture[eye]->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&resource));
    if (SUCCEEDED(hr)) hr = resource->GetSharedHandle(&handle);
    if (resource) resource->Release();
    if (FAILED(hr) || !handle) { log("GetSharedHandle failed", hr); return false; }
    bridge->sharedHandle[eye] = reinterpret_cast<uintptr_t>(handle);
    return true;
}

void setDevice(ID3D11Device* newDevice, ID3D11DeviceContext* newContext) {
    clearTextures();
    if (context) { context->Release(); context = nullptr; }
    if (device) { device->Release(); device = nullptr; }
    device = newDevice;
    if (device) device->AddRef();
    context = newContext;
    if (context) context->AddRef();
    else if (device) device->GetImmediateContext(&context);
    initializeBridge();
    if (bridge) {
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&bridge->producerError), 0);
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&bridge->consumerError), 0);
    }
    log("Captured HP2 renderer D3D11 device");
    initializeProbe();
}

bool capture(int eye) {
    if (eye < 0 || eye > 1 || !device || !context || !initializeBridge()) return false;
    ID3D11RenderTargetView* view{};
    context->OMGetRenderTargets(1, &view, nullptr);
    if (!view) return false;
    ID3D11Resource* resource{};
    view->GetResource(&resource);
    view->Release();
    ID3D11Texture2D* source{};
    HRESULT hr = resource
        ? resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&source))
        : E_NOINTERFACE;
    if (resource) resource->Release();
    if (FAILED(hr) || !source) return false;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    if (!sharedDescValid || !matching(sharedDesc, desc)) {
        char qualityReport[160]{};
        sprintf_s(qualityReport, "HP2 actual eye render target: %ux%u samples=%u quality=%u", desc.Width, desc.Height, desc.SampleDesc.Count, desc.SampleDesc.Quality);
        log(qualityReport);
        clearTextures();
        sharedDesc = desc;
        sharedDescValid = true;
        handlesDirty = true;
        bridge->width = desc.Width;
        bridge->height = desc.Height;
        bridge->format = static_cast<uint32_t>(desc.Format);
    }
    if (!sharedTexture[eye] && !makeSharedTexture(eye, desc)) {
        source->Release();
        clearTextures();
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&bridge->producerError), 1);
        return false;
    }
    hr = sharedMutex[eye]->AcquireSync(0, bridge->consumerReady ? 8 : 0);
    if (hr != S_OK) { source->Release(); return false; }
    if (desc.SampleDesc.Count > 1) context->ResolveSubresource(sharedTexture[eye], 0, source, 0, desc.Format);
    else context->CopySubresourceRegion(sharedTexture[eye], 0, 0, 0, 0, source, 0, nullptr);
    source->Release();
    context->Flush();
    hr = sharedMutex[eye]->ReleaseSync(1);
    if (FAILED(hr)) { clearTextures(); return false; }
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(&bridge->captureSequence[eye]));
    if (eye == 1 && handlesDirty && bridge->sharedHandle[0] && bridge->sharedHandle[1]) {
        MemoryBarrier();
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&bridge->generation));
        handlesDirty = false;
        log("Published HP2 GPU eye pair");
    }
    return true;
}

bool uploadPanel(const void* pixels, unsigned width, unsigned height, unsigned stride) {
    if (!pixels || !width || !height || stride < width * 4 || !device || !context ||
        !initializeBridge()) return false;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    if (!sharedDescValid || !matching(sharedDesc, desc)) {
        clearTextures();
        sharedDesc = desc;
        sharedDescValid = true;
        handlesDirty = true;
        bridge->width = width;
        bridge->height = height;
        bridge->format = static_cast<uint32_t>(desc.Format);
    }
    for (int eye = 0; eye < 2; ++eye) {
        if (!sharedTexture[eye] && !makeSharedTexture(eye, desc)) {
            clearTextures();
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&bridge->producerError), 1);
            return false;
        }
    }
    bool held[2]{};
    for (int eye = 0; eye < 2; ++eye) {
        const HRESULT hr = sharedMutex[eye]->AcquireSync(0, bridge->consumerReady ? 20 : 0);
        if (hr != S_OK) {
            for (int prior = 0; prior < eye; ++prior)
                if (held[prior]) sharedMutex[prior]->ReleaseSync(1);
            return false;
        }
        held[eye] = true;
    }
    for (int eye = 0; eye < 2; ++eye) {
        context->UpdateSubresource(sharedTexture[eye], 0, nullptr, pixels, stride, 0);
        sharedMutex[eye]->ReleaseSync(1);
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&bridge->captureSequence[eye]));
    }
    context->Flush();
    if (handlesDirty && bridge->sharedHandle[0] && bridge->sharedHandle[1]) {
        MemoryBarrier();
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&bridge->generation));
        handlesDirty = false;
        log("Published completed CPU menu frame as shared HP2 panel pair");
    }
    return true;
}

bool readPixels(void* destination, unsigned width, unsigned height, unsigned stride) {
    if (!destination || !width || !height || stride < width * 4 || !device || !context) return false;
    ID3D11RenderTargetView* view{};
    context->OMGetRenderTargets(1, &view, nullptr);
    if (!view) return false;
    ID3D11Resource* resource{};
    view->GetResource(&resource);
    view->Release();
    ID3D11Texture2D* source{};
    HRESULT hr = resource
        ? resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&source))
        : E_NOINTERFACE;
    if (resource) resource->Release();
    if (FAILED(hr) || !source) return false;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (desc.Width != width || desc.Height != height || (!bgra && !rgba)) { source->Release(); return false; }
    if (!cpuDescValid || !matching(cpuDesc, desc)) {
        if (cpuStaging) { cpuStaging->Release(); cpuStaging = nullptr; }
        if (cpuResolve) { cpuResolve->Release(); cpuResolve = nullptr; }
        cpuDesc = desc;
        cpuDescValid = true;
        D3D11_TEXTURE2D_DESC staging = desc;
        staging.MipLevels = staging.ArraySize = staging.SampleDesc.Count = 1;
        staging.SampleDesc.Quality = 0;
        staging.Usage = D3D11_USAGE_STAGING;
        staging.BindFlags = 0;
        staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        staging.MiscFlags = 0;
        hr = device->CreateTexture2D(&staging, nullptr, &cpuStaging);
        if (SUCCEEDED(hr) && desc.SampleDesc.Count > 1) {
            staging.Usage = D3D11_USAGE_DEFAULT;
            staging.CPUAccessFlags = 0;
            hr = device->CreateTexture2D(&staging, nullptr, &cpuResolve);
        }
        if (FAILED(hr) || !cpuStaging || (desc.SampleDesc.Count > 1 && !cpuResolve)) {
            source->Release();
            return false;
        }
    }
    ID3D11Texture2D* copySource = source;
    if (desc.SampleDesc.Count > 1) {
        context->ResolveSubresource(cpuResolve, 0, source, 0, desc.Format);
        copySource = cpuResolve;
    }
    context->CopyResource(cpuStaging, copySource);
    source->Release();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(cpuStaging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;
    auto* output = static_cast<unsigned char*>(destination);
    for (unsigned y = 0; y < height; ++y) {
        const auto* input = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
        auto* row = output + y * stride;
        if (bgra) std::memcpy(row, input, width * 4);
        else for (unsigned x = 0; x < width; ++x) {
            row[x * 4] = input[x * 4 + 2];
            row[x * 4 + 1] = input[x * 4 + 1];
            row[x * 4 + 2] = input[x * 4];
            row[x * 4 + 3] = input[x * 4 + 3];
        }
    }
    context->Unmap(cpuStaging, 0);
    return true;
}
}

extern "C" HRESULT WINAPI HP2VRProxyD3D11CreateDevice(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE type, HMODULE software, UINT flags,
    const D3D_FEATURE_LEVEL* levels, UINT levelCount, UINT sdk,
    ID3D11Device** output, D3D_FEATURE_LEVEL* selected, ID3D11DeviceContext** immediate) {
    if (!initializeReal()) return E_FAIL;
    const HRESULT hr = realCreateDevice(adapter, type, software, flags, levels, levelCount,
        sdk, output, selected, immediate);
    if (SUCCEEDED(hr) && output && *output) setDevice(*output, immediate ? *immediate : nullptr);
    return hr;
}

extern "C" int WINAPI HP2VR_D3D11CaptureEye(int eye) { return capture(eye) ? 1 : 0; }
extern "C" int WINAPI HP2VR_D3D11CaptureSequence(int eye) {
    if (!bridge || eye < 0 || eye > 1) return 0;
    return InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&bridge->captureSequence[eye]), 0, 0);
}
extern "C" int WINAPI HP2VR_D3D11ConsumerReady() { return bridge && bridge->consumerReady && !bridge->consumerError; }
extern "C" int WINAPI HP2VR_D3D11ReadPixels(void* destination, unsigned width, unsigned height, unsigned stride) {
    return readPixels(destination, width, height, stride) ? 1 : 0;
}
extern "C" int WINAPI HP2VR_D3D11UploadPanel(const void* pixels, unsigned width,
                                                unsigned height, unsigned stride) {
    return uploadPanel(pixels, width, height, stride) ? 1 : 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    // Do not release D3D/DXGI COM objects while the Windows loader lock is
    // held.  This proxy lives for the process lifetime; process teardown
    // reclaims these objects, mappings and handles safely after DLL detach.
    return TRUE;
}
