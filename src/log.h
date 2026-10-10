//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Debug log to hppxdark.log next to the DLL, on with [Debug] Log=1 in hppxdark.ini
//
//=============================================================================//

#pragma once

#include <string>

// Path of a file next to hppxdark.dll
std::wstring ModuleFilePath( const wchar_t *pwszName );

void Log( const char *pszFormat, ... );
