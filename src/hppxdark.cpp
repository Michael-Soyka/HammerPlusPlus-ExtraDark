//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Hammer++ rebind of the ADVAPI32 import of hammerplusplus.exe:
//			forwards its registry imports, appends -vulkan to the command line,
//			preloads the OTM DXVK build (OTM_BUILD, else Valve's DXVK from bin/x64
//			is used) and turns on the dark theme
//
//=============================================================================//

#include "dark_theme.h"
#include "iat_hook.h"
#include "log.h"

#include <windows.h>

#include <string>

#ifdef OTM_BUILD
	//will use our's dxvk build d
	static const wchar_t *const s_pwszDXVK = L"..\\..\\otm\\bin\\x64\\dxvk_d3d9.dll";
#endif

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

#ifdef OTM_BUILD
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
#endif

// Runs on the first command line request: CRT startup, outside the loader lock
static BOOL CALLBACK InitCommandLine( PINIT_ONCE, PVOID, PVOID * )
{
	using GetCommandLineW_t = LPWSTR ( WINAPI * )();
	const wchar_t *pwszOriginal = reinterpret_cast<GetCommandLineW_t>( s_pGetCommandLineW )();

	// [ForceVulkan] Enabled=0 leaves the command line as is: Direct3D 9 unless -vulkan is passed
	const bool bForceVulkan = GetPrivateProfileIntW( L"ForceVulkan", L"Enabled", 1, ModuleFilePath( L"hppxdark.ini" ).c_str() ) != 0;
	const bool bVulkanArg   = HasVulkanArg( pwszOriginal );

	s_CommandLineW = pwszOriginal;
	if ( bForceVulkan && !bVulkanArg )
	{
		s_CommandLineW += L" -vulkan";
	}

	const int nSize = WideCharToMultiByte( CP_ACP, 0, s_CommandLineW.c_str(), -1, nullptr, 0, nullptr, nullptr );
	s_CommandLineA.resize( nSize > 0 ? nSize - 1 : 0 );
	if ( nSize > 1 )
	{
		WideCharToMultiByte( CP_ACP, 0, s_CommandLineW.c_str(), -1, s_CommandLineA.data(), nSize, nullptr, nullptr );
	}

	#ifdef OTM_BUILD
		if ( bForceVulkan || bVulkanArg )
		{
			PreloadDXVK();
		}
	#endif

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
	s_pLoadLibraryExA  = GetProcAddress( hKernel32, "LoadLibraryExA" );
	s_pLoadLibraryExW  = GetProcAddress( hKernel32, "LoadLibraryExW" );
	if ( !s_pGetCommandLineA || !s_pGetCommandLineW || !s_pLoadLibraryExA || !s_pLoadLibraryExW )
	{
		return TRUE;
	}

	// Import tables are bound before any DllMain runs; tier0 builds CommandLine() for materialsystem
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
