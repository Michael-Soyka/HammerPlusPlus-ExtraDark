//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Finds Hammer++ in a game bin folder and tells whether it is patched
//
//=============================================================================//

#pragma once

#include "installer/version.h"

#include <filesystem>
#include <string>

enum HammerState_t
{
	HAMMER_STATE_EMPTY = 0,
	HAMMER_STATE_NO_EXE,
	HAMMER_STATE_NO_DLL,
	HAMMER_STATE_READY,
	HAMMER_STATE_PATCHED,
};

struct HammerDir_t
{
	HammerState_t			m_nState = HAMMER_STATE_EMPTY;

	// The folder with hammerplusplus.exe: bin, bin\x64 or bin\win64
	std::filesystem::path	m_Dir;

	// HAMMER_STATE_PATCHED: hppxdark.dll in place; builds before versioning have none
	bool					m_bHasVersion = false;
	Version_t				m_Installed;
};

HammerDir_t FindHammerDir( const std::wstring &path );
