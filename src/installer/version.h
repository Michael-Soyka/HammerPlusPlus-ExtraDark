//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: hppxdark versions: MAJOR.MINOR.BUILD.REVISION from the build time;
//			GitHub Actions builds add -git to the text
//
//=============================================================================//

#pragma once

#include <filesystem>
#include <string>

struct Version_t
{
	unsigned	m_nMajor    = 0;
	unsigned	m_nMinor    = 0;
	unsigned	m_nBuild    = 0;
	unsigned	m_nRevision = 0;

	// FileVersion string: the numbers with a suffix such as -git, empty when absent
	std::wstring	m_Text;
};

// The version this installer carries
Version_t InstallerVersion();

// From the version resource of a file; false when there is none
bool ReadFileVersion( const std::filesystem::path &path, Version_t &version );

// By the numbers only: < 0 when a is older than b, 0 when equal, > 0 when newer
int CompareVersions( const Version_t &a, const Version_t &b );

// The text version when there is one, else the numbers
std::wstring FormatVersion( const Version_t &version );
