//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Authors card: slides up from the bottom over a dimmed snapshot of the window,
//			with avatars from GitHub and links to the authors' pages
//
//=============================================================================//

#include "installer/authors.h"

#include "core/gdi.h"
#include "installer/palette.h"
#include "installer/resources.h"

#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <urlmon.h>

#include <filesystem>
#include <memory>
#include <string>
#include <thread>

struct Author_t
{
	// GitHub login, for the avatar
	const wchar_t	*m_pwszLogin;

	// Embedded PNG, for working without internet
	int				m_nAvatar;

	const wchar_t	*m_pwszName;
	const wchar_t	*m_pwszRole;
	COLORREF		m_nColor;

	// Opened by the link icon
	const wchar_t	*m_pwszUrl;
};

static const Author_t s_Authors[] =
{
	{ L"ficool2", IDR_AVATAR_FICOOL2, L"Ficool2", L"Creator of Hammer++", s_nAccent, L"https://github.com/ficool2" },
	{ L"Michael-Soyka", IDR_AVATAR_MICHAEL_SOYKA, L"Michael Soyka (Moon-6)", L"Structure, project direction, tests", s_nGood, L"https://github.com/Michael-Soyka" },
};

static const int s_nAuthors = ARRAYSIZE( s_Authors );

static const wchar_t *const s_pwszClass = L"HppxDarkAuthors";

// At 96 DPI
static const int s_nCardWidth   = 440;
static const int s_nCardPadding = 20;
static const int s_nCardRadius  = 16;
static const int s_nAvatar      = 40;
static const int s_nRowGap      = 18;
static const int s_nTextGap     = 14;
static const int s_nLink        = 14;

// Around the link icon, still a click on it
static const int s_nLinkHit = 8;

// Between the text and the link icon
static const int s_nLinkGap = 12;

static const float     s_flDimAlpha   = 160.0f;
static const ULONGLONG s_nAnimationMs = 240;
static const UINT      s_nFrameMs     = 10;
static const UINT_PTR  s_nTimer       = 1;

// Downloaded larger than drawn: scaled down on load for any DPI
static const int s_nDownloadSize = 160;

static HINSTANCE	s_hInstance = nullptr;
static int			s_nDpi      = USER_DEFAULT_SCREEN_DPI;

static std::unique_ptr<Gdiplus::Bitmap> s_Avatars[ s_nAuthors ];

static int Scale( int nValue )
{
	return MulDiv( nValue, s_nDpi, USER_DEFAULT_SCREEN_DPI );
}

