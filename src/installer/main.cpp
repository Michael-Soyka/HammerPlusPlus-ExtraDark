//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Installer window: asks for the game bin folder, patches Hammer++ in it
//			or removes the patch
//
//=============================================================================//

#include "installer/hammer_dir.h"
#include "installer/patcher.h"
#include "installer/resources.h"
#include "installer/version.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <string>

#pragma comment( linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"" )

static const COLORREF s_nBackground = RGB( 30, 30, 30 );
static const COLORREF s_nField      = RGB( 45, 45, 48 );
static const COLORREF s_nBorder     = RGB( 80, 80, 84 );
static const COLORREF s_nButton     = RGB( 55, 55, 58 );
static const COLORREF s_nPressed    = RGB( 40, 40, 42 );
static const COLORREF s_nAccent     = RGB( 0, 120, 215 );
static const COLORREF s_nText       = RGB( 220, 220, 220 );
static const COLORREF s_nDim        = RGB( 128, 128, 128 );
static const COLORREF s_nGood       = RGB( 110, 190, 110 );
static const COLORREF s_nBad        = RGB( 230, 95, 95 );
static const COLORREF s_nWarning    = RGB( 220, 190, 90 );

// DWMWA_USE_IMMERSIVE_DARK_MODE, Windows 10 20H1+
static const DWORD s_nDarkTitleBar = 20;

// Client area at 96 DPI
static const int s_nWidth  = 600;
static const int s_nHeight = 290;

enum ControlId_t
{
	ID_TITLE = 100,
	ID_LABEL,
	ID_PATH,
	ID_BROWSE,
	ID_STATUS,
	ID_VERSION,
	ID_RESULT,
	ID_PATCH,
	ID_REMOVE,
	ID_REPATCH,
	ID_LAUNCH,
};

struct App_t
{
	HWND		m_hWnd     = nullptr;
	HWND		m_hPath    = nullptr;
	HWND		m_hStatus  = nullptr;
	HWND		m_hVersion = nullptr;
	HWND		m_hResult  = nullptr;
	HWND		m_hPatch   = nullptr;
	HWND		m_hRemove  = nullptr;
	HWND		m_hRepatch = nullptr;
	HWND		m_hLaunch  = nullptr;

	HFONT		m_hFont      = nullptr;
	HFONT		m_hTitleFont = nullptr;
	HBRUSH		m_hBackground = nullptr;
	HBRUSH		m_hField      = nullptr;

	int			m_nDpi = USER_DEFAULT_SCREEN_DPI;

	HammerDir_t	m_Hammer;
	COLORREF	m_nStatusColor = s_nDim;
	COLORREF	m_nResultColor = s_nText;
};

static App_t s_App;

static int Scale( int nValue )
{
	const int nScaled = MulDiv( nValue, s_App.m_nDpi, USER_DEFAULT_SCREEN_DPI );
	return nScaled;
}

static std::wstring GetText( HWND hWnd )
{
	const int nLength = GetWindowTextLengthW( hWnd );
	std::wstring text( nLength, L'\0' );
	GetWindowTextW( hWnd, text.data(), nLength + 1 );

	return text;
}

// Frame of the path field: the edit sits inside it with a padding
static RECT PathFrame()
{
	const RECT frame = { Scale( 16 ), Scale( 84 ), Scale( 486 ), Scale( 116 ) };
	return frame;
}

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------

static std::wstring DescribeVersion( const HammerDir_t &hammer )
{
	const Version_t installer = InstallerVersion();
	std::wstring text = L"Installer version " + FormatVersion( installer );
	if ( hammer.m_nState != HAMMER_STATE_PATCHED )
	{
		return text;
	}

	if ( !hammer.m_bHasVersion )
	{
		text = L"Installed: unknown, older than versioning  \u00b7  Installer: " + FormatVersion( installer ) + L"  \u2014  outdated";
		return text;
	}

	const int nOrder = CompareVersions( hammer.m_Installed, installer );
	const wchar_t *pwszVerdict = nOrder < 0 ? L"outdated" : ( nOrder > 0 ? L"newer than this installer" : L"up to date" );
	text = L"Installed: " + FormatVersion( hammer.m_Installed ) + L"  \u00b7  Installer: " + FormatVersion( installer ) + L"  \u2014  " + pwszVerdict;

	return text;
}

