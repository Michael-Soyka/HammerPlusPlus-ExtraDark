//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: The dark palette, its brushes and the system color hooks that hand it out
//
//=============================================================================//

#include "theme/system_colors.h"
#include "core/log.h"

#include <uxtheme.h>

#include <stdio.h>

COLORREF g_ColorFace          = RGB( 45, 45, 48 );
COLORREF g_ColorWindow        = RGB( 30, 30, 30 );
COLORREF g_ColorEdit          = RGB( 37, 37, 38 );
COLORREF g_ColorText          = RGB( 220, 220, 220 );
COLORREF g_ColorGrayText      = RGB( 128, 128, 128 );
COLORREF g_ColorMenu          = RGB( 43, 43, 43 );
COLORREF g_ColorMenuHot       = RGB( 62, 62, 66 );
COLORREF g_ColorHighlight     = RGB( 0, 120, 215 );
COLORREF g_ColorHighlightText = RGB( 255, 255, 255 );
COLORREF g_ColorLight         = RGB( 62, 62, 66 );
COLORREF g_ColorHilight       = RGB( 80, 80, 84 );
COLORREF g_ColorShadow        = RGB( 28, 28, 30 );
COLORREF g_ColorDarkShadow    = RGB( 15, 15, 15 );
COLORREF g_ColorTooltip       = RGB( 50, 50, 52 );
COLORREF g_ColorHotLight      = RGB( 86, 156, 214 );
COLORREF g_ColorWorkspace     = RGB( 28, 28, 28 );
COLORREF g_ColorFrame         = RGB( 20, 20, 20 );
COLORREF g_ColorBorder        = RGB( 100, 100, 104 );

struct PaletteKey_t
{
	const wchar_t	*m_pwszName;
	COLORREF		*m_pColor;
};

static const PaletteKey_t s_PaletteKeys[] =
{
	{ L"Face",          &g_ColorFace },
	{ L"Window",        &g_ColorWindow },
	{ L"Edit",          &g_ColorEdit },
	{ L"Text",          &g_ColorText },
	{ L"GrayText",      &g_ColorGrayText },
	{ L"Menu",          &g_ColorMenu },
	{ L"MenuHot",       &g_ColorMenuHot },
	{ L"Highlight",     &g_ColorHighlight },
	{ L"HighlightText", &g_ColorHighlightText },
	{ L"Light",         &g_ColorLight },
	{ L"Hilight",       &g_ColorHilight },
	{ L"Shadow",        &g_ColorShadow },
	{ L"DarkShadow",    &g_ColorDarkShadow },
	{ L"Tooltip",       &g_ColorTooltip },
	{ L"HotLight",      &g_ColorHotLight },
	{ L"Workspace",     &g_ColorWorkspace },
	{ L"Frame",         &g_ColorFrame },
	{ L"Border",        &g_ColorBorder },
};

// System color indices answered by the hooks
struct DarkColor_t
{
	int				m_nIndex;
	const COLORREF	*m_pColor;
};

static const DarkColor_t s_Palette[] =
{
	{ COLOR_SCROLLBAR,           &g_ColorLight },
	{ COLOR_BACKGROUND,          &g_ColorWindow },
	{ COLOR_MENU,                &g_ColorMenu },
	{ COLOR_WINDOW,              &g_ColorWindow },
	{ COLOR_WINDOWFRAME,         &g_ColorFrame },
	{ COLOR_MENUTEXT,            &g_ColorText },
	{ COLOR_WINDOWTEXT,          &g_ColorText },
	{ COLOR_CAPTIONTEXT,         &g_ColorText },
	{ COLOR_APPWORKSPACE,        &g_ColorWorkspace },
	{ COLOR_HIGHLIGHT,           &g_ColorHighlight },
	{ COLOR_HIGHLIGHTTEXT,       &g_ColorHighlightText },
	{ COLOR_BTNFACE,             &g_ColorFace },
	{ COLOR_BTNSHADOW,           &g_ColorShadow },
	{ COLOR_GRAYTEXT,            &g_ColorGrayText },
	{ COLOR_BTNTEXT,             &g_ColorText },
	{ COLOR_INACTIVECAPTIONTEXT, &g_ColorGrayText },
	{ COLOR_BTNHIGHLIGHT,        &g_ColorHilight },
	{ COLOR_3DDKSHADOW,          &g_ColorDarkShadow },
	{ COLOR_3DLIGHT,             &g_ColorLight },
	{ COLOR_INFOTEXT,            &g_ColorText },
	{ COLOR_INFOBK,              &g_ColorTooltip },
	{ COLOR_HOTLIGHT,            &g_ColorHotLight },
	{ COLOR_MENUHILIGHT,         &g_ColorMenuHot },
	{ COLOR_MENUBAR,             &g_ColorMenu },
};

