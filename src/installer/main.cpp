//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Installer window: asks for the game bin folder, patches Hammer++ in it
//			or removes the patch
//
//=============================================================================//

#include "installer/hammer_dir.h"
#include "installer/hammer_paths.h"
#include "installer/patcher.h"
#include "installer/resources.h"
#include "installer/version.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <string>

#pragma comment( linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"" )

static const COLORREF s_nBackground    = RGB( 30, 30, 30 );
static const COLORREF s_nField         = RGB( 45, 45, 48 );
static const COLORREF s_nBorder        = RGB( 80, 80, 84 );
static const COLORREF s_nButton        = RGB( 55, 55, 58 );
static const COLORREF s_nPressed       = RGB( 40, 40, 42 );
static const COLORREF s_nAccent        = RGB( 0, 120, 215 );
static const COLORREF s_nAccentPressed = RGB( 0, 95, 170 );
static const COLORREF s_nButtonText    = RGB( 255, 255, 255 );
static const COLORREF s_nText          = RGB( 220, 220, 220 );
static const COLORREF s_nDim           = RGB( 128, 128, 128 );
static const COLORREF s_nGood          = RGB( 110, 190, 110 );
static const COLORREF s_nBad           = RGB( 230, 95, 95 );
static const COLORREF s_nWarning       = RGB( 220, 190, 90 );

// DWMWA_USE_IMMERSIVE_DARK_MODE, Windows 10 20H1+
static const DWORD s_nDarkTitleBar = 20;

// Client area at 96 DPI
static const int s_nWidth  = 600;
static const int s_nHeight = 290;

// ShellExecute reports success with a value above 32
static const INT_PTR s_nShellExecuteOk = 32;

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

// At 96 DPI
struct Box_t
{
	int	m_nX;
	int	m_nY;
	int	m_nWidth;
	int	m_nHeight;
};

// The path edit sits inside a drawn rounded frame with a padding
static const Box_t s_PathFrame    = { 16, 84, 470, 32 };
static const int   s_nPathPadding = 7;

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

	HFONT		m_hFont       = nullptr;
	HFONT		m_hTitleFont  = nullptr;
	HBRUSH		m_hBackground = nullptr;
	HBRUSH		m_hField      = nullptr;

	int			m_nDpi = USER_DEFAULT_SCREEN_DPI;

	HammerDir_t	m_Hammer;
	COLORREF	m_nStatusColor = s_nDim;
	COLORREF	m_nResultColor = s_nText;
};

static App_t s_App;

struct ControlSpec_t
{
	const wchar_t	*m_pwszClass;
	const wchar_t	*m_pwszText;
	DWORD			m_nStyle;
	int				m_nId;
	Box_t			m_Box;
	bool			m_bTitleFont;

	// Where the handle goes, null when nothing refers to the control
	HWND			*m_phControl;
};

struct StatusLine_t
{
	std::wstring	m_Text;
	COLORREF		m_nColor;
};

static int Scale( int nValue )
{
	return MulDiv( nValue, s_App.m_nDpi, USER_DEFAULT_SCREEN_DPI );
}

static RECT ScaleBox( const Box_t &box )
{
	return { Scale( box.m_nX ), Scale( box.m_nY ), Scale( box.m_nX + box.m_nWidth ), Scale( box.m_nY + box.m_nHeight ) };
}

static std::wstring GetText( HWND hWnd )
{
	const int nLength = GetWindowTextLengthW( hWnd );
	std::wstring text( nLength, L'\0' );
	GetWindowTextW( hWnd, text.data(), nLength + 1 );

	return text;
}

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------

static bool IsOutdated( const HammerDir_t &hammer )
{
	return hammer.m_nState == HAMMER_STATE_PATCHED && ( !hammer.m_bHasVersion || CompareVersions( hammer.m_Installed, InstallerVersion() ) < 0 );
}

