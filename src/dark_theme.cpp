//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Dark theme for Hammer++: system colors, Windows dark mode, control colors
//
//=============================================================================//

#include "dark_theme.h"
#include "iat_hook.h"
#include "icons.h"
#include "log.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <psapi.h>
#include <uxtheme.h>

#include <string>

//-----------------------------------------------------------------------------
// Palette
//-----------------------------------------------------------------------------

static const COLORREF s_ColorFace          = RGB( 45, 45, 48 );
static const COLORREF s_ColorWindow        = RGB( 30, 30, 30 );
static const COLORREF s_ColorEdit          = RGB( 37, 37, 38 );
static const COLORREF s_ColorText          = RGB( 220, 220, 220 );
static const COLORREF s_ColorGrayText      = RGB( 128, 128, 128 );
static const COLORREF s_ColorMenu          = RGB( 43, 43, 43 );
static const COLORREF s_ColorMenuHot       = RGB( 62, 62, 66 );
static const COLORREF s_ColorHighlight     = RGB( 0, 120, 215 );
static const COLORREF s_ColorHighlightText = RGB( 255, 255, 255 );
static const COLORREF s_ColorLight         = RGB( 62, 62, 66 );
static const COLORREF s_ColorHilight       = RGB( 80, 80, 84 );
static const COLORREF s_ColorShadow        = RGB( 28, 28, 30 );
static const COLORREF s_ColorDarkShadow    = RGB( 15, 15, 15 );
static const COLORREF s_ColorTooltip       = RGB( 50, 50, 52 );
static const COLORREF s_ColorHotLight      = RGB( 86, 156, 214 );
static const COLORREF s_ColorWorkspace     = RGB( 28, 28, 28 );
static const COLORREF s_ColorFrame         = RGB( 20, 20, 20 );
static const COLORREF s_ColorBorder        = RGB( 100, 100, 104 );

// System color indices answered by the hooks
struct DarkColor_t
{
	int				m_nIndex;
	const COLORREF	*m_pColor;
};

static const DarkColor_t s_Palette[] =
{
	{ COLOR_SCROLLBAR,           &s_ColorLight },
	{ COLOR_BACKGROUND,          &s_ColorWindow },
	{ COLOR_MENU,                &s_ColorMenu },
	{ COLOR_WINDOW,              &s_ColorWindow },
	{ COLOR_WINDOWFRAME,         &s_ColorFrame },
	{ COLOR_MENUTEXT,            &s_ColorText },
	{ COLOR_WINDOWTEXT,          &s_ColorText },
	{ COLOR_CAPTIONTEXT,         &s_ColorText },
	{ COLOR_APPWORKSPACE,        &s_ColorWorkspace },
	{ COLOR_HIGHLIGHT,           &s_ColorHighlight },
	{ COLOR_HIGHLIGHTTEXT,       &s_ColorHighlightText },
	{ COLOR_BTNFACE,             &s_ColorFace },
	{ COLOR_BTNSHADOW,           &s_ColorShadow },
	{ COLOR_GRAYTEXT,            &s_ColorGrayText },
	{ COLOR_BTNTEXT,             &s_ColorText },
	{ COLOR_INACTIVECAPTIONTEXT, &s_ColorGrayText },
	{ COLOR_BTNHIGHLIGHT,        &s_ColorHilight },
	{ COLOR_3DDKSHADOW,          &s_ColorDarkShadow },
	{ COLOR_3DLIGHT,             &s_ColorLight },
	{ COLOR_INFOTEXT,            &s_ColorText },
	{ COLOR_INFOBK,              &s_ColorTooltip },
	{ COLOR_HOTLIGHT,            &s_ColorHotLight },
	{ COLOR_MENUHILIGHT,         &s_ColorMenuHot },
	{ COLOR_MENUBAR,             &s_ColorMenu },
};

static HBRUSH s_hBrushes[COLOR_MENUBAR + 1];
static HBRUSH s_hEditBrush = nullptr;
static HBRUSH s_hMenuHotBrush = nullptr;
static HBRUSH s_hBorderBrush = nullptr;

static bool s_bActive = false;

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

static HBRUSH DarkBrush( int nIndex )
{
	if ( nIndex < 0 || nIndex > COLOR_MENUBAR )
	{
		return nullptr;
	}

	COLORREF color = 0;
	if ( !DarkColor( nIndex, color ) )
	{
		return nullptr;
	}

	// Kept for the process lifetime, like the system brushes
	if ( !s_hBrushes[nIndex] )
	{
		s_hBrushes[nIndex] = CreateSolidBrush( color );
	}

	return s_hBrushes[nIndex];
}

//-----------------------------------------------------------------------------
// System color hooks
//-----------------------------------------------------------------------------

using GetSysColor_t = DWORD ( WINAPI * )( int );
using GetSysColorBrush_t = HBRUSH ( WINAPI * )( int );
using GetThemeSysColor_t = COLORREF ( WINAPI * )( HTHEME, int );

static FARPROC s_pGetSysColor = nullptr;
static FARPROC s_pGetSysColorBrush = nullptr;
static FARPROC s_pGetThemeSysColor = nullptr;

static DWORD WINAPI HookGetSysColor( int nIndex )
{
	COLORREF color = 0;
	if ( DarkColor( nIndex, color ) )
	{
		return color;
	}

	const DWORD nResult = reinterpret_cast<GetSysColor_t>( s_pGetSysColor )( nIndex );
	
	return nResult;
}

static HBRUSH WINAPI HookGetSysColorBrush( int nIndex )
{
	HBRUSH hBrush = DarkBrush( nIndex );
	if ( hBrush )
	{
		return hBrush;
	}

	hBrush = reinterpret_cast<GetSysColorBrush_t>( s_pGetSysColorBrush )( nIndex );
	
	return hBrush;
}

static COLORREF WINAPI HookGetThemeSysColor( HTHEME hTheme, int nIndex )
{
	COLORREF color = 0;
	if ( DarkColor( nIndex, color ) )
	{
		return color;
	}

	color = reinterpret_cast<GetThemeSysColor_t>( s_pGetThemeSysColor )( hTheme, nIndex );
	
	return color;
}

//-----------------------------------------------------------------------------
// Message boxes: user32 paints their button strip itself with the light face color. Windows
// created while a MessageBox call runs are marked and painted flat
//-----------------------------------------------------------------------------

using MessageBoxA_t = int ( WINAPI * )( HWND, LPCSTR, LPCSTR, UINT );
using MessageBoxW_t = int ( WINAPI * )( HWND, LPCWSTR, LPCWSTR, UINT );
using MessageBoxIndirectA_t = int ( WINAPI * )( const MSGBOXPARAMSA * );
using MessageBoxIndirectW_t = int ( WINAPI * )( const MSGBOXPARAMSW * );

static FARPROC s_pMessageBoxA = nullptr;
static FARPROC s_pMessageBoxW = nullptr;
static FARPROC s_pMessageBoxIndirectA = nullptr;
static FARPROC s_pMessageBoxIndirectW = nullptr;

