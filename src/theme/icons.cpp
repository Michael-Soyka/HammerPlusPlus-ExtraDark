//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Bitmaps of hammerplusplus_dll.dll replaced by the ones built into hppxdark.dll
//			(src/assets/Bitmap<id>.bmp, same resource ids)
//
//=============================================================================//

#include "theme/icons.h"
#include "core/iat_hook.h"

#include <uxtheme.h>
#include <vssym32.h>

// RT_BITMAP as a number: the project builds without UNICODE
static const ULONG_PTR s_nBitmapType = 2;

static HMODULE s_hSelf   = nullptr;
static HMODULE s_hHammer = nullptr;

// Resources handed out from hppxdark.dll: LoadResource and SizeofResource get them with our module
static HRSRC s_hOwnResources[64];
static int s_nOwnResources = 0;

using FindResourceA_t   = HRSRC ( WINAPI * )( HMODULE, LPCSTR, LPCSTR );
using FindResourceW_t   = HRSRC ( WINAPI * )( HMODULE, LPCWSTR, LPCWSTR );
using FindResourceExW_t = HRSRC ( WINAPI * )( HMODULE, LPCWSTR, LPCWSTR, WORD );
using LoadResource_t    = HGLOBAL ( WINAPI * )( HMODULE, HRSRC );
using SizeofResource_t  = DWORD ( WINAPI * )( HMODULE, HRSRC );
using LoadBitmapW_t     = HBITMAP ( WINAPI * )( HINSTANCE, LPCWSTR );
using LoadImageA_t      = HANDLE ( WINAPI * )( HINSTANCE, LPCSTR, UINT, int, int, UINT );
using LoadImageW_t      = HANDLE ( WINAPI * )( HINSTANCE, LPCWSTR, UINT, int, int, UINT );

static FARPROC s_pFindResourceA   = nullptr;
static FARPROC s_pFindResourceW   = nullptr;
static FARPROC s_pFindResourceExW = nullptr;
static FARPROC s_pLoadResource    = nullptr;
static FARPROC s_pSizeofResource  = nullptr;
static FARPROC s_pLoadBitmapW     = nullptr;
static FARPROC s_pLoadImageA      = nullptr;
static FARPROC s_pLoadImageW      = nullptr;

// Visgroup tree check boxes: unchecked, unchecked, checked, partly checked
static const ULONG_PTR s_nCheckStrip = 223;
static const int s_nCheckCell = 16;

// Image list mask color of the strip: the cell outside the glyph
static const COLORREF s_CheckMask = RGB( 255, 255, 255 );

static COLORREF s_Background = RGB( 30, 30, 30 );

static HBITMAP CreateCheckStrip();
static bool IsCheckStrip( HINSTANCE hInstance, ULONG_PTR nName, UINT nType, UINT nFlags );

static bool IsOwnResource( HRSRC hResource )
{
	for ( int i = 0; i < s_nOwnResources; i++ )
	{
		if ( s_hOwnResources[i] == hResource )
		{
			return true;
		}
	}

	return false;
}

// Our bitmap for a request to the Hammer++ DLL by numeric id, null for anything else
static HRSRC FindOwnBitmap( HMODULE hModule, ULONG_PTR nName, ULONG_PTR nType )
{
	if ( !s_hHammer || hModule != s_hHammer || !IS_INTRESOURCE( nName ) || nType != s_nBitmapType )
	{
		return nullptr;
	}

	HRSRC hResource = reinterpret_cast<FindResourceW_t>( s_pFindResourceW )( s_hSelf, MAKEINTRESOURCEW( nName ), MAKEINTRESOURCEW( s_nBitmapType ) );
	if ( hResource && !IsOwnResource( hResource ) && s_nOwnResources < ARRAYSIZE( s_hOwnResources ) )
	{
		s_hOwnResources[s_nOwnResources++] = hResource;
	}

	return hResource;
}

static HRSRC WINAPI HookFindResourceA( HMODULE hModule, LPCSTR pszName, LPCSTR pszType )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pszName ), reinterpret_cast<ULONG_PTR>( pszType ) );
	if ( !hResource )
	{
		hResource = reinterpret_cast<FindResourceA_t>( s_pFindResourceA )( hModule, pszName, pszType );
	}

	return hResource;
}

