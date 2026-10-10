//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Import table redirection of already resolved functions
//
//=============================================================================//

#include "iat_hook.h"

static void WriteThunk( ULONG_PTR *pThunk, ULONG_PTR nValue )
{
	DWORD nOldProtect = 0;
	
	if ( VirtualProtect( pThunk, sizeof( *pThunk ), PAGE_READWRITE, &nOldProtect ) )
	{
		*pThunk = nValue;
		VirtualProtect( pThunk, sizeof( *pThunk ), nOldProtect, &nOldProtect );
	}
}

int HookImports( HMODULE hModule, const ImportHook_t *pHooks, int nHooks )
{
	int nReplaced = 0;
	if ( !hModule )
	{
		return nReplaced;
	}

	BYTE *pBase = reinterpret_cast<BYTE *>( hModule );

	const IMAGE_DOS_HEADER *pDos    = reinterpret_cast<const IMAGE_DOS_HEADER *>( pBase );
	const IMAGE_NT_HEADERS *pNt     = reinterpret_cast<const IMAGE_NT_HEADERS *>( pBase + pDos->e_lfanew );
	const IMAGE_DATA_DIRECTORY &dir = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	
	if ( !dir.VirtualAddress )
	{
		return nReplaced;
	}

	const IMAGE_IMPORT_DESCRIPTOR *pDesc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>( pBase + dir.VirtualAddress );
	for ( ; pDesc->Name; pDesc++ )
	{
		ULONG_PTR *pThunk = reinterpret_cast<ULONG_PTR *>( pBase + pDesc->FirstThunk );
		for ( ; *pThunk; pThunk++ )
		{
			for ( int i = 0; i < nHooks; i++ )
			{
				if ( pHooks[i].m_pOriginal && *pThunk == reinterpret_cast<ULONG_PTR>( pHooks[i].m_pOriginal ) )
				{
					WriteThunk( pThunk, reinterpret_cast<ULONG_PTR>( pHooks[i].m_pReplacement ) );
					nReplaced++;
					break;
				}
			}
		}
	}

	return nReplaced;
}
