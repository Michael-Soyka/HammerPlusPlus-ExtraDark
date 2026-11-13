//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Message boxes: user32 paints their button strip itself with the light face color.
//			Dialogs created while a MessageBox call runs are marked and painted flat
//
//=============================================================================//

#pragma once

#include "core/iat_hook.h"

#include <windows.h>

// Resolves the hooked functions and writes the hooks to pHooks, returns how many
int MessageBoxes_AddHooks( ImportHook_t *pHooks );

// On creation: marks the dialog when a MessageBox call is running
void MarkMessageBox( HWND hWnd );
bool IsMessageBox( HWND hWnd );
void UnmarkMessageBox( HWND hWnd );