static void Refresh()
{
	s_App.m_Hammer = FindHammerDir( GetText( s_App.m_hPath ) );
	const HammerDir_t &hammer = s_App.m_Hammer;

	std::wstring status;
	switch ( hammer.m_nState )
	{
	case HAMMER_STATE_EMPTY:
		status = L"Enter, browse or drop the game bin folder";
		s_App.m_nStatusColor = s_nDim;
		break;
	case HAMMER_STATE_NO_EXE:
		status = L"hammerplusplus.exe is not found in this folder, its x64 or win64";
		s_App.m_nStatusColor = s_nBad;
		break;
	case HAMMER_STATE_NO_DLL:
		status = L"hammerplusplus\\bin\\hammerplusplus_dll.dll is missing: Hammer++ older than 8873?";
		s_App.m_nStatusColor = s_nBad;
		break;
	case HAMMER_STATE_READY:
		status = L"Hammer++ found: " + hammer.m_Dir.wstring();
		s_App.m_nStatusColor = s_nGood;
		break;
	case HAMMER_STATE_PATCHED:
		status = L"Patched: " + hammer.m_Dir.wstring();
		s_App.m_nStatusColor = s_nWarning;
		break;
	}

	SetWindowTextW( s_App.m_hStatus, status.c_str() );
	SetWindowTextW( s_App.m_hVersion, DescribeVersion( hammer ).c_str() );

	// Not patched: Patch alone. Patched: Remove, Repatch (Update when outdated) and Launch
	const bool bPatched  = hammer.m_nState == HAMMER_STATE_PATCHED;
	const bool bOutdated = bPatched && ( !hammer.m_bHasVersion || CompareVersions( hammer.m_Installed, InstallerVersion() ) < 0 );

	SetWindowTextW( s_App.m_hRepatch, bOutdated ? L"Update" : L"Repatch" );
	EnableWindow( s_App.m_hPatch, hammer.m_nState == HAMMER_STATE_READY );

	// Patch and Launch share a place: hide first, then show
	const HWND hPatchedRow[] = { s_App.m_hRemove, s_App.m_hRepatch, s_App.m_hLaunch };
	if ( bPatched )
	{
		ShowWindow( s_App.m_hPatch, SW_HIDE );
	}

	for ( HWND hButton : hPatchedRow )
	{
		ShowWindow( hButton, bPatched ? SW_SHOW : SW_HIDE );
	}

	if ( !bPatched )
	{
		ShowWindow( s_App.m_hPatch, SW_SHOW );
	}

	InvalidateRect( s_App.m_hStatus, nullptr, TRUE );
	InvalidateRect( s_App.m_hPatch, nullptr, TRUE );
	InvalidateRect( s_App.m_hRepatch, nullptr, TRUE );
}

static void SetResult( const std::wstring &text, COLORREF nColor )
{
	s_App.m_nResultColor = nColor;
	SetWindowTextW( s_App.m_hResult, text.c_str() );
	InvalidateRect( s_App.m_hResult, nullptr, TRUE );
}

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------

static void OnPathChange()
{
	SetResult( L"", s_nText );
	Refresh();
}

static void ShowResult( const PatchResult_t &result )
{
	Refresh();
	SetResult( result.m_Output, result.m_bSuccess ? s_nGood : s_nBad );
}

// Patch and Repatch: on a patched Hammer++ PatchHammer rewrites hppxdark.dll
static void OnPatch()
{
	const HammerDir_t &hammer = s_App.m_Hammer;
	if ( hammer.m_nState != HAMMER_STATE_READY && hammer.m_nState != HAMMER_STATE_PATCHED )
	{
		return;
	}

	ShowResult( PatchHammer( hammer.m_Dir ) );
}

static void OnRemove()
{
	const HammerDir_t &hammer = s_App.m_Hammer;
	if ( hammer.m_nState != HAMMER_STATE_PATCHED )
	{
		return;
	}

	ShowResult( RestoreHammer( hammer.m_Dir ) );
}

static void OnLaunch()
{
	const HammerDir_t &hammer = s_App.m_Hammer;
	if ( hammer.m_nState != HAMMER_STATE_PATCHED )
	{
		return;
	}

	const std::filesystem::path exe = hammer.m_Dir / L"hammerplusplus.exe";
	const HINSTANCE hResult = ShellExecuteW( s_App.m_hWnd, L"open", exe.c_str(), nullptr, hammer.m_Dir.c_str(), SW_SHOWNORMAL );

	// ShellExecute reports success with a value above 32
	const bool bStarted = reinterpret_cast<INT_PTR>( hResult ) > 32;
	SetResult( bStarted ? L"Hammer++ started" : L"Failed to start hammerplusplus.exe", bStarted ? s_nGood : s_nBad );
}

