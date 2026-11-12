//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Finds Hammer++ in a game bin folder and tells whether it is patched
//
//=============================================================================//

#include "installer/hammer_dir.h"

#include <system_error>

namespace fs = std::filesystem;

static bool IsFile( const fs::path &path )
{
	std::error_code error;
	const bool bFile = fs::is_regular_file( path, error );

	return bFile;
}

HammerDir_t FindHammerDir( const std::wstring &path )
{
	HammerDir_t result;

	// Pasted paths often come quoted
	std::wstring typed = path;
	while ( !typed.empty() && ( typed.back() == L'"' || typed.back() == L' ' ) )
	{
		typed.pop_back();
	}

	while ( !typed.empty() && ( typed.front() == L'"' || typed.front() == L' ' ) )
	{
		typed.erase( typed.begin() );
	}

	if ( typed.empty() )
	{
		return result;
	}

	result.m_nState = HAMMER_STATE_NO_EXE;

	const fs::path base( typed );
	for ( const fs::path &candidate : { base, base / L"x64", base / L"win64" } )
	{
		if ( !IsFile( candidate / L"hammerplusplus.exe" ) )
		{
			continue;
		}

		std::error_code error;
		result.m_Dir = fs::canonical( candidate, error );
		if ( error )
		{
			result.m_Dir = candidate;
		}

		const fs::path moduleDir = result.m_Dir / L"hammerplusplus" / L"bin";
		const fs::path module = moduleDir / L"hammerplusplus_dll.dll";
		if ( !IsFile( module ) )
		{
			result.m_nState = HAMMER_STATE_NO_DLL;
		}
		else if ( IsFile( fs::path( module ).concat( L".orig" ) ) )
		{
			result.m_nState = HAMMER_STATE_PATCHED;
			result.m_bHasVersion = ReadFileVersion( moduleDir / L"hppxdark.dll", result.m_Installed );
		}
		else
		{
			result.m_nState = HAMMER_STATE_READY;
		}

		break;
	}

	return result;
}
