//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Controls drawn flat in the dark palette where the themes and the classic drawing
//			can't be made dark
//
//=============================================================================//

#include "theme/control_paint.h"
#include "theme/system_colors.h"
#include "theme/window_util.h"

#include <uxtheme.h>
#include <vssym32.h>

#include <string>

static const wchar_t *const s_pwszScrollProp = L"hppxdark_scroll";

// Check box glyph size when the theme can't tell
static const SIZE s_DefaultGlyph = { 13, 13 };

// Toolbar button corners
static const int s_nToolbarRound = 8;

// The font of a control selected into a DC for the lifetime of the object
struct ControlFont_t
{
	ControlFont_t( HWND hWnd, HDC hDC )
		: m_hDC( hDC )
	{
		HFONT hFont = reinterpret_cast<HFONT>( SendMessageW( hWnd, WM_GETFONT, 0, 0 ) );
		m_hOld = hFont ? SelectObject( hDC, hFont ) : nullptr;
	}

	~ControlFont_t()
	{
		if ( m_hOld )
		{
			SelectObject( m_hDC, m_hOld );
		}
	}

	HDC		m_hDC;
	HGDIOBJ	m_hOld;
};

//-----------------------------------------------------------------------------
// Colors and plain fills
//-----------------------------------------------------------------------------

LRESULT CtlColor( UINT nMsg, HDC hDC, LRESULT nDefault )
{
	// Brushes the application chose itself (color swatches) stay
	if ( !IsSystemBrush( reinterpret_cast<HBRUSH>( nDefault ) ) )
	{
		return nDefault;
	}

	const bool bEdit = nMsg == WM_CTLCOLOREDIT || nMsg == WM_CTLCOLORLISTBOX;
	SetTextColor( hDC, g_ColorText );
	SetBkColor( hDC, bEdit ? g_ColorEdit : g_ColorFace );

	return reinterpret_cast<LRESULT>( bEdit ? EditBrush() : DarkBrush( COLOR_BTNFACE ) );
}

void PaintFlat( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );
	FillRect( hDC, &paint.rcPaint, DarkBrush( COLOR_BTNFACE ) );
	EndPaint( hWnd, &paint );
}

// The splitter of a Hammer++ document is a child window of an AfxMDIFrame class
bool IsSplitter( HWND hWnd )
{
	return ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & WS_CHILD ) && ClassStartsWith( hWnd, L"AfxMDIFrame" );
}

// The client area left between the panes is the bars: filled flat, panes excluded
void PaintSplitter( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	for ( HWND hChild = GetWindow( hWnd, GW_CHILD ); hChild; hChild = GetWindow( hChild, GW_HWNDNEXT ) )
	{
		if ( IsWindowVisible( hChild ) )
		{
			const RECT pane = ChildRect( hWnd, hChild );
			ExcludeClipRect( hDC, pane.left, pane.top, pane.right, pane.bottom );
		}
	}

	FillRect( hDC, &paint.rcPaint, DarkBrush( COLOR_BTNFACE ) );
	EndPaint( hWnd, &paint );
}

//-----------------------------------------------------------------------------
// Non-client area
//-----------------------------------------------------------------------------

// Border and gripper of a control bar
void PaintToolbarNonClient( HWND hWnd )
{
	const RECT window = WindowRectAtOrigin( hWnd );
	const RECT client = ClientRectInWindow( hWnd );

	HDC hDC = GetWindowDC( hWnd );
	ExcludeClipRect( hDC, client.left, client.top, client.right, client.bottom );
	FillRect( hDC, &window, DarkBrush( COLOR_BTNFACE ) );
	ReleaseDC( hWnd, hDC );
}

// The square between both scroll bars, user32 fills it with the light face color
void PaintScrollCorner( HWND hWnd )
{
	const LONG_PTR nBoth = WS_VSCROLL | WS_HSCROLL;
	if ( ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & nBoth ) != nBoth )
	{
		return;
	}

	const RECT client = ClientRectInWindow( hWnd );
	const RECT corner = { client.right, client.bottom, client.right + GetSystemMetrics( SM_CXVSCROLL ), client.bottom + GetSystemMetrics( SM_CYHSCROLL ) };

	HDC hDC = GetWindowDC( hWnd );
	FillRect( hDC, &corner, DarkBrush( COLOR_BTNFACE ) );
	ReleaseDC( hWnd, hDC );
}