static int s_nMessageBoxDepth = 0;

static const wchar_t *const s_pwszMessageBoxProp = L"hppxdark_msgbox";

static int WINAPI HookMessageBoxA( HWND hWnd, LPCSTR pszText, LPCSTR pszCaption, UINT nType )
{
	s_nMessageBoxDepth++;
	const int nResult = reinterpret_cast<MessageBoxA_t>( s_pMessageBoxA )( hWnd, pszText, pszCaption, nType );
	s_nMessageBoxDepth--;
	
	return nResult;
}

static int WINAPI HookMessageBoxW( HWND hWnd, LPCWSTR pwszText, LPCWSTR pwszCaption, UINT nType )
{
	s_nMessageBoxDepth++;
	const int nResult = reinterpret_cast<MessageBoxW_t>( s_pMessageBoxW )( hWnd, pwszText, pwszCaption, nType );
	s_nMessageBoxDepth--;
	
	return nResult;
}

static int WINAPI HookMessageBoxIndirectA( const MSGBOXPARAMSA *pParams )
{
	s_nMessageBoxDepth++;
	const int nResult = reinterpret_cast<MessageBoxIndirectA_t>( s_pMessageBoxIndirectA )( pParams );
	s_nMessageBoxDepth--;
	
	return nResult;
}

static int WINAPI HookMessageBoxIndirectW( const MSGBOXPARAMSW *pParams )
{
	s_nMessageBoxDepth++;
	const int nResult = reinterpret_cast<MessageBoxIndirectW_t>( s_pMessageBoxIndirectW )( pParams );
	s_nMessageBoxDepth--;
	
	return nResult;
}

static ImportHook_t s_ColorHooks[7];

static bool IsHookedModule( const wchar_t *pwszName )
{
	const bool bHooked = !_wcsicmp( pwszName, L"hammerplusplus_dll.dll" ) || !_wcsicmp( pwszName, L"comctl32.dll" );
	
	return bHooked;
}

// Hammer++ loads its DLL after we start, so loaded modules are checked again on window creation
static void HookLoadedModules()
{
	static HMODULE s_hHooked[16];
	static int s_nHooked = 0;

	HMODULE hModules[1024];
	DWORD nNeeded = 0;
	if ( !EnumProcessModules( GetCurrentProcess(), hModules, sizeof( hModules ), &nNeeded ) )
	{
		return;
	}

	const int nModules = static_cast<int>( nNeeded / sizeof( HMODULE ) );
	for ( int i = 0; i < nModules && i < ARRAYSIZE( hModules ); i++ )
	{
		bool bDone = false;
		for ( int j = 0; j < s_nHooked; j++ )
		{
			bDone = bDone || s_hHooked[j] == hModules[i];
		}

		wchar_t wszName[MAX_PATH];
		if ( bDone || s_nHooked == ARRAYSIZE( s_hHooked ) || !GetModuleBaseNameW( GetCurrentProcess(), hModules[i], wszName, MAX_PATH ) )
		{
			continue;
		}

		if ( IsHookedModule( wszName ) )
		{
			const int nReplaced = HookImports( hModules[i], s_ColorHooks, ARRAYSIZE( s_ColorHooks ) );
			s_hHooked[s_nHooked++] = hModules[i];
			Log( "hooked %ls: %d imports", wszName, nReplaced );
		}
	}
}

//-----------------------------------------------------------------------------
// DLL load notification (ntdll, documented): hammerplusplus_dll.dll gets the hooks before its
// static initializers run, so MFC caches the dark colors from the start
//-----------------------------------------------------------------------------

struct LdrString_t
{
	USHORT	m_nLength;
	USHORT	m_nMaximumLength;
	PWSTR	m_pwszBuffer;
};

struct LdrDllLoaded_t
{
	ULONG				m_nFlags;
	const LdrString_t	*m_pFullName;
	const LdrString_t	*m_pBaseName;
	PVOID				m_pBase;
	ULONG				m_nSize;
};

using LdrDllNotification_t = VOID ( CALLBACK * )( ULONG, const LdrDllLoaded_t *, PVOID );
using LdrRegisterDllNotification_t = LONG ( NTAPI * )( ULONG, LdrDllNotification_t, PVOID, PVOID * );

static const ULONG s_nDllLoaded = 1;

static VOID CALLBACK OnDllNotification( ULONG nReason, const LdrDllLoaded_t *pData, PVOID )
{
	if ( nReason != s_nDllLoaded || !pData || !pData->m_pBaseName )
	{
		return;
	}

	const std::wstring name( pData->m_pBaseName->m_pwszBuffer, pData->m_pBaseName->m_nLength / sizeof( wchar_t ) );
	if ( _wcsicmp( name.c_str(), L"hammerplusplus_dll.dll" ) )
	{
		return;
	}

	// Under the loader lock: only the import tables are written
	HMODULE hHammer = static_cast<HMODULE>( pData->m_pBase );
	const int nReplaced = HookImports( hHammer, s_ColorHooks, ARRAYSIZE( s_ColorHooks ) );
	const int nIcons = Icons_HookModule( hHammer );
	Log( "load notification %ls: %d color imports, %d resource imports", name.c_str(), nReplaced, nIcons );
}

static void RegisterDllNotification()
{
	HMODULE hNtdll = GetModuleHandleW( L"ntdll.dll" );
	LdrRegisterDllNotification_t pRegister = reinterpret_cast<LdrRegisterDllNotification_t>( GetProcAddress( hNtdll, "LdrRegisterDllNotification" ) );
	if ( !pRegister )
	{
		return;
	}

	PVOID pCookie = nullptr;
	pRegister( 0, OnDllNotification, nullptr, &pCookie );
}

//-----------------------------------------------------------------------------
// Windows dark mode, uxtheme exports by ordinal (Windows 10 1903+)
//-----------------------------------------------------------------------------

enum PreferredAppMode_t
{
	APP_MODE_DEFAULT = 0,
	APP_MODE_ALLOW_DARK = 1,
	APP_MODE_FORCE_DARK = 2,
};

using SetPreferredAppMode_t = int ( WINAPI * )( int );
using AllowDarkModeForWindow_t = BOOL ( WINAPI * )( HWND, BOOL );
using FlushMenuThemes_t = void ( WINAPI * )();

static AllowDarkModeForWindow_t s_pAllowDarkModeForWindow = nullptr;

