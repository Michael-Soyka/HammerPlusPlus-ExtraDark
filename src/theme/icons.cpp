//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Bitmaps and icons of hammerplusplus_dll.dll replaced by the ones built into hppxdark.dll
//			(src/assets/Bitmap<id>.bmp and Icon<id>.ico, same resource ids)
//
//=============================================================================//

#include "theme/icons.h"
#include "core/gdi.h"
#include "core/iat_hook.h"

#include <uxtheme.h>
#include <vssym32.h>

// RT_BITMAP and RT_GROUP_ICON as numbers: the project builds without UNICODE
static const ULONG_PTR s_nBitmapType = 2;
static const ULONG_PTR s_nIconType   = 14;

static HMODULE s_hSelf   = nullptr;
static HMODULE s_hHammer = nullptr;

// Resources handed out from hppxdark.dll: LoadResource and SizeofResource get them with our module
static HRSRC s_hOwnResources[ 64 ];
static int s_nOwnResources = 0;

using FindResourceA_t   = HRSRC ( WINAPI * )( HMODULE, LPCSTR, LPCSTR );
using FindResourceW_t   = HRSRC ( WINAPI * )( HMODULE, LPCWSTR, LPCWSTR );
using FindResourceExW_t = HRSRC ( WINAPI * )( HMODULE, LPCWSTR, LPCWSTR, WORD );
using LoadResource_t    = HGLOBAL ( WINAPI * )( HMODULE, HRSRC );
using SizeofResource_t  = DWORD ( WINAPI * )( HMODULE, HRSRC );
using LoadBitmapW_t     = HBITMAP ( WINAPI * )( HINSTANCE, LPCWSTR );
using LoadImageA_t      = HANDLE ( WINAPI * )( HINSTANCE, LPCSTR, UINT, int, int, UINT );
using LoadImageW_t      = HANDLE ( WINAPI * )( HINSTANCE, LPCWSTR, UINT, int, int, UINT );
using LoadIconA_t       = HICON ( WINAPI * )( HINSTANCE, LPCSTR );
using LoadIconW_t       = HICON ( WINAPI * )( HINSTANCE, LPCWSTR );

static FARPROC s_pFindResourceA   = nullptr;
static FARPROC s_pFindResourceW   = nullptr;
static FARPROC s_pFindResourceExW = nullptr;
static FARPROC s_pLoadResource    = nullptr;
static FARPROC s_pSizeofResource  = nullptr;
static FARPROC s_pLoadBitmapW     = nullptr;
static FARPROC s_pLoadImageA      = nullptr;
static FARPROC s_pLoadImageW      = nullptr;
static FARPROC s_pLoadIconA       = nullptr;
static FARPROC s_pLoadIconW       = nullptr;

// Visgroup tree check boxes: unchecked, unchecked, checked, partly checked
static const ULONG_PTR s_nCheckStrip = 223;
static const int s_nCheckCell        = 16;
static const int s_nCheckStates[]    = { CBS_UNCHECKEDNORMAL, CBS_UNCHECKEDNORMAL, CBS_CHECKEDNORMAL, CBS_MIXEDNORMAL };

// Image list mask color of the strip, white: the cell outside the glyph. Inside the glyphs
// the white pixels become the nearest other color
static const COLORREF s_CheckMask   = RGB( 255, 255, 255 );
static const DWORD s_nMaskPixel     = 0x00FFFFFF;
static const DWORD s_nNearMaskPixel = 0x00FEFEFE;

static COLORREF s_Background = RGB( 30, 30, 30 );

//-----------------------------------------------------------------------------
// Own resources
//-----------------------------------------------------------------------------

static bool IsHammer( HMODULE hModule )
{
	return s_hHammer && hModule == s_hHammer;
}

static HRSRC FindSelfResource( ULONG_PTR nName, ULONG_PTR nType )
{
	return reinterpret_cast<FindResourceW_t>( s_pFindResourceW )( s_hSelf, MAKEINTRESOURCEW( nName ), MAKEINTRESOURCEW( nType ) );
}

static bool IsOwnResource( HRSRC hResource )
{
	for ( int i = 0; i < s_nOwnResources; i++ )
	{
		if ( s_hOwnResources[ i ] == hResource )
		{
			return true;
		}
	}

	return false;
}

static void RememberOwnResource( HRSRC hResource )
{
	if ( hResource && !IsOwnResource( hResource ) && s_nOwnResources < ARRAYSIZE( s_hOwnResources ) )
	{
		s_hOwnResources[ s_nOwnResources++ ] = hResource;
	}
}

