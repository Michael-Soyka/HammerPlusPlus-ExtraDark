//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Menu bar through the undocumented UAH messages of user32
//
//=============================================================================//

#include "theme/menu_bar.h"
#include "theme/system_colors.h"
#include "theme/window_util.h"
#include "core/log.h"

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

void MenuBar_DrawBackground( HWND hWnd, LPARAM lParam )
{
	const UahMenu_t *pMenu = reinterpret_cast<const UahMenu_t *>( lParam );

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

// MIIM_TYPE can't be combined with MIIM_BITMAP: the old style bitmap handle is asked separately
static HBITMAP MenuItemBitmap( HMENU hMenu, int nPosition, const MENUITEMINFOW &info )
{
	if ( info.hbmpItem || !( info.fType & MFT_BITMAP ) )
	{
		return info.hbmpItem;
	}

	MENUITEMINFOW type = { sizeof( type ) };
	type.fMask = MIIM_TYPE;
	if ( !GetMenuItemInfoW( hMenu, nPosition, TRUE, &type ) )
	{
		return nullptr;
	}

	return reinterpret_cast<HBITMAP>( type.dwTypeData );
}

// The system menu item of a maximized MDI document: user32 draws its icon on a light background
static bool IsSystemIconItem( HMENU hMenu, int nPosition )
{
	MENUITEMINFOW info = { sizeof( info ) };
	info.fMask = MIIM_FTYPE | MIIM_BITMAP | MIIM_DATA;
	if ( !GetMenuItemInfoW( hMenu, nPosition, TRUE, &info ) )
	{
		return false;
	}

	HBITMAP hBitmap = MenuItemBitmap( hMenu, nPosition, info );
	HWND hDocument = reinterpret_cast<HWND>( info.dwItemData );
	Log( "menu item %d: type %x, bitmap %p, data %p", nPosition, info.fType, hBitmap, hDocument );

	return hBitmap == HBMMENU_SYSTEM && IsWindow( hDocument );
}

// The icon is hidden under the menu bar color. The item stays and still opens the window menu;
// removing it would make user32 remove the wrong item on restore
static void HideSystemIcon( HDC hDC, const RECT &rect )
{
	FillRect( hDC, &rect, DarkBrush( COLOR_MENUBAR ) );
}

// False for the other bitmap items (MDI document buttons), user32 draws those
static bool DrawBitmapItem( const UahDrawMenuItem_t &item )
{
	if ( !IsSystemIconItem( item.m_Menu.m_hMenu, item.m_Item.m_nPosition ) )
	{
		return false;
	}

	HideSystemIcon( item.m_Menu.m_hDC, item.m_Draw.rcItem );
	return true;
}

static void DrawTextItem( const UahDrawMenuItem_t &item, const wchar_t *pwszText )
{
	const UINT nState = item.m_Draw.itemState;
	const bool bHot = ( nState & ( ODS_HOTLIGHT | ODS_SELECTED ) ) != 0;
	const bool bDisabled = ( nState & ( ODS_INACTIVE | ODS_DISABLED | ODS_GRAYED ) ) != 0;

	HDC hDC = item.m_Menu.m_hDC;
	RECT rect = item.m_Draw.rcItem;
	FillRect( hDC, &rect, bHot ? MenuHotBrush() : DarkBrush( COLOR_MENUBAR ) );

	UINT nFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
	if ( nState & ODS_NOACCEL )
	{
		nFlags |= DT_HIDEPREFIX;
	}

	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, bDisabled ? g_ColorGrayText : g_ColorText );
	DrawTextW( hDC, pwszText, -1, &rect, nFlags );
}

bool MenuBar_DrawItem( HWND hWnd, LPARAM lParam )
{
	const UahDrawMenuItem_t &item = *reinterpret_cast<const UahDrawMenuItem_t *>( lParam );

	// Comes for popup menu items too: those are left to the dark Windows menus
	if ( item.m_Menu.m_hMenu != GetMenu( hWnd ) )
	{
		return false;
	}

	wchar_t wszText[256] = {};
	MENUITEMINFOW info = { sizeof( info ) };
	info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_BITMAP;
	info.dwTypeData = wszText;
	info.cch = ARRAYSIZE( wszText ) - 1;
	if ( !GetMenuItemInfoW( item.m_Menu.m_hMenu, item.m_Item.m_nPosition, TRUE, &info ) )
	{
		return false;
	}

	if ( info.hbmpItem || ( info.fType & MFT_BITMAP ) )
	{
		return DrawBitmapItem( item );
	}

	DrawTextItem( item, wszText );
	return true;
}

// The light line user32 leaves between the menu bar and the client area
static void PaintBottomLine( HWND hWnd, HDC hDC )
{
	RECT line = ClientRectInWindow( hWnd );
	line.bottom = line.top;
	line.top--;

	FillRect( hDC, &line, DarkBrush( COLOR_MENUBAR ) );
}

// user32 may draw the system icon past the UAH messages
static void PaintSystemIcon( HWND hWnd, HMENU hMenu, HDC hDC )
{
	RECT rect;
	if ( !IsSystemIconItem( hMenu, 0 ) || !GetMenuItemRect( hWnd, hMenu, 0, &rect ) )
	{
		return;
	}

	RECT window;
	GetWindowRect( hWnd, &window );
	OffsetRect( &rect, -window.left, -window.top );

	HideSystemIcon( hDC, rect );
}

void MenuBar_PaintNonClient( HWND hWnd )
{
	MENUBARINFO info = { sizeof( info ) };
	HMENU hMenu = GetMenu( hWnd );
	if ( !GetMenuBarInfo( hWnd, OBJID_MENU, 0, &info ) )
	{
		return;
	}

	HDC hDC = GetWindowDC( hWnd );
	PaintBottomLine( hWnd, hDC );
	if ( hMenu )
	{
		PaintSystemIcon( hWnd, hMenu, hDC );
	}

	ReleaseDC( hWnd, hDC );
}