static std::wstring DescribeVersion( const HammerDir_t &hammer )
{
	const std::wstring installer = FormatVersion( InstallerVersion() );

	if ( hammer.m_nState != HAMMER_STATE_PATCHED )
	{
		return L"Installer version " + installer;
	}

	if ( !hammer.m_bHasVersion )
	{
		return L"Installed: unknown, older than versioning  \u00b7  Installer: " + installer + L"  \u2014  outdated";
	}

	const int nOrder = CompareVersions( hammer.m_Installed, InstallerVersion() );

	const wchar_t *pwszVerdict = L"up to date";

	if ( nOrder < 0 )
	{
		pwszVerdict = L"outdated";
	}
	else if ( nOrder > 0 )
	{
		pwszVerdict = L"newer than this installer";
	}

	return L"Installed: " + FormatVersion( hammer.m_Installed ) + L"  \u00b7  Installer: " + installer + L"  \u2014  " + pwszVerdict;
}

static StatusLine_t DescribeStatus( const HammerDir_t &hammer )
{
	StatusLine_t status = { L"Enter, browse or drop the game bin folder", s_nDim };

	switch ( hammer.m_nState )
	{
		case HAMMER_STATE_EMPTY:
			break;

		case HAMMER_STATE_NO_EXE:
			status = { L"hammerplusplus.exe is not found in this folder, its x64 or win64", s_nBad };
			break;

		case HAMMER_STATE_NO_DLL:
			status = { L"hammerplusplus\\bin\\hammerplusplus_dll.dll is missing: Hammer++ older than 8873?", s_nBad };
			break;

		case HAMMER_STATE_READY:
			status = { L"Hammer++ found: " + hammer.m_Dir.wstring(), s_nGood };
			break;

		case HAMMER_STATE_PATCHED:
			status = { L"Patched: " + hammer.m_Dir.wstring(), s_nWarning };
			break;
	}

	return status;
}

// Not patched: Patch alone. Patched: Remove, Repatch (Update when outdated) and Launch
static void UpdateButtons( const HammerDir_t &hammer )
{
	const bool bPatched = hammer.m_nState == HAMMER_STATE_PATCHED;

	const wchar_t *pwszRepatch = L"Repatch";

	if ( IsOutdated( hammer ) )
	{
		pwszRepatch = L"Update";
	}

	SetWindowTextW( s_App.m_hRepatch, pwszRepatch );
	EnableWindow( s_App.m_hPatch, hammer.m_nState == HAMMER_STATE_READY );

	// Patch and Launch share a place: hide first, then show
	const HWND hPatchedRow[] = { s_App.m_hRemove, s_App.m_hRepatch, s_App.m_hLaunch };

	if ( bPatched )
	{
		ShowWindow( s_App.m_hPatch, SW_HIDE );
	}

	int nShowRow = SW_HIDE;

	if ( bPatched )
	{
		nShowRow = SW_SHOW;
	}

	for ( HWND hButton : hPatchedRow )
	{
		ShowWindow( hButton, nShowRow );
	}

	if ( !bPatched )
	{
		ShowWindow( s_App.m_hPatch, SW_SHOW );
	}
}