// Our bitmap for a request to the Hammer++ DLL by numeric id, null for anything else
static HRSRC FindOwnBitmap( HMODULE hModule, ULONG_PTR nName, ULONG_PTR nType )
{
	if ( !IsHammer( hModule ) || !IS_INTRESOURCE( nName ) || nType != s_nBitmapType )
	{
		return nullptr;
	}

	HRSRC hResource = FindSelfResource( nName, s_nBitmapType );
	RememberOwnResource( hResource );

	return hResource;
}

// Our icon for a request to the Hammer++ DLL by numeric id: user32 then reads it from our module
static bool HasOwnIcon( HMODULE hModule, ULONG_PTR nName )
{
	return IsHammer( hModule ) && IS_INTRESOURCE( nName ) && FindSelfResource( nName, s_nIconType );
}

// This DLL for its own resources, the caller's module otherwise
static HMODULE ResourceModule( bool bOwn, HMODULE hModule )
{
	if ( bOwn )
	{
		return s_hSelf;
	}

	return hModule;
}

// Module LoadImage takes the image from: ours when we have it
static HINSTANCE ImageModule( HINSTANCE hInstance, ULONG_PTR nName, UINT nType, UINT nFlags )
{
	if ( nFlags & LR_LOADFROMFILE )
	{
		return hInstance;
	}

	const bool bOwn = ( nType == IMAGE_BITMAP && FindOwnBitmap( hInstance, nName, s_nBitmapType ) ) || ( nType == IMAGE_ICON && HasOwnIcon( hInstance, nName ) );

	return ResourceModule( bOwn, hInstance );
}

//-----------------------------------------------------------------------------
// Visgroup check box strip, drawn by the dark theme
//-----------------------------------------------------------------------------

static RECT CheckGlyphBox( int nCell, const SIZE &glyph )
{
	RECT box;
	box.left   = ( nCell * s_nCheckCell ) + ( ( s_nCheckCell - glyph.cx ) / 2 );
	box.top    = ( s_nCheckCell - glyph.cy ) / 2;
	box.right  = box.left + glyph.cx;
	box.bottom = box.top + glyph.cy;

	return box;
}

static SIZE CheckGlyphSize( HTHEME hTheme, HDC hDC )
{
	SIZE glyph = { 13, 13 };
	GetThemePartSize( hTheme, hDC, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, nullptr, TS_DRAW, &glyph );
	glyph.cx = min( glyph.cx, static_cast<LONG>( s_nCheckCell ) );
	glyph.cy = min( glyph.cy, static_cast<LONG>( s_nCheckCell ) );

	return glyph;
}

static void FillWith( HDC hDC, const RECT &rect, COLORREF color )
{
	const GdiObject_t<HBRUSH> brush( CreateSolidBrush( color ) );
	FillRect( hDC, &rect, brush.m_hObject );
}

// Rounded corners blend into the tree background, not into the mask
static void DrawCheckGlyphs( HTHEME hTheme, HDC hDC, const SIZE &glyph )
{
	for ( int i = 0; i < ARRAYSIZE( s_nCheckStates ); i++ )
	{
		const RECT box = CheckGlyphBox( i, glyph );
		FillWith( hDC, box, s_Background );
		DrawThemeBackground( hTheme, hDC, BP_CHECKBOX, s_nCheckStates[ i ], &box, nullptr );
	}
}

static void KeepGlyphsOpaque( DWORD *pPixels, int nWidth, const SIZE &glyph )
{
	for ( int i = 0; i < ARRAYSIZE( s_nCheckStates ); i++ )
	{
		const RECT box = CheckGlyphBox( i, glyph );

		for ( LONG y = box.top; y < box.bottom; y++ )
		{
			for ( LONG x = box.left; x < box.right; x++ )
			{
				DWORD &pixel = pPixels[ ( y * nWidth ) + x ];

				if ( ( pixel & s_nMaskPixel ) == s_nMaskPixel )
				{
					pixel = s_nNearMaskPixel;
				}
			}
		}
	}
}

// Made on request: the image list takes the bitmap and deletes it, and the theme cannot be
// opened under the loader lock
static HBITMAP CreateCheckStrip()
{
	const ThemeData_t theme( nullptr, L"DarkMode_Explorer::Button" );

	if ( !theme.m_hTheme )
	{
		return nullptr;
	}

	const int nWidth = s_nCheckCell * ARRAYSIZE( s_nCheckStates );

	BITMAPINFO info              = {};
	info.bmiHeader.biSize        = sizeof( info.bmiHeader );
	info.bmiHeader.biWidth       = nWidth;
	info.bmiHeader.biHeight      = -s_nCheckCell;
	info.bmiHeader.biPlanes      = 1;
	info.bmiHeader.biBitCount    = 32;
	info.bmiHeader.biCompression = BI_RGB;

	const MemoryDC_t memory( nullptr );
	HDC hDC = memory.m_hDC;

	DWORD *pPixels  = nullptr;
	HBITMAP hBitmap = CreateDIBSection( hDC, &info, DIB_RGB_COLORS, reinterpret_cast<void **>( &pPixels ), nullptr, 0 );

	if ( !hBitmap )
	{
		return nullptr;
	}

	const SelectedObject_t selected( hDC, hBitmap );
	FillWith( hDC, { 0, 0, nWidth, s_nCheckCell }, s_CheckMask );
	const SIZE glyph = CheckGlyphSize( theme.m_hTheme, hDC );
	DrawCheckGlyphs( theme.m_hTheme, hDC, glyph );
	GdiFlush();
	KeepGlyphsOpaque( pPixels, nWidth, glyph );

	return hBitmap;
}

