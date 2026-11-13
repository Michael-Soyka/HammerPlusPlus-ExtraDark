//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Points the ADVAPI32 import of hammerplusplus_dll.dll at hppxdark.dll,
//			or restores the original DLL
//
//=============================================================================//

#include "installer/patcher.h"
#include "installer/hammer_paths.h"
#include "installer/pe_imports.h"
#include "installer/resources.h"

#include <cstring>

namespace fs = std::filesystem;

static const char *const s_pszOldName = "ADVAPI32.dll";
static const char *const s_pszNewName = "hppxdark.dll";

// ADVAPI32 functions forwarded by hppxdark.def
static const char *const s_pszForwarded[] =
{
	"AdjustTokenPrivileges", "GetFileSecurityA", "GetUserNameA", "LookupPrivilegeValueW", "OpenProcessToken",
	"RegCloseKey", "RegCreateKeyExA", "RegDeleteKeyA", "RegDeleteValueA", "RegEnumKeyA", "RegEnumKeyExA",
	"RegEnumValueA", "RegOpenKeyExA", "RegOpenKeyExW", "RegQueryValueA", "RegQueryValueExA", "RegSetValueA",
	"RegSetValueExA", "SetFileSecurityA",
};

// Builds before 8873 had the import patched in hammerplusplus.exe, under these names
static const wchar_t *const s_pwszLegacyNames[] = { L"hppxdark.dll", L"otm_hpp.dll" };

static std::wstring Widen( const std::string &ascii )
{
	const std::wstring wide( ascii.begin(), ascii.end() );
	return wide;
}

static bool IsForwarded( const std::string &function )
{
	for ( const char *pszForwarded : s_pszForwarded )
	{
		if ( function == pszForwarded )
		{
			return true;
		}
	}

	return false;
}

// Comma separated, empty when hppxdark.dll forwards them all
static std::string FindUnforwardedFunctions( const Import_t &import )
{
	std::string unforwarded;
	for ( const std::string &function : import.m_Functions )
	{
		if ( !IsForwarded( function ) )
		{
			unforwarded += unforwarded.empty() ? function : ", " + function;
		}
	}

	return unforwarded;
}

// The new name with its terminator fits in the old one
static void RenameImport( Bytes_t &bytes, const Import_t &import )
{
	BYTE *pName = bytes.data() + import.m_nNameOffset;
	std::memset( pName, 0, std::strlen( s_pszOldName ) + 1 );
	std::memcpy( pName, s_pszNewName, std::strlen( s_pszNewName ) );
}

static bool WriteProxy( bool b64, const fs::path &path )
{
	HRSRC hResource = FindResourceW( nullptr, MAKEINTRESOURCEW( b64 ? IDR_PROXY_X64 : IDR_PROXY_X86 ), RT_RCDATA );
	HGLOBAL hData = hResource ? LoadResource( nullptr, hResource ) : nullptr;
	const void *pData = hData ? LockResource( hData ) : nullptr;
	if ( !pData )
	{
		return false;
	}

	const bool bWritten = WriteFileBytes( path, pData, SizeofResource( nullptr, hResource ) );
	return bWritten;
}

// hammerplusplus.exe patched before 8873: the original exe back, its proxy DLL and settings moved out
static void RestoreLegacyExe( const HammerPaths_t &paths, std::wstring &output )
{
	const fs::path backup = BackupOf( paths.m_Exe );
	if ( !IsFile( backup ) )
	{
		return;
	}

	if ( !CopyOver( backup, paths.m_Exe ) )
	{
		output += L"Failed to restore hammerplusplus.exe patched by an earlier version\n";
		return;
	}

	RemoveFile( backup );
	for ( const wchar_t *pwszName : s_pwszLegacyNames )
	{
		RemoveFile( paths.m_HammerDir / pwszName );
	}

	const fs::path legacyIni = paths.m_HammerDir / L"hppxdark.ini";
	if ( IsFile( legacyIni ) && !IsFile( paths.m_Ini ) )
	{
		std::error_code error;
		fs::rename( legacyIni, paths.m_Ini, error );
	}

	output += L"Restored hammerplusplus.exe patched by an earlier version\n";
}