static void InitDarkMode()
{
	HMODULE hUxTheme = LoadLibraryExW( L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
	if ( !hUxTheme )
	{
		return;
	}

	SetPreferredAppMode_t pSetPreferredAppMode = reinterpret_cast<SetPreferredAppMode_t>( GetProcAddress( hUxTheme, MAKEINTRESOURCEA( 135 ) ) );
	FlushMenuThemes_t pFlushMenuThemes = reinterpret_cast<FlushMenuThemes_t>( GetProcAddress( hUxTheme, MAKEINTRESOURCEA( 136 ) ) );
	s_pAllowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindow_t>( GetProcAddress( hUxTheme, MAKEINTRESOURCEA( 133 ) ) );

	// Missing exports leave the light look, never a failure
	if ( pSetPreferredAppMode )
	{
		pSetPreferredAppMode( APP_MODE_FORCE_DARK );
	}

	if ( pFlushMenuThemes )
	{
		pFlushMenuThemes();
	}
}

//-----------------------------------------------------------------------------
// Per window
//-----------------------------------------------------------------------------

static void ApplyWindowTheme( HWND hWnd )
{
	if ( s_pAllowDarkModeForWindow )
	{
		s_pAllowDarkModeForWindow( hWnd, TRUE );
	}

	const LONG_PTR nStyle = GetWindowLongPtrW( hWnd, GWL_STYLE );
	if ( !( nStyle & WS_CHILD ) )
	{
		const BOOL bDark = TRUE;
		DwmSetWindowAttribute( hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &bDark, sizeof( bDark ) );
	}

	wchar_t wszClass[64];
	if ( !GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) )
	{
		return;
	}

	const std::wstring className = wszClass;
	if ( className == WC_LISTVIEWW )
	{
		SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
		ListView_SetBkColor( hWnd, s_ColorWindow );
		ListView_SetTextBkColor( hWnd, s_ColorWindow );
		ListView_SetTextColor( hWnd, s_ColorText );

		HWND hHeader = ListView_GetHeader( hWnd );
		if ( hHeader )
		{
			SetWindowTheme( hHeader, L"DarkMode_ItemsView", nullptr );
		}
	}
	else if ( className == WC_TREEVIEWW )
	{
		SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
		TreeView_SetBkColor( hWnd, s_ColorWindow );
		TreeView_SetTextColor( hWnd, s_ColorText );
	}
	else if ( className == WC_HEADERW )
	{
		SetWindowTheme( hWnd, L"DarkMode_ItemsView", nullptr );
	}
	else if ( className == WC_COMBOBOXW || className == WC_EDITW )
	{
		SetWindowTheme( hWnd, L"DarkMode_CFD", nullptr );
	}
	else if ( className == WC_BUTTONW )
	{
		// Themed check boxes, radio buttons and group boxes ignore WM_CTLCOLORBTN text colors;
		// push-like ones look like push buttons
		const LONG_PTR nType = nStyle & BS_TYPEMASK;
		const bool bPush = nType == BS_PUSHBUTTON || nType == BS_DEFPUSHBUTTON || nType == BS_SPLITBUTTON || nType == BS_DEFSPLITBUTTON || ( nStyle & BS_PUSHLIKE );
		SetWindowTheme( hWnd, bPush ? L"DarkMode_Explorer" : L"", bPush ? nullptr : L"" );
	}
	else if ( className == TOOLBARCLASSNAMEW || className == STATUSCLASSNAMEW || className == REBARCLASSNAMEW || className == WC_TABCONTROLW || className == UPDOWN_CLASSW )
	{
		// No dark theme classes for these: classic drawing takes the hooked system colors
		SetWindowTheme( hWnd, L"", L"" );
	}
	else if ( className == WC_SCROLLBARW || ( nStyle & ( WS_VSCROLL | WS_HSCROLL ) ) )
	{
		SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
	}
}

// Brushes of the light system colors that default handlers return
static bool IsSystemBrush( HBRUSH hBrush )
{
	if ( !hBrush )
	{
		return true;
	}

	const int nIndices[] = { COLOR_BTNFACE, COLOR_WINDOW, COLOR_3DFACE };
	for ( const int nIndex : nIndices )
	{
		if ( hBrush == reinterpret_cast<GetSysColorBrush_t>( s_pGetSysColorBrush )( nIndex ) || hBrush == DarkBrush( nIndex ) )
		{
			return true;
		}
	}

	return false;
}

static LRESULT CtlColor( UINT nMsg, HDC hDC, LRESULT nDefault )
{
	// Brushes the application chose itself (color swatches) stay
	if ( !IsSystemBrush( reinterpret_cast<HBRUSH>( nDefault ) ) )
	{
		return nDefault;
	}

	const bool bEdit = nMsg == WM_CTLCOLOREDIT || nMsg == WM_CTLCOLORLISTBOX;
	const COLORREF background = bEdit ? s_ColorEdit : s_ColorFace;
	if ( !s_hEditBrush )
	{
		s_hEditBrush = CreateSolidBrush( s_ColorEdit );
	}

	SetTextColor( hDC, s_ColorText );
	SetBkColor( hDC, background );

	HBRUSH hBrush = bEdit ? s_hEditBrush : DarkBrush( COLOR_BTNFACE );
	
	return reinterpret_cast<LRESULT>( hBrush );
}

//-----------------------------------------------------------------------------
// Menu bar, undocumented UAH messages of user32
//-----------------------------------------------------------------------------

#define WM_UAHDRAWMENU		0x0091
#define WM_UAHDRAWMENUITEM	0x0092

struct UahMenu_t
{
	HMENU	m_hMenu;
	HDC		m_hDC;
	DWORD	m_nFlags;
};

union UahMenuItemMetrics_t
{
	SIZE	m_Bar[2];
	SIZE	m_Popup[4];
};

struct UahMenuPopupMetrics_t
{
	DWORD	m_nWidths[4];
	DWORD	m_nUpdateMaxWidths : 2;
};

struct UahMenuItem_t
{
	int						m_nPosition;
	UahMenuItemMetrics_t	m_Metrics;
	UahMenuPopupMetrics_t	m_PopupMetrics;
};

struct UahDrawMenuItem_t
{
	DRAWITEMSTRUCT	m_Draw;
	UahMenu_t		m_Menu;
	UahMenuItem_t	m_Item;
};

static void DrawMenuBar( HWND hWnd, const UahMenu_t *pMenu )
{
	MENUBARINFO info = { sizeof( info ) };
	if ( !GetMenuBarInfo( hWnd, OBJID_MENU, 0, &info ) )
	{
		return;
	}

	RECT window;
	GetWindowRect( hWnd, &window );

	RECT bar = info.rcBar;
	OffsetRect( &bar, -window.left, -window.top );
	FillRect( pMenu->m_hDC, &bar, DarkBrush( COLOR_MENUBAR ) );
}