// The strip is generated whatever way it is loaded; null for every other image
static HANDLE LoadCheckStrip( HINSTANCE hInstance, ULONG_PTR nName, UINT nType, UINT nFlags )
{
	const bool bStrip = IsHammer( hInstance ) && nType == IMAGE_BITMAP && !( nFlags & LR_LOADFROMFILE ) && nName == s_nCheckStrip;

	if ( !bStrip )
	{
		return nullptr;
	}

	return CreateCheckStrip();
}

//-----------------------------------------------------------------------------
// Hooks of the Hammer++ DLL imports
//-----------------------------------------------------------------------------

static HRSRC WINAPI HookFindResourceA( HMODULE hModule, LPCSTR pszName, LPCSTR pszType )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pszName ), reinterpret_cast<ULONG_PTR>( pszType ) );

	if ( hResource )
	{
		return hResource;
	}

	return reinterpret_cast<FindResourceA_t>( s_pFindResourceA )( hModule, pszName, pszType );
}

static HRSRC WINAPI HookFindResourceW( HMODULE hModule, LPCWSTR pwszName, LPCWSTR pwszType )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pwszName ), reinterpret_cast<ULONG_PTR>( pwszType ) );

	if ( hResource )
	{
		return hResource;
	}

	return reinterpret_cast<FindResourceW_t>( s_pFindResourceW )( hModule, pwszName, pwszType );
}

static HRSRC WINAPI HookFindResourceExW( HMODULE hModule, LPCWSTR pwszType, LPCWSTR pwszName, WORD nLanguage )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pwszName ), reinterpret_cast<ULONG_PTR>( pwszType ) );

	if ( hResource )
	{
		return hResource;
	}

	return reinterpret_cast<FindResourceExW_t>( s_pFindResourceExW )( hModule, pwszType, pwszName, nLanguage );
}

static HGLOBAL WINAPI HookLoadResource( HMODULE hModule, HRSRC hResource )
{
	return reinterpret_cast<LoadResource_t>( s_pLoadResource )( ResourceModule( IsOwnResource( hResource ), hModule ), hResource );
}

static DWORD WINAPI HookSizeofResource( HMODULE hModule, HRSRC hResource )
{
	return reinterpret_cast<SizeofResource_t>( s_pSizeofResource )( ResourceModule( IsOwnResource( hResource ), hModule ), hResource );
}

static HBITMAP WINAPI HookLoadBitmapW( HINSTANCE hInstance, LPCWSTR pwszName )
{
	const bool bOwn = FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), s_nBitmapType ) != nullptr;

	return reinterpret_cast<LoadBitmapW_t>( s_pLoadBitmapW )( ResourceModule( bOwn, hInstance ), pwszName );
}

static HANDLE WINAPI HookLoadImageA( HINSTANCE hInstance, LPCSTR pszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	const ULONG_PTR nName = reinterpret_cast<ULONG_PTR>( pszName );

	HANDLE hStrip = LoadCheckStrip( hInstance, nName, nType, nFlags );

	if ( hStrip )
	{
		return hStrip;
	}

	return reinterpret_cast<LoadImageA_t>( s_pLoadImageA )( ImageModule( hInstance, nName, nType, nFlags ), pszName, nType, nWidth, nHeight, nFlags );
}

static HANDLE WINAPI HookLoadImageW( HINSTANCE hInstance, LPCWSTR pwszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	const ULONG_PTR nName = reinterpret_cast<ULONG_PTR>( pwszName );

	HANDLE hStrip = LoadCheckStrip( hInstance, nName, nType, nFlags );

	if ( hStrip )
	{
		return hStrip;
	}

	return reinterpret_cast<LoadImageW_t>( s_pLoadImageW )( ImageModule( hInstance, nName, nType, nFlags ), pwszName, nType, nWidth, nHeight, nFlags );
}