static void OnBrowse()
{
	IFileOpenDialog *pDialog = nullptr;
	if ( FAILED( CoCreateInstance( CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &pDialog ) ) ) )
	{
		return;
	}

	DWORD nOptions = 0;
	pDialog->GetOptions( &nOptions );
	pDialog->SetOptions( nOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM );
	pDialog->SetTitle( L"Game bin folder" );

	IShellItem *pItem = nullptr;
	PWSTR pwszPath = nullptr;
	if ( SUCCEEDED( pDialog->Show( s_App.m_hWnd ) ) && SUCCEEDED( pDialog->GetResult( &pItem ) ) && SUCCEEDED( pItem->GetDisplayName( SIGDN_FILESYSPATH, &pwszPath ) ) )
	{
		SetWindowTextW( s_App.m_hPath, pwszPath );
		CoTaskMemFree( pwszPath );
	}

	if ( pItem )
	{
		pItem->Release();
	}

	pDialog->Release();
}

// A dropped file stands for its folder: hammerplusplus.exe works as well as bin
static void OnDropFiles( HDROP hDrop )
{
	wchar_t wszPath[ MAX_PATH ];
	if ( DragQueryFileW( hDrop, 0, wszPath, MAX_PATH ) )
	{
		std::filesystem::path path( wszPath );
		std::error_code error;
		if ( !std::filesystem::is_directory( path, error ) )
		{
			path = path.parent_path();
		}

		SetWindowTextW( s_App.m_hPath, path.c_str() );
	}

	DragFinish( hDrop );
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

static void FillRound( HDC hDC, const RECT &rect, COLORREF nFill, COLORREF nEdge )
{
	HBRUSH hBrush = CreateSolidBrush( nFill );
	HPEN hPen = CreatePen( PS_SOLID, 1, nEdge );
	HGDIOBJ hOldBrush = SelectObject( hDC, hBrush );
	HGDIOBJ hOldPen = SelectObject( hDC, hPen );

	RoundRect( hDC, rect.left, rect.top, rect.right, rect.bottom, Scale( 6 ), Scale( 6 ) );

	SelectObject( hDC, hOldBrush );
	SelectObject( hDC, hOldPen );
	DeleteObject( hBrush );
	DeleteObject( hPen );
}

static void OnDrawButton( const DRAWITEMSTRUCT &item )
{
	const bool bAccent   = item.CtlID == ID_PATCH || item.CtlID == ID_LAUNCH;
	const bool bDisabled = ( item.itemState & ODS_DISABLED ) != 0;
	const bool bPressed  = ( item.itemState & ODS_SELECTED ) != 0;

	COLORREF nFill = bAccent ? s_nAccent : s_nButton;
	if ( bDisabled )
	{
		nFill = s_nButton;
	}
	else if ( bPressed )
	{
		nFill = bAccent ? RGB( 0, 95, 170 ) : s_nPressed;
	}

	HBRUSH hBackground = CreateSolidBrush( s_nBackground );
	FillRect( item.hDC, &item.rcItem, hBackground );
	DeleteObject( hBackground );

	FillRound( item.hDC, item.rcItem, nFill, bDisabled ? s_nBorder : nFill );

	wchar_t wszText[ 64 ];
	GetWindowTextW( item.hwndItem, wszText, ARRAYSIZE( wszText ) );

	RECT text = item.rcItem;
	SetBkMode( item.hDC, TRANSPARENT );
	SetTextColor( item.hDC, bDisabled ? s_nDim : RGB( 255, 255, 255 ) );
	SelectObject( item.hDC, s_App.m_hFont );
	DrawTextW( item.hDC, wszText, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE );

	// DrawFocusRect inverts to orange over the accent, an outline stays readable
	if ( item.itemState & ODS_FOCUS )
	{
		RECT focus = item.rcItem;
		InflateRect( &focus, -Scale( 2 ), -Scale( 2 ) );

		HPEN hPen = CreatePen( PS_SOLID, 1, s_nText );
		HGDIOBJ hOldPen = SelectObject( item.hDC, hPen );
		HGDIOBJ hOldBrush = SelectObject( item.hDC, GetStockObject( NULL_BRUSH ) );
		RoundRect( item.hDC, focus.left, focus.top, focus.right, focus.bottom, Scale( 5 ), Scale( 5 ) );
		SelectObject( item.hDC, hOldBrush );
		SelectObject( item.hDC, hOldPen );
		DeleteObject( hPen );
	}
}

static void OnPaint()
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( s_App.m_hWnd, &paint );

	FillRound( hDC, PathFrame(), s_nField, s_nBorder );

	EndPaint( s_App.m_hWnd, &paint );
}

