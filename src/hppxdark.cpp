//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Hammer++ rebind of the ADVAPI32 import of hammerplusplus.exe:
//			forwards its registry imports and turns on the dark theme
//
//=============================================================================//

#include "dark_theme.h"
#include "iat_hook.h"

#include <windows.h>

static FARPROC s_pGetCommandLineA = nullptr;
static FARPROC s_pGetCommandLineW = nullptr;

static INIT_ONCE s_InitOnce = INIT_ONCE_STATIC_INIT;

// Runs on the first command line request: CRT startup, outside the loader lock
static BOOL CALLBACK InitDarkTheme( PINIT_ONCE, PVOID, PVOID * )
{
	DarkTheme_Init();

	return TRUE;
}

static FARPROC s_pLoadLibraryExA = nullptr;
static FARPROC s_pLoadLibraryExW = nullptr;

// Hammer++ loads hammerplusplus_dll.dll at runtime; its imports are hooked before it registers window classes
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

static LPSTR WINAPI HookGetCommandLineA()
{
	InitOnceExecuteOnce( &s_InitOnce, InitDarkTheme, nullptr, nullptr );

	using GetCommandLineA_t = LPSTR ( WINAPI * )();
	return reinterpret_cast<GetCommandLineA_t>( s_pGetCommandLineA )();
}

static LPWSTR WINAPI HookGetCommandLineW()
{
	InitOnceExecuteOnce( &s_InitOnce, InitDarkTheme, nullptr, nullptr );

	using GetCommandLineW_t = LPWSTR ( WINAPI * )();
	return reinterpret_cast<GetCommandLineW_t>( s_pGetCommandLineW )();
}

// The ADVAPI32 imports of hammerplusplus.exe, exported under their names by hppxdark.def
extern "C" LSTATUS WINAPI Shim_RegOpenKeyExA( HKEY hKey, LPCSTR pszSubKey, DWORD nOptions, REGSAM nSam, PHKEY phResult )
{
	return RegOpenKeyExA( hKey, pszSubKey, nOptions, nSam, phResult );
}

extern "C" LSTATUS WINAPI Shim_RegQueryValueExA( HKEY hKey, LPCSTR pszValue, LPDWORD pReserved, LPDWORD pType, LPBYTE pData, LPDWORD pnData )
{
	return RegQueryValueExA( hKey, pszValue, pReserved, pType, pData, pnData );
}

extern "C" LSTATUS WINAPI Shim_RegCloseKey( HKEY hKey )
{
	return RegCloseKey( hKey );
}

BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD nReason, LPVOID )
{
	if ( nReason != DLL_PROCESS_ATTACH )
	{
		return TRUE;
	}

	DisableThreadLibraryCalls( hInstance );

	HMODULE hKernel32  = GetModuleHandleW( L"kernel32.dll" );
	s_pGetCommandLineA = GetProcAddress( hKernel32, "GetCommandLineA" );
	s_pGetCommandLineW = GetProcAddress( hKernel32, "GetCommandLineW" );
	s_pLoadLibraryExA  = GetProcAddress( hKernel32, "LoadLibraryExA" );
	s_pLoadLibraryExW  = GetProcAddress( hKernel32, "LoadLibraryExW" );
	if ( !s_pGetCommandLineA || !s_pGetCommandLineW || !s_pLoadLibraryExA || !s_pLoadLibraryExW )
	{
		return TRUE;
	}

	// Import tables are bound before any DllMain runs
	const ImportHook_t hooks[] =
	{
		{ s_pGetCommandLineA, reinterpret_cast<void *>( &HookGetCommandLineA ) },
		{ s_pGetCommandLineW, reinterpret_cast<void *>( &HookGetCommandLineW ) },
		{ s_pLoadLibraryExA,  reinterpret_cast<void *>( &HookLoadLibraryExA )  },
		{ s_pLoadLibraryExW,  reinterpret_cast<void *>( &HookLoadLibraryExW )  },
	};

	HookImports( GetModuleHandleW( nullptr ),      hooks, ARRAYSIZE( hooks ) );
	HookImports( GetModuleHandleW( L"tier0.dll" ), hooks, ARRAYSIZE( hooks ) );

	return TRUE;
}