static HICON WINAPI HookLoadIconA( HINSTANCE hInstance, LPCSTR pszName )
{
	const bool bOwn = HasOwnIcon( hInstance, reinterpret_cast<ULONG_PTR>( pszName ) );

	return reinterpret_cast<LoadIconA_t>( s_pLoadIconA )( ResourceModule( bOwn, hInstance ), pszName );
}

static HICON WINAPI HookLoadIconW( HINSTANCE hInstance, LPCWSTR pwszName )
{
	const bool bOwn = HasOwnIcon( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ) );

	return reinterpret_cast<LoadIconW_t>( s_pLoadIconW )( ResourceModule( bOwn, hInstance ), pwszName );
}

//-----------------------------------------------------------------------------
// Hooks of the comctl32 imports: image lists load the check box strip through them
//-----------------------------------------------------------------------------

static HANDLE WINAPI HookComctlLoadImageA( HINSTANCE hInstance, LPCSTR pszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	HANDLE hStrip = LoadCheckStrip( hInstance, reinterpret_cast<ULONG_PTR>( pszName ), nType, nFlags );

	if ( hStrip )
	{
		return hStrip;
	}

	return reinterpret_cast<LoadImageA_t>( s_pLoadImageA )( hInstance, pszName, nType, nWidth, nHeight, nFlags );
}

static HANDLE WINAPI HookComctlLoadImageW( HINSTANCE hInstance, LPCWSTR pwszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	HANDLE hStrip = LoadCheckStrip( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), nType, nFlags );

	if ( hStrip )
	{
		return hStrip;
	}

	return reinterpret_cast<LoadImageW_t>( s_pLoadImageW )( hInstance, pwszName, nType, nWidth, nHeight, nFlags );
}

//-----------------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------------

void Icons_Init( COLORREF background )
{
	s_Background = background;

	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>( &Icons_Init ),
		&s_hSelf
	);

	HMODULE hKernel32  = GetModuleHandleW( L"kernel32.dll" );
	HMODULE hUser32    = GetModuleHandleW( L"user32.dll" );
	s_pFindResourceA   = GetProcAddress( hKernel32, "FindResourceA" );
	s_pFindResourceW   = GetProcAddress( hKernel32, "FindResourceW" );
	s_pFindResourceExW = GetProcAddress( hKernel32, "FindResourceExW" );
	s_pLoadResource    = GetProcAddress( hKernel32, "LoadResource" );
	s_pSizeofResource  = GetProcAddress( hKernel32, "SizeofResource" );
	s_pLoadBitmapW     = GetProcAddress( hUser32, "LoadBitmapW" );
	s_pLoadImageA      = GetProcAddress( hUser32, "LoadImageA" );
	s_pLoadImageW      = GetProcAddress( hUser32, "LoadImageW" );
	s_pLoadIconA       = GetProcAddress( hUser32, "LoadIconA" );
	s_pLoadIconW       = GetProcAddress( hUser32, "LoadIconW" );
}

int Icons_HookModule( HMODULE hHammer )
{
	if ( !s_hSelf || !s_pFindResourceW || !s_pLoadResource || !s_pSizeofResource )
	{
		return 0;
	}

	s_hHammer = hHammer;

	const ImportHook_t hooks[] =
	{
		{ s_pFindResourceA,   reinterpret_cast<void *>( &HookFindResourceA )   },
		{ s_pFindResourceW,   reinterpret_cast<void *>( &HookFindResourceW )   },
		{ s_pFindResourceExW, reinterpret_cast<void *>( &HookFindResourceExW ) },
		{ s_pLoadResource,    reinterpret_cast<void *>( &HookLoadResource )    },
		{ s_pSizeofResource,  reinterpret_cast<void *>( &HookSizeofResource )  },
		{ s_pLoadBitmapW,     reinterpret_cast<void *>( &HookLoadBitmapW )     },
		{ s_pLoadImageA,      reinterpret_cast<void *>( &HookLoadImageA )      },
		{ s_pLoadImageW,      reinterpret_cast<void *>( &HookLoadImageW )      },
		{ s_pLoadIconA,       reinterpret_cast<void *>( &HookLoadIconA )       },
		{ s_pLoadIconW,       reinterpret_cast<void *>( &HookLoadIconW )       },
	};

	return HookImports( hHammer, hooks, ARRAYSIZE( hooks ) );
}

int Icons_HookComctl( HMODULE hComctl )
{
	if ( !s_pLoadImageA || !s_pLoadImageW )
	{
		return 0;
	}

	const ImportHook_t hooks[] =
	{
		{ s_pLoadImageA, reinterpret_cast<void *>( &HookComctlLoadImageA ) },
		{ s_pLoadImageW, reinterpret_cast<void *>( &HookComctlLoadImageW ) },
	};

	return HookImports( hComctl, hooks, ARRAYSIZE( hooks ) );
}
