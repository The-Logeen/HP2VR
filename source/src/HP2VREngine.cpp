#include <windows.h>
#include <Engine.h>
#include <UnCon.h>
#include <Render.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>
#include "HP2NativeInput.h"
#include "HP2Locomotion.h"
#include "HP2VRBridge.h"
#include "HP2StereoProjection.h"
#include "HP2VRSettings.h"
#include "HP2Comfort.h"
// Same SDK-container compatibility implementation used by our research host.
// Retail Core supplies GMalloc but this SDK import library omits FArray::Realloc.
void FArray::Realloc(INT elementSize) {
    Data=GMalloc->Realloc(Data,ArrayMax*elementSize,TEXT("HP2VR FArray"));
}

namespace {
FILE* trace{};
HANDLE bridgeMapping{};
HP2VR::SharedState* bridge{};
HP2VR::InputPayload latestInput{};
bool inputWasLive{};
int currentEye{-1};
HP2VR::InputPayload pairInput{};
bool pairTracked{};
float originHead[3]{};
float originYaw{};
uint32_t originGeneration{};
bool haveOrigin{};
uint32_t pairNumber{};
UViewport* inputViewport{};
bool inputHeld[7]{};
bool menuButtonHeld{};
bool menuPresentation{};
APlayerPawn* bodyActor{};
INT bodyYaw{};
uint32_t bodyTrackingGeneration{};
HP2Locomotion::Turn bodyTurn{};
HP2Comfort::Height stairHeight;
float comfortOffset{};
APlayerPawn* comfortOwner{};
void line(const char* format, ...);

struct ComposedWindowCapture {
    HWND window{};
    HDC memory{};
    HBITMAP bitmap{};
    HGDIOBJ prior{};
    void* bits{};
    unsigned width{};
    unsigned height{};
    LONG bestArea{};

    ~ComposedWindowCapture() {
        if (memory && prior) SelectObject(memory, prior);
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
    }

    static BOOL CALLBACK findWindow(HWND candidate, LPARAM parameter) {
        auto* self = reinterpret_cast<ComposedWindowCapture*>(parameter);
        DWORD process{};
        GetWindowThreadProcessId(candidate, &process);
        if (process != GetCurrentProcessId() || !IsWindowVisible(candidate)) return TRUE;
        RECT client{};
        if (!GetClientRect(candidate, &client)) return TRUE;
        const LONG area = (client.right - client.left) * (client.bottom - client.top);
        if (area > self->bestArea) {
            self->bestArea = area;
            self->window = candidate;
        }
        return TRUE;
    }

    bool find() {
        if (window && IsWindow(window)) return true;
        window = nullptr;
        bestArea = 0;
        EnumWindows(findWindow, reinterpret_cast<LPARAM>(this));
        if (window) {
            wchar_t title[160]{};
            GetWindowTextW(window, title, static_cast<int>(_countof(title)));
            line("HP2 composed menu source window=%p title=%ls", window, title);
        }
        return window != nullptr;
    }

    bool resize(unsigned wantedWidth, unsigned wantedHeight) {
        if (memory && bitmap && bits && width == wantedWidth && height == wantedHeight)
            return true;
        if (memory && prior) { SelectObject(memory, prior); prior = nullptr; }
        if (bitmap) { DeleteObject(bitmap); bitmap = nullptr; }
        if (!memory) memory = CreateCompatibleDC(nullptr);
        if (!memory) return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = static_cast<LONG>(wantedWidth);
        info.bmiHeader.biHeight = -static_cast<LONG>(wantedHeight);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits) return false;
        prior = SelectObject(memory, bitmap);
        width = wantedWidth;
        height = wantedHeight;
        return true;
    }

    bool read(std::vector<FColor>& output, unsigned wantedWidth, unsigned wantedHeight) {
        if (!find() || !resize(wantedWidth, wantedHeight)) return false;
        RECT client{};
        if (!GetClientRect(window, &client)) return false;
        const int sourceWidth = client.right - client.left;
        const int sourceHeight = client.bottom - client.top;
        if (sourceWidth <= 0 || sourceHeight <= 0) return false;
        POINT origin{};
        if (!ClientToScreen(window, &origin)) return false;
        HDC screen = GetDC(nullptr);
        if (!screen) return false;
        SetStretchBltMode(memory, COLORONCOLOR);
        const BOOL copied = StretchBlt(memory, 0, 0,
            static_cast<int>(wantedWidth), static_cast<int>(wantedHeight), screen,
            origin.x, origin.y, sourceWidth, sourceHeight, SRCCOPY | CAPTUREBLT);
        ReleaseDC(nullptr, screen);
        if (!copied) return false;
        GdiFlush();
        output.resize(static_cast<size_t>(wantedWidth) * wantedHeight);
        const auto* bgra = static_cast<const BYTE*>(bits);
        for (size_t index = 0; index < output.size(); ++index) {
            output[index].R = bgra[index * 4 + 2];
            output[index].G = bgra[index * 4 + 1];
            output[index].B = bgra[index * 4];
            output[index].A = 255;
        }
        return true;
    }
} composedWindow;

using CaptureEyeFn = int (WINAPI*)(int);
using CaptureSequenceFn = int (WINAPI*)(int);
using UploadPanelFn = int (WINAPI*)(const void*, unsigned, unsigned, unsigned);
using ReadPixelsFn = int (WINAPI*)(void*, unsigned, unsigned, unsigned);
ReadPixelsFn readRenderPixels{};
CaptureEyeFn captureEye{};
CaptureSequenceFn captureSequence{};
UploadPanelFn uploadPanel{};
bool captureLookupReported{};

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = 2.0f * Pi;
// HP1 V31 allowed this to be user-calibrated.  Keep HP2 conservative until
// headset testing gives us a game-specific scale measurement.
// Match HP1VR V31's established default.  HP2 can expose the same calibration
// later, but the first playable build must not invent a wider physical scale.
float& WorldUnitsPerMetre = HP2Settings::units;

