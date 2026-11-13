//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Image lists of controls that comctl32 makes light: disabled toolbar images and
//			tree view check boxes, replaced with dark ones
//
//=============================================================================//

#pragma once

#include <windows.h>

// Before each paint: MFC fills toolbars past the subclass chain
void CheckDisabledImages( HWND hToolbar );

// On WM_NCDESTROY
void ReleaseDisabledImages( HWND hToolbar );

// Before each paint: the TVS_CHECKBOXES style can come after the creation
void CheckTreeCheckBoxes( HWND hTree );

// On WM_DESTROY, before the tree destroys its state images itself
void ReleaseTreeCheckBoxes( HWND hTree );
