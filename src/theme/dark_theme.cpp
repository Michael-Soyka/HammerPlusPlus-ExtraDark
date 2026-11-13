//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Dark theme for Hammer++: hooks into the loaded modules, Windows dark mode and the
//			subclass every window of the thread gets
//
//=============================================================================//

#include "theme/dark_theme.h"
#include "theme/control_images.h"
#include "theme/control_paint.h"
#include "theme/icons.h"
#include "theme/menu_bar.h"
#include "theme/message_boxes.h"
#include "theme/system_colors.h"
#include "theme/window_util.h"
#include "core/iat_hook.h"
#include "core/log.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <psapi.h>
#include <uxtheme.h>

#include <string>

static const wchar_t *const s_pwszHammerModule = L"hammerplusplus_dll.dll";

// System color and message box hooks
static ImportHook_t s_Hooks[ 7 ];
static int s_nHooks = 0;

static bool s_bActive = false;

// Set once Hammer++ created its first document window, i.e. finished loading
static bool s_bDocumentSeen = false;

//-----------------------------------------------------------------------------
// Hooked modules: Hammer++ and comctl32, which draws the common controls
//-----------------------------------------------------------------------------

static int HookModule( HMODULE hModule, bool bHammer )
{
	const int nReplaced = HookImports( hModule, s_Hooks, s_nHooks );

	int nIcons = 0;

	if ( bHammer )
	{
		nIcons = Icons_HookModule( hModule );
	}
	else
	{
		nIcons = Icons_HookComctl( hModule );
	}

	Log( "hooked %p: %d color imports, %d resource imports", hModule, nReplaced, nIcons );

	return nReplaced;
}

static bool IsHooked( const HMODULE *phHooked, int nHooked, HMODULE hModule )
{
	for ( int i = 0; i < nHooked; i++ )
	{
		if ( phHooked[ i ] == hModule )
		{
			return true;
		}
	}

	return false;
}

