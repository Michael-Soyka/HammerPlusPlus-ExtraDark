//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Debug log to hppxdark.log next to the DLL, built in with HPPXDARK_LOG
//
//=============================================================================//

#include "log.h"

#include <windows.h>

#include <share.h>
#include <stdarg.h>
#include <stdio.h>

std::wstring ModuleFilePath( const wchar_t *pwszName )
{
	HMODULE hSelf = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>( &ModuleFilePath ),
		&hSelf
	);

	wchar_t wszPath[MAX_PATH] = {};
	GetModuleFileNameW( hSelf, wszPath, MAX_PATH );

	std::wstring path = wszPath;
	path.resize( path.find_last_of( L'\\' ) + 1 );
	path += pwszName;
	return path;
}

#ifdef HPPXDARK_LOG
	static FILE *OpenLog()
	{
		static FILE *s_pFile  = nullptr;
		static bool s_bOpened = false;
		if ( s_bOpened )
		{
			return s_pFile;
		}

		s_bOpened = true;

		// Shared, so the log can be read while Hammer++ runs
		s_pFile = _wfsopen( ModuleFilePath( L"hppxdark.log" ).c_str(), L"w", _SH_DENYNO );
		return s_pFile;
	}

	void Log( const char *pszFormat, ... )
	{
		FILE *pFile = OpenLog();
		if ( !pFile )
		{
			return;
		}

		va_list args;
		va_start( args, pszFormat );
		vfprintf( pFile, pszFormat, args );
		va_end( args );

		fputc( '\n', pFile );
		fflush( pFile );
	}
#endif
