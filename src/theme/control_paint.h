//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Controls drawn flat in the dark palette where the themes and the classic drawing
//			can't be made dark
//
//=============================================================================//

#pragma once

#include <windows.h>
#include <commctrl.h>

// WM_CTLCOLOR*: the dark colors unless the application chose its own brush
LRESULT CtlColor( UINT nMsg, HDC hDC, LRESULT nDefault );

// Background of a message box: text, icon and buttons are child windows
void PaintFlat( HWND hWnd );

// MFC draws splitter bars with colors it cached at startup, light ones
bool IsSplitter( HWND hWnd );
void PaintSplitter( HWND hWnd );

// After the default non-client paint
void PaintToolbarNonClient( HWND hWnd );
void PaintScrollCorner( HWND hWnd );
void PaintClientEdge( HWND hWnd );

// Hammer++ views turn their scroll bars on after creation, past the theme set on WM_CREATE
void ThemeLateScrollBars( HWND hWnd );
void ForgetLateScrollBars( HWND hWnd );

void PaintTabControl( HWND hWnd );

// After the default paint
void PaintStatusBarBorders( HWND hWnd );

bool IsGroupBox( HWND hWnd );
void PaintGroupBox( HWND hWnd );

// Group boxes of property sheet pages stretched to keep the margin on the right
void AlignGroupBoxes( HWND hDialog );

bool IsCheckButton( HWND hWnd );
void PaintCheckButton( HWND hWnd );
void PrintCheckButton( HWND hWnd, HDC hDC );

// The button draws its state changes straight to the screen, past WM_PAINT: painted again after them
void RepaintCheckButton( HWND hWnd );

void PaintUpDown( HWND hWnd );

// NM_CUSTOMDRAW of a toolbar, at its parent
LRESULT ToolbarCustomDraw( NMTBCUSTOMDRAW *pDraw );
