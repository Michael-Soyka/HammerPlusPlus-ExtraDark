//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Import table of a PE file read from its bytes
//
//=============================================================================//

#pragma once

#include "installer/hammer_paths.h"

#include <string>
#include <vector>

struct Import_t
{
	std::string					m_Name;

	// File offset of the DLL name, where the patch renames it
	size_t						m_nNameOffset = 0;

	// By name, ordinals as #<number>
	std::vector<std::string>	m_Functions;
};

struct Image_t
{
	bool						m_b64 = false;
	std::vector<Import_t>		m_Imports;
};

// false with the reason in error when the file is not a readable PE
bool ReadImage( const Bytes_t &bytes, Image_t &image, std::string &error );

// By DLL name, case insensitive; null when not imported
const Import_t *FindImport( const Image_t &image, const char *pszName );
