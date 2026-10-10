//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Hammer++ rebind of the ADVAPI32 import of hammerplusplus.exe:
//			forwards its registry imports, appends -vulkan to the command line
//			and preloads the OTM DXVK build
//
//=============================================================================//

#include "iat_hook.h"

#include <windows.h>

#include <string>

// Relative to the folder of hammerplusplus.exe
static const wchar_t *const s_pwszDXVK = L"..\\..\\otm\\bin\\x64\\dxvk_d3d9.dll";

static FARPROC s_pGetCommandLineA = nullptr;
static FARPROC s_pGetCommandLineW = nullptr;

static std::string  s_CommandLineA;
static std::wstring s_CommandLineW;

static INIT_ONCE s_InitOnce = INIT_ONCE_STATIC_INIT;

static bool HasVulkanArg( const wchar_t *pwszCommandLine )
{
	std::wstring lower = pwszCommandLine;
	for ( wchar_t &c : lower )
	{
		c = towlower( c );
	}

	const bool bFound = lower.find( L" -vulkan" ) != std::wstring::npos;
	return bFound;
}

// Loaded before the engine asks for dxvk_d3d9.dll by name, which then gets this module
static void PreloadDXVK()
{
	wchar_t wszPath[ MAX_PATH ];
	const DWORD nLen = GetModuleFileNameW( nullptr, wszPath, MAX_PATH );
	if ( !nLen || nLen == MAX_PATH )
	{
		return;
	}

	std::wstring path = wszPath;
	path.resize( path.find_last_of( L'\\' ) + 1 );
	path += s_pwszDXVK;

	if ( !LoadLibraryExW( path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH ) )
	{
		std::wstring message = L"Failed to load the OTM DXVK build, OTM shaders fall back:\n\n" + path;
		MessageBoxW( nullptr, message.c_str(), L"hppxdark", MB_OK | MB_ICONWARNING );
	}
}

// Runs on the first command line request: CRT startup, outside the loader lock
static BOOL CALLBACK InitCommandLine( PINIT_ONCE, PVOID, PVOID * )
{
	using GetCommandLineW_t = LPWSTR ( WINAPI * )();
	const wchar_t *pwszOriginal = reinterpret_cast<GetCommandLineW_t>( s_pGetCommandLineW )();

	s_CommandLineW = pwszOriginal;
	if ( !HasVulkanArg( pwszOriginal ) )
	{
		s_CommandLineW += L" -vulkan";
	}

	const int nSize = WideCharToMultiByte( CP_ACP, 0, s_CommandLineW.c_str(), -1, nullptr, 0, nullptr, nullptr );
	s_CommandLineA.resize( nSize > 0 ? nSize - 1 : 0 );
	if ( nSize > 1 )
	{
		WideCharToMultiByte( CP_ACP, 0, s_CommandLineW.c_str(), -1, s_CommandLineA.data(), nSize, nullptr, nullptr );
	}

	PreloadDXVK();

	return TRUE;
}

static LPSTR WINAPI HookGetCommandLineA()
{
	InitOnceExecuteOnce( &s_InitOnce, InitCommandLine, nullptr, nullptr );

	return s_CommandLineA.data();
}

static LPWSTR WINAPI HookGetCommandLineW()
{
	InitOnceExecuteOnce( &s_InitOnce, InitCommandLine, nullptr, nullptr );

	return s_CommandLineW.data();
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
	if ( !s_pGetCommandLineA || !s_pGetCommandLineW )
	{
		return TRUE;
	}

	// Import tables are bound before any DllMain runs; tier0 builds CommandLine() for materialsystem
	const ImportHook_t hooks[] =
	{
		{ s_pGetCommandLineA, reinterpret_cast<void *>( &HookGetCommandLineA ) },
		{ s_pGetCommandLineW, reinterpret_cast<void *>( &HookGetCommandLineW ) },
	};

	HookImports( GetModuleHandleW( nullptr ),      hooks, ARRAYSIZE( hooks ) );
	HookImports( GetModuleHandleW( L"tier0.dll" ), hooks, ARRAYSIZE( hooks ) );

	return TRUE;
}