static HFONT CreateUiFont( int nSize, int nWeight )
{
	return CreateFontW( -Scale( nSize ), 0, 0, 0, nWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI" );
}

// A memory DC with its own bitmap
struct Canvas_t
{
	Canvas_t( HDC hReference, int nWidth, int nHeight )
		: m_DC( hReference ), m_Bitmap( CreateCompatibleBitmap( hReference, nWidth, nHeight ) ), m_Selected( m_DC.m_hDC, m_Bitmap.m_hObject )
	{
	}

	Canvas_t( const Canvas_t & ) = delete;
	Canvas_t &operator=( const Canvas_t & ) = delete;

	MemoryDC_t				m_DC;
	GdiObject_t<HBITMAP>	m_Bitmap;
	SelectedObject_t		m_Selected;
};

struct Modal_t
{
	HWND		m_hOverlay = nullptr;

	// Gets the focus back on closing
	HWND		m_hFocus = nullptr;

	std::unique_ptr<Canvas_t>	m_pSnapshot;

	int			m_nWidth  = 0;
	int			m_nHeight = 0;

	// 0 is hidden below the window, 1 is open
	float		m_flFrom = 0.0f;
	float		m_flTo   = 1.0f;
	ULONGLONG	m_nStart = 0;

	// Author whose link is under the cursor, -1 for none
	int			m_nLink = -1;

	GdiObject_t<HFONT>	m_NameFont { CreateUiFont( 15, FW_SEMIBOLD ) };
	GdiObject_t<HFONT>	m_RoleFont { CreateUiFont( 14, FW_NORMAL ) };
	GdiObject_t<HFONT>	m_LetterFont { CreateUiFont( 18, FW_SEMIBOLD ) };
};

static std::unique_ptr<Modal_t> s_pModal;

static LRESULT CALLBACK OverlayProc( HWND hOverlay, UINT nMessage, WPARAM wParam, LPARAM lParam );

//-----------------------------------------------------------------------------
// Geometry
//-----------------------------------------------------------------------------

// Eased out
static float ModalValue( const Modal_t &modal )
{
	float flProgress = static_cast<float>( GetTickCount64() - modal.m_nStart ) / static_cast<float>( s_nAnimationMs );

	if ( flProgress > 1.0f )
	{
		flProgress = 1.0f;
	}

	const float flRest  = 1.0f - flProgress;
	const float flEased = 1.0f - ( flRest * flRest * flRest );

	return modal.m_flFrom + ( ( modal.m_flTo - modal.m_flFrom ) * flEased );
}

static bool IsAnimationOver( const Modal_t &modal )
{
	return GetTickCount64() - modal.m_nStart >= s_nAnimationMs;
}

static RECT CardRect( const Modal_t &modal, float flValue )
{
	const int nWidth   = Scale( s_nCardWidth );
	const int nHeight  = Scale( ( 2 * s_nCardPadding ) + ( s_nAuthors * s_nAvatar ) + ( ( s_nAuthors - 1 ) * s_nRowGap ) );
	const int nLeft    = ( modal.m_nWidth - nWidth ) / 2;
	const int nOpenTop = ( modal.m_nHeight - nHeight ) / 2;
	const int nTop     = nOpenTop + static_cast<int>( ( 1.0f - flValue ) * static_cast<float>( modal.m_nHeight - nOpenTop ) );

	return { nLeft, nTop, nLeft + nWidth, nTop + nHeight };
}

static int RowTop( const RECT &card, int nIndex )
{
	return card.top + Scale( s_nCardPadding ) + ( nIndex * Scale( s_nAvatar + s_nRowGap ) );
}

// Right in the row, centered on the avatar
static RECT LinkRect( const RECT &card, int nIndex )
{
	const int nSize  = Scale( s_nLink );
	const int nRight = card.right - Scale( s_nCardPadding );
	const int nTop   = RowTop( card, nIndex ) + ( ( Scale( s_nAvatar ) - nSize ) / 2 );

	return { nRight - nSize, nTop, nRight, nTop + nSize };
}

static int LinkAt( const Modal_t &modal, POINT point )
{
	const RECT card = CardRect( modal, ModalValue( modal ) );

	for ( int nIndex = 0; nIndex < s_nAuthors; nIndex++ )
	{
		RECT hit = LinkRect( card, nIndex );
		InflateRect( &hit, Scale( s_nLinkHit ), Scale( s_nLinkHit ) );

		if ( PtInRect( &hit, point ) )
		{
			return nIndex;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Avatars
//-----------------------------------------------------------------------------

// The source scaled to a square of nSize pixels, null when it did not decode
static std::unique_ptr<Gdiplus::Bitmap> ScaleAvatar( Gdiplus::Bitmap &source, int nSize )
{
	if ( source.GetLastStatus() != Gdiplus::Ok )
	{
		return nullptr;
	}

	auto pScaled = std::make_unique<Gdiplus::Bitmap>( nSize, nSize, PixelFormat32bppARGB );
	Gdiplus::Graphics graphics( pScaled.get() );
	graphics.SetInterpolationMode( Gdiplus::InterpolationModeHighQualityBicubic );
	graphics.DrawImage( &source, 0, 0, nSize, nSize );

	return pScaled;
}

static std::unique_ptr<Gdiplus::Bitmap> LoadEmbeddedAvatar( int nId )
{
	HRSRC hResource = FindResourceW( s_hInstance, MAKEINTRESOURCEW( nId ), RT_RCDATA );
	HGLOBAL hData   = nullptr;

	if ( hResource )
	{
		hData = LoadResource( s_hInstance, hResource );
	}

	if ( !hData )
	{
		return nullptr;
	}

	const BYTE *pData = static_cast<const BYTE *>( LockResource( hData ) );
	IStream *pStream  = SHCreateMemStream( pData, SizeofResource( s_hInstance, hResource ) );

	if ( !pStream )
	{
		return nullptr;
	}

	std::unique_ptr<Gdiplus::Bitmap> pScaled;

	// The source reads the stream lazily: it goes before the stream
	{
		Gdiplus::Bitmap source( pStream );
		pScaled = ScaleAvatar( source, Scale( s_nAvatar ) );
	}

	pStream->Release();

	return pScaled;
}

static std::filesystem::path DownloadPath( const Author_t &author )
{
	std::error_code error;

	const std::wstring name = L"hppxdark-avatar-" + std::to_wstring( GetCurrentProcessId() ) + L"-" + author.m_pwszLogin + L".png";

	return std::filesystem::temp_directory_path( error ) / name;
}

void InitAuthors( HINSTANCE hInstance, int nDpi )
{
	s_hInstance = hInstance;
	s_nDpi      = nDpi;

	WNDCLASSEXW windowClass   = { sizeof( windowClass ) };
	windowClass.lpfnWndProc   = OverlayProc;
	windowClass.hInstance     = hInstance;
	windowClass.hCursor       = LoadCursorW( nullptr, IDC_ARROW );
	windowClass.lpszClassName = s_pwszClass;

	RegisterClassExW( &windowClass );
}

// Without internet the files just do not appear, and the embedded avatars stay
void LoadAuthorAvatars( HWND hWnd )
{
	for ( int nIndex = 0; nIndex < s_nAuthors; nIndex++ )
	{
		s_Avatars[ nIndex ] = LoadEmbeddedAvatar( s_Authors[ nIndex ].m_nAvatar );
	}

	std::thread( [hWnd]()
	{
		CoInitializeEx( nullptr, COINIT_MULTITHREADED );

		for ( const Author_t &author : s_Authors )
		{
			const std::wstring url = std::wstring( L"https://github.com/" ) + author.m_pwszLogin + L".png?size=" + std::to_wstring( s_nDownloadSize );
			URLDownloadToFileW( nullptr, url.c_str(), DownloadPath( author ).c_str(), 0, nullptr );
		}

		CoUninitialize();
		PostMessageW( hWnd, s_nAuthorAvatarsMessage, 0, 0 );
	} ).detach();
}

void OnAuthorAvatars()
{
	for ( int nIndex = 0; nIndex < s_nAuthors; nIndex++ )
	{
		const std::filesystem::path path = DownloadPath( s_Authors[ nIndex ] );
		std::error_code error;

		if ( !std::filesystem::exists( path, error ) )
		{
			continue;
		}

		// The file stays locked while the source lives
		{
			Gdiplus::Bitmap source( path.c_str() );
			std::unique_ptr<Gdiplus::Bitmap> pScaled = ScaleAvatar( source, Scale( s_nAvatar ) );

			if ( pScaled )
			{
				s_Avatars[ nIndex ] = std::move( pScaled );
			}
		}

		std::filesystem::remove( path, error );
	}

	if ( s_pModal )
	{
		InvalidateRect( s_pModal->m_hOverlay, nullptr, FALSE );
	}
}

void FreeAuthorAvatars()
{
	for ( std::unique_ptr<Gdiplus::Bitmap> &pAvatar : s_Avatars )
	{
		pAvatar.reset();
	}
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

static Gdiplus::Color ToColor( COLORREF nColor )
{
	return Gdiplus::Color( 255, GetRValue( nColor ), GetGValue( nColor ), GetBValue( nColor ) );
}

static void DrawLabel( HDC hDC, const wchar_t *pwszText, RECT rect, HFONT hFont, COLORREF nColor, UINT nFormat )
{
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, nColor );
	const SelectedObject_t font( hDC, hFont );

	DrawTextW( hDC, pwszText, -1, &rect, nFormat | DT_NOPREFIX );
}

static void Dim( HDC hDC, int nWidth, int nHeight, BYTE nAlpha )
{
	const Canvas_t black( hDC, 1, 1 );
	const RECT pixel = { 0, 0, 1, 1 };
	FillRect( black.m_DC.m_hDC, &pixel, static_cast<HBRUSH>( GetStockObject( BLACK_BRUSH ) ) );

	const BLENDFUNCTION blend = { AC_SRC_OVER, 0, nAlpha, 0 };
	AlphaBlend( hDC, 0, 0, nWidth, nHeight, black.m_DC.m_hDC, 0, 0, 1, 1, blend );
}

// A smooth circle with the avatar, or with the author color and the first letter of the name
static void DrawAvatar( HDC hDC, const RECT &rect, int nIndex, HFONT hLetterFont )
{
	const Author_t &author   = s_Authors[ nIndex ];
	Gdiplus::Bitmap *pAvatar = s_Avatars[ nIndex ].get();
	const int nSize          = rect.right - rect.left;

	{
		Gdiplus::Graphics graphics( hDC );
		graphics.SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );

		if ( pAvatar )
		{
			Gdiplus::TextureBrush brush( pAvatar, Gdiplus::WrapModeClamp );
			brush.TranslateTransform( static_cast<Gdiplus::REAL>( rect.left ), static_cast<Gdiplus::REAL>( rect.top ) );
			graphics.FillEllipse( &brush, static_cast<INT>( rect.left ), static_cast<INT>( rect.top ), nSize, nSize );

			return;
		}

		Gdiplus::SolidBrush brush( ToColor( author.m_nColor ) );
		graphics.FillEllipse( &brush, static_cast<INT>( rect.left ), static_cast<INT>( rect.top ), nSize, nSize );
	}

	const wchar_t wszLetter[] = { author.m_pwszName[ 0 ], L'\0' };
	DrawLabel( hDC, wszLetter, rect, hLetterFont, s_nButtonText, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
}

// A box open at the top right with an arrow leaving it, on a 14x14 grid
static void DrawLink( HDC hDC, const RECT &rect, COLORREF nColor )
{
	static const POINT s_Lines[][ 2 ] =
	{
		{ { 0, 3 }, { 0, 14 } },
		{ { 0, 14 }, { 11, 14 } },
		{ { 11, 14 }, { 11, 8 } },
		{ { 0, 3 }, { 6, 3 } },
		{ { 5, 9 }, { 14, 0 } },
		{ { 8, 0 }, { 14, 0 } },
		{ { 14, 0 }, { 14, 6 } },
	};

	const int nSize = rect.right - rect.left;

	Gdiplus::Graphics graphics( hDC );
	graphics.SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );

	Gdiplus::Pen pen( ToColor( nColor ), static_cast<Gdiplus::REAL>( Scale( 15 ) ) / 10.0f );
	pen.SetStartCap( Gdiplus::LineCapRound );
	pen.SetEndCap( Gdiplus::LineCapRound );

	for ( const POINT ( &line )[ 2 ] : s_Lines )
	{
		const INT nFromX = rect.left + ( ( line[ 0 ].x * nSize ) / 14 );
		const INT nFromY = rect.top + ( ( line[ 0 ].y * nSize ) / 14 );
		const INT nToX   = rect.left + ( ( line[ 1 ].x * nSize ) / 14 );
		const INT nToY   = rect.top + ( ( line[ 1 ].y * nSize ) / 14 );

		graphics.DrawLine( &pen, nFromX, nFromY, nToX, nToY );
	}
}

// The avatar, then the name over the role, then the link
static void DrawAuthor( HDC hDC, const Modal_t &modal, const RECT &card, int nIndex )
{
	const Author_t &author = s_Authors[ nIndex ];
	const int nSize        = Scale( s_nAvatar );
	const int nLeft        = card.left + Scale( s_nCardPadding );
	const int nTop         = RowTop( card, nIndex );
	const RECT avatar      = { nLeft, nTop, nLeft + nSize, nTop + nSize };
	const RECT link        = LinkRect( card, nIndex );

	DrawAvatar( hDC, avatar, nIndex, modal.m_LetterFont.m_hObject );

	const int nTextLeft  = avatar.right + Scale( s_nTextGap );
	const int nTextRight = link.left - Scale( s_nLinkGap );
	const int nMiddle    = nTop + ( nSize / 2 );
	const UINT nLine     = DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS;

	DrawLabel( hDC, author.m_pwszName, { nTextLeft, nTop, nTextRight, nMiddle }, modal.m_NameFont.m_hObject, s_nText, nLine | DT_BOTTOM );
	DrawLabel( hDC, author.m_pwszRole, { nTextLeft, nMiddle + Scale( 2 ), nTextRight, avatar.bottom }, modal.m_RoleFont.m_hObject, s_nDim, nLine | DT_TOP );

	COLORREF nLinkColor = s_nDim;

	if ( modal.m_nLink == nIndex )
	{
		nLinkColor = s_nText;
	}

	DrawLink( hDC, link, nLinkColor );
}

static void DrawCard( HDC hDC, const Modal_t &modal, const RECT &card )
{
	const GdiObject_t<HBRUSH> brush( CreateSolidBrush( s_nField ) );
	const GdiObject_t<HPEN> pen( CreatePen( PS_SOLID, 1, s_nBorder ) );
	const SelectedObject_t selectedBrush( hDC, brush.m_hObject );
	const SelectedObject_t selectedPen( hDC, pen.m_hObject );

	RoundRect( hDC, card.left, card.top, card.right, card.bottom, Scale( s_nCardRadius ), Scale( s_nCardRadius ) );

	for ( int nIndex = 0; nIndex < s_nAuthors; nIndex++ )
	{
		DrawAuthor( hDC, modal, card, nIndex );
	}
}

// Composed off screen, so a frame never flickers
static void DrawFrame( HDC hDC, const Modal_t &modal )
{
	const float flValue = ModalValue( modal );
	const Canvas_t frame( hDC, modal.m_nWidth, modal.m_nHeight );

	BitBlt( frame.m_DC.m_hDC, 0, 0, modal.m_nWidth, modal.m_nHeight, modal.m_pSnapshot->m_DC.m_hDC, 0, 0, SRCCOPY );
	Dim( frame.m_DC.m_hDC, modal.m_nWidth, modal.m_nHeight, static_cast<BYTE>( flValue * s_flDimAlpha ) );
	DrawCard( frame.m_DC.m_hDC, modal, CardRect( modal, flValue ) );
	BitBlt( hDC, 0, 0, modal.m_nWidth, modal.m_nHeight, frame.m_DC.m_hDC, 0, 0, SRCCOPY );
}

//-----------------------------------------------------------------------------
// Opening and closing
//-----------------------------------------------------------------------------

void OpenAuthors( HWND hWnd )
{
	if ( s_pModal )
	{
		return;
	}

	RECT client;
	GetClientRect( hWnd, &client );

	auto pModal       = std::make_unique<Modal_t>();
	pModal->m_nWidth  = client.right;
	pModal->m_nHeight = client.bottom;
	pModal->m_hFocus  = GetFocus();

	// Pending paints first, or the snapshot catches controls half drawn
	RedrawWindow( hWnd, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN );

	{
		const WindowDC_t window( hWnd, GetDC( hWnd ) );
		pModal->m_pSnapshot = std::make_unique<Canvas_t>( window.m_hDC, client.right, client.bottom );
		BitBlt( pModal->m_pSnapshot->m_DC.m_hDC, 0, 0, client.right, client.bottom, window.m_hDC, 0, 0, SRCCOPY );
	}

	HWND hOverlay = CreateWindowExW( 0, s_pwszClass, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, client.right, client.bottom, hWnd, nullptr, s_hInstance, nullptr );

	if ( !hOverlay )
	{
		return;
	}

	pModal->m_hOverlay = hOverlay;
	pModal->m_nStart   = GetTickCount64();
	s_pModal           = std::move( pModal );

	SetWindowPos( hOverlay, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
	SetFocus( hOverlay );
	SetTimer( hOverlay, s_nTimer, s_nFrameMs, nullptr );
}

// Slides the card back down from wherever it is now
static void CloseAuthors()
{
	if ( !s_pModal || s_pModal->m_flTo == 0.0f )
	{
		return;
	}

	s_pModal->m_flFrom = ModalValue( *s_pModal );
	s_pModal->m_flTo   = 0.0f;
	s_pModal->m_nStart = GetTickCount64();

	SetTimer( s_pModal->m_hOverlay, s_nTimer, s_nFrameMs, nullptr );
}

static void FinishClose()
{
	const HWND hOverlay = s_pModal->m_hOverlay;
	const HWND hFocus   = s_pModal->m_hFocus;

	s_pModal.reset();

	DestroyWindow( hOverlay );
	SetFocus( hFocus );
}

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------

static void OnOverlayPaint( HWND hOverlay )
{
	const PaintDC_t paint( hOverlay );

	if ( s_pModal )
	{
		DrawFrame( paint.m_hDC, *s_pModal );
	}
}

static void OnOverlayTimer( HWND hOverlay )
{
	InvalidateRect( hOverlay, nullptr, FALSE );

	if ( !s_pModal || !IsAnimationOver( *s_pModal ) )
	{
		return;
	}

	KillTimer( hOverlay, s_nTimer );

	if ( s_pModal->m_flTo == 0.0f )
	{
		FinishClose();
	}
}

// A link opens its page, outside the card closes it, the rest of the card does nothing
static void OnOverlayClick( POINT point )
{
	if ( !s_pModal )
	{
		return;
	}

	const int nLink = LinkAt( *s_pModal, point );

	if ( nLink >= 0 )
	{
		ShellExecuteW( s_pModal->m_hOverlay, L"open", s_Authors[ nLink ].m_pwszUrl, nullptr, nullptr, SW_SHOWNORMAL );

		return;
	}

	const RECT card = CardRect( *s_pModal, ModalValue( *s_pModal ) );

	if ( !PtInRect( &card, point ) )
	{
		CloseAuthors();
	}
}

static void SetHoveredLink( HWND hOverlay, int nLink )
{
	if ( !s_pModal || s_pModal->m_nLink == nLink )
	{
		return;
	}

	s_pModal->m_nLink = nLink;
	InvalidateRect( hOverlay, nullptr, FALSE );
}

// WM_MOUSELEAVE has to be asked for again after each one
static void OnOverlayMouseMove( HWND hOverlay, POINT point )
{
	TRACKMOUSEEVENT track = { sizeof( track ), TME_LEAVE, hOverlay, 0 };
	TrackMouseEvent( &track );

	if ( s_pModal )
	{
		SetHoveredLink( hOverlay, LinkAt( *s_pModal, point ) );
	}
}

// A hand over a link. Comes before WM_MOUSEMOVE, so the position is read here
static bool OnOverlaySetCursor( HWND hOverlay )
{
	POINT point;
	GetCursorPos( &point );
	ScreenToClient( hOverlay, &point );

	const bool bOverLink = s_pModal && LinkAt( *s_pModal, point ) >= 0;

	if ( bOverLink )
	{
		SetCursor( LoadCursorW( nullptr, IDC_HAND ) );
	}

	return bOverLink;
}

static POINT PointFromLParam( LPARAM lParam )
{
	return { static_cast<short>( LOWORD( lParam ) ), static_cast<short>( HIWORD( lParam ) ) };
}

static LRESULT CALLBACK OverlayProc( HWND hOverlay, UINT nMessage, WPARAM wParam, LPARAM lParam )
{
	switch ( nMessage )
	{
		case WM_PAINT:
			OnOverlayPaint( hOverlay );

			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_TIMER:
			OnOverlayTimer( hOverlay );

			return 0;

		case WM_LBUTTONDOWN:
			OnOverlayClick( PointFromLParam( lParam ) );

			return 0;

		case WM_MOUSEMOVE:
			OnOverlayMouseMove( hOverlay, PointFromLParam( lParam ) );

			return 0;

		case WM_MOUSELEAVE:
			SetHoveredLink( hOverlay, -1 );

			return 0;

		case WM_SETCURSOR:
			if ( OnOverlaySetCursor( hOverlay ) )
			{
				return TRUE;
			}

			break;

		case WM_KEYDOWN:
			if ( wParam == VK_ESCAPE )
			{
				CloseAuthors();
			}

			return 0;

		// Esc reaches the overlay through IsDialogMessage
		case WM_GETDLGCODE:
			return DLGC_WANTALLKEYS;
	}

	return DefWindowProcW( hOverlay, nMessage, wParam, lParam );
}
