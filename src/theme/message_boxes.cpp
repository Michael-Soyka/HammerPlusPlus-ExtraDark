//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Message boxes: user32 paints their button strip itself with the light face color.
//			Dialogs created while a MessageBox call runs are marked and painted flat
//
//=============================================================================//

#include "theme/message_boxes.h"
#include "theme/window_util.h"

using MessageBoxA_t         = int ( WINAPI * )( HWND, LPCSTR, LPCSTR, UINT );
using MessageBoxW_t         = int ( WINAPI * )( HWND, LPCWSTR, LPCWSTR, UINT );
using MessageBoxIndirectA_t = int ( WINAPI * )( const MSGBOXPARAMSA * );
using MessageBoxIndirectW_t = int ( WINAPI * )( const MSGBOXPARAMSW * );

static FARPROC s_pMessageBoxA         = nullptr;
static FARPROC s_pMessageBoxW         = nullptr;
static FARPROC s_pMessageBoxIndirectA = nullptr;
static FARPROC s_pMessageBoxIndirectW = nullptr;

static const wchar_t *const s_pwszMessageBoxProp = L"hppxdark_msgbox";

// MessageBox calls in progress, nested ones included
static int s_nDepth = 0;

struct MessageBoxScope_t
{
	MessageBoxScope_t()
	{
		s_nDepth++;
	}

	~MessageBoxScope_t()
	{
		s_nDepth--;
	}
};

static int WINAPI HookMessageBoxA( HWND hWnd, LPCSTR pszText, LPCSTR pszCaption, UINT nType )
{
	const MessageBoxScope_t scope;

	return reinterpret_cast<MessageBoxA_t>( s_pMessageBoxA )( hWnd, pszText, pszCaption, nType );
}

static int WINAPI HookMessageBoxW( HWND hWnd, LPCWSTR pwszText, LPCWSTR pwszCaption, UINT nType )
{
	const MessageBoxScope_t scope;

	return reinterpret_cast<MessageBoxW_t>( s_pMessageBoxW )( hWnd, pwszText, pwszCaption, nType );
}

static int WINAPI HookMessageBoxIndirectA( const MSGBOXPARAMSA *pParams )
{
	const MessageBoxScope_t scope;

	return reinterpret_cast<MessageBoxIndirectA_t>( s_pMessageBoxIndirectA )( pParams );
}

static int WINAPI HookMessageBoxIndirectW( const MSGBOXPARAMSW *pParams )
{
	const MessageBoxScope_t scope;

	return reinterpret_cast<MessageBoxIndirectW_t>( s_pMessageBoxIndirectW )( pParams );
}

int MessageBoxes_AddHooks( ImportHook_t *pHooks )
{
	HMODULE hUser32 = GetModuleHandleW( L"user32.dll" );

	s_pMessageBoxA         = GetProcAddress( hUser32, "MessageBoxA" );
	s_pMessageBoxW         = GetProcAddress( hUser32, "MessageBoxW" );
	s_pMessageBoxIndirectA = GetProcAddress( hUser32, "MessageBoxIndirectA" );
	s_pMessageBoxIndirectW = GetProcAddress( hUser32, "MessageBoxIndirectW" );

	pHooks[ 0 ] = { s_pMessageBoxA,         reinterpret_cast<void *>( &HookMessageBoxA ) };
	pHooks[ 1 ] = { s_pMessageBoxW,         reinterpret_cast<void *>( &HookMessageBoxW ) };
	pHooks[ 2 ] = { s_pMessageBoxIndirectA, reinterpret_cast<void *>( &HookMessageBoxIndirectA ) };
	pHooks[ 3 ] = { s_pMessageBoxIndirectW, reinterpret_cast<void *>( &HookMessageBoxIndirectW ) };

	return 4;
}

void MarkMessageBox( HWND hWnd )
{
	if ( s_nDepth > 0 && IsClass( hWnd, L"#32770" ) )
	{
		SetPropW( hWnd, s_pwszMessageBoxProp, reinterpret_cast<HANDLE>( 1 ) );
	}
}

bool IsMessageBox( HWND hWnd )
{
	return GetPropW( hWnd, s_pwszMessageBoxProp ) != nullptr;
}

void UnmarkMessageBox( HWND hWnd )
{
	RemovePropW( hWnd, s_pwszMessageBoxProp );
}