// The document window whose icon a menu bar item shows: the system menu item of a maximized
// MDI document, null for any other item
static HWND MenuSystemIconWindow( HMENU hMenu, int nPosition )
{
	// MIIM_TYPE can't be combined with MIIM_BITMAP: the old style bitmap handle is asked separately
	MENUITEMINFOW info = { sizeof( info ) };
	info.fMask = MIIM_FTYPE | MIIM_BITMAP | MIIM_DATA;
	if ( !GetMenuItemInfoW( hMenu, nPosition, TRUE, &info ) )
	{
		return nullptr;
	}

	HBITMAP hBitmap = info.hbmpItem;
	if ( !hBitmap && ( info.fType & MFT_BITMAP ) )
	{
		MENUITEMINFOW type = { sizeof( type ) };
		type.fMask = MIIM_TYPE;
		if ( GetMenuItemInfoW( hMenu, nPosition, TRUE, &type ) )
		{
			hBitmap = reinterpret_cast<HBITMAP>( type.dwTypeData );
		}
	}

	HWND hDocument = reinterpret_cast<HWND>( info.dwItemData );
	Log( "menu item %d: type %x, bitmap %p, data %p", nPosition, info.fType, hBitmap, hDocument );

	if ( hBitmap != HBMMENU_SYSTEM || !IsWindow( hDocument ) )
	{
		return nullptr;
	}

	return hDocument;
}

// The system menu icon of a maximized document window (user32 draws it on a light background) is
// hidden: only the menu bar color. The item stays and still opens the window menu; removing it
// would make user32 remove the wrong item on restore
static void DrawSystemIcon( HDC hDC, const RECT &rect )
{
	FillRect( hDC, &rect, DarkBrush( COLOR_MENUBAR ) );
}

// Painted over after the default non-client paint: user32 may draw the icon past the UAH messages
static void DrawMenuBarSystemIconNonClient( HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );
	HWND hDocument = hMenu ? MenuSystemIconWindow( hMenu, 0 ) : nullptr;
	if ( !hDocument )
	{
		return;
	}

	RECT rect;
	if ( !GetMenuItemRect( hWnd, hMenu, 0, &rect ) )
	{
		return;
	}

	RECT window;
	GetWindowRect( hWnd, &window );
	OffsetRect( &rect, -window.left, -window.top );

	HDC hDC = GetWindowDC( hWnd );
	DrawSystemIcon( hDC, rect );
	ReleaseDC( hWnd, hDC );
}

static bool DrawMenuBarSystemIcon( const UahDrawMenuItem_t *pItem )
{
	HWND hDocument = MenuSystemIconWindow( pItem->m_Menu.m_hMenu, pItem->m_Item.m_nPosition );
	if ( !hDocument )
	{
		return false;
	}

	HDC hDC = pItem->m_Menu.m_hDC;
	const RECT rect = pItem->m_Draw.rcItem;
	DrawSystemIcon( hDC, rect );
	
	return true;
}

// False for the other bitmap items (MDI document buttons), user32 draws those
static bool DrawMenuBarItem( const UahDrawMenuItem_t *pItem )
{
	wchar_t wszText[256] = {};
	MENUITEMINFOW info = { sizeof( info ) };
	info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_BITMAP;
	info.dwTypeData = wszText;
	info.cch = ARRAYSIZE( wszText ) - 1;
	if ( !GetMenuItemInfoW( pItem->m_Menu.m_hMenu, pItem->m_Item.m_nPosition, TRUE, &info ) )
	{
		return false;
	}

	if ( info.hbmpItem || ( info.fType & MFT_BITMAP ) )
	{
		const bool bDrawn = DrawMenuBarSystemIcon( pItem );
		return bDrawn;
	}

	if ( !s_hMenuHotBrush )
	{
		s_hMenuHotBrush = CreateSolidBrush( s_ColorMenuHot );
	}

	const UINT nState = pItem->m_Draw.itemState;
	const bool bHot = ( nState & ( ODS_HOTLIGHT | ODS_SELECTED ) ) != 0;
	const bool bDisabled = ( nState & ( ODS_INACTIVE | ODS_DISABLED | ODS_GRAYED ) ) != 0;

	HDC hDC = pItem->m_Menu.m_hDC;
	RECT rect = pItem->m_Draw.rcItem;
	FillRect( hDC, &rect, bHot ? s_hMenuHotBrush : DarkBrush( COLOR_MENUBAR ) );

	UINT nFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
	if ( nState & ODS_NOACCEL )
	{
		nFlags |= DT_HIDEPREFIX;
	}

	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, bDisabled ? s_ColorGrayText : s_ColorText );
	DrawTextW( hDC, wszText, -1, &rect, nFlags );

	return true;
}

// The light line user32 leaves between the menu bar and the client area
static void DrawMenuBarBottomLine( HWND hWnd )
{
	MENUBARINFO info = { sizeof( info ) };
	if ( !GetMenuBarInfo( hWnd, OBJID_MENU, 0, &info ) )
	{
		return;
	}

	RECT client;
	GetClientRect( hWnd, &client );
	MapWindowPoints( hWnd, nullptr, reinterpret_cast<POINT *>( &client ), 2 );

	RECT window;
	GetWindowRect( hWnd, &window );
	OffsetRect( &client, -window.left, -window.top );

	RECT line = client;
	line.bottom = line.top;
	line.top--;

	HDC hDC = GetWindowDC( hWnd );
	FillRect( hDC, &line, DarkBrush( COLOR_MENUBAR ) );
	ReleaseDC( hWnd, hDC );
}

// Dark brush for a class background given as a system color (COLOR_* + 1) or a system brush:
// user32 resolves those itself, past the hooks
static HBRUSH ClassDarkBrush( HWND hWnd )
{
	const ULONG_PTR nBrush = GetClassLongPtrW( hWnd, GCLP_HBRBACKGROUND );
	if ( !nBrush )
	{
		return nullptr;
	}

	if ( nBrush <= COLOR_MENUBAR + 1 )
	{
		HBRUSH hBrush = DarkBrush( static_cast<int>( nBrush ) - 1 );
		return hBrush;
	}

	for ( int i = 0; i <= COLOR_MENUBAR; i++ )
	{
		if ( reinterpret_cast<HBRUSH>( nBrush ) == reinterpret_cast<GetSysColorBrush_t>( s_pGetSysColorBrush )( i ) )
		{
			HBRUSH hBrush = DarkBrush( i );
			return hBrush;
		}
	}

	return nullptr;
}

static bool IsClass( HWND hWnd, const wchar_t *pwszClass )
{
	wchar_t wszClass[64];
	const bool bMatch = GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) && !_wcsicmp( wszClass, pwszClass );
	
	return bMatch;
}

//-----------------------------------------------------------------------------
// Toolbar disabled images: comctl32 embosses them from a mask that the dark
// face color breaks, so dimmed copies of the normal images are set instead
//-----------------------------------------------------------------------------

static const wchar_t *const s_pwszDisabledProp = L"hppxdark_disabled";

// Key color the images are drawn over; pixels left in it are transparent
static const DWORD s_nKey = 0x00FF00FF;