static HRSRC WINAPI HookFindResourceW( HMODULE hModule, LPCWSTR pwszName, LPCWSTR pwszType )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pwszName ), reinterpret_cast<ULONG_PTR>( pwszType ) );
	if ( !hResource )
	{
		hResource = reinterpret_cast<FindResourceW_t>( s_pFindResourceW )( hModule, pwszName, pwszType );
	}

	return hResource;
}

static HRSRC WINAPI HookFindResourceExW( HMODULE hModule, LPCWSTR pwszType, LPCWSTR pwszName, WORD nLanguage )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pwszName ), reinterpret_cast<ULONG_PTR>( pwszType ) );
	if ( !hResource )
	{
		hResource = reinterpret_cast<FindResourceExW_t>( s_pFindResourceExW )( hModule, pwszType, pwszName, nLanguage );
	}

	return hResource;
}

static HGLOBAL WINAPI HookLoadResource( HMODULE hModule, HRSRC hResource )
{
	HGLOBAL hData = reinterpret_cast<LoadResource_t>( s_pLoadResource )( IsOwnResource( hResource ) ? s_hSelf : hModule, hResource );
	return hData;
}

static DWORD WINAPI HookSizeofResource( HMODULE hModule, HRSRC hResource )
{
	const DWORD nSize = reinterpret_cast<SizeofResource_t>( s_pSizeofResource )( IsOwnResource( hResource ) ? s_hSelf : hModule, hResource );
	return nSize;
}

static HBITMAP WINAPI HookLoadBitmapW( HINSTANCE hInstance, LPCWSTR pwszName )
{
	const bool bOwn = FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), s_nBitmapType ) != nullptr;
	HBITMAP hBitmap = reinterpret_cast<LoadBitmapW_t>( s_pLoadBitmapW )( bOwn ? s_hSelf : hInstance, pwszName );
	return hBitmap;
}

static HANDLE WINAPI HookLoadImageA( HINSTANCE hInstance, LPCSTR pszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	if ( IsCheckStrip( hInstance, reinterpret_cast<ULONG_PTR>( pszName ), nType, nFlags ) )
	{
		HANDLE hStrip = CreateCheckStrip();
		if ( hStrip )
		{
			return hStrip;
		}
	}

	const bool bOwn = nType == IMAGE_BITMAP && !( nFlags & LR_LOADFROMFILE ) && FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pszName ), s_nBitmapType );
	HANDLE hImage = reinterpret_cast<LoadImageA_t>( s_pLoadImageA )( bOwn ? s_hSelf : hInstance, pszName, nType, nWidth, nHeight, nFlags );
	return hImage;
}

static HANDLE WINAPI HookLoadImageW( HINSTANCE hInstance, LPCWSTR pwszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	if ( IsCheckStrip( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), nType, nFlags ) )
	{
		HANDLE hStrip = CreateCheckStrip();
		if ( hStrip )
		{
			return hStrip;
		}
	}

	const bool bOwn = nType == IMAGE_BITMAP && !( nFlags & LR_LOADFROMFILE ) && FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), s_nBitmapType );
	HANDLE hImage = reinterpret_cast<LoadImageW_t>( s_pLoadImageW )( bOwn ? s_hSelf : hInstance, pwszName, nType, nWidth, nHeight, nFlags );
	return hImage;
}

