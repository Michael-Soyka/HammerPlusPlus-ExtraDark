//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Import table of a PE file read from its bytes
//
//=============================================================================//

#include "installer/pe_imports.h"

#include <cstring>

using Sections_t = std::vector<IMAGE_SECTION_HEADER>;

// IMAGE_IMPORT_BY_NAME RVA in a thunk; the ordinal flag is the top bit
static const ULONGLONG s_nNameRvaMask = 0x7FFFFFFF;
static const ULONGLONG s_nOrdinalMask = 0xFFFF;

struct NtHeaders_t
{
	bool					m_b64           = false;
	IMAGE_DATA_DIRECTORY	m_Imports       = {};
	size_t					m_nSections     = 0;
	WORD					m_nSectionCount = 0;
};

struct Thunk_t
{
	ULONGLONG	m_nValue   = 0;
	bool		m_bOrdinal = false;
};

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
		if ( !bytes[ n ] )
		{
			return true;
		}

		text.push_back( static_cast<char>( bytes[ n ] ) );
	}

	return false;
}

static bool RvaToOffset( const Sections_t &sections, ULONGLONG nRva, size_t &nOffset )
{
	for ( const IMAGE_SECTION_HEADER &section : sections )
	{
		const ULONGLONG nEnd = static_cast<ULONGLONG>( section.VirtualAddress ) + section.Misc.VirtualSize;

		if ( nRva >= section.VirtualAddress && nRva < nEnd )
		{
			nOffset = static_cast<size_t>( nRva - section.VirtualAddress + section.PointerToRawData );

			return true;
		}
	}

	return false;
}

template <typename Optional_t>
static bool ReadImportDirectory( const Bytes_t &bytes, size_t nOptional, IMAGE_DATA_DIRECTORY &imports )
{
	Optional_t optional = {};

	if ( !ReadAt( bytes, nOptional, optional ) )
	{
		return false;
	}

	imports = optional.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];

	return true;
}

static bool ReadNtHeaders( const Bytes_t &bytes, NtHeaders_t &headers, std::string &error )
{
	IMAGE_DOS_HEADER dos     = {};
	DWORD nSignature         = 0;
	IMAGE_FILE_HEADER file   = {};
	const size_t nFileHeader = sizeof( DWORD );

	const bool bPe = ReadAt( bytes, 0, dos ) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
		ReadAt( bytes, dos.e_lfanew, nSignature ) && nSignature == IMAGE_NT_SIGNATURE &&
		ReadAt( bytes, dos.e_lfanew + nFileHeader, file );

	if ( !bPe )
	{
		error = "not a PE file";

		return false;
	}

	const size_t nOptional = static_cast<size_t>( dos.e_lfanew ) + nFileHeader + sizeof( IMAGE_FILE_HEADER );

	WORD nMagic = 0;
	ReadAt( bytes, nOptional, nMagic );

	bool bRead = false;

	if ( nMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC )
	{
		headers.m_b64 = true;
		bRead = ReadImportDirectory<IMAGE_OPTIONAL_HEADER64>( bytes, nOptional, headers.m_Imports );
	}
	else if ( nMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC )
	{
		bRead = ReadImportDirectory<IMAGE_OPTIONAL_HEADER32>( bytes, nOptional, headers.m_Imports );
	}

	if ( !bRead )
	{
		error = "unknown PE optional header";

		return false;
	}

	headers.m_nSections     = nOptional + file.SizeOfOptionalHeader;
	headers.m_nSectionCount = file.NumberOfSections;

	return true;
}

// Unreadable entries are left out: their RVAs then resolve to nothing
static Sections_t ReadSections( const Bytes_t &bytes, const NtHeaders_t &headers )
{
	Sections_t sections;

	for ( WORD i = 0; i < headers.m_nSectionCount; i++ )
	{
		IMAGE_SECTION_HEADER section = {};

		if ( ReadAt( bytes, headers.m_nSections + ( i * sizeof( IMAGE_SECTION_HEADER ) ), section ) )
		{
			sections.push_back( section );
		}
	}

	return sections;
}