static void UpdateDisabledImages( HWND hToolbar )
{
	HIMAGELIST hNormal = reinterpret_cast<HIMAGELIST>( SendMessageW( hToolbar, TB_GETIMAGELIST, 0, 0 ) );
	if ( !hNormal )
	{
		return;
	}

	int nWidth = 0;
	int nHeight = 0;
	ImageList_GetIconSize( hNormal, &nWidth, &nHeight );
	const int nCount = ImageList_GetImageCount( hNormal );
	Log( "toolbar %p: images %p, %dx%d, %d", hToolbar, hNormal, nWidth, nHeight, nCount );
	if ( nWidth <= 0 || nHeight <= 0 || nCount <= 0 )
	{
		return;
	}

	HIMAGELIST hDisabled = ImageList_Create( nWidth, nHeight, ILC_COLOR32, nCount, 0 );
	if ( !hDisabled )
	{
		return;
	}

	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof( info.bmiHeader );
	info.bmiHeader.biWidth = nWidth;
	info.bmiHeader.biHeight = -nHeight;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	HDC hDC = CreateCompatibleDC( nullptr );
	for ( int i = 0; i < nCount; i++ )
	{
		DWORD *pPixels = nullptr;
		HBITMAP hBitmap = CreateDIBSection( hDC, &info, DIB_RGB_COLORS, reinterpret_cast<void **>( &pPixels ), nullptr, 0 );
		if ( !hBitmap )
		{
			continue;
		}

		const int nPixels = nWidth * nHeight;
		for ( int n = 0; n < nPixels; n++ )
		{
			pPixels[n] = s_nKey;
		}

		HGDIOBJ hOld = SelectObject( hDC, hBitmap );
		ImageList_Draw( hNormal, i, hDC, 0, 0, ILD_TRANSPARENT );
		GdiFlush();
		SelectObject( hDC, hOld );

		// Gray at 40% opacity, premultiplied
		for ( int n = 0; n < nPixels; n++ )
		{
			const DWORD nColor = pPixels[n] & 0x00FFFFFF;
			if ( nColor == s_nKey )
			{
				pPixels[n] = 0;
				continue;
			}

			const DWORD nRed = ( nColor >> 16 ) & 0xFF;
			const DWORD nGreen = ( nColor >> 8 ) & 0xFF;
			const DWORD nBlue = nColor & 0xFF;
			const DWORD nGray = ( ( nRed * 77 ) + ( nGreen * 150 ) + ( nBlue * 29 ) ) >> 8;
			const DWORD nAlpha = 102;
			const DWORD nValue = ( nGray * nAlpha ) / 255;
			pPixels[n] = ( nAlpha << 24 ) | ( nValue << 16 ) | ( nValue << 8 ) | nValue;
		}

		ImageList_Add( hDisabled, hBitmap, nullptr );
		DeleteObject( hBitmap );
	}

	DeleteDC( hDC );

	HIMAGELIST hPrevious = reinterpret_cast<HIMAGELIST>( SendMessageW( hToolbar, TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>( hDisabled ) ) );
	if ( hPrevious && hPrevious == GetPropW( hToolbar, s_pwszDisabledProp ) )
	{
		ImageList_Destroy( hPrevious );
	}

	SetPropW( hToolbar, s_pwszDisabledProp, hDisabled );
	InvalidateRect( hToolbar, nullptr, TRUE );

	Log(
		"toolbar %p: disabled %p (%d images), previous %p, now %p",
		hToolbar,
		hDisabled,
		ImageList_GetImageCount( hDisabled ),
		hPrevious,
		reinterpret_cast<HIMAGELIST>( SendMessageW( hToolbar, TB_GETDISABLEDIMAGELIST, 0, 0 ) )
	);
}

static void StartColorRefresh( HWND hWnd );
static void ReloadMfcColors( HWND hWnd );

// Set once Hammer++ created its first document window, i.e. finished loading
static bool s_bDocumentSeen = false;

// MFC draws splitter bars and control bar borders with colors it cached at startup, light ones.
// The splitter of a Hammer++ document is a child window of an AfxMDIFrame class
static bool IsSplitter( HWND hWnd )
{
	wchar_t wszClass[64];
	const bool bSplitter = ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & WS_CHILD ) && GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) && !wcsncmp( wszClass, L"AfxMDIFrame", 11 );
	return bSplitter;
}

// The client area left between the panes is the bars: filled flat, panes excluded
static void PaintSplitter( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	for ( HWND hChild = GetWindow( hWnd, GW_CHILD ); hChild; hChild = GetWindow( hChild, GW_HWNDNEXT ) )
	{
		if ( !IsWindowVisible( hChild ) )
		{
			continue;
		}

		RECT rect;
		GetWindowRect( hChild, &rect );
		MapWindowPoints( nullptr, hWnd, reinterpret_cast<POINT *>( &rect ), 2 );
		ExcludeClipRect( hDC, rect.left, rect.top, rect.right, rect.bottom );
	}

	FillRect( hDC, &paint.rcPaint, DarkBrush( COLOR_BTNFACE ) );
	EndPaint( hWnd, &paint );
}

// Non-client border and gripper of a control bar, painted over after the default
static void PaintNonClient( HWND hWnd )
{
	RECT window;
	GetWindowRect( hWnd, &window );

	RECT client;
	GetClientRect( hWnd, &client );
	MapWindowPoints( hWnd, nullptr, reinterpret_cast<POINT *>( &client ), 2 );
	OffsetRect( &client, -window.left, -window.top );
	OffsetRect( &window, -window.left, -window.top );

	HDC hDC = GetWindowDC( hWnd );
	ExcludeClipRect( hDC, client.left, client.top, client.right, client.bottom );
	FillRect( hDC, &window, DarkBrush( COLOR_BTNFACE ) );
	ReleaseDC( hWnd, hDC );
}

static const wchar_t *const s_pwszScrollProp = L"hppxdark_scroll";

// Hammer++ views turn their scroll bars on after creation, past the theme set on WM_CREATE
static void ThemeLateScrollBars( HWND hWnd )
{
	const LONG_PTR nStyle = GetWindowLongPtrW( hWnd, GWL_STYLE );
	wchar_t wszClass[64];
	if ( !( nStyle & ( WS_VSCROLL | WS_HSCROLL ) ) || GetPropW( hWnd, s_pwszScrollProp ) || !GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) || wcsncmp( wszClass, L"Afx", 3 ) )
	{
		return;
	}

	SetPropW( hWnd, s_pwszScrollProp, reinterpret_cast<HANDLE>( 1 ) );
	SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
}

// The square between both scroll bars, user32 fills it with the light face color
static void PaintScrollCorner( HWND hWnd )
{
	const LONG_PTR nStyle = GetWindowLongPtrW( hWnd, GWL_STYLE );
	if ( ( nStyle & ( WS_VSCROLL | WS_HSCROLL ) ) != ( WS_VSCROLL | WS_HSCROLL ) )
	{
		return;
	}

	RECT window;
	GetWindowRect( hWnd, &window );

	RECT client;
	GetClientRect( hWnd, &client );
	MapWindowPoints( hWnd, nullptr, reinterpret_cast<POINT *>( &client ), 2 );

	RECT corner = { client.right, client.bottom, client.right + GetSystemMetrics( SM_CXVSCROLL ), client.bottom + GetSystemMetrics( SM_CYHSCROLL ) };
	OffsetRect( &corner, -window.left, -window.top );

	HDC hDC = GetWindowDC( hWnd );
	FillRect( hDC, &corner, DarkBrush( COLOR_BTNFACE ) );
	ReleaseDC( hWnd, hDC );
}

