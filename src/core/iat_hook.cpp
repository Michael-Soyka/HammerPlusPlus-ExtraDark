//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Import table redirection of already resolved functions
//
//=============================================================================//

#include "core/iat_hook.h"

static void WriteThunk( ULONG_PTR *pThunk, ULONG_PTR nValue )
{
	DWORD nOldProtect = 0;
	
	if ( VirtualProtect( pThunk, sizeof( *pThunk ), PAGE_READWRITE, &nOldProtect ) )
	{
		*pThunk = nValue;
		VirtualProtect( pThunk, sizeof( *pThunk ), nOldProtect, &nOldProtect );
	}
}

// Replacement for a resolved import, null when none of the hooks is for it
static void *FindReplacement( ULONG_PTR nResolved, const ImportHook_t *pHooks, int nHooks )
{
	for ( int i = 0; i < nHooks; i++ )
	{
		if ( pHooks[i].m_pOriginal && nResolved == reinterpret_cast<ULONG_PTR>( pHooks[i].m_pOriginal ) )
		{
			return pHooks[i].m_pReplacement;
		}
	}

	return nullptr;
}

int HookImports( HMODULE hModule, const ImportHook_t *pHooks, int nHooks )
{
	if ( !hModule )
	{
		return 0;
	}

	BYTE *pBase = reinterpret_cast<BYTE *>( hModule );

	const IMAGE_DOS_HEADER *pDos    = reinterpret_cast<const IMAGE_DOS_HEADER *>( pBase );
	const IMAGE_NT_HEADERS *pNt     = reinterpret_cast<const IMAGE_NT_HEADERS *>( pBase + pDos->e_lfanew );
	const IMAGE_DATA_DIRECTORY &dir = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if ( !dir.VirtualAddress )
	{
		return 0;
	}

	int nReplaced = 0;
	const IMAGE_IMPORT_DESCRIPTOR *pDesc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>( pBase + dir.VirtualAddress );
	for ( ; pDesc->Name; pDesc++ )
	{
		for ( ULONG_PTR *pThunk = reinterpret_cast<ULONG_PTR *>( pBase + pDesc->FirstThunk ); *pThunk; pThunk++ )
		{
			void *pReplacement = FindReplacement( *pThunk, pHooks, nHooks );
			if ( pReplacement )
			{
				WriteThunk( pThunk, reinterpret_cast<ULONG_PTR>( pReplacement ) );
				nReplaced++;
			}
		}
	}

	return nReplaced;
}