static HBRUSH s_hBrushes[ COLOR_MENUBAR + 1 ];
static HBRUSH s_hBorderBrush  = nullptr;
static HBRUSH s_hEditBrush    = nullptr;
static HBRUSH s_hMenuHotBrush = nullptr;

using GetSysColor_t      = DWORD ( WINAPI * )( int );
using GetSysColorBrush_t = HBRUSH ( WINAPI * )( int );
using GetThemeSysColor_t = COLORREF ( WINAPI * )( HTHEME, int );

static FARPROC s_pGetSysColor      = nullptr;
static FARPROC s_pGetSysColorBrush = nullptr;
static FARPROC s_pGetThemeSysColor = nullptr;

//-----------------------------------------------------------------------------
// Palette
//-----------------------------------------------------------------------------

// "#RRGGBB" or "R,G,B"
static bool ParseColor( const wchar_t *pwszValue, COLORREF &color )
{
	while ( *pwszValue == L' ' || *pwszValue == L'\t' )
	{
		pwszValue++;
	}

	if ( *pwszValue == L'#' )
	{
		const wchar_t *pwszDigits  = pwszValue + 1;
		wchar_t *pwszEnd           = nullptr;
		const unsigned long nValue = wcstoul( pwszDigits, &pwszEnd, 16 );

		if ( pwszEnd - pwszDigits != 6 )
		{
			return false;
		}

		color = RGB( ( nValue >> 16 ) & 0xFF, ( nValue >> 8 ) & 0xFF, nValue & 0xFF );

		return true;
	}

	int nRed   = -1;
	int nGreen = -1;
	int nBlue  = -1;

	if ( swscanf_s( pwszValue, L"%d , %d , %d", &nRed, &nGreen, &nBlue ) != 3 )
	{
		return false;
	}

	if ( nRed < 0 || nRed > 255 || nGreen < 0 || nGreen > 255 || nBlue < 0 || nBlue > 255 )
	{
		return false;
	}

	color = RGB( nRed, nGreen, nBlue );

	return true;
}

void LoadPalette( const std::wstring &ini )
{
	for ( const PaletteKey_t &key : s_PaletteKeys )
	{
		wchar_t wszValue[ 64 ] = {};
		GetPrivateProfileStringW( L"Palette", key.m_pwszName, L"", wszValue, ARRAYSIZE( wszValue ), ini.c_str() );

		if ( wszValue[ 0 ] && !ParseColor( wszValue, *key.m_pColor ) )
		{
			Log( "palette: %ls=%ls is not #RRGGBB or R,G,B, default kept", key.m_pwszName, wszValue );
		}
	}
}

