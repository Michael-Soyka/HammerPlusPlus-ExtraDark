//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Bitmaps of hammerplusplus_dll.dll replaced by the ones built into hppxdark.dll
//			(src/assets/Bitmap<id>.bmp, same resource ids)
//
//=============================================================================//

#pragma once

#include <windows.h>

// Resolves the hooked functions; call before Icons_HookModule
void Icons_Init();

// Redirects the bitmap resource loading of the Hammer++ DLL to hppxdark.dll, returns hooked imports
int Icons_HookModule( HMODULE hHammer );
