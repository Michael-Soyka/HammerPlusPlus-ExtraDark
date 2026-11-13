//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: hppxdark versions: MAJOR.MINOR.BUILD.REVISION from the build time;
//			GitHub Actions builds add -git to the text
//
//=============================================================================//

#include "installer/version.h"

#include "hppxdark_version.h"

#include <windows.h>

#include <vector>

Version_t InstallerVersion()
{
	Version_t version;
	version.m_nMajor = HPPXDARK_VERSION_MAJOR;
	version.m_nMinor = HPPXDARK_VERSION_MINOR;
	version.m_nBuild    = HPPXDARK_VERSION_BUILD;
	version.m_nRevision = HPPXDARK_VERSION_REVISION;
	version.m_Text      = HPPXDARK_VERSION_TEXT_W;

	return version;
}

bool ReadFileVersion( const std::filesystem::path &path, Version_t &version )
{
	DWORD nHandle = 0;
	const DWORD nSize = GetFileVersionInfoSizeW( path.c_str(), &nHandle );
	if ( !nSize )
	{
		return false;
	}

	std::vector<BYTE> data( nSize );
	VS_FIXEDFILEINFO *pInfo = nullptr;
	UINT nInfo = 0;
	if ( !GetFileVersionInfoW( path.c_str(), 0, nSize, data.data() ) || !VerQueryValueW( data.data(), L"\\", reinterpret_cast<void **>( &pInfo ), &nInfo ) || !pInfo )
	{
		return false;
	}

	version.m_nMajor = HIWORD( pInfo->dwFileVersionMS );
	version.m_nMinor = LOWORD( pInfo->dwFileVersionMS );
	version.m_nBuild    = HIWORD( pInfo->dwFileVersionLS );
	version.m_nRevision = LOWORD( pInfo->dwFileVersionLS );

	// Language block of src/version.rc.in: US English, Unicode
	wchar_t *pwszText = nullptr;
	UINT nText = 0;
	if ( VerQueryValueW( data.data(), L"\\StringFileInfo\\040904b0\\FileVersion", reinterpret_cast<void **>( &pwszText ), &nText ) && pwszText && nText )
	{
		version.m_Text = pwszText;
	}

	return true;
}

int CompareVersions( const Version_t &a, const Version_t &b )
{
	const unsigned nA[] = { a.m_nMajor, a.m_nMinor, a.m_nBuild, a.m_nRevision };
	const unsigned nB[] = { b.m_nMajor, b.m_nMinor, b.m_nBuild, b.m_nRevision };
	for ( size_t i = 0; i < ARRAYSIZE( nA ); i++ )
	{
		if ( nA[i] != nB[i] )
		{
			const int nOrder = nA[i] < nB[i] ? -1 : 1;
			return nOrder;
		}
	}

	return 0;
}

std::wstring FormatVersion( const Version_t &version )
{
	if ( !version.m_Text.empty() )
	{
		return version.m_Text;
	}

	const std::wstring text = std::to_wstring( version.m_nMajor ) + L"." + std::to_wstring( version.m_nMinor ) + L"." + std::to_wstring( version.m_nBuild ) + L"." + std::to_wstring( version.m_nRevision );
	return text;
}