// The 2 px sunken client edge of input controls becomes a 1 px line like the dark combo boxes:
// the outer pixel in the border color, the inner one in the control background
void PaintClientEdge( HWND hWnd )
{
	if ( !( GetWindowLongPtrW( hWnd, GWL_EXSTYLE ) & WS_EX_CLIENTEDGE ) )
	{
		return;
	}

	const std::wstring className = ClassName( hWnd );
	const bool bEdit = className == WC_EDITW || className == WC_LISTBOXW;
	if ( !bEdit && className != WC_LISTVIEWW && className != WC_TREEVIEWW )
	{
		return;
	}

	RECT rect = WindowRectAtOrigin( hWnd );

	HDC hDC = GetWindowDC( hWnd );
	FrameRect( hDC, &rect, BorderBrush() );
	InflateRect( &rect, -1, -1 );
	FrameRect( hDC, &rect, bEdit ? EditBrush() : DarkBrush( COLOR_WINDOW ) );
	ReleaseDC( hWnd, hDC );
}

void ThemeLateScrollBars( HWND hWnd )
{
	const bool bScrolls = ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & ( WS_VSCROLL | WS_HSCROLL ) ) != 0;
	if ( !bScrolls || GetPropW( hWnd, s_pwszScrollProp ) || !ClassStartsWith( hWnd, L"Afx" ) )
	{
		return;
	}

	SetPropW( hWnd, s_pwszScrollProp, reinterpret_cast<HANDLE>( 1 ) );
	SetWindowTheme( hWnd, L"DarkMode_Explorer", nullptr );
}

void ForgetLateScrollBars( HWND hWnd )
{
	RemovePropW( hWnd, s_pwszScrollProp );
}

//-----------------------------------------------------------------------------
// Tab control: flat like the inputs, 1 px border around the body and each tab, the selected
// tab in the face color merged with the body, the others darker with dimmed text
//-----------------------------------------------------------------------------

// The page sits inside the body: only its frame and the header strip are painted
static RECT PaintTabBody( HWND hWnd, HDC hDC )
{
	RECT client;
	GetClientRect( hWnd, &client );

	RECT body = client;
	TabCtrl_AdjustRect( hWnd, FALSE, &body );
	InflateRect( &body, 2, 2 );

	const int nSaved = SaveDC( hDC );
	RECT inside = body;
	InflateRect( &inside, -1, -1 );
	ExcludeClipRect( hDC, inside.left, inside.top, inside.right, inside.bottom );
	FillRect( hDC, &client, DarkBrush( COLOR_BTNFACE ) );
	RestoreDC( hDC, nSaved );

	FrameRect( hDC, &body, BorderBrush() );
	return body;
}

// Hammer++ creates ANSI tab controls: their text is read and drawn as ANSI
static void DrawTabText( HWND hWnd, HDC hDC, int nTab, RECT &rect )
{
	const UINT nFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX;
	if ( IsWindowUnicode( hWnd ) )
	{
		wchar_t wszText[128] = {};
		TCITEMW item = {};
		item.mask = TCIF_TEXT;
		item.pszText = wszText;
		item.cchTextMax = ARRAYSIZE( wszText );
		SendMessageW( hWnd, TCM_GETITEMW, nTab, reinterpret_cast<LPARAM>( &item ) );
		DrawTextW( hDC, wszText, -1, &rect, nFormat );
	}
	else
	{
		char szText[128] = {};
		TCITEMA item = {};
		item.mask = TCIF_TEXT;
		item.pszText = szText;
		item.cchTextMax = ARRAYSIZE( szText );
		SendMessageA( hWnd, TCM_GETITEMA, nTab, reinterpret_cast<LPARAM>( &item ) );
		DrawTextA( hDC, szText, -1, &rect, nFormat );
	}
}

