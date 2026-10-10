//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Import table redirection of already resolved functions
//
//=============================================================================//

#pragma once

#include <windows.h>

struct ImportHook_t
{
	FARPROC	m_pOriginal;
	void	*m_pReplacement;
};

// Replaces every import table entry of hModule that points at a hook's original, returns how many
int HookImports( HMODULE hModule, const ImportHook_t *pHooks, int nHooks );
