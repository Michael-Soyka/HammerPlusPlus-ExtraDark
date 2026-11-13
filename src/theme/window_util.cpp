//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Window class and geometry helpers shared by the theme code
//
//=============================================================================//

#include "theme/window_util.h"

#include <wchar.h>

std::wstring ClassName( HWND hWnd )
{
	wchar_t wszClass[ 64 ];

	if ( !GetClassNameW( hWnd, wszClass, ARRAYSIZE( wszClass ) ) )
	{
		return {};
	}

	return wszClass;
}

bool IsClass( HWND hWnd, const wchar_t *pwszClass )
{
	return !_wcsicmp( ClassName( hWnd ).c_str(), pwszClass );
}

bool ClassStartsWith( HWND hWnd, const wchar_t *pwszPrefix )
{
	return !wcsncmp( ClassName( hWnd ).c_str(), pwszPrefix, wcslen( pwszPrefix ) );
}

bool IsChildDialog( HWND hWnd )
{
	return ( GetWindowLongPtrW( hWnd, GWL_STYLE ) & WS_CHILD ) && IsClass( hWnd, L"#32770" );
}

RECT ChildRect( HWND hParent, HWND hChild )
{
	RECT rect;
	GetWindowRect( hChild, &rect );
	MapWindowPoints( nullptr, hParent, reinterpret_cast<POINT *>( &rect ), 2 );

	return rect;
}

RECT WindowRectAtOrigin( HWND hWnd )
{
	RECT rect;
	GetWindowRect( hWnd, &rect );
	OffsetRect( &rect, -rect.left, -rect.top );

	return rect;
}

RECT ClientRectInWindow( HWND hWnd )
{
	RECT window;
	GetWindowRect( hWnd, &window );

	RECT client;
	GetClientRect( hWnd, &client );
	MapWindowPoints( hWnd, nullptr, reinterpret_cast<POINT *>( &client ), 2 );
	OffsetRect( &client, -window.left, -window.top );

	return client;
}
