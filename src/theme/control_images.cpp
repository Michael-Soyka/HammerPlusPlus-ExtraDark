//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Image lists of controls that comctl32 makes light: disabled toolbar images and
//			tree view check boxes, replaced with dark ones
//
//=============================================================================//

#include "theme/control_images.h"
#include "theme/system_colors.h"
#include "core/log.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>

static const wchar_t *const s_pwszDisabledProp   = L"hppxdark_disabled";
static const wchar_t *const s_pwszTreeChecksProp = L"hppxdark_treechecks";

// Key color the toolbar images are drawn over; pixels left in it are transparent
static const DWORD s_nKey     = 0x00FF00FF;
static const DWORD s_nRgbMask = 0x00FFFFFF;

// Disabled images: gray at 40% opacity, luminance weights in 1/256
static const DWORD s_nDisabledAlpha = 102;
static const DWORD s_nRedWeight     = 77;
static const DWORD s_nGreenWeight   = 150;
static const DWORD s_nBlueWeight    = 29;

//-----------------------------------------------------------------------------
// Toolbar disabled images: comctl32 embosses them from a mask that the dark face color breaks,
// so dimmed copies of the normal images are set instead
//-----------------------------------------------------------------------------

// Premultiplied gray at the disabled opacity, key pixels transparent
static void DimPixels( DWORD *pPixels, int nPixels )
{
	for ( int n = 0; n < nPixels; n++ )
	{
		const DWORD nColor = pPixels[ n ] & s_nRgbMask;

		if ( nColor == s_nKey )
		{
			pPixels[ n ] = 0;
			continue;
		}

		const DWORD nRed   = ( nColor >> 16 ) & 0xFF;
		const DWORD nGreen = ( nColor >> 8 ) & 0xFF;
		const DWORD nBlue  = nColor & 0xFF;
		const DWORD nGray  = ( ( nRed * s_nRedWeight ) + ( nGreen * s_nGreenWeight ) + ( nBlue * s_nBlueWeight ) ) >> 8;
		const DWORD nValue = ( nGray * s_nDisabledAlpha ) / 255;

		pPixels[ n ] = ( s_nDisabledAlpha << 24 ) | ( nValue << 16 ) | ( nValue << 8 ) | nValue;
	}
}

static void AddDimmedImage( HIMAGELIST hDisabled, HIMAGELIST hNormal, int nImage, HDC hDC, const BITMAPINFO &info )
{
	DWORD *pPixels  = nullptr;
	HBITMAP hBitmap = CreateDIBSection( hDC, &info, DIB_RGB_COLORS, reinterpret_cast<void **>( &pPixels ), nullptr, 0 );

	if ( !hBitmap )
	{
		return;
	}

	const int nPixels = info.bmiHeader.biWidth * -info.bmiHeader.biHeight;

	for ( int n = 0; n < nPixels; n++ )
	{
		pPixels[ n ] = s_nKey;
	}

	HGDIOBJ hOld = SelectObject( hDC, hBitmap );
	ImageList_Draw( hNormal, nImage, hDC, 0, 0, ILD_TRANSPARENT );
	GdiFlush();
	SelectObject( hDC, hOld );

	DimPixels( pPixels, nPixels );
	ImageList_Add( hDisabled, hBitmap, nullptr );

	DeleteObject( hBitmap );
}

static HIMAGELIST CreateDisabledImages( HIMAGELIST hNormal, int nWidth, int nHeight, int nCount )
{
	HIMAGELIST hDisabled = ImageList_Create( nWidth, nHeight, ILC_COLOR32, nCount, 0 );

	if ( !hDisabled )
	{
		return nullptr;
	}

	BITMAPINFO info              = {};
	info.bmiHeader.biSize        = sizeof( info.bmiHeader );
	info.bmiHeader.biWidth       = nWidth;
	info.bmiHeader.biHeight      = -nHeight;
	info.bmiHeader.biPlanes      = 1;
	info.bmiHeader.biBitCount    = 32;
	info.bmiHeader.biCompression = BI_RGB;

	HDC hDC = CreateCompatibleDC( nullptr );

	for ( int i = 0; i < nCount; i++ )
	{
		AddDimmedImage( hDisabled, hNormal, i, hDC, info );
	}

	DeleteDC( hDC );

	return hDisabled;
}

static void UpdateDisabledImages( HWND hToolbar, HIMAGELIST hNormal )
{
	int nWidth  = 0;
	int nHeight = 0;
	ImageList_GetIconSize( hNormal, &nWidth, &nHeight );
	const int nCount = ImageList_GetImageCount( hNormal );
	Log( "toolbar %p: images %p, %dx%d, %d", hToolbar, hNormal, nWidth, nHeight, nCount );

	if ( nWidth <= 0 || nHeight <= 0 || nCount <= 0 )
	{
		return;
	}

	HIMAGELIST hDisabled = CreateDisabledImages( hNormal, nWidth, nHeight, nCount );

	if ( !hDisabled )
	{
		return;
	}

	HIMAGELIST hPrevious = reinterpret_cast<HIMAGELIST>( SendMessageW( hToolbar, TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>( hDisabled ) ) );

	if ( hPrevious && hPrevious == GetPropW( hToolbar, s_pwszDisabledProp ) )
	{
		ImageList_Destroy( hPrevious );
	}

	SetPropW( hToolbar, s_pwszDisabledProp, hDisabled );
	InvalidateRect( hToolbar, nullptr, TRUE );

	Log( "toolbar %p: disabled %p (%d images), previous %p", hToolbar, hDisabled, ImageList_GetImageCount( hDisabled ), hPrevious );
}