// The 2 px sunken client edge of input controls becomes a 1 px line like the dark combo boxes:
// the outer pixel in the border color, the inner one in the control background
static void PaintClientEdge( HWND hWnd )
{
	if ( !( GetWindowLongPtrW( hWnd, GWL_EXSTYLE ) & WS_EX_CLIENTEDGE ) )
	{
		return;
	}

	wchar_t wszClass[64];
	if ( !GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) )
	{
		return;
	}

	const std::wstring className = wszClass;
	const bool bEdit = className == WC_EDITW || className == WC_LISTBOXW;
	if ( !bEdit && className != WC_LISTVIEWW && className != WC_TREEVIEWW )
	{
		return;
	}

	if ( !s_hBorderBrush )
	{
		s_hBorderBrush = CreateSolidBrush( s_ColorBorder );
	}

	if ( !s_hEditBrush )
	{
		s_hEditBrush = CreateSolidBrush( s_ColorEdit );
	}

	RECT rect;
	GetWindowRect( hWnd, &rect );
	OffsetRect( &rect, -rect.left, -rect.top );

	HDC hDC = GetWindowDC( hWnd );
	FrameRect( hDC, &rect, s_hBorderBrush );
	InflateRect( &rect, -1, -1 );
	FrameRect( hDC, &rect, bEdit ? s_hEditBrush : DarkBrush( COLOR_WINDOW ) );
	ReleaseDC( hWnd, hDC );
}

// Tab control drawn flat like the inputs: 1 px border around the body and each tab, the selected
// tab in the face color merged with the body, the others darker with dimmed text
static void PaintTabControl( HWND hWnd )
{
	if ( !s_hBorderBrush )
	{
		s_hBorderBrush = CreateSolidBrush( s_ColorBorder );
	}

	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	RECT client;
	GetClientRect( hWnd, &client );

	// Body: the page sits inside it, only the frame and the header strip are painted
	RECT body = client;
	TabCtrl_AdjustRect( hWnd, FALSE, &body );
	InflateRect( &body, 2, 2 );

	const int nSaved = SaveDC( hDC );
	RECT inside = body;
	InflateRect( &inside, -1, -1 );
	ExcludeClipRect( hDC, inside.left, inside.top, inside.right, inside.bottom );
	FillRect( hDC, &client, DarkBrush( COLOR_BTNFACE ) );
	RestoreDC( hDC, nSaved );

	FrameRect( hDC, &body, s_hBorderBrush );

	HFONT hFont = reinterpret_cast<HFONT>( SendMessageW( hWnd, WM_GETFONT, 0, 0 ) );
	HGDIOBJ hOldFont = hFont ? SelectObject( hDC, hFont ) : nullptr;
	SetBkMode( hDC, TRANSPARENT );

	const int nSelected = TabCtrl_GetCurSel( hWnd );
	const int nCount = TabCtrl_GetItemCount( hWnd );
	for ( int i = 0; i < nCount; i++ )
	{
		RECT tab;
		if ( !TabCtrl_GetItemRect( hWnd, i, &tab ) )
		{
			continue;
		}

		const bool bSelected = i == nSelected;
		FillRect( hDC, &tab, DarkBrush( bSelected ? COLOR_BTNFACE : COLOR_WINDOW ) );
		FrameRect( hDC, &tab, s_hBorderBrush );

		// The selected tab opens into the body
		if ( bSelected && tab.bottom >= body.top - 1 )
		{
			RECT gap = { tab.left + 1, body.top, tab.right - 1, body.top + 1 };
			FillRect( hDC, &gap, DarkBrush( COLOR_BTNFACE ) );
		}

		SetTextColor( hDC, bSelected ? s_ColorText : s_ColorGrayText );
		const UINT nFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX;

		// Hammer++ creates ANSI tab controls: their text is read and drawn as ANSI
		if ( IsWindowUnicode( hWnd ) )
		{
			wchar_t wszText[128] = {};
			TCITEMW item = {};
			item.mask = TCIF_TEXT;
			item.pszText = wszText;
			item.cchTextMax = ARRAYSIZE( wszText );
			SendMessageW( hWnd, TCM_GETITEMW, i, reinterpret_cast<LPARAM>( &item ) );
			DrawTextW( hDC, wszText, -1, &tab, nFormat );
		}
		else
		{
			char szText[128] = {};
			TCITEMA item = {};
			item.mask = TCIF_TEXT;
			item.pszText = szText;
			item.cchTextMax = ARRAYSIZE( szText );
			SendMessageA( hWnd, TCM_GETITEMA, i, reinterpret_cast<LPARAM>( &item ) );
			DrawTextA( hDC, szText, -1, &tab, nFormat );
		}
	}

	if ( hOldFont )
	{
		SelectObject( hDC, hOldFont );
	}

	EndPaint( hWnd, &paint );
}

// Status bar panes: the sunken 3D edge drawn over with the flat 1 px border of the inputs
static void PaintStatusBarBorders( HWND hWnd )
{
	if ( !s_hBorderBrush )
	{
		s_hBorderBrush = CreateSolidBrush( s_ColorBorder );
	}

	const int nParts = static_cast<int>( SendMessageW( hWnd, SB_GETPARTS, 0, 0 ) );

	HDC hDC = GetDC( hWnd );
	for ( int i = 0; i < nParts; i++ )
	{
		RECT rect;
		if ( SendMessageW( hWnd, SB_GETRECT, i, reinterpret_cast<LPARAM>( &rect ) ) )
		{
			FrameRect( hDC, &rect, s_hBorderBrush );
		}
	}

	ReleaseDC( hWnd, hDC );
}

static bool IsGroupBox( HWND hWnd )
{
	const bool bGroupBox = ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & BS_TYPEMASK ) == BS_GROUPBOX && IsClass( hWnd, WC_BUTTONW );
	
	return bGroupBox;
}

// Group box like the tab controls: 1 px border, the caption over it on the face color.
// The inside is left alone, the grouped controls are windows of their own
static void PaintGroupBox( HWND hWnd )
{
	if ( !s_hBorderBrush )
	{
		s_hBorderBrush = CreateSolidBrush( s_ColorBorder );
	}

	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	HFONT hFont = reinterpret_cast<HFONT>( SendMessageW( hWnd, WM_GETFONT, 0, 0 ) );
	HGDIOBJ hOldFont = hFont ? SelectObject( hDC, hFont ) : nullptr;

	wchar_t wszText[256] = {};
	const int nLength = GetWindowTextW( hWnd, wszText, ARRAYSIZE( wszText ) );

	SIZE textSize = {};
	GetTextExtentPoint32W( hDC, nLength ? wszText : L"X", nLength ? nLength : 1, &textSize );

	RECT client;
	GetClientRect( hWnd, &client );

	RECT frame = client;
	frame.top += textSize.cy / 2;
	FrameRect( hDC, &frame, s_hBorderBrush );

	if ( nLength )
	{
		RECT caption = { client.left + 6, client.top, client.left + 6 + textSize.cx + 4, client.top + textSize.cy };
		FillRect( hDC, &caption, DarkBrush( COLOR_BTNFACE ) );

		SetBkMode( hDC, TRANSPARENT );
		SetTextColor( hDC, IsWindowEnabled( hWnd ) ? s_ColorText : s_ColorGrayText );
		DrawTextW( hDC, wszText, nLength, &caption, DT_CENTER | DT_SINGLELINE | DT_HIDEPREFIX );
	}

	if ( hOldFont )
	{
		SelectObject( hDC, hOldFont );
	}

	EndPaint( hWnd, &paint );
}