void line(const char* format, ...) {
    if (!trace) fopen_s(&trace, "HP2VREngine.log", "w");
    if (!trace) return;
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(trace, format, arguments);
    va_end(arguments);
    std::fputc('\n', trace);
    std::fflush(trace);
}

void savePanelDiagnostic(const std::vector<FColor>& pixels, unsigned width,
                         unsigned height) {
    if (!GetEnvironmentVariableA("HP2VR_PANEL_DUMP", nullptr, 0) || pixels.empty()) return;
    static bool saved{};
    if (saved) return;
    std::vector<FColor> bgra(pixels);
    // The D3D11 readback export already produces BGRA bytes for a Windows DIB.
    for (auto& pixel : bgra) pixel.A = 255;
    FILE* file{};
    fopen_s(&file, "HP2VRPanelCapture.bmp", "wb");
    if (!file) return;
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + width * height * sizeof(FColor);
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = -static_cast<LONG>(height);
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    std::fwrite(&header, sizeof(header), 1, file);
    std::fwrite(&info, sizeof(info), 1, file);
    std::fwrite(bgra.data(), sizeof(FColor), bgra.size(), file);
    std::fclose(file);
    saved = true;
    line("HP2 completed panel diagnostic saved %ux%u", width, height);
}

uint64_t qpc() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return static_cast<uint64_t>(value.QuadPart);
}

FVector trackingVector(float x, float y, float z) { return FVector(-z, x, y); }

FVector rotateYaw(FVector value, float yaw) {
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    return FVector(cosine * value.X - sine * value.Y,
                   sine * value.X + cosine * value.Y, value.Z);
}

FVector basisVector(FVector value, const FCoords& basis) {
    return basis.XAxis * value.X + basis.YAxis * value.Y + basis.ZAxis * value.Z;
}

float trackingFacingYaw(const float* matrix) {
    if (matrix[0] * matrix[0] + matrix[8] * matrix[8] > 0.01f)
        return std::atan2(matrix[8], matrix[0]);
    return std::atan2(-matrix[2], matrix[10]);
}

FName playerState(UObject* pawn) {
    FName result(NAME_None);
    if (!pawn) return result;
    struct StateQuery : FFrame {
        BYTE nativeTail[32]{};
        explicit StateQuery(UObject* object) : FFrame(object) {}
    };
    BYTE arguments[]{EX_EndFunctionParms};
    StateQuery frame(pawn);
    frame.Node = pawn->GetClass();
    frame.Code = arguments;
    frame.Locals = nullptr;
    pawn->execGetStateName(frame, &result);
    return result;
}

bool broomPlayer(APlayerPawn* pawn) {
    static UClass* cls{};if(!cls)cls=FindObject<UClass>(nullptr,TEXT("HGame.BroomHarry"));
    return pawn&&cls&&pawn->IsA(cls);
}
bool walkingView(APlayerPawn* pawn) {
    if (!pawn) return false;
    if(broomPlayer(pawn))return false;
    const TCHAR* state = *playerState(pawn);
    return appStricmp(state, TEXT("PlayerWalking")) == 0 ||
        appStricmp(state, TEXT("PlayerSwimming")) == 0;
}
bool activeFlight(APlayerPawn* pawn){return broomPlayer(pawn)&&!appStricmp(*playerState(pawn),TEXT("PlayerWalking"));}

bool frontEnd(UViewport* viewport) {
    if (!viewport || !viewport->Actor) return false;
    const TCHAR* level = viewport->Actor->GetOuter()->GetName();
    return appStricmp(level, TEXT("Startup")) == 0 ||
        appStricmp(level, TEXT("Entry")) == 0;
}

void synchronizeBodyForward(APlayerPawn* actor, uint32_t generation, bool force) {
    if (!actor) return;
    if (actor != bodyActor || generation != bodyTrackingGeneration) {
        bodyActor = actor;
        bodyYaw = actor->Rotation.Yaw;
        bodyTrackingGeneration = generation;
        bodyTurn.fraction = 0.0f;
        line("HP2 authoritative forward synchronized actor=%ls yaw=%d generation=%u",
            actor->GetName(), bodyYaw, generation);
    }
    if (force && walkingView(actor)) {
        actor->Rotation.Yaw = bodyYaw;
        actor->DesiredRotation.Yaw = bodyYaw;
        UStructProperty* view = FindField<UStructProperty>(actor->GetClass(), TEXT("ViewRotation"));
        static bool offsetsReported{};
        if (view && !offsetsReported) {
            const auto sdkOffset = reinterpret_cast<BYTE*>(&actor->ViewRotation) - reinterpret_cast<BYTE*>(actor);
            line("HP2 ViewRotation runtime offset=%d SDK offset=%d (runtime offset used)", view->Offset, static_cast<int>(sdkOffset));
            offsetsReported = true;
        }
        if (view && view->ElementSize == sizeof(FRotator))
            reinterpret_cast<FRotator*>(reinterpret_cast<BYTE*>(actor) + view->Offset)->Yaw = bodyYaw;
    }
}

void resolveCaptureExports() {
    if (captureEye && captureSequence && uploadPanel) return;
    HMODULE proxy = GetModuleHandleW(L"d3d11.dll");
    if (proxy) {
        captureEye = reinterpret_cast<CaptureEyeFn>(
            GetProcAddress(proxy, "HP2VR_D3D11CaptureEye"));
        captureSequence = reinterpret_cast<CaptureSequenceFn>(
            GetProcAddress(proxy, "HP2VR_D3D11CaptureSequence"));
        uploadPanel = reinterpret_cast<UploadPanelFn>(
            GetProcAddress(proxy, "HP2VR_D3D11UploadPanel"));
        readRenderPixels = reinterpret_cast<ReadPixelsFn>(
            GetProcAddress(proxy, "HP2VR_D3D11ReadPixels"));
    }
    if (!captureLookupReported) {
        captureLookupReported = true;
        line("HP2 D3D11 capture exports: eye=%p sequence=%p panel=%p",
            captureEye, captureSequence, uploadPanel);
    }
}

