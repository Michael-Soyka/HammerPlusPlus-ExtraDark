//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: The dark palette, its brushes and the system color hooks that hand it out
//
//=============================================================================//

#pragma once

#include "core/iat_hook.h"

#include <windows.h>

#include <string>

// Defaults, each one can be set in [Palette] of hppxdark.ini
extern COLORREF g_ColorFace;
extern COLORREF g_ColorWindow;
extern COLORREF g_ColorEdit;
extern COLORREF g_ColorText;
extern COLORREF g_ColorGrayText;
extern COLORREF g_ColorMenu;
extern COLORREF g_ColorMenuHot;
extern COLORREF g_ColorHighlight;
extern COLORREF g_ColorHighlightText;
extern COLORREF g_ColorLight;
extern COLORREF g_ColorHilight;
extern COLORREF g_ColorShadow;
extern COLORREF g_ColorDarkShadow;
extern COLORREF g_ColorTooltip;
extern COLORREF g_ColorHotLight;
extern COLORREF g_ColorWorkspace;
extern COLORREF g_ColorFrame;
extern COLORREF g_ColorBorder;

void LoadPalette( const std::wstring &ini );

// Brush of a dark system color (COLOR_*), null for the ones the palette leaves alone.
// Kept for the process lifetime, like the system brushes
HBRUSH DarkBrush( int nIndex );
HBRUSH BorderBrush();
HBRUSH EditBrush();
HBRUSH MenuHotBrush();

// Null or a brush of the light system colors that default handlers return
bool IsSystemBrush( HBRUSH hBrush );

// Dark brush for a class background given as a system color (COLOR_* + 1) or a system brush:
// user32 resolves those itself, past the hooks. Null for any other background
HBRUSH ClassDarkBrush( HWND hWnd );

// Resolves the hooked functions; false when user32 lacks them and nothing can be hooked
bool SystemColors_Init();

// Writes the hooks to pHooks, returns how many
int SystemColors_AddHooks( ImportHook_t *pHooks );
