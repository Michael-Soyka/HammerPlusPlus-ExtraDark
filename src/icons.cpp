//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Bitmaps of hammerplusplus_dll.dll replaced by the ones built into hppxdark.dll
//			(src/assets/Bitmap<id>.bmp, same resource ids)
//
//=============================================================================//

#include "icons.h"
#include "iat_hook.h"

// RT_BITMAP as a number: the project builds without UNICODE
static const ULONG_PTR s_nBitmapType = 2;

static HMODULE s_hSelf   = nullptr;
static HMODULE s_hHammer = nullptr;

// Resources handed out from hppxdark.dll: LoadResource and SizeofResource get them with our module
static HRSRC s_hOwnResources[64];
static int s_nOwnResources = 0;

using FindResourceA_t   = HRSRC ( WINAPI * )( HMODULE, LPCSTR, LPCSTR );
using FindResourceW_t   = HRSRC ( WINAPI * )( HMODULE, LPCWSTR, LPCWSTR );
using FindResourceExW_t = HRSRC ( WINAPI * )( HMODULE, LPCWSTR, LPCWSTR, WORD );
using LoadResource_t    = HGLOBAL ( WINAPI * )( HMODULE, HRSRC );
using SizeofResource_t  = DWORD ( WINAPI * )( HMODULE, HRSRC );
using LoadBitmapW_t     = HBITMAP ( WINAPI * )( HINSTANCE, LPCWSTR );
using LoadImageA_t      = HANDLE ( WINAPI * )( HINSTANCE, LPCSTR, UINT, int, int, UINT );
using LoadImageW_t      = HANDLE ( WINAPI * )( HINSTANCE, LPCWSTR, UINT, int, int, UINT );

static FARPROC s_pFindResourceA   = nullptr;
static FARPROC s_pFindResourceW   = nullptr;
static FARPROC s_pFindResourceExW = nullptr;
static FARPROC s_pLoadResource    = nullptr;
static FARPROC s_pSizeofResource  = nullptr;
static FARPROC s_pLoadBitmapW     = nullptr;
static FARPROC s_pLoadImageA      = nullptr;
static FARPROC s_pLoadImageW      = nullptr;

static bool IsOwnResource( HRSRC hResource )
{
	for ( int i = 0; i < s_nOwnResources; i++ )
	{
		if ( s_hOwnResources[i] == hResource )
		{
			return true;
		}
	}

	return false;
}

// Our bitmap for a request to the Hammer++ DLL by numeric id, null for anything else
static HRSRC FindOwnBitmap( HMODULE hModule, ULONG_PTR nName, ULONG_PTR nType )
{
	if ( !s_hHammer || hModule != s_hHammer || !IS_INTRESOURCE( nName ) || nType != s_nBitmapType )
	{
		return nullptr;
	}

	HRSRC hResource = reinterpret_cast<FindResourceW_t>( s_pFindResourceW )( s_hSelf, MAKEINTRESOURCEW( nName ), MAKEINTRESOURCEW( s_nBitmapType ) );
	if ( hResource && !IsOwnResource( hResource ) && s_nOwnResources < ARRAYSIZE( s_hOwnResources ) )
	{
		s_hOwnResources[s_nOwnResources++] = hResource;
	}

	return hResource;
}

static HRSRC WINAPI HookFindResourceA( HMODULE hModule, LPCSTR pszName, LPCSTR pszType )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pszName ), reinterpret_cast<ULONG_PTR>( pszType ) );
	if ( !hResource )
	{
		hResource = reinterpret_cast<FindResourceA_t>( s_pFindResourceA )( hModule, pszName, pszType );
	}

	return hResource;
}

static HRSRC WINAPI HookFindResourceW( HMODULE hModule, LPCWSTR pwszName, LPCWSTR pwszType )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pwszName ), reinterpret_cast<ULONG_PTR>( pwszType ) );
	if ( !hResource )
	{
		hResource = reinterpret_cast<FindResourceW_t>( s_pFindResourceW )( hModule, pwszName, pwszType );
	}

	return hResource;
}

static HRSRC WINAPI HookFindResourceExW( HMODULE hModule, LPCWSTR pwszType, LPCWSTR pwszName, WORD nLanguage )
{
	HRSRC hResource = FindOwnBitmap( hModule, reinterpret_cast<ULONG_PTR>( pwszName ), reinterpret_cast<ULONG_PTR>( pwszType ) );
	if ( !hResource )
	{
		hResource = reinterpret_cast<FindResourceExW_t>( s_pFindResourceExW )( hModule, pwszType, pwszName, nLanguage );
	}

	return hResource;
}

