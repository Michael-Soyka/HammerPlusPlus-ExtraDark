//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Finds Hammer++ in a game bin folder and tells whether it is patched
//
//=============================================================================//

#include "installer/hammer_dir.h"
#include "installer/hammer_paths.h"

#include <system_error>

namespace fs = std::filesystem;

// Pasted paths often come quoted
static std::wstring TrimPastedPath( const std::wstring &path )
{
	const wchar_t *const pwszEdges = L"\" ";

	const size_t nStart = path.find_first_not_of( pwszEdges );

	if ( nStart == std::wstring::npos )
	{
		return {};
	}

	return path.substr( nStart, path.find_last_not_of( pwszEdges ) - nStart + 1 );
}

// The folder itself, x64 or win64 under it: where hammerplusplus.exe is, empty when nowhere
static fs::path FindExeDir( const fs::path &base )
{
	for ( const fs::path &candidate : { base, base / L"x64", base / L"win64" } )
	{
		if ( !IsFile( MakeHammerPaths( candidate ).m_Exe ) )
		{
			continue;
		}

		std::error_code error;
		const fs::path canonical = fs::canonical( candidate, error );
		return error ? candidate : canonical;
	}

	return {};
}

static void DetectState( HammerDir_t &hammer )
{
	const HammerPaths_t paths = MakeHammerPaths( hammer.m_Dir );

	if ( !IsFile( paths.m_Module ) )
	{
		hammer.m_nState = HAMMER_STATE_NO_DLL;
	}
	else if ( IsFile( paths.m_Backup ) )
	{
		hammer.m_nState      = HAMMER_STATE_PATCHED;
		hammer.m_bHasVersion = ReadFileVersion( paths.m_Proxy, hammer.m_Installed );
	}
	else
	{
		hammer.m_nState = HAMMER_STATE_READY;
	}
}

HammerDir_t FindHammerDir( const std::wstring &path )
{
	HammerDir_t hammer;

	const std::wstring typed = TrimPastedPath( path );

	if ( typed.empty() )
	{
		return hammer;
	}

	hammer.m_Dir = FindExeDir( typed );
	
	if ( hammer.m_Dir.empty() )
	{
		hammer.m_nState = HAMMER_STATE_NO_EXE;

		return hammer;
	}

	DetectState( hammer );

	return hammer;
}