// Property sheet pages get the light tab texture from comctl32, which also answers WM_CTLCOLOR*
// of their controls with its own brush. Child dialogs drop it to take the dark colors
static RECT ChildRect( HWND hParent, HWND hChild )
{
	RECT rect;
	GetWindowRect( hChild, &rect );
	MapWindowPoints( nullptr, hParent, reinterpret_cast<POINT *>( &rect ), 2 );
	return rect;
}

// Group boxes that stop short of the right edge are stretched to keep the left margin on the
// right too. One with another group box to its right (side by side) or ending far from the edge
// keeps its width. Only grows, so it runs again whenever the dialog is resized: comctl32 sizes
// property sheet pages to the tab body after they show
static void AlignGroupBoxes( HWND hDialog )
{
	// Property sheet pages only (a tab control next to them), not the Hammer++ panels
	HWND hSheet = GetParent( hDialog );
	if ( !hSheet || !FindWindowExW( hSheet, nullptr, WC_TABCONTROLW, nullptr ) )
	{
		return;
	}

	RECT client;
	GetClientRect( hDialog, &client );

	for ( HWND hBox = GetWindow( hDialog, GW_CHILD ); hBox; hBox = GetWindow( hBox, GW_HWNDNEXT ) )
	{
		if ( !IsGroupBox( hBox ) )
		{
			continue;
		}

		const RECT box = ChildRect( hDialog, hBox );
		const LONG nTarget = client.right - box.left;
		if ( box.right >= nTarget || box.right < ( client.right * 3 ) / 4 )
		{
			continue;
		}

		bool bNeighbor = false;
		for ( HWND hOther = GetWindow( hDialog, GW_CHILD ); hOther && !bNeighbor; hOther = GetWindow( hOther, GW_HWNDNEXT ) )
		{
			if ( hOther == hBox || !IsGroupBox( hOther ) )
			{
				continue;
			}

			const RECT other = ChildRect( hDialog, hOther );
			bNeighbor = other.left >= box.right - 1 && other.top < box.bottom && other.bottom > box.top;
		}

		if ( !bNeighbor )
		{
			SetWindowPos( hBox, nullptr, 0, 0, nTarget - box.left, box.bottom - box.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
		}
	}
}

static void DisableDialogTexture( HWND hWnd )
{
	if ( ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & WS_CHILD ) && IsClass( hWnd, L"#32770" ) )
	{
		EnableThemeDialogTexture( hWnd, ETDT_DISABLE );
		AlignGroupBoxes( hWnd );
	}
}

// MFC fills toolbars past the subclass chain, so the disabled images are checked before each paint
static void CheckDisabledImages( HWND hToolbar )
{
	HIMAGELIST hNormal = reinterpret_cast<HIMAGELIST>( SendMessageW( hToolbar, TB_GETIMAGELIST, 0, 0 ) );
	HIMAGELIST hDisabled = reinterpret_cast<HIMAGELIST>( GetPropW( hToolbar, s_pwszDisabledProp ) );
	if ( !hNormal )
	{
		return;
	}

	const bool bCurrent = hDisabled && ImageList_GetImageCount( hDisabled ) == ImageList_GetImageCount( hNormal );
	if ( !bCurrent )
	{
		UpdateDisabledImages( hToolbar );
	}
}

static LRESULT CALLBACK DarkSubclassProc( HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam, UINT_PTR nId, DWORD_PTR )
{
	switch ( nMsg )
	{
		case WM_CREATE:
		{
			const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
			ApplyWindowTheme( hWnd );

			return nResult;
		}

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLORMSGBOX:
		case WM_CTLCOLORSCROLLBAR:
		{
			const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
			const LRESULT nColor = CtlColor( nMsg, reinterpret_cast<HDC>( wParam ), nResult );

			return nColor;
		}

		case WM_UAHDRAWMENU:
			DrawMenuBar( hWnd, reinterpret_cast<const UahMenu_t *>( lParam ) );

			return TRUE;

		case WM_UAHDRAWMENUITEM:
		{
			// Comes for popup menu items too: those are left to the dark Windows menus
			const UahDrawMenuItem_t *pItem = reinterpret_cast<const UahDrawMenuItem_t *>( lParam );
			if ( pItem->m_Menu.m_hMenu == GetMenu( hWnd ) && DrawMenuBarItem( pItem ) )
			{
				return TRUE;
			}

			break;
		}

		case WM_NCPAINT:
		case WM_NCACTIVATE:
		{
			ThemeLateScrollBars( hWnd );

			const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
			DrawMenuBarBottomLine( hWnd );
			DrawMenuBarSystemIconNonClient( hWnd );
			PaintScrollCorner( hWnd );
			PaintClientEdge( hWnd );
			if ( IsClass( hWnd, TOOLBARCLASSNAMEW ) )
			{
				PaintNonClient( hWnd );
			}

			return nResult;
		}

		case WM_ERASEBKGND:
		{
			// Painted whole in WM_PAINT
			if ( IsClass( hWnd, WC_TABCONTROLW ) )
			{
				return TRUE;
			}

			HBRUSH hBrush = ClassDarkBrush( hWnd );
			if ( hBrush )
			{
				RECT rect;
				GetClientRect( hWnd, &rect );
				FillRect( reinterpret_cast<HDC>( wParam ), &rect, hBrush );
				return TRUE;
			}

			break;
		}

		case WM_PAINT:
			if ( GetPropW( hWnd, s_pwszMessageBoxProp ) )
			{
				// Text, icon and buttons are child windows: only the background is the dialog's
				PAINTSTRUCT paint;
				HDC hDC = BeginPaint( hWnd, &paint );
				FillRect( hDC, &paint.rcPaint, DarkBrush( COLOR_BTNFACE ) );
				EndPaint( hWnd, &paint );

				return 0;
			}
			else if ( IsClass( hWnd, TOOLBARCLASSNAMEW ) )
			{
				CheckDisabledImages( hWnd );
			}
			else if ( IsSplitter( hWnd ) )
			{
				PaintSplitter( hWnd );

				return 0;
			}
			else if ( IsClass( hWnd, WC_TABCONTROLW ) )
			{
				PaintTabControl( hWnd );

				return 0;
			}
			else if ( IsGroupBox( hWnd ) )
			{
				PaintGroupBox( hWnd );

				return 0;
			}
			else if ( IsClass( hWnd, STATUSCLASSNAMEW ) )
			{
				const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
				PaintStatusBarBorders( hWnd );

				return nResult;
			}

			break;

		case WM_SETCURSOR:
			if ( s_bDocumentSeen )
			{
				ReloadMfcColors( GetAncestor( hWnd, GA_ROOT ) );
			}

			break;

		case WM_SIZE:
			if ( ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & WS_CHILD ) && IsClass( hWnd, L"#32770" ) )
			{
				AlignGroupBoxes( hWnd );
			}

			break;

		case WM_INITDIALOG:
		{
			const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
			DisableDialogTexture( hWnd );
			return nResult;
		}

		case WM_SHOWWINDOW:
			if ( wParam )
			{
				DisableDialogTexture( hWnd );
			}

			if ( wParam )
			{
				StartColorRefresh( hWnd );
			}

			break;

		case WM_NCDESTROY:
		{
			RemovePropW( hWnd, s_pwszScrollProp );
			RemovePropW( hWnd, s_pwszMessageBoxProp );
			HIMAGELIST hDisabled = reinterpret_cast<HIMAGELIST>( RemovePropW( hWnd, s_pwszDisabledProp ) );
			if ( hDisabled )
			{
				ImageList_Destroy( hDisabled );
			}

			RemoveWindowSubclass( hWnd, DarkSubclassProc, nId );

			break;
		}
	}

	const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
	return nResult;
}