// Loaded modules checked again on the window creation
static void HookLoadedModules()
{
	static HMODULE s_hHooked[ 16 ];
	static int s_nHooked = 0;

	HMODULE hModules[ 1024 ];
	DWORD nNeeded = 0;

	if ( !EnumProcessModules( GetCurrentProcess(), hModules, sizeof( hModules ), &nNeeded ) )
	{
		return;
	}

	const int nModules = min( static_cast<int>( nNeeded / sizeof( HMODULE ) ), static_cast<int>( ARRAYSIZE( hModules ) ) );

	for ( int i = 0; i < nModules && s_nHooked < ARRAYSIZE( s_hHooked ); i++ )
	{
		wchar_t wszName[ MAX_PATH ];

		if ( IsHooked( s_hHooked, s_nHooked, hModules[ i ] ) || !GetModuleBaseNameW( GetCurrentProcess(), hModules[ i ], wszName, MAX_PATH ) )
		{
			continue;
		}

		const bool bHammer = !_wcsicmp( wszName, s_pwszHammerModule );

		if ( bHammer || !_wcsicmp( wszName, L"comctl32.dll" ) )
		{
			HookModule( hModules[ i ], bHammer );
			s_hHooked[ s_nHooked++ ] = hModules[ i ];
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

// Under the loader lock: only the import tables are written
static VOID CALLBACK OnDllNotification( ULONG nReason, const LdrDllLoaded_t *pData, PVOID )
{
	if ( nReason != s_nDllLoaded || !pData || !pData->m_pBaseName )
	{
		return;
	}

	const std::wstring name( pData->m_pBaseName->m_pwszBuffer, pData->m_pBaseName->m_nLength / sizeof( wchar_t ) );

	if ( !_wcsicmp( name.c_str(), s_pwszHammerModule ) )
	{
		HookModule( static_cast<HMODULE>( pData->m_pBase ), true );
	}
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

static const WORD s_nAllowDarkModeForWindow = 133;
static const WORD s_nSetPreferredAppMode    = 135;
static const WORD s_nFlushMenuThemes        = 136;

static AllowDarkModeForWindow_t s_pAllowDarkModeForWindow = nullptr;

// Missing exports leave the light look, never a failure
static void InitDarkMode()
{
	HMODULE hUxTheme = LoadLibraryExW( L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );

	if ( !hUxTheme )
	{
		return;
	}

	SetPreferredAppMode_t pSetPreferredAppMode = reinterpret_cast<SetPreferredAppMode_t>( GetProcAddress( hUxTheme, MAKEINTRESOURCEA( s_nSetPreferredAppMode ) ) );
	FlushMenuThemes_t pFlushMenuThemes         = reinterpret_cast<FlushMenuThemes_t>( GetProcAddress( hUxTheme, MAKEINTRESOURCEA( s_nFlushMenuThemes ) ) );

	s_pAllowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindow_t>( GetProcAddress( hUxTheme, MAKEINTRESOURCEA( s_nAllowDarkModeForWindow ) ) );

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

static void ThemeListView( HWND hWnd )
{
	SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
	ListView_SetBkColor( hWnd, g_ColorWindow );
	ListView_SetTextBkColor( hWnd, g_ColorWindow );
	ListView_SetTextColor( hWnd, g_ColorText );

	HWND hHeader = ListView_GetHeader( hWnd );

	if ( hHeader )
	{
		SetWindowTheme( hHeader, L"DarkMode_ItemsView", nullptr );
	}
}

static void ThemeTreeView( HWND hWnd )
{
	SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
	TreeView_SetBkColor( hWnd, g_ColorWindow );
	TreeView_SetTextColor( hWnd, g_ColorText );
}

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

	const std::wstring className = ClassName( hWnd );

	if ( className == WC_LISTVIEWW )
	{
		ThemeListView( hWnd );
	}
	else if ( className == WC_TREEVIEWW )
	{
		ThemeTreeView( hWnd );
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
		// Check box and radio glyphs come from the theme, their captions and the group boxes
		// are painted here
		SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
	}
	else if ( className == TOOLBARCLASSNAMEW || className == STATUSCLASSNAMEW || className == REBARCLASSNAMEW || className == WC_TABCONTROLW || className == UPDOWN_CLASSW )
	{
		// No dark theme classes for these: classic drawing takes the hooked system colors,
		// toolbar buttons and up-down controls are painted flat here
		SetWindowTheme( hWnd, L"", L"" );
	}
	else if ( className == WC_SCROLLBARW || ( nStyle & ( WS_VSCROLL | WS_HSCROLL ) ) )
	{
		SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
	}
}

// Property sheet pages get the light tab texture from comctl32, which also answers WM_CTLCOLOR*
// of their controls with its own brush. Child dialogs drop it to take the dark colors
static void DisableDialogTexture( HWND hWnd )
{
	if ( IsChildDialog( hWnd ) )
	{
		EnableThemeDialogTexture( hWnd, ETDT_DISABLE );
		AlignGroupBoxes( hWnd );
	}
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

	if ( s_bRefreshed || !ClassStartsWith( hWnd, L"AfxMDIFrame" ) )
	{
		return;
	}

	s_bRefreshed = true;
	Log( "show %p: color refresh", hWnd );

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

//-----------------------------------------------------------------------------
// Subclass messages
//-----------------------------------------------------------------------------

static LRESULT OnNcPaint( HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam )
{
	ThemeLateScrollBars( hWnd );

	const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
	MenuBar_PaintNonClient( hWnd );
	PaintScrollCorner( hWnd );
	PaintClientEdge( hWnd );

	if ( IsClass( hWnd, TOOLBARCLASSNAMEW ) )
	{
		PaintToolbarNonClient( hWnd );
	}

	return nResult;
}

// False when the default handler erases
static bool OnEraseBackground( HWND hWnd, HDC hDC )
{
	// Painted whole in WM_PAINT
	if ( IsClass( hWnd, WC_TABCONTROLW ) || IsClass( hWnd, UPDOWN_CLASSW ) || IsCheckButton( hWnd ) )
	{
		return true;
	}

	HBRUSH hBrush = ClassDarkBrush( hWnd );

	if ( !hBrush )
	{
		return false;
	}

	RECT rect;
	GetClientRect( hWnd, &rect );
	FillRect( hDC, &rect, hBrush );

	return true;
}

// Controls painted here entirely; false leaves the paint to the default handler
static bool PaintWhole( HWND hWnd )
{
	if ( IsMessageBox( hWnd ) )
	{
		PaintFlat( hWnd );
	}
	else if ( IsSplitter( hWnd ) )
	{
		PaintSplitter( hWnd );
	}
	else if ( IsClass( hWnd, WC_TABCONTROLW ) )
	{
		PaintTabControl( hWnd );
	}
	else if ( IsGroupBox( hWnd ) )
	{
		PaintGroupBox( hWnd );
	}
	else if ( IsCheckButton( hWnd ) )
	{
		PaintCheckButton( hWnd );
	}
	else if ( IsClass( hWnd, UPDOWN_CLASSW ) )
	{
		PaintUpDown( hWnd );
	}
	else
	{
		return false;
	}

	return true;
}

static LRESULT OnPaint( HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam )
{
	if ( PaintWhole( hWnd ) )
	{
		return 0;
	}

	const std::wstring className = ClassName( hWnd );

	if ( className == TOOLBARCLASSNAMEW )
	{
		CheckDisabledImages( hWnd );
	}
	else if ( className == WC_TREEVIEWW )
	{
		CheckTreeCheckBoxes( hWnd );
	}

	const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );

	if ( className == STATUSCLASSNAMEW )
	{
		PaintStatusBarBorders( hWnd );
	}

	return nResult;
}

static bool IsToolbarCustomDraw( LPARAM lParam )
{
	const NMHDR *pHeader = reinterpret_cast<const NMHDR *>( lParam );

	return pHeader && pHeader->code == NM_CUSTOMDRAW && IsClass( pHeader->hwndFrom, TOOLBARCLASSNAMEW );
}

// State changes of check boxes, radio buttons and up-down controls: they redraw themselves
static LRESULT OnStateChange( HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam )
{
	if ( IsCheckButton( hWnd ) )
	{
		const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
		RepaintCheckButton( hWnd );

		return nResult;
	}

	if ( !IsClass( hWnd, UPDOWN_CLASSW ) )
	{
		return DefSubclassProc( hWnd, nMsg, wParam, lParam );
	}

	// Classic up-down controls track no hover: the mouse leaving has to be asked for
	if ( nMsg == WM_MOUSEMOVE )
	{
		TRACKMOUSEEVENT track = { sizeof( track ), TME_LEAVE, hWnd, 0 };
		TrackMouseEvent( &track );
	}

	const LRESULT nResult = DefSubclassProc( hWnd, nMsg, wParam, lParam );
	InvalidateRect( hWnd, nullptr, FALSE );

	return nResult;
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
			return CtlColor( nMsg, reinterpret_cast<HDC>( wParam ), DefSubclassProc( hWnd, nMsg, wParam, lParam ) );

		case WM_UAHDRAWMENU:
			MenuBar_DrawBackground( hWnd, lParam );

			return TRUE;

		case WM_UAHDRAWMENUITEM:
			if ( MenuBar_DrawItem( hWnd, lParam ) )
			{
				return TRUE;
			}

			break;

		case WM_NCPAINT:
		case WM_NCACTIVATE:
			return OnNcPaint( hWnd, nMsg, wParam, lParam );

		case WM_ERASEBKGND:
			if ( OnEraseBackground( hWnd, reinterpret_cast<HDC>( wParam ) ) )
			{
				return TRUE;
			}

			break;

		case WM_PAINT:
			return OnPaint( hWnd, nMsg, wParam, lParam );

		case WM_PRINTCLIENT:
			if ( IsCheckButton( hWnd ) )
			{
				PrintCheckButton( hWnd, reinterpret_cast<HDC>( wParam ) );

				return 0;
			}

			break;

		case WM_NOTIFY:
			if ( IsToolbarCustomDraw( lParam ) )
			{
				return ToolbarCustomDraw( reinterpret_cast<NMTBCUSTOMDRAW *>( lParam ) );
			}

			break;

		case BM_SETCHECK:
		case BM_SETSTATE:
		case WM_ENABLE:
		case WM_SETTEXT:
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
		case WM_UPDATEUISTATE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_MOUSELEAVE:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CAPTURECHANGED:
			return OnStateChange( hWnd, nMsg, wParam, lParam );

		case WM_SETCURSOR:
			if ( s_bDocumentSeen )
			{
				ReloadMfcColors( GetAncestor( hWnd, GA_ROOT ) );
			}

			break;

		case WM_SIZE:
			if ( IsChildDialog( hWnd ) )
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
				StartColorRefresh( hWnd );
			}

			break;

		case WM_DESTROY:
			ReleaseTreeCheckBoxes( hWnd );
			break;

		case WM_NCDESTROY:
			ForgetLateScrollBars( hWnd );
			UnmarkMessageBox( hWnd );
			ReleaseDisabledImages( hWnd );
			RemoveWindowSubclass( hWnd, DarkSubclassProc, nId );
			break;
	}

	return DefSubclassProc( hWnd, nMsg, wParam, lParam );
}

//-----------------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------------

static LRESULT CALLBACK CbtProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	if ( nCode == HCBT_CREATEWND )
	{
		HWND hWnd = reinterpret_cast<HWND>( wParam );
		HookLoadedModules();
		SetWindowSubclass( hWnd, DarkSubclassProc, 1, 0 );
		MarkMessageBox( hWnd );

		const CBT_CREATEWNDW *pCreate = reinterpret_cast<const CBT_CREATEWNDW *>( lParam );

		if ( pCreate->lpcs->dwExStyle & WS_EX_MDICHILD )
		{
			s_bDocumentSeen = true;
		}
	}

	return CallNextHookEx( nullptr, nCode, wParam, lParam );
}

void DarkTheme_Init()
{
	const std::wstring ini = ModuleFilePath( L"hppxdark.ini" );

	if ( !GetPrivateProfileIntW( L"DarkTheme", L"Enabled", 1, ini.c_str() ) )
	{
		return;
	}

	LoadPalette( ini );

	if ( !SystemColors_Init() )
	{
		return;
	}

	s_nHooks = SystemColors_AddHooks( s_Hooks );
	s_nHooks += MessageBoxes_AddHooks( s_Hooks + s_nHooks );

	InitDarkMode();
	Icons_Init( g_ColorWindow );
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
