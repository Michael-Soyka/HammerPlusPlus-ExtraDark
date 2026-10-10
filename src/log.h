//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Debug log to hppxdark.log next to the DLL, built in with HPPXDARK_LOG
//
//=============================================================================//

#pragma once

#include <string>

// Path of a file next to hppxdark.dll
std::wstring ModuleFilePath( const wchar_t *pwszName );

#ifdef HPPXDARK_LOG
	void Log( const char *pszFormat, ... );
#else
	// Compiled out: no HPPXDARK_LOG in CMake
	inline void Log( const char *, ... )
	{
	}
#endif