static void PaintTab( HWND hWnd, HDC hDC, int nTab, bool bSelected, const RECT &body )
{
	RECT tab;
	if ( !TabCtrl_GetItemRect( hWnd, nTab, &tab ) )
	{
		return;
	}

	FillRect( hDC, &tab, DarkBrush( bSelected ? COLOR_BTNFACE : COLOR_WINDOW ) );
	FrameRect( hDC, &tab, BorderBrush() );

	// The selected tab opens into the body
	if ( bSelected && tab.bottom >= body.top - 1 )
	{
		const RECT gap = { tab.left + 1, body.top, tab.right - 1, body.top + 1 };
		FillRect( hDC, &gap, DarkBrush( COLOR_BTNFACE ) );
	}

	SetTextColor( hDC, bSelected ? g_ColorText : g_ColorGrayText );
	DrawTabText( hWnd, hDC, nTab, tab );
}

void PaintTabControl( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	const RECT body = PaintTabBody( hWnd, hDC );

	{
		const ControlFont_t font( hWnd, hDC );
		SetBkMode( hDC, TRANSPARENT );

		const int nSelected = TabCtrl_GetCurSel( hWnd );
		const int nCount = TabCtrl_GetItemCount( hWnd );
		for ( int i = 0; i < nCount; i++ )
		{
			PaintTab( hWnd, hDC, i, i == nSelected, body );
		}
	}

	EndPaint( hWnd, &paint );
}

//-----------------------------------------------------------------------------
// Status bar and group boxes
//-----------------------------------------------------------------------------

// Panes: the sunken 3D edge drawn over with the flat 1 px border of the inputs
void PaintStatusBarBorders( HWND hWnd )
{
	const int nParts = static_cast<int>( SendMessageW( hWnd, SB_GETPARTS, 0, 0 ) );

	HDC hDC = GetDC( hWnd );
	for ( int i = 0; i < nParts; i++ )
	{
		RECT rect;
		if ( SendMessageW( hWnd, SB_GETRECT, i, reinterpret_cast<LPARAM>( &rect ) ) )
		{
			FrameRect( hDC, &rect, BorderBrush() );
		}
	}

	ReleaseDC( hWnd, hDC );
}

bool IsGroupBox( HWND hWnd )
{
	return ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & BS_TYPEMASK ) == BS_GROUPBOX && IsClass( hWnd, WC_BUTTONW );
}

// Like the tab controls: 1 px border, the caption over it on the face color. The inside is
// left alone, the grouped controls are windows of their own
void PaintGroupBox( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	{
		const ControlFont_t font( hWnd, hDC );

		wchar_t wszText[256] = {};
		const int nLength = GetWindowTextW( hWnd, wszText, ARRAYSIZE( wszText ) );

		// An empty caption still keeps the line at half the text height
		SIZE textSize = {};
		GetTextExtentPoint32W( hDC, nLength ? wszText : L"X", nLength ? nLength : 1, &textSize );

		RECT client;
		GetClientRect( hWnd, &client );

		RECT frame = client;
		frame.top += textSize.cy / 2;
		FrameRect( hDC, &frame, BorderBrush() );

		if ( nLength )
		{
			RECT caption = { client.left + 6, client.top, client.left + 6 + textSize.cx + 4, client.top + textSize.cy };
			FillRect( hDC, &caption, DarkBrush( COLOR_BTNFACE ) );

			SetBkMode( hDC, TRANSPARENT );
			SetTextColor( hDC, IsWindowEnabled( hWnd ) ? g_ColorText : g_ColorGrayText );
			DrawTextW( hDC, wszText, nLength, &caption, DT_CENTER | DT_SINGLELINE | DT_HIDEPREFIX );
		}
	}

	EndPaint( hWnd, &paint );
}

// A group box side by side with this one: another group box right of it at the same height
static bool HasNeighborGroupBox( HWND hDialog, HWND hBox, const RECT &box )
{
	for ( HWND hOther = GetWindow( hDialog, GW_CHILD ); hOther; hOther = GetWindow( hOther, GW_HWNDNEXT ) )
	{
		if ( hOther == hBox || !IsGroupBox( hOther ) )
		{
			continue;
		}

		const RECT other = ChildRect( hDialog, hOther );
		if ( other.left >= box.right - 1 && other.top < box.bottom && other.bottom > box.top )
		{
			return true;
		}
	}

	return false;
}

