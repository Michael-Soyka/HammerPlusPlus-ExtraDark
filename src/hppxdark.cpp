//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Hammer++ rebind of the ADVAPI32 import of hammerplusplus_dll.dll:
//			forwards its ADVAPI32 imports (hppxdark.def) and turns on the dark theme
//
//=============================================================================//

#include "theme/dark_theme.h"
#include "core/iat_hook.h"

#include <windows.h>

static FARPROC s_pLoadLibraryExA = nullptr;
static FARPROC s_pLoadLibraryExW = nullptr;

// Modules loaded later will get the hooks before they create window
static HMODULE WINAPI HookLoadLibraryExA( LPCSTR pszName, HANDLE hFile, DWORD nFlags )
{
	using LoadLibraryExA_t = HMODULE ( WINAPI * )( LPCSTR, HANDLE, DWORD );
	HMODULE hModule = reinterpret_cast<LoadLibraryExA_t>( s_pLoadLibraryExA )( pszName, hFile, nFlags );
	DarkTheme_OnModuleLoaded();

	return hModule;
}

static HMODULE WINAPI HookLoadLibraryExW( LPCWSTR pwszName, HANDLE hFile, DWORD nFlags )
{
	using LoadLibraryExW_t = HMODULE ( WINAPI * )( LPCWSTR, HANDLE, DWORD );
	HMODULE hModule = reinterpret_cast<LoadLibraryExW_t>( s_pLoadLibraryExW )( pwszName, hFile, nFlags );
	DarkTheme_OnModuleLoaded();

	return hModule;
}

BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD nReason, LPVOID )
{
	if ( nReason != DLL_PROCESS_ATTACH )
	{
		return TRUE;
	}

	DisableThreadLibraryCalls( hInstance );

	// hammerplusplus_dll.dll is mapped with its imports bound, its initializers run after ours
	DarkTheme_Init();

	HMODULE hKernel32 = GetModuleHandleW( L"kernel32.dll" );
	s_pLoadLibraryExA = GetProcAddress( hKernel32, "LoadLibraryExA" );
	s_pLoadLibraryExW = GetProcAddress( hKernel32, "LoadLibraryExW" );

	if ( !s_pLoadLibraryExA || !s_pLoadLibraryExW )
	{
		return TRUE;
	}

	const ImportHook_t hooks[] =
	{
		{ s_pLoadLibraryExA, reinterpret_cast<void *>( &HookLoadLibraryExA ) },
		{ s_pLoadLibraryExW, reinterpret_cast<void *>( &HookLoadLibraryExW ) },
	};

	HookImports( GetModuleHandleW( L"hammerplusplus_dll.dll" ), hooks, ARRAYSIZE( hooks ) );
	HookImports( GetModuleHandleW( L"tier0.dll" ),              hooks, ARRAYSIZE( hooks ) );

	return TRUE;
}
