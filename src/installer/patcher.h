//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Points the ADVAPI32 import of hammerplusplus_dll.dll at hppxdark.dll,
//			or restores the original DLL
//
//=============================================================================//

#pragma once

#include <filesystem>
#include <string>

struct PatchResult_t
{
	bool		m_bSuccess = false;

	// One line per step
	std::wstring	m_Output;
};

// hammerDir holds hammerplusplus.exe
PatchResult_t PatchHammer( const std::filesystem::path &hammerDir );
PatchResult_t RestoreHammer( const std::filesystem::path &hammerDir );