static bool LoadModuleImage( const HammerPaths_t &paths, Bytes_t &bytes, Image_t &image, std::wstring &output )
{
	if ( !ReadFileBytes( paths.m_Module, bytes ) )
	{
		output += L"Failed to read hammerplusplus_dll.dll\n";
		return false;
	}

	std::string error;
	if ( !ReadImage( bytes, image, error ) )
	{
		output += L"hammerplusplus_dll.dll: " + Widen( error ) + L"\n";
		return false;
	}

	return true;
}

// Already patched: only hppxdark.dll is written again
static bool UpdateProxy( const HammerPaths_t &paths, bool b64, std::wstring &output )
{
	const bool bWritten = WriteProxy( b64, paths.m_Proxy );
	output += bWritten ? L"hppxdark.dll updated\n" : L"Failed to write hppxdark.dll, is Hammer++ running?\n";

	return bWritten;
}

// Can the ADVAPI32 import be renamed: present and every function forwarded
static const Import_t *FindPatchTarget( const Image_t &image, std::wstring &output )
{
	const Import_t *pTarget = FindImport( image, s_pszOldName );
	if ( !pTarget )
	{
		output += Widen( s_pszOldName ) + L" import is not found, this Hammer++ build is not supported\n";
		return nullptr;
	}

	const std::string unforwarded = FindUnforwardedFunctions( *pTarget );
	if ( !unforwarded.empty() )
	{
		output += L"This Hammer++ build imports functions hppxdark.dll does not forward: " + Widen( unforwarded ) + L"\n";
		return nullptr;
	}

	return pTarget;
}

// Backup, patched DLL and proxy; on a failure the original goes back
static bool WritePatchedModule( const HammerPaths_t &paths, const Bytes_t &bytes, bool b64, std::wstring &output )
{
	if ( !CopyOver( paths.m_Module, paths.m_Backup ) )
	{
		output += L"Failed to save hammerplusplus_dll.dll.orig\n";
		return false;
	}

	if ( !WriteFileBytes( paths.m_Module, bytes.data(), bytes.size() ) || !WriteProxy( b64, paths.m_Proxy ) )
	{
		CopyOver( paths.m_Backup, paths.m_Module );
		RemoveFile( paths.m_Backup );
		RemoveFile( paths.m_Proxy );
		output += L"Failed to write hammerplusplus_dll.dll, is Hammer++ running?\n";
		return false;
	}

	output += b64 ? L"Patched (x64)\n" : L"Patched (x86)\n";
	return true;
}

PatchResult_t PatchHammer( const fs::path &hammerDir )
{
	PatchResult_t result;
	const HammerPaths_t paths = MakeHammerPaths( hammerDir );

	RestoreLegacyExe( paths, result.m_Output );

	Bytes_t bytes;
	Image_t image;
	if ( !LoadModuleImage( paths, bytes, image, result.m_Output ) )
	{
		return result;
	}

	if ( FindImport( image, s_pszNewName ) )
	{
		result.m_bSuccess = UpdateProxy( paths, image.m_b64, result.m_Output );
		return result;
	}

	const Import_t *pTarget = FindPatchTarget( image, result.m_Output );
	if ( !pTarget )
	{
		return result;
	}

	RenameImport( bytes, *pTarget );
	result.m_bSuccess = WritePatchedModule( paths, bytes, image.m_b64, result.m_Output );

	return result;
}

PatchResult_t RestoreHammer( const fs::path &hammerDir )
{
	PatchResult_t result;
	const HammerPaths_t paths = MakeHammerPaths( hammerDir );

	RestoreLegacyExe( paths, result.m_Output );

	if ( !IsFile( paths.m_Backup ) )
	{
		result.m_Output += L"No backup to restore: hammerplusplus_dll.dll.orig\n";
		return result;
	}

	if ( !CopyOver( paths.m_Backup, paths.m_Module ) )
	{
		result.m_Output += L"Failed to restore hammerplusplus_dll.dll, is Hammer++ running?\n";
		return result;
	}

	RemoveFile( paths.m_Backup );
	RemoveFile( paths.m_Proxy );

	result.m_bSuccess = true;
	result.m_Output += L"Patch removed, the original hammerplusplus_dll.dll is back\n";

	return result;
}