// Group boxes that stop short of the right edge are stretched to keep the left margin on the
// right too. One with a neighbor or ending far from the edge keeps its width. Only grows, so it
// runs again whenever the dialog is resized: comctl32 sizes property sheet pages to the tab body
// after they show
void AlignGroupBoxes( HWND hDialog )
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
		const bool bFarFromEdge = box.right < ( client.right * 3 ) / 4;
		if ( box.right >= nTarget || bFarFromEdge || HasNeighborGroupBox( hDialog, hBox, box ) )
		{
			continue;
		}

		SetWindowPos( hBox, nullptr, 0, 0, nTarget - box.left, box.bottom - box.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
	}
}

//-----------------------------------------------------------------------------
// Check boxes and radio buttons: the glyph from the dark theme, the caption in the dark text
// color that the themed drawing ignores. Drawn off screen in one go, no flicker
//-----------------------------------------------------------------------------

struct CheckButton_t
{
	LONG_PTR	m_nStyle;
	bool		m_bRadio;
	bool		m_bEnabled;
	LRESULT		m_nState;
	LRESULT		m_nUiState;
};

bool IsCheckButton( HWND hWnd )
{
	const LONG_PTR nStyle = GetWindowLongPtrW( hWnd, GWL_STYLE );
	switch ( nStyle & BS_TYPEMASK )
	{
		case BS_CHECKBOX:
		case BS_AUTOCHECKBOX:
		case BS_3STATE:
		case BS_AUTO3STATE:
		case BS_RADIOBUTTON:
		case BS_AUTORADIOBUTTON:
			return !( nStyle & ( BS_PUSHLIKE | BS_BITMAP | BS_ICON ) ) && IsClass( hWnd, WC_BUTTONW );
	}

	return false;
}

static CheckButton_t ReadCheckButton( HWND hWnd )
{
	CheckButton_t button;
	button.m_nStyle = GetWindowLongPtrW( hWnd, GWL_STYLE );
	const LONG_PTR nType = button.m_nStyle & BS_TYPEMASK;
	button.m_bRadio = nType == BS_RADIOBUTTON || nType == BS_AUTORADIOBUTTON;
	button.m_bEnabled = IsWindowEnabled( hWnd ) != FALSE;
	button.m_nState = SendMessageW( hWnd, BM_GETSTATE, 0, 0 );
	button.m_nUiState = SendMessageW( hWnd, WM_QUERYUISTATE, 0, 0 );

	return button;
}

// 0 unchecked, 1 checked, 2 mixed
static int CheckValue( const CheckButton_t &button )
{
	if ( button.m_nState & BST_CHECKED )
	{
		return 1;
	}

	return ( button.m_nState & BST_INDETERMINATE ) ? 2 : 0;
}

// Part states go in fours, one four per check value: normal, hot, pressed, disabled
static int CheckPartState( const CheckButton_t &button )
{
	int nLook = 0;
	if ( !button.m_bEnabled )
	{
		nLook = 3;
	}
	else if ( button.m_nState & BST_PUSHED )
	{
		nLook = 2;
	}
	else if ( button.m_nState & BST_HOT )
	{
		nLook = 1;
	}

	return ( CheckValue( button ) * 4 ) + nLook + 1;
}

// Top of something nHeight tall in an area by the BS_TOP / BS_BOTTOM / centered style
static LONG AlignVertically( LONG_PTR nVertical, LONG nHeight, LONG nArea )
{
	if ( nVertical == BS_TOP )
	{
		return 0;
	}

	return nVertical == BS_BOTTOM ? nArea - nHeight : ( nArea - nHeight ) / 2;
}