// Check box strip drawn by the dark theme. Made on request: the image list takes the bitmap
// and deletes it, and the theme cannot be opened under the loader lock
static HBITMAP CreateCheckStrip()
{
	HTHEME hTheme = OpenThemeData( nullptr, L"DarkMode_Explorer::Button" );
	if ( !hTheme )
	{
		return nullptr;
	}

	const int nStates[] = { CBS_UNCHECKEDNORMAL, CBS_UNCHECKEDNORMAL, CBS_CHECKEDNORMAL, CBS_MIXEDNORMAL };
	const int nWidth = s_nCheckCell * ARRAYSIZE( nStates );

	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof( info.bmiHeader );
	info.bmiHeader.biWidth = nWidth;
	info.bmiHeader.biHeight = -s_nCheckCell;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	DWORD *pPixels = nullptr;
	HDC hDC = CreateCompatibleDC( nullptr );
	HBITMAP hBitmap = CreateDIBSection( hDC, &info, DIB_RGB_COLORS, reinterpret_cast<void **>( &pPixels ), nullptr, 0 );
	if ( !hBitmap )
	{
		DeleteDC( hDC );
		CloseThemeData( hTheme );
		return nullptr;
	}

	HGDIOBJ hOld = SelectObject( hDC, hBitmap );

	RECT all = { 0, 0, nWidth, s_nCheckCell };
	HBRUSH hMask = CreateSolidBrush( s_CheckMask );
	FillRect( hDC, &all, hMask );
	DeleteObject( hMask );

	SIZE glyph = { 13, 13 };
	GetThemePartSize( hTheme, hDC, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, nullptr, TS_DRAW, &glyph );
	glyph.cx = min( glyph.cx, static_cast<LONG>( s_nCheckCell ) );
	glyph.cy = min( glyph.cy, static_cast<LONG>( s_nCheckCell ) );

	// Rounded corners blend into the tree background, not into the mask
	HBRUSH hBack = CreateSolidBrush( s_Background );
	for ( int i = 0; i < ARRAYSIZE( nStates ); i++ )
	{
		RECT box;
		box.left = ( i * s_nCheckCell ) + ( ( s_nCheckCell - glyph.cx ) / 2 );
		box.top = ( s_nCheckCell - glyph.cy ) / 2;
		box.right = box.left + glyph.cx;
		box.bottom = box.top + glyph.cy;

		FillRect( hDC, &box, hBack );
		DrawThemeBackground( hTheme, hDC, BP_CHECKBOX, nStates[i], &box, nullptr );
	}

	DeleteObject( hBack );
	GdiFlush();

	// Inside the glyphs nothing may come out in the mask color
	const LONG nTop = ( s_nCheckCell - glyph.cy ) / 2;
	for ( LONG y = nTop; y < nTop + glyph.cy; y++ )
	{
		for ( int i = 0; i < ARRAYSIZE( nStates ); i++ )
		{
			const LONG nLeft = ( i * s_nCheckCell ) + ( ( s_nCheckCell - glyph.cx ) / 2 );
			for ( LONG x = nLeft; x < nLeft + glyph.cx; x++ )
			{
				DWORD &pixel = pPixels[( y * nWidth ) + x];
				if ( ( pixel & 0x00FFFFFF ) == 0x00FFFFFF )
				{
					pixel = 0x00FEFEFE;
				}
			}
		}
	}

	SelectObject( hDC, hOld );
	DeleteDC( hDC );
	CloseThemeData( hTheme );

	return hBitmap;
}

static bool IsCheckStrip( HINSTANCE hInstance, ULONG_PTR nName, UINT nType, UINT nFlags )
{
	const bool bStrip = s_hHammer && hInstance == s_hHammer && nType == IMAGE_BITMAP && !( nFlags & LR_LOADFROMFILE ) && nName == s_nCheckStrip;

	return bStrip;
}

static HANDLE WINAPI HookComctlLoadImageA( HINSTANCE hInstance, LPCSTR pszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	HANDLE hImage = IsCheckStrip( hInstance, reinterpret_cast<ULONG_PTR>( pszName ), nType, nFlags ) ? CreateCheckStrip() : nullptr;
	if ( !hImage )
	{
		hImage = reinterpret_cast<LoadImageA_t>( s_pLoadImageA )( hInstance, pszName, nType, nWidth, nHeight, nFlags );
	}

	return hImage;
}

static HANDLE WINAPI HookComctlLoadImageW( HINSTANCE hInstance, LPCWSTR pwszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	HANDLE hImage = IsCheckStrip( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), nType, nFlags ) ? CreateCheckStrip() : nullptr;
	if ( !hImage )
	{
		hImage = reinterpret_cast<LoadImageW_t>( s_pLoadImageW )( hInstance, pwszName, nType, nWidth, nHeight, nFlags );
	}

	return hImage;
}

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
	};

	const int nReplaced = HookImports( hHammer, hooks, ARRAYSIZE( hooks ) );
	return nReplaced;
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

	const int nReplaced = HookImports( hComctl, hooks, ARRAYSIZE( hooks ) );
	return nReplaced;
}