static BOOL CALLBACK PostSysColorChange( HWND hWnd, LPARAM )
{
	PostMessageW( hWnd, WM_SYSCOLORCHANGE, 0, 0 );

	return TRUE;
}

// comctl32 cached the light colors before the hooks: once the main frame shows, it and every
// control below it reload them. Showing, not activation: a frame started in the background is
// not activated
static void StartColorRefresh( HWND hWnd )
{
	static bool s_bRefreshed = false;
	wchar_t wszClass[ 64];
	if ( s_bRefreshed || !GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) || wcsncmp( wszClass, L"AfxMDIFrame", 11 ) )
	{
		return;
	}

	s_bRefreshed = true;
	Log( "show %p %ls: color refresh", hWnd, wszClass );

	PostSysColorChange( hWnd, 0 );
	EnumChildWindows( hWnd, PostSysColorChange, 0 );
}

// MFC caches its colors (splitters, control bar edges) at startup and reloads them only in its
// main window, which is assigned long after the frame shows while Hammer++ loads. The first mouse
// move over Hammer++ after the first document window exists is past that: reload and repaint once
static void ReloadMfcColors( HWND hWnd )
{
	static bool s_bDone = false;
	if ( s_bDone )
	{
		return;
	}

	s_bDone = true;
	Log( "idle %p: MFC color reload", hWnd );

	SendMessageW( hWnd, WM_SYSCOLORCHANGE, 0, 0 );
	RedrawWindow( hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN );
}

static LRESULT CALLBACK CbtProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	if ( nCode == HCBT_CREATEWND )
	{
		HookLoadedModules();
		SetWindowSubclass( reinterpret_cast<HWND>( wParam ), DarkSubclassProc, 1, 0 );

		if ( s_nMessageBoxDepth > 0 && IsClass( reinterpret_cast<HWND>( wParam ), L"#32770" ) )
		{
			SetPropW( reinterpret_cast<HWND>( wParam ), s_pwszMessageBoxProp, reinterpret_cast<HANDLE>( 1 ) );
		}

		const CBT_CREATEWNDW *pCreate = reinterpret_cast<const CBT_CREATEWNDW *>( lParam );
		if ( pCreate->lpcs->dwExStyle & WS_EX_MDICHILD )
		{
			s_bDocumentSeen = true;
		}
	}

	const LRESULT nResult = CallNextHookEx( nullptr, nCode, wParam, lParam );
	
	return nResult;
}

static bool IsEnabled( const std::wstring &ini )
{
	const bool bEnabled = ( GetPrivateProfileIntW( L"DarkTheme", L"Enabled", 1, ini.c_str() ) != 0 );

	return bEnabled;
}

void DarkTheme_Init()
{
	const std::wstring ini = ModuleFilePath( L"hppxdark.ini" );
	if ( !IsEnabled( ini ) )
	{
		return;
	}

	HMODULE hUser32  = GetModuleHandleW( L"user32.dll" );
	HMODULE hUxTheme = LoadLibraryExW( L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );

	s_pGetSysColor      = GetProcAddress( hUser32, "GetSysColor" );
	s_pGetSysColorBrush = GetProcAddress( hUser32, "GetSysColorBrush" );
	s_pGetThemeSysColor = hUxTheme ? GetProcAddress( hUxTheme, "GetThemeSysColor" ) : nullptr;
	if ( !s_pGetSysColor || !s_pGetSysColorBrush )
	{
		return;
	}

	s_ColorHooks[ 0 ] = { s_pGetSysColor,      reinterpret_cast<void *>( &HookGetSysColor ) };
	s_ColorHooks[ 1 ] = { s_pGetSysColorBrush, reinterpret_cast<void *>( &HookGetSysColorBrush ) };
	s_ColorHooks[ 2 ] = { s_pGetThemeSysColor, reinterpret_cast<void *>( &HookGetThemeSysColor ) };

	s_pMessageBoxA         = GetProcAddress( hUser32, "MessageBoxA" );
	s_pMessageBoxW         = GetProcAddress( hUser32, "MessageBoxW" );
	s_pMessageBoxIndirectA = GetProcAddress( hUser32, "MessageBoxIndirectA" );
	s_pMessageBoxIndirectW = GetProcAddress( hUser32, "MessageBoxIndirectW" );

	s_ColorHooks[ 3 ] = { s_pMessageBoxA,         reinterpret_cast<void *>( &HookMessageBoxA ) };
	s_ColorHooks[ 4 ] = { s_pMessageBoxW,         reinterpret_cast<void *>( &HookMessageBoxW ) };
	s_ColorHooks[ 5 ] = { s_pMessageBoxIndirectA, reinterpret_cast<void *>( &HookMessageBoxIndirectA ) };
	s_ColorHooks[ 6 ] = { s_pMessageBoxIndirectW, reinterpret_cast<void *>( &HookMessageBoxIndirectW ) };

	InitDarkMode();
	Icons_Init();
	RegisterDllNotification();
	HookLoadedModules();

	SetWindowsHookExW( WH_CBT, CbtProc, nullptr, GetCurrentThreadId() );
	s_bActive = true;
	Log( "dark theme on, thread %lu", GetCurrentThreadId() );
}

void DarkTheme_OnModuleLoaded()
{
	if ( s_bActive )
	{
		HookLoadedModules();
	}
}
