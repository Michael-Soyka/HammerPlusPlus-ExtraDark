//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Bitmaps of hammerplusplus_dll.dll replaced by the ones built into hppxdark.dll
//			(src/assets/Bitmap<id>.bmp, same resource ids)
//
//=============================================================================//

#pragma once

#include <windows.h>

// Resolves the hooked functions; call before Icons_HookModule. Generated bitmaps are drawn over background
void Icons_Init( COLORREF background );

// Redirects the bitmap resource loading of the Hammer++ DLL to hppxdark.dll, returns hooked imports
int Icons_HookModule( HMODULE hHammer );

// Image lists of comctl32 loading the Hammer++ check box strip get the generated one, returns hooked imports
int Icons_HookComctl( HMODULE hComctl );