// Returns where the glyph went
static RECT DrawCheckGlyph( HWND hWnd, HDC hDC, const CheckButton_t &button, const RECT &client )
{
	const int nPart = button.m_bRadio ? BP_RADIOBUTTON : BP_CHECKBOX;
	const int nPartState = CheckPartState( button );

	HTHEME hTheme = OpenThemeData( hWnd, L"Button" );
	SIZE glyph = s_DefaultGlyph;
	if ( hTheme )
	{
		GetThemePartSize( hTheme, hDC, nPart, nPartState, nullptr, TS_DRAW, &glyph );
	}

	RECT box;
	box.left = ( button.m_nStyle & BS_LEFTTEXT ) ? client.right - glyph.cx : 0;
	box.right = box.left + glyph.cx;
	box.top = AlignVertically( button.m_nStyle & BS_VCENTER, glyph.cy, client.bottom );
	box.bottom = box.top + glyph.cy;

	if ( hTheme )
	{
		DrawThemeBackground( hTheme, hDC, nPart, nPartState, &box, nullptr );
		CloseThemeData( hTheme );
	}
	else
	{
		const UINT nFrame = ( button.m_bRadio ? DFCS_BUTTONRADIO : DFCS_BUTTONCHECK ) | ( CheckValue( button ) ? DFCS_CHECKED : 0 ) | ( button.m_bEnabled ? 0 : DFCS_INACTIVE );
		DrawFrameControl( hDC, &box, DFC_BUTTON, nFrame );
	}

	return box;
}

static UINT CaptionFormat( const CheckButton_t &button )
{
	const LONG_PTR nHorizontal = button.m_nStyle & BS_CENTER;

	UINT nFormat = ( button.m_nStyle & BS_MULTILINE ) ? DT_WORDBREAK : DT_SINGLELINE;
	nFormat |= nHorizontal == BS_CENTER ? DT_CENTER : ( nHorizontal == BS_RIGHT ? DT_RIGHT : DT_LEFT );
	nFormat |= ( button.m_nUiState & UISF_HIDEACCEL ) ? DT_HIDEPREFIX : 0;

	return nFormat;
}

// Around the text itself, not the whole caption area
static void DrawCaptionFocus( HDC hDC, const RECT &text, LONG nTextWidth, UINT nFormat )
{
	RECT focus = text;
	if ( nFormat & DT_CENTER )
	{
		focus.left += ( text.right - text.left - nTextWidth ) / 2;
	}
	else if ( nFormat & DT_RIGHT )
	{
		focus.left = text.right - nTextWidth;
	}

	focus.right = focus.left + nTextWidth;
	InflateRect( &focus, 1, 0 );
	SetTextColor( hDC, g_ColorText );
	SetBkColor( hDC, RGB( 0, 0, 0 ) );
	DrawFocusRect( hDC, &focus );
}

static void DrawCaption( HWND hWnd, HDC hDC, const CheckButton_t &button, const RECT &client, const RECT &box )
{
	wchar_t wszText[256] = {};
	const int nLength = GetWindowTextW( hWnd, wszText, ARRAYSIZE( wszText ) );
	if ( !nLength )
	{
		return;
	}

	const LONG nGap = ( box.right - box.left ) / 3;
	RECT text = client;
	if ( button.m_nStyle & BS_LEFTTEXT )
	{
		text.right = box.left - nGap;
	}
	else
	{
		text.left = box.right + nGap;
	}

	const UINT nFormat = CaptionFormat( button );
	RECT bounds = text;
	DrawTextW( hDC, wszText, nLength, &bounds, nFormat | DT_CALCRECT );
	const LONG nTextHeight = bounds.bottom - bounds.top;

	text.top = AlignVertically( button.m_nStyle & BS_VCENTER, nTextHeight, client.bottom );
	text.bottom = text.top + nTextHeight;

	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, button.m_bEnabled ? g_ColorText : g_ColorGrayText );
	DrawTextW( hDC, wszText, nLength, &text, nFormat );

	if ( ( button.m_nState & BST_FOCUS ) && !( button.m_nUiState & UISF_HIDEFOCUS ) )
	{
		DrawCaptionFocus( hDC, text, bounds.right - bounds.left, nFormat );
	}
}