bool initializeBridge() {
    if (bridge) return true;
    bridgeMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(HP2VR::SharedState), HP2VR::MappingName);
    if (!bridgeMapping) return false;
    const bool fresh = GetLastError() != ERROR_ALREADY_EXISTS;
    bridge = static_cast<HP2VR::SharedState*>(MapViewOfFile(bridgeMapping,
        FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HP2VR::SharedState)));
    if (!bridge) return false;
    if (fresh || !HP2VR::compatible(*bridge)) {
        ZeroMemory(bridge, sizeof(*bridge));
        bridge->magic = HP2VR::BridgeMagic;
        bridge->version = HP2VR::BridgeVersion;
        bridge->bytes = sizeof(*bridge);
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        bridge->qpcFrequency = static_cast<uint64_t>(frequency.QuadPart);
    }
    bridge->gamePid = GetCurrentProcessId();
    bridge->gameHeartbeatQpc = qpc();
    return true;
}

void closeBridge() {
    if (bridge) {
        bridge->gamePid = 0;
        bridge->gameHeartbeatQpc = qpc();
        UnmapViewOfFile(bridge);
        bridge = nullptr;
    }
    if (bridgeMapping) { CloseHandle(bridgeMapping); bridgeMapping = nullptr; }
}

void signalCompanionShutdown() {
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, HP2VR::ShutdownEventName);
    if (event) {
        SetEvent(event);
        CloseHandle(event);
        line("HP2 companion shutdown signaled before renderer teardown");
    }
}

UObject* reflectedObject(UObject* owner, const TCHAR* propertyName) {
    if (!owner) return nullptr;
    UObjectProperty* property = FindField<UObjectProperty>(owner->GetClass(), propertyName);
    return property ? *reinterpret_cast<UObject**>(
        reinterpret_cast<BYTE*>(owner) + property->Offset) : nullptr;
}

bool invokeNoParameters(UObject* target, const TCHAR* functionName) {
    if (!target) return false;
    UFunction* function = target->FindFunction(FName(functionName));
    if (!function || function->ParmsSize != 0) return false;
    target->ProcessEvent(function, nullptr);
    return true;
}

void nativeMouseRelease(UViewport* viewport, bool released) {
    // HP2's HPConsole.KeyEvent sets this on IK_LeftMouse press/release.
    // UEngine::InputEvent alone does not traverse that console event.
    auto* console = viewport ? viewport->Console : nullptr;
    auto* p = console ? FindField<UBoolProperty>(console->GetClass(), TEXT("bSpaceReleased")) : nullptr;
    if (p) {
        auto* word = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(console)+p->Offset);
        if (released) *word |= p->BitMask; else *word &= ~p->BitMask;
        line("HP2 native cast release flag=%d", released);
    }
}

bool launchNativeBook(UViewport* viewport) {
    if (!viewport || !viewport->Console) return false;
    UObject* book = reflectedObject(viewport->Console, TEXT("menuBook"));
    // HP2's own entry point selects/shows InGamePage and respects bLocked.
    // MAIN is intentionally not shown by ChangePage and resets bGamePlaying.
    const bool invoked = invokeNoParameters(book, TEXT("EscFromConsole"));
    line("HP2 native FEBook.EscFromConsole invoked=%d", invoked ? 1 : 0);
    return invoked && viewport->bShowWindowsMouse != 0;
}

bool closeNativeBook(UViewport* viewport) {
    return viewport && viewport->Console &&
        invokeNoParameters(reflectedObject(viewport->Console, TEXT("menuBook")), TEXT("CloseBook"));
}
}

#include "HP2ClimbRecovery.h"
namespace HP2Vendor {static bool active();static UObject* manager();static int choice();static void consumed();}
static bool cinematicBodyAbsent{};
static bool bodyView(APlayerPawn* pawn) {
    return pawn && (activeFlight(pawn)||HP2Climb::mounting(pawn)||HP2Vendor::active()||(!cinematicBodyAbsent&&(walkingView(pawn)||HP2Settings::bodyOnly)));
}
#include "HP2TrackedControls.h"
#include "HP2Vendor.h"
#include "HP2Avatar.h"