static COLORREF StaticColor( int nId )
{
	COLORREF nColor = s_nText;
	switch ( nId )
	{
	case ID_LABEL:
	case ID_VERSION:
		nColor = s_nDim;
		break;
	case ID_STATUS:
		nColor = s_App.m_nStatusColor;
		break;
	case ID_RESULT:
		nColor = s_App.m_nResultColor;
		break;
	}

	return nColor;
}

//-----------------------------------------------------------------------------
// Window
//-----------------------------------------------------------------------------

static HWND AddControl( const wchar_t *pwszClass, const wchar_t *pwszText, DWORD nStyle, int nId, int nX, int nY, int nWidth, int nHeight, HFONT hFont )
{
	HWND hControl = CreateWindowExW( 0, pwszClass, pwszText, WS_CHILD | WS_VISIBLE | nStyle, Scale( nX ), Scale( nY ), Scale( nWidth ), Scale( nHeight ), s_App.m_hWnd, reinterpret_cast<HMENU>( static_cast<INT_PTR>( nId ) ), nullptr, nullptr );
	SendMessageW( hControl, WM_SETFONT, reinterpret_cast<WPARAM>( hFont ), FALSE );

	return hControl;
}

static void OnCreate( HWND hWnd )
{
	s_App.m_hWnd = hWnd;

	BOOL bDark = TRUE;
	DwmSetWindowAttribute( hWnd, s_nDarkTitleBar, &bDark, sizeof( bDark ) );

	s_App.m_hFont      = CreateFontW( -Scale( 15 ), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI" );
	s_App.m_hTitleFont = CreateFontW( -Scale( 26 ), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI" );

	AddControl( L"STATIC", L"Hammer++ Extra Dark", SS_LEFT, ID_TITLE, 16, 14, 568, 36, s_App.m_hTitleFont );
	AddControl( L"STATIC", L"Game bin folder", SS_LEFT, ID_LABEL, 16, 62, 568, 20, s_App.m_hFont );

	const int nPadding = 7;
	s_App.m_hPath = AddControl( L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_PATH, 16 + nPadding, 84 + nPadding, 470 - ( 2 * nPadding ), 32 - ( 2 * nPadding ), s_App.m_hFont );
	SendMessageW( s_App.m_hPath, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>( L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\<game>\\bin" ) );

	AddControl( L"BUTTON", L"Browse\u2026", WS_TABSTOP | BS_OWNERDRAW, ID_BROWSE, 494, 84, 90, 32, s_App.m_hFont );

	s_App.m_hStatus  = AddControl( L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS, ID_STATUS, 16, 126, 568, 20, s_App.m_hFont );
	s_App.m_hVersion = AddControl( L"STATIC", L"", SS_LEFT, ID_VERSION, 16, 148, 568, 20, s_App.m_hFont );
	s_App.m_hResult  = AddControl( L"STATIC", L"", SS_LEFT, ID_RESULT, 16, 180, 568, 48, s_App.m_hFont );
	s_App.m_hPatch   = AddControl( L"BUTTON", L"Patch", WS_TABSTOP | BS_OWNERDRAW, ID_PATCH, 444, 242, 140, 34, s_App.m_hFont );
	s_App.m_hRemove  = AddControl( L"BUTTON", L"Remove", WS_TABSTOP | BS_OWNERDRAW, ID_REMOVE, 232, 242, 112, 34, s_App.m_hFont );
	s_App.m_hRepatch = AddControl( L"BUTTON", L"Repatch", WS_TABSTOP | BS_OWNERDRAW, ID_REPATCH, 352, 242, 112, 34, s_App.m_hFont );
	s_App.m_hLaunch  = AddControl( L"BUTTON", L"Launch", WS_TABSTOP | BS_OWNERDRAW, ID_LAUNCH, 472, 242, 112, 34, s_App.m_hFont );

	DragAcceptFiles( hWnd, TRUE );
	Refresh();
	SetFocus( s_App.m_hPath );
}

static LRESULT CALLBACK WindowProc( HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam )
{
	switch ( nMessage )
	{
	case WM_CREATE:
		OnCreate( hWnd );
		return 0;

	case WM_COMMAND:
		if ( LOWORD( wParam ) == ID_PATH && HIWORD( wParam ) == EN_CHANGE )
		{
			OnPathChange();
		}
		else if ( LOWORD( wParam ) == ID_BROWSE && HIWORD( wParam ) == BN_CLICKED )
		{
			OnBrowse();
		}
		else if ( HIWORD( wParam ) == BN_CLICKED && ( LOWORD( wParam ) == ID_PATCH || LOWORD( wParam ) == ID_REPATCH ) )
		{
			OnPatch();
		}
		else if ( LOWORD( wParam ) == ID_REMOVE && HIWORD( wParam ) == BN_CLICKED )
		{
			OnRemove();
		}
		else if ( LOWORD( wParam ) == ID_LAUNCH && HIWORD( wParam ) == BN_CLICKED )
		{
			OnLaunch();
		}
		return 0;

	case WM_DROPFILES:
		OnDropFiles( reinterpret_cast<HDROP>( wParam ) );
		return 0;

	case WM_DRAWITEM:
		OnDrawButton( *reinterpret_cast<const DRAWITEMSTRUCT *>( lParam ) );
		return TRUE;

	case WM_PAINT:
		OnPaint();
		return 0;

	case WM_CTLCOLOREDIT:
	{
		HDC hDC = reinterpret_cast<HDC>( wParam );
		SetTextColor( hDC, s_nText );
		SetBkColor( hDC, s_nField );
		return reinterpret_cast<LRESULT>( s_App.m_hField );
	}

	case WM_CTLCOLORSTATIC:
	{
		HDC hDC = reinterpret_cast<HDC>( wParam );
		SetTextColor( hDC, StaticColor( GetDlgCtrlID( reinterpret_cast<HWND>( lParam ) ) ) );
		SetBkColor( hDC, s_nBackground );
		return reinterpret_cast<LRESULT>( s_App.m_hBackground );
	}

	case WM_DESTROY:
		PostQuitMessage( 0 );
		return 0;
	}

	return DefWindowProcW( hWnd, nMessage, wParam, lParam );
}

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE, PWSTR, int nShowCmd )
{
	SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_SYSTEM_AWARE );
	CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

	s_App.m_nDpi        = static_cast<int>( GetDpiForSystem() );
	s_App.m_hBackground = CreateSolidBrush( s_nBackground );
	s_App.m_hField      = CreateSolidBrush( s_nField );

	WNDCLASSEXW windowClass = { sizeof( windowClass ) };
	windowClass.lpfnWndProc   = WindowProc;
	windowClass.hInstance     = hInstance;
	windowClass.hCursor       = LoadCursorW( nullptr, IDC_ARROW );
	windowClass.hIcon         = static_cast<HICON>( LoadImageW( hInstance, MAKEINTRESOURCEW( IDI_APP ), IMAGE_ICON, GetSystemMetricsForDpi( SM_CXICON, s_App.m_nDpi ), GetSystemMetricsForDpi( SM_CYICON, s_App.m_nDpi ), LR_DEFAULTCOLOR ) );
	windowClass.hIconSm       = static_cast<HICON>( LoadImageW( hInstance, MAKEINTRESOURCEW( IDI_APP ), IMAGE_ICON, GetSystemMetricsForDpi( SM_CXSMICON, s_App.m_nDpi ), GetSystemMetricsForDpi( SM_CYSMICON, s_App.m_nDpi ), LR_DEFAULTCOLOR ) );
	windowClass.hbrBackground = s_App.m_hBackground;
	windowClass.lpszClassName = L"HppxDarkSetup";
	RegisterClassExW( &windowClass );

	// Fixed size: no resizing, no maximize
	const DWORD nStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	RECT window = { 0, 0, Scale( s_nWidth ), Scale( s_nHeight ) };
	AdjustWindowRectExForDpi( &window, nStyle, FALSE, 0, s_App.m_nDpi );

	const std::wstring title = L"Hammer++ Extra Dark " + FormatVersion( InstallerVersion() );
	HWND hWnd = CreateWindowExW( 0, windowClass.lpszClassName, title.c_str(), nStyle, CW_USEDEFAULT, CW_USEDEFAULT, window.right - window.left, window.bottom - window.top, nullptr, nullptr, hInstance, nullptr );
	if ( !hWnd )
	{
		return 1;
	}

	ShowWindow( hWnd, nShowCmd );

	MSG message;
	while ( GetMessageW( &message, nullptr, 0, 0 ) > 0 )
	{
		if ( IsDialogMessageW( hWnd, &message ) )
		{
			continue;
		}

		TranslateMessage( &message );
		DispatchMessageW( &message );
	}

	CoUninitialize();

	return 0;
}