// The background the parent gives its static controls, dark when it leaves the default
static HBRUSH CheckButtonBackground( HWND hWnd, HDC hDC )
{
	HWND hParent = GetParent( hWnd );
	HBRUSH hBrush = hParent ? reinterpret_cast<HBRUSH>( SendMessageW( hParent, WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>( hDC ), reinterpret_cast<LPARAM>( hWnd ) ) ) : nullptr;

	return IsSystemBrush( hBrush ) ? DarkBrush( COLOR_BTNFACE ) : hBrush;
}

void PrintCheckButton( HWND hWnd, HDC hTarget )
{
	RECT client;
	GetClientRect( hWnd, &client );
	if ( client.right <= 0 || client.bottom <= 0 )
	{
		return;
	}

	HDC hDC = CreateCompatibleDC( hTarget );
	HBITMAP hBitmap = CreateCompatibleBitmap( hTarget, client.right, client.bottom );
	HGDIOBJ hOldBitmap = SelectObject( hDC, hBitmap );

	FillRect( hDC, &client, CheckButtonBackground( hWnd, hDC ) );

	{
		const ControlFont_t font( hWnd, hDC );
		const CheckButton_t button = ReadCheckButton( hWnd );
		const RECT box = DrawCheckGlyph( hWnd, hDC, button, client );
		DrawCaption( hWnd, hDC, button, client, box );

		BitBlt( hTarget, 0, 0, client.right, client.bottom, hDC, 0, 0, SRCCOPY );
	}

	SelectObject( hDC, hOldBitmap );
	DeleteObject( hBitmap );
	DeleteDC( hDC );
}

void PaintCheckButton( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );
	PrintCheckButton( hWnd, hDC );
	EndPaint( hWnd, &paint );
}

void RepaintCheckButton( HWND hWnd )
{
	// A click can close the dialog
	if ( !IsWindow( hWnd ) || !IsWindowVisible( hWnd ) )
	{
		return;
	}

	HDC hDC = GetDC( hWnd );
	PrintCheckButton( hWnd, hDC );
	ReleaseDC( hWnd, hDC );
}

//-----------------------------------------------------------------------------
// Up-down control: flat like the inputs, 1 px border, the half under the mouse lighter, the
// pressed one lighter still
//-----------------------------------------------------------------------------

struct UpDownHalves_t
{
	// Up or left, then down or right
	RECT	m_Half[2];
	RECT	m_Divider;
};

static UpDownHalves_t SplitUpDown( const RECT &client, bool bHorizontal )
{
	UpDownHalves_t halves = { { client, client }, client };
	if ( bHorizontal )
	{
		halves.m_Half[0].right = client.right / 2;
		halves.m_Half[1].left = halves.m_Half[0].right;
		halves.m_Divider = { halves.m_Half[1].left, client.top, halves.m_Half[1].left + 1, client.bottom };
	}
	else
	{
		halves.m_Half[0].bottom = client.bottom / 2;
		halves.m_Half[1].top = halves.m_Half[0].bottom;
		halves.m_Divider = { client.left, halves.m_Half[1].top, client.right, halves.m_Half[1].top + 1 };
	}

	return halves;
}

// Triangle pointing away from the middle of the control
static void DrawUpDownArrow( HDC hDC, const RECT &half, bool bHorizontal, bool bFirst )
{
	const LONG nX = ( half.left + half.right ) / 2;
	const LONG nY = ( half.top + half.bottom ) / 2;
	const LONG nSize = max( 2L, min( half.right - half.left, half.bottom - half.top ) / 4 );
	const LONG nHalfSize = nSize / 2;
	const LONG nCenter = bHorizontal ? nX : nY;
	const LONG nTip = bFirst ? nCenter - nHalfSize : nCenter + nHalfSize;
	const LONG nBase = bFirst ? nCenter + nHalfSize : nCenter - nHalfSize;

	POINT points[3];
	if ( bHorizontal )
	{
		points[0] = { nTip, nY };
		points[1] = { nBase, nY - nSize };
		points[2] = { nBase, nY + nSize };
	}
	else
	{
		points[0] = { nX, nTip };
		points[1] = { nX - nSize, nBase };
		points[2] = { nX + nSize, nBase };
	}

	Polygon( hDC, points, ARRAYSIZE( points ) );
}