static HGLOBAL WINAPI HookLoadResource( HMODULE hModule, HRSRC hResource )
{
	HGLOBAL hData = reinterpret_cast<LoadResource_t>( s_pLoadResource )( IsOwnResource( hResource ) ? s_hSelf : hModule, hResource );
	return hData;
}

static DWORD WINAPI HookSizeofResource( HMODULE hModule, HRSRC hResource )
{
	const DWORD nSize = reinterpret_cast<SizeofResource_t>( s_pSizeofResource )( IsOwnResource( hResource ) ? s_hSelf : hModule, hResource );
	return nSize;
}

static HBITMAP WINAPI HookLoadBitmapW( HINSTANCE hInstance, LPCWSTR pwszName )
{
	const bool bOwn = FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), s_nBitmapType ) != nullptr;
	HBITMAP hBitmap = reinterpret_cast<LoadBitmapW_t>( s_pLoadBitmapW )( bOwn ? s_hSelf : hInstance, pwszName );
	return hBitmap;
}

static HANDLE WINAPI HookLoadImageA( HINSTANCE hInstance, LPCSTR pszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	const bool bOwn = nType == IMAGE_BITMAP && !( nFlags & LR_LOADFROMFILE ) && FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pszName ), s_nBitmapType );
	HANDLE hImage = reinterpret_cast<LoadImageA_t>( s_pLoadImageA )( bOwn ? s_hSelf : hInstance, pszName, nType, nWidth, nHeight, nFlags );
	return hImage;
}

static HANDLE WINAPI HookLoadImageW( HINSTANCE hInstance, LPCWSTR pwszName, UINT nType, int nWidth, int nHeight, UINT nFlags )
{
	const bool bOwn = nType == IMAGE_BITMAP && !( nFlags & LR_LOADFROMFILE ) && FindOwnBitmap( hInstance, reinterpret_cast<ULONG_PTR>( pwszName ), s_nBitmapType );
	HANDLE hImage = reinterpret_cast<LoadImageW_t>( s_pLoadImageW )( bOwn ? s_hSelf : hInstance, pwszName, nType, nWidth, nHeight, nFlags );
	return hImage;
}

void Icons_Init()
{
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>( &Icons_Init ),
		&s_hSelf
	);

	HMODULE hKernel32  = GetModuleHandleW( L"kernel32.dll" );
	HMODULE hUser32    = GetModuleHandleW( L"user32.dll" );
	s_pFindResourceA   = GetProcAddress( hKernel32, "FindResourceA" );
	s_pFindResourceW   = GetProcAddress( hKernel32, "FindResourceW" );
	s_pFindResourceExW = GetProcAddress( hKernel32, "FindResourceExW" );
	s_pLoadResource    = GetProcAddress( hKernel32, "LoadResource" );
	s_pSizeofResource  = GetProcAddress( hKernel32, "SizeofResource" );
	s_pLoadBitmapW     = GetProcAddress( hUser32, "LoadBitmapW" );
	s_pLoadImageA      = GetProcAddress( hUser32, "LoadImageA" );
	s_pLoadImageW      = GetProcAddress( hUser32, "LoadImageW" );
}

int Icons_HookModule( HMODULE hHammer )
{
	if ( !s_hSelf || !s_pFindResourceW || !s_pLoadResource || !s_pSizeofResource )
	{
		return 0;
	}

	s_hHammer = hHammer;

	const ImportHook_t hooks[] =
	{
		{ s_pFindResourceA,   reinterpret_cast<void *>( &HookFindResourceA )   },
		{ s_pFindResourceW,   reinterpret_cast<void *>( &HookFindResourceW )   },
		{ s_pFindResourceExW, reinterpret_cast<void *>( &HookFindResourceExW ) },
		{ s_pLoadResource,    reinterpret_cast<void *>( &HookLoadResource )    },
		{ s_pSizeofResource,  reinterpret_cast<void *>( &HookSizeofResource )  },
		{ s_pLoadBitmapW,     reinterpret_cast<void *>( &HookLoadBitmapW )     },
		{ s_pLoadImageA,      reinterpret_cast<void *>( &HookLoadImageA )      },
		{ s_pLoadImageW,      reinterpret_cast<void *>( &HookLoadImageW )      },
	};

	const int nReplaced = HookImports( hHammer, hooks, ARRAYSIZE( hooks ) );
	return nReplaced;
}
