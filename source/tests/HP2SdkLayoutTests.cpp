#include <windows.h>
#include <Engine.h>
#include <Render.h>

// Compile-only ABI sentinels.  Do not turn this into a standalone executable:
// loading HP2's retail Core.dll outside Game.exe runs engine initialization and
// hangs before main.  The values below were independently obtained with MSVC's
// /d1reportSingleClassLayout switch using the same x86 packing flags.
static_assert(sizeof(void*) == 4, "HP2 game-side SDK tests must be x86");
static_assert(sizeof(UGameEngine) == 224, "HP2 SDK UGameEngine layout changed");
static_assert(sizeof(URender) == 280, "HP2 SDK URender layout changed");
static_assert(sizeof(UViewport) == 240, "HP2 SDK UViewport layout changed");
static_assert(sizeof(APlayerPawn) == 2344, "HP2 SDK APlayerPawn layout changed");
