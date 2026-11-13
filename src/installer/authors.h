//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Authors card: slides up from the bottom over a dimmed snapshot of the window,
//			with avatars from GitHub and links to the authors' pages
//
//=============================================================================//

#pragma once

#include <windows.h>

// Posted to the window given to LoadAuthorAvatars once the downloads are over
static const UINT s_nAuthorAvatarsMessage = WM_APP + 1;

// After GdiplusStartup, before the window: nDpi scales the card
void InitAuthors( HINSTANCE hInstance, int nDpi );

// The embedded avatars at once, the GitHub ones in the background
void LoadAuthorAvatars( HWND hWnd );

// On s_nAuthorAvatarsMessage: takes the downloaded avatars over the embedded ones
void OnAuthorAvatars();

void OpenAuthors( HWND hWnd );

// Before GdiplusShutdown
void FreeAuthorAvatars();
