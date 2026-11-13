//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Window class and geometry helpers shared by the theme code
//
//=============================================================================//

#pragma once

#include <windows.h>

#include <string>

// Empty when the class can't be read
std::wstring ClassName( HWND hWnd );

bool IsClass( HWND hWnd, const wchar_t *pwszClass );
bool ClassStartsWith( HWND hWnd, const wchar_t *pwszPrefix );

// A dialog inside another window: property sheet pages, Hammer++ panels
bool IsChildDialog( HWND hWnd );

// Window rectangle of a child in its parent's client coordinates
RECT ChildRect( HWND hParent, HWND hChild );

// In the coordinates of the window DC: the whole window from 0,0 and its client area
RECT WindowRectAtOrigin( HWND hWnd );
RECT ClientRectInWindow( HWND hWnd );