void CheckDisabledImages( HWND hToolbar )
{
	HIMAGELIST hNormal   = reinterpret_cast<HIMAGELIST>( SendMessageW( hToolbar, TB_GETIMAGELIST, 0, 0 ) );
	HIMAGELIST hDisabled = static_cast<HIMAGELIST>( GetPropW( hToolbar, s_pwszDisabledProp ) );

	if ( !hNormal )
	{
		return;
	}

	if ( !hDisabled || ImageList_GetImageCount( hDisabled ) != ImageList_GetImageCount( hNormal ) )
	{
		UpdateDisabledImages( hToolbar, hNormal );
	}
}

void ReleaseDisabledImages( HWND hToolbar )
{
	HIMAGELIST hDisabled = static_cast<HIMAGELIST>( RemovePropW( hToolbar, s_pwszDisabledProp ) );

	if ( hDisabled )
	{
		ImageList_Destroy( hDisabled );
	}
}

//-----------------------------------------------------------------------------
// Tree view check boxes: comctl32 draws its state images classic once, they are replaced with
// the dark theme glyphs
//-----------------------------------------------------------------------------

static COLORREF TreeBackground( HWND hTree )
{
	const COLORREF color = TreeView_GetBkColor( hTree );

	if ( color == static_cast<COLORREF>( -1 ) )
	{
		return g_ColorWindow;
	}

	return color;
}

// One state image: the glyph centered in the cell, no glyph for state 0
static HBITMAP DrawTreeCheckBox( HTHEME hTheme, HDC hScreen, HDC hDC, int nState, const SIZE &cell, HBRUSH hBackground )
{
	HBITMAP hBitmap = CreateCompatibleBitmap( hScreen, cell.cx, cell.cy );
	HGDIOBJ hOld    = SelectObject( hDC, hBitmap );

	const RECT rect = { 0, 0, cell.cx, cell.cy };
	FillRect( hDC, &rect, hBackground );

	if ( nState )
	{
		SIZE glyph = cell;
		GetThemePartSize( hTheme, hDC, BP_CHECKBOX, nState, nullptr, TS_DRAW, &glyph );

		RECT box;
		box.left   = ( cell.cx - glyph.cx ) / 2;
		box.top    = ( cell.cy - glyph.cy ) / 2;
		box.right  = box.left + glyph.cx;
		box.bottom = box.top + glyph.cy;
		DrawThemeBackground( hTheme, hDC, BP_CHECKBOX, nState, &box, nullptr );
	}

	SelectObject( hDC, hOld );

	return hBitmap;
}

static HIMAGELIST CreateTreeCheckBoxes( HWND hTree, HTHEME hTheme, const SIZE &cell )
{
	// Blank, unchecked, checked
	const int nStates[] = { 0, CBS_UNCHECKEDNORMAL, CBS_CHECKEDNORMAL };
	HIMAGELIST hChecks  = ImageList_Create( cell.cx, cell.cy, ILC_COLOR32, ARRAYSIZE( nStates ), 0 );

	HDC hScreen        = GetDC( hTree );
	HDC hDC            = CreateCompatibleDC( hScreen );
	HBRUSH hBackground = CreateSolidBrush( TreeBackground( hTree ) );

	for ( const int nState : nStates )
	{
		HBITMAP hBitmap = DrawTreeCheckBox( hTheme, hScreen, hDC, nState, cell, hBackground );
		ImageList_Add( hChecks, hBitmap, nullptr );

		DeleteObject( hBitmap );
	}

	DeleteObject( hBackground );
	DeleteDC( hDC );
	ReleaseDC( hTree, hScreen );

	return hChecks;
}

void CheckTreeCheckBoxes( HWND hTree )
{
	if ( !( GetWindowLongPtrW( hTree, GWL_STYLE ) & TVS_CHECKBOXES ) )
	{
		return;
	}

	HIMAGELIST hCurrent = TreeView_GetImageList( hTree, TVSIL_STATE );

	if ( !hCurrent || hCurrent == GetPropW( hTree, s_pwszTreeChecksProp ) )
	{
		return;
	}

	int nWidth  = 0;
	int nHeight = 0;
	ImageList_GetIconSize( hCurrent, &nWidth, &nHeight );

	if ( nWidth <= 0 || nHeight <= 0 )
	{
		return;
	}

	const SIZE cell = { nWidth, nHeight };

	HTHEME hTheme = OpenThemeData( hTree, L"Button" );

	if ( !hTheme )
	{
		return;
	}

	HIMAGELIST hChecks = CreateTreeCheckBoxes( hTree, hTheme, cell );
	CloseThemeData( hTheme );

	// The replaced list is the one comctl32 made for the style, nobody else holds it
	SetPropW( hTree, s_pwszTreeChecksProp, hChecks );
	HIMAGELIST hPrevious = TreeView_SetImageList( hTree, hChecks, TVSIL_STATE );

	if ( hPrevious )
	{
		ImageList_Destroy( hPrevious );
	}
}

void ReleaseTreeCheckBoxes( HWND hTree )
{
	HIMAGELIST hChecks = static_cast<HIMAGELIST>( RemovePropW( hTree, s_pwszTreeChecksProp ) );

	if ( hChecks && TreeView_GetImageList( hTree, TVSIL_STATE ) == hChecks )
	{
		TreeView_SetImageList( hTree, nullptr, TVSIL_STATE );
		ImageList_Destroy( hChecks );
	}
}
