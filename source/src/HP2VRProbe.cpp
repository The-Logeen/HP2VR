#include <windows.h>
#include <Engine.h>
#include <Render.h>
#include <cstdarg>
#include <cstdio>

namespace {
FILE* trace{};

void line(const char* format, ...) {
    if (!trace) fopen_s(&trace, "HP2VRProbe.log", "w");
    if (!trace) return;
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(trace, format, arguments);
    va_end(arguments);
    std::fputc('\n', trace);
    std::fflush(trace);
}

UClass* findClass(const TCHAR* name) {
    return FindObject<UClass>(ANY_PACKAGE, name);
}

void reportField(UClass* owner, const TCHAR* name) {
    UProperty* property = owner ? FindField<UProperty>(owner, name) : nullptr;
    if (!property) {
        line("  field %ls: absent", name);
        return;
    }
    line("  field %ls: type=%ls offset=%d element=%d array=%d",
        name, property->GetClass()->GetName(), property->Offset,
        property->ElementSize, property->ArrayDim);
}

void reportClass(const TCHAR* name, const TCHAR* const* fields, size_t fieldCount) {
    UClass* value = findClass(name);
    if (!value) {
        line("class %ls: not loaded yet", name);
        return;
    }
    line("class %ls: runtimeSize=%d super=%ls", name, value->GetPropertiesSize(),
        value->GetSuperClass() ? value->GetSuperClass()->GetName() : TEXT("None"));
    for (size_t i = 0; i < fieldCount; ++i) reportField(value, fields[i]);
}

void reportViewport() {
    int count = 0;
    for (TObjectIterator<UViewport> it; it; ++it) {
        UViewport* viewport = *it;
        APlayerPawn* actor = viewport ? viewport->Actor : nullptr;
        line("viewport[%d]: %ls size=%dx%d actor=%ls class=%ls level=%ls",
            count++, viewport->GetName(), viewport->SizeX, viewport->SizeY,
            actor ? actor->GetName() : TEXT("None"),
            actor ? actor->GetClass()->GetName() : TEXT("None"),
            actor && actor->GetOuter() ? actor->GetOuter()->GetName() : TEXT("None"));
    }
    if (!count) line("viewport: none present at D3D11 device creation");
}

int objectCount() {
    int count = 0;
    for (FObjectIterator it; it; ++it) ++count;
    return count;
}
}

extern "C" __declspec(dllexport) int WINAPI HP2VR_InitializeProbe() {
    __try {
        line("HP2 live ABI probe v1 (read-only)");
        line("compile sizes: UGameEngine=%u URender=%u UViewport=%u APlayerPawn=%u",
            static_cast<unsigned>(sizeof(UGameEngine)), static_cast<unsigned>(sizeof(URender)),
            static_cast<unsigned>(sizeof(UViewport)), static_cast<unsigned>(sizeof(APlayerPawn)));
        line("runtime sizes: UGameEngine=%d URender=%d UViewport=%d APlayerPawn=%d objects=%d",
            UGameEngine::StaticClass()->GetPropertiesSize(), URender::StaticClass()->GetPropertiesSize(),
            UViewport::StaticClass()->GetPropertiesSize(), APlayerPawn::StaticClass()->GetPropertiesSize(),
            objectCount());

        static const TCHAR* harryFields[] = {
            TEXT("Cam"), TEXT("SpellCursor"), TEXT("bIsAiming"),
            TEXT("bIsAimingWithCharge"), TEXT("bLockedOnTarget"), TEXT("bMoveWhileCasting"),
            TEXT("bAutoCenterCamera"), TEXT("aBroomYaw"), TEXT("aBroomPitch"),
            TEXT("aJoyBroomYaw"), TEXT("aJoyBroomPitch"), TEXT("aSpellLessonX"),
            TEXT("aSpellLessonY"), TEXT("AimRotOffset"), TEXT("focusActor"), TEXT("Weapon")
        };
        static const TCHAR* cameraFields[] = {
            TEXT("CamTarget"), TEXT("CurrentSet"), TEXT("CameraMode"),
            TEXT("rCurrRotation"), TEXT("rDestRotation"), TEXT("vCurrPosition"),
            TEXT("vDestPosition"), TEXT("fMouseDeltaX"), TEXT("fMouseDeltaY")
        };
        static const TCHAR* broomFields[] = {
            TEXT("fPitchControl"), TEXT("fYawControl"), TEXT("fMousePitch"),
            TEXT("fMouseYaw"), TEXT("ePitchControlDevice"), TEXT("eYawControlDevice"),
            TEXT("WatchTarget"), TEXT("KickTarget"), TEXT("bAuxBoost")
        };
        static const TCHAR* cursorFields[] = {
            TEXT("PlayerHarry"), TEXT("aCurrentTarget"), TEXT("aPossibleTarget"),
            TEXT("vGestureOffset"), TEXT("vLOS_End"), TEXT("fLOS_Distance")
        };
        reportClass(TEXT("harry"), harryFields, sizeof(harryFields) / sizeof(harryFields[0]));
        reportClass(TEXT("BaseCam"), cameraFields, sizeof(cameraFields) / sizeof(cameraFields[0]));
        reportClass(TEXT("BroomHarry"), broomFields, sizeof(broomFields) / sizeof(broomFields[0]));
        reportClass(TEXT("SpellCursor"), cursorFields, sizeof(cursorFields) / sizeof(cursorFields[0]));
        reportClass(TEXT("baseWand"), nullptr, 0);
        reportClass(TEXT("SpellLessonTrigger"), nullptr, 0);
        reportClass(TEXT("HPHud"), nullptr, 0);
        reportViewport();
        line("probe complete; no engine or object state was changed");
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        line("probe trapped exception 0x%08lx", static_cast<unsigned long>(GetExceptionCode()));
        return 0;
    }
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH && trace) {
        std::fclose(trace);
        trace = nullptr;
    }
    return TRUE;
}
