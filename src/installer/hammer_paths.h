//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Files of a Hammer++ install the installer works with, and the file
//			operations it needs
//
//=============================================================================//

#pragma once

#include <windows.h>

#include <filesystem>
#include <vector>

using Bytes_t = std::vector<BYTE>;

struct HammerPaths_t
{
	// Folder of hammerplusplus.exe: bin, bin\x64 or bin\win64
	std::filesystem::path	m_HammerDir;
	std::filesystem::path	m_Exe;

	// hammerplusplus\bin: the patched DLL, its backup, our proxy and settings
	std::filesystem::path	m_ModuleDir;
	std::filesystem::path	m_Module;
	std::filesystem::path	m_Backup;
	std::filesystem::path	m_Proxy;
	std::filesystem::path	m_Ini;
};

HammerPaths_t MakeHammerPaths( const std::filesystem::path &hammerDir );

// file.orig: the backup of a patched file
std::filesystem::path BackupOf( const std::filesystem::path &file );

bool IsFile( const std::filesystem::path &path );
void RemoveFile( const std::filesystem::path &path );
bool CopyOver( const std::filesystem::path &from, const std::filesystem::path &to );
bool ReadFileBytes( const std::filesystem::path &path, Bytes_t &bytes );
bool WriteFileBytes( const std::filesystem::path &path, const void *pData, size_t nSize );