// An unreadable entry ends the table like the null one
static Thunk_t ReadThunk( const Bytes_t &bytes, size_t nOffset, bool b64 )
{
	Thunk_t thunk;

	if ( b64 )
	{
		ReadAt( bytes, nOffset, thunk.m_nValue );
		thunk.m_bOrdinal = ( thunk.m_nValue & IMAGE_ORDINAL_FLAG64 ) != 0;
	}
	else
	{
		DWORD nValue = 0;
		ReadAt( bytes, nOffset, nValue );
		thunk.m_nValue   = nValue;
		thunk.m_bOrdinal = ( nValue & IMAGE_ORDINAL_FLAG32 ) != 0;
	}

	return thunk;
}

static std::string ReadFunctionName( const Bytes_t &bytes, const Sections_t &sections, const Thunk_t &thunk )
{
	std::string function;
	size_t nName = 0;

	if ( thunk.m_bOrdinal )
	{
		function = "#" + std::to_string( thunk.m_nValue & s_nOrdinalMask );
	}
	else if ( RvaToOffset( sections, thunk.m_nValue & s_nNameRvaMask, nName ) )
	{
		// IMAGE_IMPORT_BY_NAME: a WORD hint, then the name
		ReadString( bytes, nName + sizeof( WORD ), function );
	}

	return function;
}

static std::vector<std::string> ReadImportFunctions( const Bytes_t &bytes, const Sections_t &sections, size_t nThunk, bool b64 )
{
	const size_t nThunkSize = b64 ? sizeof( ULONGLONG ) : sizeof( DWORD );

	std::vector<std::string> functions;

	for ( ;; nThunk += nThunkSize )
	{
		const Thunk_t thunk = ReadThunk( bytes, nThunk, b64 );

		if ( !thunk.m_nValue )
		{
			break;
		}

		functions.push_back( ReadFunctionName( bytes, sections, thunk ) );
	}

	return functions;
}

static bool ReadImport( const Bytes_t &bytes, const Sections_t &sections, const IMAGE_IMPORT_DESCRIPTOR &descriptor, bool b64, Import_t &import )
{
	// No lookup table: the address table holds the names until bound
	const DWORD nLookup = descriptor.OriginalFirstThunk ? descriptor.OriginalFirstThunk : descriptor.FirstThunk;

	size_t nThunk = 0;
	const bool bRead = RvaToOffset( sections, descriptor.Name, import.m_nNameOffset ) &&
		ReadString( bytes, import.m_nNameOffset, import.m_Name ) &&
		RvaToOffset( sections, nLookup, nThunk );

	if ( !bRead )
	{
		return false;
	}

	import.m_Functions = ReadImportFunctions( bytes, sections, nThunk, b64 );

	return true;
}

bool ReadImage( const Bytes_t &bytes, Image_t &image, std::string &error )
{
	NtHeaders_t headers;

	if ( !ReadNtHeaders( bytes, headers, error ) )
	{
		return false;
	}

	image.m_b64 = headers.m_b64;

	const Sections_t sections = ReadSections( bytes, headers );

	size_t nDescriptor = 0;

	if ( !RvaToOffset( sections, headers.m_Imports.VirtualAddress, nDescriptor ) )
	{
		error = "no import table";

		return false;
	}

	for ( ;; nDescriptor += sizeof( IMAGE_IMPORT_DESCRIPTOR ) )
	{
		IMAGE_IMPORT_DESCRIPTOR descriptor = {};

		if ( !ReadAt( bytes, nDescriptor, descriptor ) || !descriptor.Name )
		{
			break;
		}

		Import_t import;

		if ( !ReadImport( bytes, sections, descriptor, image.m_b64, import ) )
		{
			error = "broken import descriptor";

			return false;
		}

		image.m_Imports.push_back( import );
	}

	return true;
}

const Import_t *FindImport( const Image_t &image, const char *pszName )
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
