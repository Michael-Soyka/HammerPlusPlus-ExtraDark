//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Files of a Hammer++ install the installer works with, and the file
//			operations it needs
//
//=============================================================================//

#include "installer/hammer_paths.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

HammerPaths_t MakeHammerPaths( const fs::path &hammerDir )
{
	HammerPaths_t paths;
	paths.m_HammerDir = hammerDir;
	paths.m_Exe       = hammerDir / L"hammerplusplus.exe";
	paths.m_ModuleDir = hammerDir / L"hammerplusplus" / L"bin";
	paths.m_Module    = paths.m_ModuleDir / L"hammerplusplus_dll.dll";
	paths.m_Backup    = BackupOf( paths.m_Module );
	paths.m_Proxy     = paths.m_ModuleDir / L"hppxdark.dll";
	paths.m_Ini       = paths.m_ModuleDir / L"hppxdark.ini";

	return paths;
}

fs::path BackupOf( const fs::path &file )
{
	return fs::path( file ).concat( L".orig" );
}

bool IsFile( const fs::path &path )
{
	std::error_code error;
	return fs::is_regular_file( path, error );
}

void RemoveFile( const fs::path &path )
{
	std::error_code error;
	fs::remove( path, error );
}

bool CopyOver( const fs::path &from, const fs::path &to )
{
	std::error_code error;
	fs::copy_file( from, to, fs::copy_options::overwrite_existing, error );
	return !error;
}

bool ReadFileBytes( const fs::path &path, Bytes_t &bytes )
{
	std::ifstream file( path, std::ios::binary );
	if ( !file )
	{
		return false;
	}

	bytes.assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

bool WriteFileBytes( const fs::path &path, const void *pData, size_t nSize )
{
	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	file.write( static_cast<const char *>( pData ), static_cast<std::streamsize>( nSize ) );
	return file.good();
}