static void Refresh()
{
	s_App.m_Hammer = FindHammerDir( GetText( s_App.m_hPath ) );

	const HammerDir_t &hammer = s_App.m_Hammer;
	const StatusLine_t status = DescribeStatus( hammer );

	s_App.m_nStatusColor = status.m_nColor;
	
	SetWindowTextW( s_App.m_hStatus, status.m_Text.c_str() );
	SetWindowTextW( s_App.m_hVersion, DescribeVersion( hammer ).c_str() );

	UpdateButtons( hammer );

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

static void ShowResult( const PatchResult_t &result )
{
	COLORREF nColor = s_nBad;

	if ( result.m_bSuccess )
	{
		nColor = s_nGood;
	}

	Refresh();
	SetResult( result.m_Output, nColor );
}

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------

static void OnPathChange()
{
	SetResult( L"", s_nText );
	Refresh();
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

	const std::filesystem::path exe = MakeHammerPaths( hammer.m_Dir ).m_Exe;

	const HINSTANCE hResult = ShellExecuteW( s_App.m_hWnd, L"open", exe.c_str(), nullptr, hammer.m_Dir.c_str(), SW_SHOWNORMAL );

	const bool bStarted = reinterpret_cast<INT_PTR>( hResult ) > s_nShellExecuteOk;

	if ( bStarted )
	{
		SetResult( L"Hammer++ started", s_nGood );

		return;
	}

	SetResult( L"Failed to start hammerplusplus.exe", s_nBad );
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
	PWSTR pwszPath    = nullptr;

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

static void OnCommand( int nId, int nCode )
{
	if ( nId == ID_PATH )
	{
		if ( nCode == EN_CHANGE )
		{
			OnPathChange();
		}

		return;
	}

	if ( nCode != BN_CLICKED )
	{
		return;
	}

	switch ( nId )
	{
		case ID_BROWSE:
			OnBrowse();
			break;

		case ID_PATCH:
		case ID_REPATCH:
			OnPatch();
			break;

		case ID_REMOVE:
			OnRemove();
			break;

		case ID_LAUNCH:
			OnLaunch();
			break;
	}
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

static void FillRound( HDC hDC, const RECT &rect, COLORREF nFill, COLORREF nEdge )
{
	HBRUSH hBrush     = CreateSolidBrush( nFill );
	HPEN hPen         = CreatePen( PS_SOLID, 1, nEdge );
	HGDIOBJ hOldBrush = SelectObject( hDC, hBrush );
	HGDIOBJ hOldPen   = SelectObject( hDC, hPen );

	RoundRect( hDC, rect.left, rect.top, rect.right, rect.bottom, Scale( 6 ), Scale( 6 ) );

	SelectObject( hDC, hOldBrush );
	SelectObject( hDC, hOldPen );
	DeleteObject( hBrush );
	DeleteObject( hPen );
}

static COLORREF ButtonFillColor( const DRAWITEMSTRUCT &item )
{
	const bool bAccent = item.CtlID == ID_PATCH || item.CtlID == ID_LAUNCH;

	if ( item.itemState & ODS_DISABLED )
	{
		return s_nButton;
	}

	const bool bSelected = ( item.itemState & ODS_SELECTED ) != 0;

	if ( bAccent && bSelected )
	{
		return s_nAccentPressed;
	}

	if ( bSelected )
	{
		return s_nPressed;
	}

	if ( bAccent )
	{
		return s_nAccent;
	}

	return s_nButton;
}

static void DrawButtonText( const DRAWITEMSTRUCT &item, bool bDisabled )
{
	wchar_t wszText[ 64 ];
	GetWindowTextW( item.hwndItem, wszText, ARRAYSIZE( wszText ) );

	RECT text       = item.rcItem;
	COLORREF nColor = s_nButtonText;

	if ( bDisabled )
	{
		nColor = s_nDim;
	}

	SetBkMode( item.hDC, TRANSPARENT );
	SetTextColor( item.hDC, nColor );
	SelectObject( item.hDC, s_App.m_hFont );

	DrawTextW( item.hDC, wszText, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
}

// DrawFocusRect inverts to orange over the accent, an outline stays readable
static void DrawButtonFocus( const DRAWITEMSTRUCT &item )
{
	RECT focus = item.rcItem;
	InflateRect( &focus, -Scale( 2 ), -Scale( 2 ) );

	HPEN hPen         = CreatePen( PS_SOLID, 1, s_nText );
	HGDIOBJ hOldPen   = SelectObject( item.hDC, hPen );
	HGDIOBJ hOldBrush = SelectObject( item.hDC, GetStockObject( NULL_BRUSH ) );

	RoundRect( item.hDC, focus.left, focus.top, focus.right, focus.bottom, Scale( 5 ), Scale( 5 ) );
	SelectObject( item.hDC, hOldBrush );
	SelectObject( item.hDC, hOldPen );

	DeleteObject( hPen );
}

static void OnDrawButton( const DRAWITEMSTRUCT &item )
{
	const bool bDisabled = ( item.itemState & ODS_DISABLED ) != 0;
	const COLORREF nFill = ButtonFillColor( item );
	COLORREF nEdge       = nFill;

	if ( bDisabled )
	{
		nEdge = s_nBorder;
	}

	FillRect( item.hDC, &item.rcItem, s_App.m_hBackground );
	FillRound( item.hDC, item.rcItem, nFill, nEdge );
	DrawButtonText( item, bDisabled );

	if ( item.itemState & ODS_FOCUS )
	{
		DrawButtonFocus( item );
	}
}

static void OnPaint()
{
	PAINTSTRUCT paint;
	
	HDC hDC = BeginPaint( s_App.m_hWnd, &paint );

	FillRound( hDC, ScaleBox( s_PathFrame ), s_nField, s_nBorder );

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

static LRESULT OnCtlColor( HDC hDC, COLORREF nText, COLORREF nBackground, HBRUSH hBrush )
{
	SetTextColor( hDC, nText );
	SetBkColor( hDC, nBackground );

	return reinterpret_cast<LRESULT>( hBrush );
}

//-----------------------------------------------------------------------------
// Window
//-----------------------------------------------------------------------------

static HFONT CreateUiFont( int nSize, int nWeight )
{
	return CreateFontW( -Scale( nSize ), 0, 0, 0, nWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI" );
}

static HICON LoadAppIcon( HINSTANCE hInstance, int nWidthMetric, int nHeightMetric )
{
	const int nWidth  = GetSystemMetricsForDpi( nWidthMetric, s_App.m_nDpi );
	const int nHeight = GetSystemMetricsForDpi( nHeightMetric, s_App.m_nDpi );

	return static_cast<HICON>( LoadImageW( hInstance, MAKEINTRESOURCEW( IDI_APP ), IMAGE_ICON, nWidth, nHeight, LR_DEFAULTCOLOR ) );
}

static void AddControl( const ControlSpec_t &spec )
{
	const Box_t &box = spec.m_Box;
	HWND hControl    = CreateWindowExW( 0, spec.m_pwszClass, spec.m_pwszText, WS_CHILD | WS_VISIBLE | spec.m_nStyle, Scale( box.m_nX ), Scale( box.m_nY ), Scale( box.m_nWidth ), Scale( box.m_nHeight ), s_App.m_hWnd, reinterpret_cast<HMENU>( static_cast<INT_PTR>( spec.m_nId ) ), nullptr, nullptr );

	HFONT hFont = s_App.m_hFont;

	if ( spec.m_bTitleFont )
	{
		hFont = s_App.m_hTitleFont;
	}

	SendMessageW( hControl, WM_SETFONT, reinterpret_cast<WPARAM>( hFont ), FALSE );

	if ( spec.m_phControl )
	{
		*spec.m_phControl = hControl;
	}
}

static void AddControls()
{
	const Box_t pathEdit = { s_PathFrame.m_nX + s_nPathPadding, s_PathFrame.m_nY + s_nPathPadding, s_PathFrame.m_nWidth - ( 2 * s_nPathPadding ), s_PathFrame.m_nHeight - ( 2 * s_nPathPadding ) };
	const DWORD nButton  = WS_TABSTOP | BS_OWNERDRAW;

	const ControlSpec_t controls[] =
	{
		{ L"STATIC", L"Hammer++ Extra Dark", SS_LEFT, ID_TITLE, { 16, 14, 568, 36 }, true, nullptr },
		{ L"STATIC", L"Game bin folder", SS_LEFT, ID_LABEL, { 16, 62, 568, 20 }, false, nullptr },
		{ L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_PATH, pathEdit, false, &s_App.m_hPath },
		{ L"BUTTON", L"Browse\u2026", nButton, ID_BROWSE, { 494, 84, 90, 32 }, false, nullptr },
		{ L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS, ID_STATUS, { 16, 126, 568, 20 }, false, &s_App.m_hStatus },
		{ L"STATIC", L"", SS_LEFT, ID_VERSION, { 16, 148, 568, 20 }, false, &s_App.m_hVersion },
		{ L"STATIC", L"", SS_LEFT, ID_RESULT, { 16, 180, 568, 48 }, false, &s_App.m_hResult },
		{ L"BUTTON", L"Patch", nButton, ID_PATCH, { 444, 242, 140, 34 }, false, &s_App.m_hPatch },
		{ L"BUTTON", L"Remove", nButton, ID_REMOVE, { 232, 242, 112, 34 }, false, &s_App.m_hRemove },
		{ L"BUTTON", L"Repatch", nButton, ID_REPATCH, { 352, 242, 112, 34 }, false, &s_App.m_hRepatch },
		{ L"BUTTON", L"Launch", nButton, ID_LAUNCH, { 472, 242, 112, 34 }, false, &s_App.m_hLaunch },
	};

	for ( const ControlSpec_t &spec : controls )
	{
		AddControl( spec );
	}

	SendMessageW( s_App.m_hPath, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>( L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\<game>\\bin" ) );
}

static void OnCreate( HWND hWnd )
{
	s_App.m_hWnd = hWnd;

	BOOL bDark = TRUE;
	DwmSetWindowAttribute( hWnd, s_nDarkTitleBar, &bDark, sizeof( bDark ) );

	s_App.m_hFont      = CreateUiFont( 15, FW_NORMAL );
	s_App.m_hTitleFont = CreateUiFont( 26, FW_SEMIBOLD );

	AddControls();

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
			OnCommand( LOWORD( wParam ), HIWORD( wParam ) );

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
			return OnCtlColor( reinterpret_cast<HDC>( wParam ), s_nText, s_nField, s_App.m_hField );

		case WM_CTLCOLORSTATIC:
			return OnCtlColor( reinterpret_cast<HDC>( wParam ), StaticColor( GetDlgCtrlID( reinterpret_cast<HWND>( lParam ) ) ), s_nBackground, s_App.m_hBackground );

		case WM_DESTROY:
			PostQuitMessage( 0 );

			return 0;
	}

	return DefWindowProcW( hWnd, nMessage, wParam, lParam );
}

static bool RegisterWindowClass( HINSTANCE hInstance, const wchar_t *pwszName )
{
	WNDCLASSEXW windowClass   = { sizeof( windowClass ) };
	windowClass.lpfnWndProc   = WindowProc;
	windowClass.hInstance     = hInstance;
	windowClass.hCursor       = LoadCursorW( nullptr, IDC_ARROW );
	windowClass.hIcon         = LoadAppIcon( hInstance, SM_CXICON, SM_CYICON );
	windowClass.hIconSm       = LoadAppIcon( hInstance, SM_CXSMICON, SM_CYSMICON );
	windowClass.hbrBackground = s_App.m_hBackground;
	windowClass.lpszClassName = pwszName;

	return RegisterClassExW( &windowClass ) != 0;
}

// Fixed size: no resizing, no maximize
static HWND CreateMainWindow( HINSTANCE hInstance, const wchar_t *pwszClass )
{
	const DWORD nStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	RECT window        = { 0, 0, Scale( s_nWidth ), Scale( s_nHeight ) };
	AdjustWindowRectExForDpi( &window, nStyle, FALSE, 0, s_App.m_nDpi );

	const std::wstring title = L"Hammer++ Extra Dark " + FormatVersion( InstallerVersion() );

	return CreateWindowExW( 0, pwszClass, title.c_str(), nStyle, CW_USEDEFAULT, CW_USEDEFAULT, window.right - window.left, window.bottom - window.top, nullptr, nullptr, hInstance, nullptr );
}

static void RunMessageLoop( HWND hWnd )
{
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
}

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE, PWSTR, int nShowCmd )
{
	SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_SYSTEM_AWARE );
	CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

	s_App.m_nDpi        = static_cast<int>( GetDpiForSystem() );
	s_App.m_hBackground = CreateSolidBrush( s_nBackground );
	s_App.m_hField      = CreateSolidBrush( s_nField );

	const wchar_t *const pwszClass = L"HppxDarkSetup";
	RegisterWindowClass( hInstance, pwszClass );

	HWND hWnd = CreateMainWindow( hInstance, pwszClass );

	if ( !hWnd )
	{
		return 1;
	}

	ShowWindow( hWnd, nShowCmd );
	RunMessageLoop( hWnd );

	CoUninitialize();

	return 0;
}