void PaintUpDown( HWND hWnd )
{
	PAINTSTRUCT paint;
	HDC hDC = BeginPaint( hWnd, &paint );

	RECT client;
	GetClientRect( hWnd, &client );

	const bool bHorizontal = ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & UDS_HORZ ) != 0;
	const bool bEnabled = IsWindowEnabled( hWnd ) != FALSE;
	const bool bCaptured = GetCapture() == hWnd;

	POINT cursor;
	GetCursorPos( &cursor );
	const bool bOver = WindowFromPoint( cursor ) == hWnd;
	ScreenToClient( hWnd, &cursor );

	const UpDownHalves_t halves = SplitUpDown( client, bHorizontal );
	const COLORREF arrow = bEnabled ? g_ColorText : g_ColorGrayText;

	const int nSaved = SaveDC( hDC );
	SelectObject( hDC, GetStockObject( DC_BRUSH ) );
	SelectObject( hDC, GetStockObject( DC_PEN ) );

	for ( int i = 0; i < ARRAYSIZE( halves.m_Half ); i++ )
	{
		const RECT &half = halves.m_Half[i];
		const bool bHot = bEnabled && ( bOver || bCaptured ) && PtInRect( &half, cursor );
		COLORREF fill = g_ColorFace;
		if ( bHot )
		{
			fill = bCaptured ? g_ColorHilight : g_ColorMenuHot;
		}

		SetDCBrushColor( hDC, fill );
		FillRect( hDC, &half, static_cast<HBRUSH>( GetStockObject( DC_BRUSH ) ) );

		SetDCBrushColor( hDC, arrow );
		SetDCPenColor( hDC, arrow );
		DrawUpDownArrow( hDC, half, bHorizontal, i == 0 );
	}

	RestoreDC( hDC, nSaved );

	FrameRect( hDC, &client, BorderBrush() );
	FillRect( hDC, &halves.m_Divider, BorderBrush() );

	EndPaint( hWnd, &paint );
}

//-----------------------------------------------------------------------------
// Toolbar buttons flat: no 3D edges, a rounded fill under the hot and pressed ones, checked
// ones also outlined
//-----------------------------------------------------------------------------

LRESULT ToolbarCustomDraw( NMTBCUSTOMDRAW *pDraw )
{
	if ( pDraw->nmcd.dwDrawStage == CDDS_PREPAINT )
	{
		return CDRF_NOTIFYITEMDRAW;
	}

	if ( pDraw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT )
	{
		return CDRF_DODEFAULT;
	}

	const UINT nState = pDraw->nmcd.uItemState;
	const bool bEnabled = !( nState & CDIS_DISABLED );
	const bool bPressed = ( nState & CDIS_SELECTED ) != 0;
	const bool bChecked = ( nState & CDIS_CHECKED ) != 0;
	const bool bHot = ( nState & CDIS_HOT ) != 0;
	if ( bEnabled && ( bPressed || bChecked || bHot ) )
	{
		const COLORREF fill = ( bPressed || ( bChecked && bHot ) ) ? g_ColorHilight : g_ColorMenuHot;
		const RECT &rect = pDraw->nmcd.rc;
		HDC hDC = pDraw->nmcd.hdc;

		const int nSaved = SaveDC( hDC );
		SelectObject( hDC, GetStockObject( DC_BRUSH ) );
		SelectObject( hDC, GetStockObject( DC_PEN ) );
		SetDCBrushColor( hDC, fill );
		SetDCPenColor( hDC, bChecked ? g_ColorBorder : fill );
		RoundRect( hDC, rect.left, rect.top, rect.right, rect.bottom, s_nToolbarRound, s_nToolbarRound );
		RestoreDC( hDC, nSaved );
	}

	pDraw->clrText = bEnabled ? g_ColorText : g_ColorGrayText;

	return TBCDRF_NOEDGES | TBCDRF_NOOFFSET | TBCDRF_NOBACKGROUND | TBCDRF_NOMARK | TBCDRF_NOETCHEDEFFECT;
}
