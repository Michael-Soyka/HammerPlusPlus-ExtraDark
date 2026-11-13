//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Menu bar through the undocumented UAH messages of user32
//
//=============================================================================//

#pragma once

#include <windows.h>

#define WM_UAHDRAWMENU		0x0091
#define WM_UAHDRAWMENUITEM	0x0092

// WM_UAHDRAWMENU: the bar background
void MenuBar_DrawBackground( HWND hWnd, LPARAM lParam );

// WM_UAHDRAWMENUITEM: false when user32 has to draw the item
bool MenuBar_DrawItem( HWND hWnd, LPARAM lParam );

// After the default non-client paint: what user32 draws there past the UAH messages
void MenuBar_PaintNonClient( HWND hWnd );
