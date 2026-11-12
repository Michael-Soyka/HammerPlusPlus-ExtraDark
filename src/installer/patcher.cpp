//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Points the ADVAPI32 import of hammerplusplus_dll.dll at hppxdark.dll,
//			or restores the original DLL
//
//=============================================================================//

#include "installer/patcher.h"
#include "installer/resources.h"

#include <windows.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

using Bytes_t = std::vector<BYTE>;

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

struct Import_t
{
	std::string					m_Name;
	size_t						m_nNameOffset = 0;
	std::vector<std::string>	m_Functions;
};

struct Image_t
{
	bool						m_b64 = false;
	std::vector<Import_t>		m_Imports;
};

//-----------------------------------------------------------------------------
// Files
//-----------------------------------------------------------------------------

static bool ReadFileBytes( const fs::path &path, Bytes_t &bytes )
{
	std::ifstream file( path, std::ios::binary );
	if ( !file )
	{
		return false;
	}

	bytes.assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

static bool WriteFileBytes( const fs::path &path, const void *pData, size_t nSize )
{
	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	file.write( static_cast<const char *>( pData ), static_cast<std::streamsize>( nSize ) );

	const bool bWritten = file.good();
	return bWritten;
}

static bool FileExists( const fs::path &path )
{
	std::error_code error;
	const bool bExists = fs::is_regular_file( path, error );

	return bExists;
}

static void RemoveFile( const fs::path &path )
{
	std::error_code error;
	fs::remove( path, error );
}

//-----------------------------------------------------------------------------
// PE imports
//-----------------------------------------------------------------------------

template <typename T>
static bool ReadAt( const Bytes_t &bytes, size_t nOffset, T &value )
{
	if ( nOffset > bytes.size() || bytes.size() - nOffset < sizeof( T ) )
	{
		return false;
	}

	std::memcpy( &value, bytes.data() + nOffset, sizeof( T ) );
	return true;
}

static bool ReadString( const Bytes_t &bytes, size_t nOffset, std::string &text )
{
	text.clear();
	for ( size_t n = nOffset; n < bytes.size(); n++ )
	{
		if ( !bytes[n] )
		{
			return true;
		}

		text.push_back( static_cast<char>( bytes[n] ) );
	}

	return false;
}

static bool RvaToOffset( const std::vector<IMAGE_SECTION_HEADER> &sections, ULONGLONG nRva, size_t &nOffset )
{
	for ( const IMAGE_SECTION_HEADER &section : sections )
	{
		if ( nRva >= section.VirtualAddress && nRva < static_cast<ULONGLONG>( section.VirtualAddress ) + section.Misc.VirtualSize )
		{
			nOffset = static_cast<size_t>( nRva - section.VirtualAddress + section.PointerToRawData );
			return true;
		}
	}

	return false;
}

static bool ReadImage( const Bytes_t &bytes, Image_t &image, std::string &error )
{
	IMAGE_DOS_HEADER dos = {};
	DWORD nSignature = 0;
	if ( !ReadAt( bytes, 0, dos ) || dos.e_magic != IMAGE_DOS_SIGNATURE || !ReadAt( bytes, dos.e_lfanew, nSignature ) || nSignature != IMAGE_NT_SIGNATURE )
	{
		error = "not a PE file";
		return false;
	}

	const size_t nFileHeader = static_cast<size_t>( dos.e_lfanew ) + sizeof( DWORD );
	const size_t nOptional   = nFileHeader + sizeof( IMAGE_FILE_HEADER );

	IMAGE_FILE_HEADER file = {};
	WORD nMagic = 0;
	ReadAt( bytes, nFileHeader, file );
	ReadAt( bytes, nOptional, nMagic );

	IMAGE_DATA_DIRECTORY imports = {};
	if ( nMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC )
	{
		IMAGE_OPTIONAL_HEADER64 optional = {};
		ReadAt( bytes, nOptional, optional );
		imports = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		image.m_b64 = true;
	}
	else if ( nMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC )
	{
		IMAGE_OPTIONAL_HEADER32 optional = {};
		ReadAt( bytes, nOptional, optional );
		imports = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	}
	else
	{
		error = "unknown PE optional header";
		return false;
	}

	std::vector<IMAGE_SECTION_HEADER> sections( file.NumberOfSections );
	for ( WORD i = 0; i < file.NumberOfSections; i++ )
	{
		ReadAt( bytes, nOptional + file.SizeOfOptionalHeader + ( i * sizeof( IMAGE_SECTION_HEADER ) ), sections[i] );
	}

	size_t nDescriptor = 0;
	if ( !RvaToOffset( sections, imports.VirtualAddress, nDescriptor ) )
	{
		error = "no import table";
		return false;
	}

	const size_t nThunkSize = image.m_b64 ? sizeof( ULONGLONG ) : sizeof( DWORD );
	for ( ;; nDescriptor += sizeof( IMAGE_IMPORT_DESCRIPTOR ) )
	{
		IMAGE_IMPORT_DESCRIPTOR descriptor = {};
		if ( !ReadAt( bytes, nDescriptor, descriptor ) || !descriptor.Name )
		{
			break;
		}

		Import_t import;
		size_t nThunk = 0;

		// No lookup table: the address table holds the names until bound
		const DWORD nLookup = descriptor.OriginalFirstThunk ? descriptor.OriginalFirstThunk : descriptor.FirstThunk;
		if ( !RvaToOffset( sections, descriptor.Name, import.m_nNameOffset ) || !ReadString( bytes, import.m_nNameOffset, import.m_Name ) || !RvaToOffset( sections, nLookup, nThunk ) )
		{
			error = "broken import descriptor";
			return false;
		}

		for ( ;; nThunk += nThunkSize )
		{
			ULONGLONG nEntry = 0;
			bool bOrdinal = false;
			if ( image.m_b64 )
			{
				ReadAt( bytes, nThunk, nEntry );
				bOrdinal = ( nEntry & IMAGE_ORDINAL_FLAG64 ) != 0;
			}
			else
			{
				DWORD nEntry32 = 0;
				ReadAt( bytes, nThunk, nEntry32 );
				nEntry   = nEntry32;
				bOrdinal = ( nEntry32 & IMAGE_ORDINAL_FLAG32 ) != 0;
			}

			if ( !nEntry )
			{
				break;
			}

			std::string function;
			size_t nName = 0;
			if ( bOrdinal )
			{
				function = "#" + std::to_string( nEntry & 0xFFFF );
			}
			else if ( RvaToOffset( sections, nEntry & 0x7FFFFFFF, nName ) )
			{
				// IMAGE_IMPORT_BY_NAME: a WORD hint, then the name
				ReadString( bytes, nName + sizeof( WORD ), function );
			}

			import.m_Functions.push_back( function );
		}

		image.m_Imports.push_back( import );
	}

	return true;
}

static const Import_t *FindImport( const Image_t &image, const char *pszName )
{
	for ( const Import_t &import : image.m_Imports )
	{
		if ( !_stricmp( import.m_Name.c_str(), pszName ) )
		{
			return &import;
		}
	}

	return nullptr;
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

//-----------------------------------------------------------------------------
// Steps
//-----------------------------------------------------------------------------

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

static std::wstring Widen( const std::string &ascii )
{
	const std::wstring wide( ascii.begin(), ascii.end() );
	return wide;
}

// hammerplusplus.exe patched before 8873: the original exe back, its proxy DLL and settings moved out
static void RestoreLegacyExe( const fs::path &hammerDir, const fs::path &moduleDir, std::wstring &output )
{
	const fs::path exe = hammerDir / L"hammerplusplus.exe";
	const fs::path backup = fs::path( exe ).concat( L".orig" );
	if ( !FileExists( backup ) )
	{
		return;
	}

	std::error_code error;
	fs::copy_file( backup, exe, fs::copy_options::overwrite_existing, error );
	if ( error )
	{
		output += L"Failed to restore hammerplusplus.exe patched by an earlier version\n";
		return;
	}

	RemoveFile( backup );
	for ( const wchar_t *pwszName : s_pwszLegacyNames )
	{
		RemoveFile( hammerDir / pwszName );
	}

	const fs::path legacyIni = hammerDir / L"hppxdark.ini";
	const fs::path ini = moduleDir / L"hppxdark.ini";
	if ( FileExists( legacyIni ) && !FileExists( ini ) )
	{
		fs::rename( legacyIni, ini, error );
	}

	output += L"Restored hammerplusplus.exe patched by an earlier version\n";
}

PatchResult_t PatchHammer( const fs::path &hammerDir )
{
	PatchResult_t result;

	const fs::path moduleDir = hammerDir / L"hammerplusplus" / L"bin";
	const fs::path module    = moduleDir / L"hammerplusplus_dll.dll";
	const fs::path backup    = fs::path( module ).concat( L".orig" );
	const fs::path proxy     = moduleDir / L"hppxdark.dll";

	RestoreLegacyExe( hammerDir, moduleDir, result.m_Output );

	Bytes_t bytes;
	Image_t image;
	std::string error;
	if ( !ReadFileBytes( module, bytes ) )
	{
		result.m_Output += L"Failed to read hammerplusplus_dll.dll\n";
		return result;
	}

	if ( !ReadImage( bytes, image, error ) )
	{
		result.m_Output += L"hammerplusplus_dll.dll: " + Widen( error ) + L"\n";
		return result;
	}

	if ( FindImport( image, s_pszNewName ) )
	{
		result.m_bSuccess = WriteProxy( image.m_b64, proxy );
		result.m_Output += result.m_bSuccess ? L"hppxdark.dll updated\n" : L"Failed to write hppxdark.dll, is Hammer++ running?\n";
		return result;
	}

	const Import_t *pTarget = FindImport( image, s_pszOldName );
	if ( !pTarget )
	{
		result.m_Output += Widen( s_pszOldName ) + L" import is not found, this Hammer++ build is not supported\n";
		return result;
	}

	std::string unknown;
	for ( const std::string &function : pTarget->m_Functions )
	{
		if ( !IsForwarded( function ) )
		{
			unknown += unknown.empty() ? function : ", " + function;
		}
	}

	if ( !unknown.empty() )
	{
		result.m_Output += L"This Hammer++ build imports functions hppxdark.dll does not forward: " + Widen( unknown ) + L"\n";
		return result;
	}

	// The new name with its terminator fits in the old one
	const size_t nOldLength = std::strlen( s_pszOldName );
	const size_t nNewLength = std::strlen( s_pszNewName );
	std::memset( bytes.data() + pTarget->m_nNameOffset, 0, nOldLength + 1 );
	std::memcpy( bytes.data() + pTarget->m_nNameOffset, s_pszNewName, nNewLength );

	std::error_code copyError;
	fs::copy_file( module, backup, fs::copy_options::overwrite_existing, copyError );
	if ( copyError )
	{
		result.m_Output += L"Failed to save hammerplusplus_dll.dll.orig\n";
		return result;
	}

	if ( !WriteFileBytes( module, bytes.data(), bytes.size() ) || !WriteProxy( image.m_b64, proxy ) )
	{
		fs::copy_file( backup, module, fs::copy_options::overwrite_existing, copyError );
		RemoveFile( backup );
		RemoveFile( proxy );
		result.m_Output += L"Failed to write hammerplusplus_dll.dll, is Hammer++ running?\n";
		return result;
	}

	result.m_bSuccess = true;
	result.m_Output += image.m_b64 ? L"Patched (x64)\n" : L"Patched (x86)\n";

	return result;
}

PatchResult_t RestoreHammer( const fs::path &hammerDir )
{
	PatchResult_t result;

	const fs::path moduleDir = hammerDir / L"hammerplusplus" / L"bin";
	const fs::path module    = moduleDir / L"hammerplusplus_dll.dll";
	const fs::path backup    = fs::path( module ).concat( L".orig" );

	RestoreLegacyExe( hammerDir, moduleDir, result.m_Output );

	if ( !FileExists( backup ) )
	{
		result.m_Output += L"No backup to restore: hammerplusplus_dll.dll.orig\n";
		return result;
	}

	std::error_code error;
	fs::copy_file( backup, module, fs::copy_options::overwrite_existing, error );
	if ( error )
	{
		result.m_Output += L"Failed to restore hammerplusplus_dll.dll, is Hammer++ running?\n";
		return result;
	}

	RemoveFile( backup );
	RemoveFile( moduleDir / L"hppxdark.dll" );

	result.m_bSuccess = true;
	result.m_Output += L"Patch removed, the original hammerplusplus_dll.dll is back\n";

	return result;
}