class UHP2VRRender : public URender {
    DECLARE_CLASS(UHP2VRRender, URender, CLASS_Config, HP2VREngine)
public:
    UHP2VRRender() : URender() {
        line("HP2 VR render constructed; size=%u", static_cast<unsigned>(sizeof(URender)));
    }
    void StaticConstructor() { URender::StaticConstructor(); }
    void DrawWorld(FSceneNode* frame) override {
        HP2Tracked::RunePresentation rune;
        HP2Tracked::CarryPresentation carry;
        URender::DrawWorld(frame);
        auto* rider=frame->Viewport->Actor;
        if(pairTracked&&currentEye>=0&&broomPlayer(rider)&&bodyView(rider)){
            static ULevel* broomLevel{};static AActor* broom{};static bool tried{};
            if(broomLevel!=rider->GetLevel()){broomLevel=rider->GetLevel();broom=nullptr;tried=false;}
            if(!broom&&!tried){tried=true;
                // Load the packaged script class, not an unlinked native class
                // or FindObject on an asset whose class export is still lazy.
                auto* cls=LoadClass<AActor>(nullptr,TEXT("HProps.skBroomQudditch"),nullptr,LOAD_NoWarn,nullptr);
                if(cls){broom=broomLevel->SpawnActor(cls);if(broom){broom->SetFlags(RF_Transient);broom->bHidden=1;broom->bCollideActors=0;broom->bCollideWorld=0;broom->bBlockActors=0;broom->bBlockPlayers=0;broom->Physics=PHYS_None;broom->AmbientGlow=100;line("HP2 packaged broom-only presentation created");}}
                if(!broom)line("HP2 packaged broom-only actor unavailable");
            }
            if(broom&&!broom->bDeleteMe){broom->Location=HP2Tracked::anchor(rider)-FVector(0,0,.65f*WorldUnitsPerMetre);broom->Rotation=FRotator(0,rider->Rotation.Yaw,0);broom->bHidden=0;URender::DrawActor(frame,broom);broom->bHidden=1;}
        }
        HP2Avatar::draw(frame);
        HP2Tracked::draw(frame);
        HP2Vendor::draw(frame);
    }
    FSceneNode* CreateMasterFrame(UViewport* viewport, FVector location,
                                  FRotator rotation, FScreenBounds* bounds) override {
        static bool reported = false;
        if (!reported && viewport) {
            reported = true;
            line("HP2 render frame observed: viewport=%dx%d actor=%ls class=%ls",
                viewport->SizeX, viewport->SizeY,
                viewport->Actor ? viewport->Actor->GetName() : TEXT("None"),
                viewport->Actor ? viewport->Actor->GetClass()->GetName() : TEXT("None"));
        }
        const FVector originalLocation = location;
        const FRotator originalRotation = rotation;
        if (currentEye >= 0 && currentEye < 2 && pairTracked) {
            // Ordinary locomotion is anchored at Harry's eyes, not BaseCam's
            // third-person boom. Special native camera states remain untouched.
            if (viewport && bodyView(viewport->Actor)) {
                auto* pawn = viewport->Actor;
                auto* eyeHeight = FindField<UFloatProperty>(pawn->GetClass(), TEXT("BaseEyeHeight"));
                if (eyeHeight && eyeHeight->ElementSize == sizeof(FLOAT)) {
                    const FLOAT height = *reinterpret_cast<FLOAT*>(reinterpret_cast<BYTE*>(pawn) + eyeHeight->Offset);
                    if (std::isfinite(height) && height > 0.0f && height < 200.0f) {
                        location = pawn->Location;
                        location.Z += height + (broomPlayer(pawn)?Max(0.f,HP2Settings::eyeOffset):HP2Settings::eyeOffset) * .01f * WorldUnitsPerMetre + comfortOffset;
                        if(HP2Climb::mounting(pawn))location.Z+=Max(0.f,HP2Climb::visualHalfHeight(pawn)-pawn->CollisionHeight);
                        rotation = FRotator(0, bodyYaw, 0);
                        static bool reported{};
                        if (!reported) {
                            line("HP2 reflected body eye: BaseEyeHeight offset=%d value=%.3f collisionHalf=%.3f", eyeHeight->Offset, height, pawn->CollisionHeight);
                            reported = true;
                        }
                    }
                }
            }
            const float* head = pairInput.head.m;
            const float* eye = pairInput.eyeToHead[currentEye].m;
            if (!haveOrigin || originGeneration != pairInput.head.trackingGeneration) {
                originHead[0] = head[3];
                originHead[1] = head[7];
                originHead[2] = head[11];
                originYaw = trackingFacingYaw(head);
                originGeneration = pairInput.head.trackingGeneration;
                haveOrigin = true;
                line("HP2 tracking origin reset generation=%u yaw=%.6f head=%.3f,%.3f,%.3f",
                    originGeneration, originYaw, originHead[0], originHead[1], originHead[2]);
            }

            // Treat HP2's camera as the body-space anchor.  These values are
            // parameters passed by value, so no player/BaseCam/spell state is mutated.
            const FCoords basis = GMath.UnitCoords / rotation;
            const float x = head[3] - originHead[0] +
                head[0] * eye[3] + head[1] * eye[7] + head[2] * eye[11];
            const float y = head[7] - originHead[1] +
                head[4] * eye[3] + head[5] * eye[7] + head[6] * eye[11];
            const float z = head[11] - originHead[2] +
                head[8] * eye[3] + head[9] * eye[7] + head[10] * eye[11];
            location += basisVector(rotateYaw(trackingVector(x, y, z), -originYaw), basis) *
                WorldUnitsPerMetre;

            auto eyeAxis = [&](int column, float sign) {
                float value[3]{};
                for (int row = 0; row < 3; ++row)
                    for (int index = 0; index < 3; ++index)
                        value[row] += head[row * 4 + index] * eye[index * 4 + column] * sign;
                return basisVector(rotateYaw(
                    trackingVector(value[0], value[1], value[2]), -originYaw), basis);
            };
            FVector forward = eyeAxis(2, -1.0f);
            const FVector right = eyeAxis(0, 1.0f);
            const FVector up = eyeAxis(1, 1.0f);
            rotation = forward.Rotation();
            rotation.Roll = appRound(std::atan2(-right.Z, up.Z) * (65536.0f / TwoPi));
        }

        FSceneNode* result = URender::CreateMasterFrame(viewport, location, rotation, bounds);
        if (currentEye >= 0 && currentEye < 2 && pairTracked && result) {
            const float* raw = pairInput.eyeTangents[currentEye];
            static bool projectionReported[2]{};
            if (!projectionReported[currentEye]) {
                projectionReported[currentEye] = true;
                line("HP2 centered stereo eye=%d camera %.2f,%.2f,%.2f -> %.2f,%.2f,%.2f "
                     "rotation %d,%d,%d -> %d,%d,%d crop %.4f,%.4f,%.4f,%.4f",
                    currentEye, originalLocation.X, originalLocation.Y, originalLocation.Z,
                    location.X, location.Y, location.Z,
                    originalRotation.Pitch, originalRotation.Yaw, originalRotation.Roll,
                    rotation.Pitch, rotation.Yaw, rotation.Roll,
                    raw[0], raw[1], raw[2], raw[3]);
            }
        }
        return result;
    }
};