static bool DarkColor( int nIndex, COLORREF &color )
{
	for ( const DarkColor_t &entry : s_Palette )
	{
		if ( entry.m_nIndex == nIndex )
		{
			color = *entry.m_pColor;

			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Brushes
//-----------------------------------------------------------------------------

static HBRUSH CachedBrush( HBRUSH &hBrush, COLORREF color )
{
	if ( !hBrush )
	{
		hBrush = CreateSolidBrush( color );
	}

	return hBrush;
}

HBRUSH DarkBrush( int nIndex )
{
	COLORREF color = 0;

	if ( nIndex < 0 || nIndex > COLOR_MENUBAR || !DarkColor( nIndex, color ) )
	{
		return nullptr;
	}

	return CachedBrush( s_hBrushes[ nIndex ], color );
}

HBRUSH BorderBrush()
{
	return CachedBrush( s_hBorderBrush, g_ColorBorder );
}

HBRUSH EditBrush()
{
	return CachedBrush( s_hEditBrush, g_ColorEdit );
}

HBRUSH MenuHotBrush()
{
	return CachedBrush( s_hMenuHotBrush, g_ColorMenuHot );
}

static HBRUSH SystemBrush( int nIndex )
{
	return reinterpret_cast<GetSysColorBrush_t>( s_pGetSysColorBrush )( nIndex );
}

bool IsSystemBrush( HBRUSH hBrush )
{
	if ( !hBrush )
	{
		return true;
	}

	for ( const int nIndex : { COLOR_BTNFACE, COLOR_WINDOW, COLOR_3DFACE } )
	{
		if ( hBrush == SystemBrush( nIndex ) || hBrush == DarkBrush( nIndex ) )
		{
			return true;
		}
	}

	return false;
}

HBRUSH ClassDarkBrush( HWND hWnd )
{
	const ULONG_PTR nBrush = GetClassLongPtrW( hWnd, GCLP_HBRBACKGROUND );

	if ( !nBrush )
	{
		return nullptr;
	}

	if ( nBrush <= COLOR_MENUBAR + 1 )
	{
		return DarkBrush( static_cast<int>( nBrush ) - 1 );
	}

	for ( int i = 0; i <= COLOR_MENUBAR; i++ )
	{
		if ( reinterpret_cast<HBRUSH>( nBrush ) == SystemBrush( i ) )
		{
			return DarkBrush( i );
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Hooks
//-----------------------------------------------------------------------------

static DWORD WINAPI HookGetSysColor( int nIndex )
{
	COLORREF color = 0;

	if ( DarkColor( nIndex, color ) )
	{
		return color;
	}

	return reinterpret_cast<GetSysColor_t>( s_pGetSysColor )( nIndex );
}

static HBRUSH WINAPI HookGetSysColorBrush( int nIndex )
{
	HBRUSH hBrush = DarkBrush( nIndex );

	if ( hBrush )
	{
		return hBrush;
	}

	return SystemBrush( nIndex );
}

static COLORREF WINAPI HookGetThemeSysColor( HTHEME hTheme, int nIndex )
{
	COLORREF color = 0;

	if ( DarkColor( nIndex, color ) )
	{
		return color;
	}

	return reinterpret_cast<GetThemeSysColor_t>( s_pGetThemeSysColor )( hTheme, nIndex );
}

bool SystemColors_Init()
{
	HMODULE hUser32  = GetModuleHandleW( L"user32.dll" );
	HMODULE hUxTheme = LoadLibraryExW( L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );

	s_pGetSysColor      = GetProcAddress( hUser32, "GetSysColor" );
	s_pGetSysColorBrush = GetProcAddress( hUser32, "GetSysColorBrush" );
	s_pGetThemeSysColor = nullptr;

	if ( hUxTheme )
	{
		s_pGetThemeSysColor = GetProcAddress( hUxTheme, "GetThemeSysColor" );
	}

	return s_pGetSysColor && s_pGetSysColorBrush;
}

int SystemColors_AddHooks( ImportHook_t *pHooks )
{
	pHooks[ 0 ] = { s_pGetSysColor,      reinterpret_cast<void *>( &HookGetSysColor ) };
	pHooks[ 1 ] = { s_pGetSysColorBrush, reinterpret_cast<void *>( &HookGetSysColorBrush ) };
	pHooks[ 2 ] = { s_pGetThemeSysColor, reinterpret_cast<void *>( &HookGetThemeSysColor ) };

	return 3;
}