class UHP2VRGameEngine : public UGameEngine {
    DECLARE_CLASS(UHP2VRGameEngine, UGameEngine, CLASS_Config, HP2VREngine)
    BYTE nativeTail[52];
public:
    UHP2VRGameEngine() : UGameEngine() {}
    void StaticConstructor() { UGameEngine::StaticConstructor(); }
    void Init() override {
        HP2Settings::load();
        line("HP2 VR engine init: SDK=%u retail=%d subclass=%d",
            static_cast<unsigned>(sizeof(UGameEngine)),
            UGameEngine::StaticClass()->GetPropertiesSize(), GetClass()->GetPropertiesSize());
        line("HP2 bridge game side initialized=%d", initializeBridge() ? 1 : 0);
        // The Windows front end recognizes only its original renderers and can
        // replace the config with SoftDrv. Reassert the VR renderer in the live
        // config cache before UGameEngine constructs the viewport/renderer.
        GConfig->SetString(TEXT("Engine.Engine"), TEXT("GameRenderDevice"), TEXT("D3D11Drv.D3D11RenderDevice"));
        GConfig->SetString(TEXT("Engine.Engine"), TEXT("WindowedRenderDevice"), TEXT("D3D11Drv.D3D11RenderDevice"));
        GConfig->SetString(TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("D3D11Drv.D3D11RenderDevice"));
        // The native front end also writes its low-resolution selection after
        // the launcher's INI repair. Set the live cache before viewport creation.
        const TCHAR* width=HP2Settings::quality==0?TEXT("1280"):HP2Settings::quality==1?TEXT("1600"):TEXT("1920");
        const TCHAR* height=HP2Settings::quality==0?TEXT("960"):HP2Settings::quality==1?TEXT("1200"):TEXT("1440");
        GConfig->SetString(TEXT("WinDrv.WindowsClient"), TEXT("WindowedViewportX"), width);
        GConfig->SetString(TEXT("WinDrv.WindowsClient"), TEXT("WindowedViewportY"), height);
        GConfig->SetString(TEXT("WinDrv.WindowsClient"), TEXT("FullscreenViewportX"), width);
        GConfig->SetString(TEXT("WinDrv.WindowsClient"), TEXT("FullscreenViewportY"), height);
        GConfig->SetString(TEXT("WinDrv.WindowsClient"), TEXT("WindowedColorBits"), TEXT("32"));
        GConfig->SetString(TEXT("WinDrv.WindowsClient"), TEXT("LowDetailTextures"), TEXT("False"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("Anisotropy"), TEXT("16"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("Antialiasing"), TEXT("4"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("Tesselation"), HP2Settings::tessellation?TEXT("True"):TEXT("False"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("TessDefaultFactor"), HP2Settings::tessellation?TEXT("0.15"):TEXT("0"));
        line("HP2 tessellation experiment: enabled=%d factor=%.2f",int(HP2Settings::tessellation),HP2Settings::tessellation?.15f:0.f);
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("AlphaToCoverage"), TEXT("True"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("FullMeshLOD"), TEXT("True"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("DetailTextures"), TEXT("True"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("LODBias"), TEXT("0"));
        GConfig->SetString(TEXT("D3D11Drv.D3D11RenderDevice"), TEXT("DisableTextureFiltering"), TEXT("False"));
        line("HP2 VR quality: %lsx%ls viewport, requested MSAA=4, alpha coverage, full mesh LOD, detail textures, anisotropy=16, LOD bias=0",width,height);
        UGameEngine::Init();
    }
    void Tick(FLOAT deltaSeconds) override {
        // Draw's cached viewport may have been closed by a pumped Windows
        // message or replaced during travel. Never dispatch into that cache.
        inputViewport = Client && Client->Viewports.Num() ? Client->Viewports(0) : nullptr;
        if (inputViewport && (!inputViewport->Actor || !inputViewport->Input)) inputViewport = nullptr;
        if (!inputViewport) { for (auto& held : inputHeld) held = false; }
        bool live{};
        if (bridge) {
            bridge->gameHeartbeatQpc = qpc();
            live = HP2VR::readInput(*bridge, bridge->gameHeartbeatQpc, latestInput);
            if (live != inputWasLive) {
                inputWasLive = live;
                line("HP2 companion input became %s", live ? "live" : "neutral");
            }
        }
        if (inputViewport) {
            HP2Tracked::install();
            HP2Vendor::update(live,deltaSeconds);
            if(live && HP2Vendor::active())bodyYaw+=HP2Settings::turn(latestInput.turn[0],deltaSeconds,bodyTurn);
            const bool y = live && (latestInput.buttons & HP2VR::Menu);
            if (y && !menuButtonHeld) {
                if (HP2Settings::open) HP2Settings::open=false;
                else if ((latestInput.buttons & HP2VR::RightGrip) && !inputViewport->bShowWindowsMouse) {
                    HP2Settings::open=true;
                    HP2Settings::clickHeld=latestInput.pointerPressed || latestInput.trigger[1]>.65f;
                } else if(inputViewport->bShowWindowsMouse) closeNativeBook(inputViewport);
                else launchNativeBook(inputViewport);
            }
            menuButtonHeld=y;
            if(HP2Settings::open)HP2Settings::pointer(latestInput.pointerUv[0],latestInput.pointerUv[1],live&&latestInput.pointerValid,live&&latestInput.pointerPressed);
            const bool menuOwned = inputViewport->bShowWindowsMouse != 0 || HP2Settings::open;
            APlayerPawn* actor = inputViewport->Actor;
            const uint32_t generation = live ? latestInput.head.trackingGeneration : bodyTrackingGeneration;
            synchronizeBodyForward(actor, generation, !menuOwned);
            if(activeFlight(actor))bodyYaw=actor->Rotation.Yaw;
            if (!menuOwned && bodyView(actor) && live && !activeFlight(actor)) {
                bodyYaw += HP2Settings::turn(latestInput.turn[0], deltaSeconds, bodyTurn);
                synchronizeBodyForward(actor, generation, true);
            }
            HP2Tracked::update(deltaSeconds, live, menuOwned);
            if (menuOwned != menuPresentation) {
                menuPresentation = menuOwned;
                line("HP2 presentation changed to %s", menuOwned
                    ? "stationary native menu panel" : "stereo world");
            }
            const auto desired = HP2NativeInput::map(latestInput, live, menuOwned);
            // Update the cursor before dispatching the trigger edge, as V31
            // does, so the click targets this frame's hovered control.
            if (menuOwned && !HP2Settings::open && !HP2Vendor::active() && desired.pointerValid) {
                const FLOAT x = desired.pointerU * (inputViewport->SizeX - 1);
                const FLOAT y = desired.pointerV * (inputViewport->SizeY - 1);
                inputViewport->WindowsMouseX = x;
                inputViewport->WindowsMouseY = y;
                MousePosition(inputViewport, desired.pointerClick ? MOUSE_Left : 0, x, y);
            }
            const bool next[7]{desired.forward, desired.backward, desired.left,
                desired.right, menuOwned ? (!HP2Vendor::active() && !HP2Settings::open && desired.pointerClick) : (HP2Tracked::carryHeld || (HP2Tracked::active() && HP2Tracked::wand.charging)),
                desired.jump && HP2Tracked::wand.owner != 0, desired.skip};
            const EInputKey keys[7]{IK_W, IK_S, IK_A, IK_D, IK_LeftMouse,
                IK_RightMouse, IK_Enter};
            for (int index = 0; index < 7; ++index) {
                if (index == 6) {
                    // Match native Enter through HP2's own guarded fast-forward.
                    // No forced states, synthetic rewards or timer/cue clearing.
                    if (next[index] && !inputHeld[index] && !activeFlight(actor)) {
                        UObject* hud=reflectedObject(actor,TEXT("myHUD"));
                        auto* p=hud?FindField<UBoolProperty>(hud->GetClass(),TEXT("bCutSceneMode")):nullptr;
                        if(p && (*reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(hud)+p->Offset)&p->BitMask))
                            line("HP2 A native Enter-equivalent skip invoked=%d",invokeNoParameters(inputViewport->Console,TEXT("StartFastForward")));
                    }
                    inputHeld[index] = next[index];
                    continue;
                }
                if (next[index] != inputHeld[index]) {
                    if(index==4 && !menuOwned && !HP2Tracked::wand.cancel)
                        nativeMouseRelease(inputViewport,!next[index]);
                    InputEvent(inputViewport, keys[index],
                        next[index] ? IST_Press : IST_Release, 0.0f);
                    inputHeld[index] = next[index];
                }
                if (index < 4 && next[index])
                    InputEvent(inputViewport, keys[index], IST_Hold, deltaSeconds);
            }
            if (!menuOwned && desired.moveY != 0.0f && !walkingView(actor) && !HP2Climb::mounting(actor) && !HP2Climb::moving)
                InputEvent(inputViewport, IK_JoyY, IST_Axis, desired.moveY);
            if (!menuOwned && desired.moveX != 0.0f && !walkingView(actor) && !HP2Climb::mounting(actor) && !HP2Climb::moving)
                InputEvent(inputViewport, IK_JoyX, IST_Axis, desired.moveX);
            static bool inputReported{};
            if (!inputReported && live && (desired.forward || desired.backward ||
                    desired.left || desired.right || desired.cast || desired.jump ||
                    desired.skip || desired.turn != 0.0f)) {
                inputReported = true;
                line("HP2 native controller adapter active: continuous W/S/A/D + JoyY, "
                     "authoritative body-yaw turn, LeftMouse, RightMouse; A reserved for guarded speech skip");
            }
        }
        UGameEngine::Tick(deltaSeconds);
        inputViewport = Client && Client->Viewports.Num() ? Client->Viewports(0) : nullptr;
        HP2Climb::update(inputViewport ? inputViewport->Actor : nullptr, deltaSeconds);
        if(inputViewport && inputViewport->Actor){
            auto* p=inputViewport->Actor;
            const bool cut=HP2Tracked::flag(reflectedObject(p,TEXT("myHUD")),TEXT("bCutSceneMode"));
            auto* cam=Cast<AActor>(reflectedObject(p,TEXT("Cam")));
            static float remoteIdleTime{};
            bool remoteIdle=false;
            if(cut&&cam&&!appStricmp(*playerState(p),TEXT("stateCutIdle"))&&(cam->Location-p->Location).Size()>1200.f){
                FCheckResult sight(1.f);p->GetLevel()->SingleLineCheck(sight,p,p->Location,cam->Location,TRACE_AllColliding);
                remoteIdle=sight.Time<.95f;
            }
            remoteIdleTime=remoteIdle?remoteIdleTime+Clamp(deltaSeconds,0.f,.1f):0.f;
            const bool absent=cut&&!HP2Climb::mounting(p)&&!HP2Vendor::active()&&
                (p->bHidden || !p->Mesh || remoteIdleTime>.5f || (p->Region.ZoneNumber==0 && cam && (cam->Location-p->Location).Size()>1000.f));
            if(absent!=cinematicBodyAbsent)line("HP2 absent-body cinematic fallback=%d hidden=%d zone=%d",absent,p->bHidden,p->Region.ZoneNumber);
            cinematicBodyAbsent=absent;
        }else cinematicBodyAbsent=false;
        HP2Tracked::projectileSelfTest();
        if (inputViewport && inputViewport->Actor) {
            static DWORD lastReport{};
            static FName lastState(NAME_None);
            APlayerPawn* actor = inputViewport->Actor;
            const FName state = playerState(actor);
            const DWORD now = GetTickCount();
            if (state != lastState || (GetEnvironmentVariableA("HP2VR_CONTROL_TEST", nullptr, 0) && now - lastReport >= 500)) {
                line("HP2 motion level=%ls state=%ls live=%d menu=%d stick=%.3f,%.3f turn=%.3f yaw=%d body=%d pos=%.2f,%.2f,%.2f velocity=%.2f,%.2f,%.2f accel=%.2f,%.2f,%.2f",
                    actor->GetOuter()->GetName(), *state, live, inputViewport->bShowWindowsMouse,
                    latestInput.move[0], latestInput.move[1], latestInput.turn[0], actor->Rotation.Yaw, bodyYaw,
                    actor->Location.X, actor->Location.Y, actor->Location.Z,
                    actor->Velocity.X, actor->Velocity.Y, actor->Velocity.Z,
                    actor->Acceleration.X, actor->Acceleration.Y, actor->Acceleration.Z);
                lastReport = now; lastState = state;
                if(GetEnvironmentVariableA("HP2VR_CONTROL_TEST",nullptr,0))
                    line("HP2 cast state aiming=%d charged=%d fired=%d altFired=%d consoleRelease=%d",
                        HP2Tracked::flag(actor,TEXT("bIsAiming")),HP2Tracked::flag(actor,TEXT("bIsAimingWithCharge")),
                        HP2Tracked::flag(actor,TEXT("bJustFired")),HP2Tracked::flag(actor,TEXT("bJustAltFired")),
                        HP2Tracked::flag(inputViewport->Console,TEXT("bSpaceReleased")));
            }
        }
        if (inputViewport && !inputViewport->bShowWindowsMouse)
            synchronizeBodyForward(inputViewport->Actor,
                bodyTrackingGeneration, true);
    }
    void Draw(UViewport* viewport, UBOOL blit = 1, BYTE* hitData = nullptr,
              INT* hitSize = nullptr) override {
        // Present and driver work can pump a nested WinDrv paint.  Never let a
        // nested draw overwrite the eye index or the pair snapshot.
        static bool drawing{};
        if (drawing || !viewport) return;
        struct DrawScope {
            bool& active;
            explicit DrawScope(bool& value) : active(value) { active = true; }
            ~DrawScope() { active = false; }
        } drawScope(drawing);

        inputViewport = viewport;

        resolveCaptureExports();
        HP2VR::InputPayload input{};
        const bool tracked = bridge &&
            HP2VR::readInput(*bridge, qpc(), input) && HP2VR::validStereo(input);
        if (!tracked || !captureEye || !captureSequence || !uploadPanel || hitData ||
            viewport->SizeX <= 0 || viewport->SizeY <= 0) {
            currentEye = -1;
            pairTracked = false;
            UGameEngine::Draw(viewport, blit, hitData, hitSize);
            return;
        }

        pairInput = input;
        pairTracked = true;
        struct VendorDrawScope {
            UViewport* v{};UBOOL mouse{};HP2Tracked::SavedFlag bar;
            VendorDrawScope(UViewport* viewport){if(HP2Vendor::active()){v=viewport;mouse=v->bShowWindowsMouse;v->bShowWindowsMouse=0;bar.clear(v->Console,TEXT("bVendorBar"));}}
            ~VendorDrawScope(){if(v)v->bShowWindowsMouse=mouse;}
        } vendorDraw(viewport);
        // Calculate from this draw's pawn position, once for the complete eye
        // pair. A previous tick's offset cannot cancel a newly stepped camera.
        static DWORD lastComfortDraw{};
        const DWORD comfortNow=GetTickCount();
        const float comfortDt=lastComfortDraw?float(comfortNow-lastComfortDraw)*.001f:0.f;
        lastComfortDraw=comfortNow;
        auto* comfortPawn=viewport->Actor;
        if(comfortPawn!=comfortOwner){stairHeight.reset();comfortOwner=comfortPawn;}
        comfortOffset=0;
        if(comfortPawn && HP2Settings::smoothSteps && !viewport->bShowWindowsMouse && !HP2Settings::open) {
            const bool walking=walkingView(comfortPawn);
            const bool grounded=walking&&comfortPawn->Physics==PHYS_Walking;
            const bool descent=walking&&comfortPawn->Physics==PHYS_Falling&&comfortPawn->Velocity.Z<=0&&comfortPawn->Velocity.Z>-160;
            const float z=comfortPawn->Location.Z;
            comfortOffset=stairHeight.update(z,comfortDt,grounded,HP2Climb::mounting(comfortPawn)||HP2Climb::recoveredFrame,descent)-z;
        }else stairHeight.reset();
        const bool screenPresentation = HP2Settings::open || frontEnd(viewport) ||
            viewport->bShowWindowsMouse != 0;
        // As in V31, the unmodified third-person mesh cannot surround the
        // first-person camera. Hide only during drawing, never during gameplay
        // updates or native cinematic shots. A tracked body is a separate pass.
        struct NativeBodyVisibility {
            DWORD* word{}; DWORD mask{}; bool prior{};
            NativeBodyVisibility(UObject* pawn, bool hide) {
                if (!pawn || !hide) return;
                auto* property = FindField<UBoolProperty>(pawn->GetClass(), TEXT("bHidden"));
                if (!property) return;
                word = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pawn) + property->Offset);
                mask = property->BitMask; prior = (*word & mask) != 0;
                *word |= mask;
            }
            ~NativeBodyVisibility() {
                if (word) { if (prior) *word |= mask; else *word &= ~mask; }
            }
        } nativeBody(viewport->Actor, !screenPresentation && bodyView(viewport->Actor));
        struct ProjectionScope {
            FLOAT* value{};
            FLOAT saved{};
            ~ProjectionScope() { if (value) *value = saved; }
        } projection;
        if (!screenPresentation && viewport->Actor) {
            UFloatProperty* property = FindField<UFloatProperty>(
                viewport->Actor->GetClass(), TEXT("FovAngle"));
            if (property) {
                projection.value = reinterpret_cast<FLOAT*>(
                    reinterpret_cast<BYTE*>(viewport->Actor) + property->Offset);
                projection.saved = *projection.value;
                const float extent = HP2Stereo::horizontalExtent(pairInput.eyeTangents, viewport->SizeX, viewport->SizeY);
                if (extent > 0.1f && extent < 10.0f)
                    *projection.value = 2.0f * std::atan(extent) * (180.0f / Pi);
            }
        }

        bool captured[2]{};
        int sequence[2]{};
        if (HP2Settings::open) {
            currentEye=-1;
            static std::vector<FColor> pixels;
            if(HP2Settings::pixels(viewport->SizeX,viewport->SizeY,pixels)) {
                savePanelDiagnostic(pixels,viewport->SizeX,viewport->SizeY);
                captured[0]=captured[1]=uploadPanel(pixels.data(),viewport->SizeX,viewport->SizeY,viewport->SizeX*sizeof(FColor))!=0;
                sequence[0]=captureSequence(0);sequence[1]=captureSequence(1);
            }
        } else if (screenPresentation) {
            // V31 rule: front-end and menu pages are a single flat source.
            // Rendering them twice through eye cameras can lose HP2's UWindow
            // pass and exposes the empty Startup map instead of the page.
            currentEye = -1;
            UGameEngine::Draw(viewport, 0, hitData, hitSize);
            static std::vector<FColor> panelPixels;
            static DWORD lastPanelRead{};
            const DWORD now = GetTickCount();
            if (!lastPanelRead || now - lastPanelRead >= 33) {
                panelPixels.resize(static_cast<size_t>(viewport->SizeX) * viewport->SizeY);
                const bool composed = readRenderPixels &&
                    readRenderPixels(panelPixels.data(),
                        static_cast<unsigned>(viewport->SizeX),
                        static_cast<unsigned>(viewport->SizeY),
                        static_cast<unsigned>(viewport->SizeX * sizeof(FColor)));
                if (!composed) {
                    line("HP2 direct menu capture failed; no stale desktop screenshot published");
                    currentEye = -1; pairTracked = false;
                    return;
                }
                if (viewport->bShowWindowsMouse)
                    savePanelDiagnostic(panelPixels,
                        static_cast<unsigned>(viewport->SizeX),
                        static_cast<unsigned>(viewport->SizeY));
                const bool uploaded = uploadPanel(panelPixels.data(),
                    static_cast<unsigned>(viewport->SizeX),
                    static_cast<unsigned>(viewport->SizeY),
                    static_cast<unsigned>(viewport->SizeX * sizeof(FColor))) != 0;
                captured[0] = captured[1] = uploaded;
                sequence[0] = captureSequence(0);
                sequence[1] = captureSequence(1);
                if (uploaded) lastPanelRead = now;
                static bool reported{};
                if (!reported) {
                    reported = true;
                    line("HP2 menu source=%s -> shared panel upload=%d",
                        composed ? "direct D3D11 render target" : "unavailable",
                        uploaded ? 1 : 0);
                }
            }
        } else {
            for (int eye = 0; eye < 2; ++eye) {
                currentEye = eye;
                // Match V31 direct capture: never Present either eye before
                // copying it. Present can discard/rotate the right backbuffer.
                UGameEngine::Draw(viewport, 0, hitData, hitSize);
                // Flight diagnostics wait past initial level staging and only
                // capture a controllable body view, not the intro's black room.
                if (eye == 0 && ((HP2Tracked::active() && HP2Tracked::wand.charging)||HP2Vendor::active()||(activeFlight(viewport->Actor)&&pairNumber>=300)) &&
                    GetEnvironmentVariableA("HP2VR_PANEL_DUMP", nullptr, 0)) {
                    std::vector<FColor> diagnostic(viewport->SizeX * viewport->SizeY);
                    if (readRenderPixels && readRenderPixels(diagnostic.data(), viewport->SizeX, viewport->SizeY, viewport->SizeX * sizeof(FColor)))
                        savePanelDiagnostic(diagnostic, viewport->SizeX, viewport->SizeY);
                }
                captured[eye] = captureEye(eye) != 0;
                sequence[eye] = captureSequence(eye);
            }
        }
        currentEye = -1;
        pairTracked = false;

        if (captured[0] && captured[1] && sequence[0] > 0 && sequence[1] > 0) {
            HP2VR::FramePayload frame{};
            frame.renderedHead = pairInput.head;
            appMemcpy(frame.eyeTangents, pairInput.eyeTangents, sizeof(frame.eyeTangents));
            frame.captureSequence[0] = sequence[0];
            frame.captureSequence[1] = sequence[1];
            frame.width = static_cast<uint32_t>(viewport->SizeX);
            frame.height = static_cast<uint32_t>(viewport->SizeY);
            frame.pairNumber = ++pairNumber;
            frame.stereoValid = 1;
            frame.presentation = screenPresentation
                ? HP2VR::Presentation::StationaryPanel
                : HP2VR::Presentation::WorldStereo;
            HP2VR::publishFrame(*bridge, frame);
            if (pairNumber == 1 || pairNumber % 300 == 0)
                line("HP2 stereo published pair=%u size=%ux%u capture=%d,%d",
                    pairNumber, frame.width, frame.height, sequence[0], sequence[1]);
        }
    }
    void Destroy() override {
        HP2Tracked::remove();
        signalCompanionShutdown();
        UGameEngine::Destroy();
        closeBridge();
        line("HP2 VR engine destroyed cleanly");
    }
};

static_assert(sizeof(UHP2VRRender) == sizeof(URender));
static_assert(sizeof(UHP2VRGameEngine) == 276);
IMPLEMENT_PACKAGE(HP2VREngine);
IMPLEMENT_CLASS(UHP2VRRender);
IMPLEMENT_CLASS(UHP2VRGameEngine);
