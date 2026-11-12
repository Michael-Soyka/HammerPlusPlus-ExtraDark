#============ Copyright 2026 Moon-6 Team, All rights reserved. ===============#
#
# Purpose: Points the ADVAPI32 import of hammerplusplus_dll.dll at hppxdark.dll,
#			or restores the original DLL with -Restore
#
#=============================================================================#

param(
	# The game bin folder or the one with hammerplusplus.exe itself; asked for when not given
	[string] $HammerDir = '',
	[switch] $Restore
)

$ErrorActionPreference = 'Stop'

# bin, bin\x64 or bin\win64: the folder that holds hammerplusplus.exe, empty if none does
function Find-HammerDir( [string] $Path )
{
	foreach ( $candidate in @( $Path, ( Join-Path $Path 'x64' ), ( Join-Path $Path 'win64' ) ) )
	{
		if ( Test-Path ( Join-Path $candidate 'hammerplusplus.exe' ) )
		{
			$resolved = ( Resolve-Path $candidate ).Path
			return $resolved
		}
	}

	return ''
}

if ( $HammerDir )
{
	$found = Find-HammerDir $HammerDir
	if ( -not $found )
	{
		throw "hammerplusplus.exe is not found in $HammerDir, its x64 or win64"
	}

	$HammerDir = $found
}
elseif ( [Console]::IsInputRedirected )
{
	throw 'Pass the game bin folder: patch.bat C:\path\to\bin'
}
else
{
	# Asked until a folder with hammerplusplus.exe is given
	while ( -not $HammerDir )
	{
		# Quotes come with a folder dragged into the console
		$answer = ( Read-Host 'Game bin folder' ).Trim().Trim( '"' )
		if ( -not $answer )
		{
			continue
		}

		$HammerDir = Find-HammerDir $answer
		if ( -not $HammerDir )
		{
			Write-Host "hammerplusplus.exe is not found in $answer, its x64 or win64"
		}
	}
}

$moduleDir = Join-Path $HammerDir 'hammerplusplus\bin'
$module    = Join-Path $moduleDir 'hammerplusplus_dll.dll'
$backup    = "$module.orig"
$shim      = Join-Path $moduleDir 'hppxdark.dll'

$oldName = 'ADVAPI32.dll'
$newName = 'hppxdark.dll'

# ADVAPI32 functions forwarded by hppxdark.def
$forwarded = @(
	'AdjustTokenPrivileges', 'GetFileSecurityA', 'GetUserNameA', 'LookupPrivilegeValueW', 'OpenProcessToken',
	'RegCloseKey', 'RegCreateKeyExA', 'RegDeleteKeyA', 'RegDeleteValueA', 'RegEnumKeyA', 'RegEnumKeyExA',
	'RegEnumValueA', 'RegOpenKeyExA', 'RegOpenKeyExW', 'RegQueryValueA', 'RegQueryValueExA', 'RegSetValueA',
	'RegSetValueExA', 'SetFileSecurityA'
)

# Builds before 8873 had the import patched in hammerplusplus.exe, under these shim names
$legacyExe   = Join-Path $HammerDir 'hammerplusplus.exe'
$legacyShims = @( 'hppxdark.dll', 'otm_hpp.dll' )

if ( -not ( Test-Path $module ) )
{
	throw "$module is not found, this Hammer++ build is not supported"
}

function Read-CString( [byte[]] $Bytes, [int] $Offset )
{
	$end = $Offset
	while ( $Bytes[$end] -ne 0 )
	{
		$end++
	}

	$text = [Text.Encoding]::ASCII.GetString( $Bytes, $Offset, $end - $Offset )
	return $text
}

function Get-FileOffset( [byte[]] $Bytes, [long] $Rva )
{
	$pe = [BitConverter]::ToInt32( $Bytes, 0x3C )
	$sections = [BitConverter]::ToUInt16( $Bytes, $pe + 6 )
	$optionalSize = [BitConverter]::ToUInt16( $Bytes, $pe + 20 )
	$table = $pe + 24 + $optionalSize

	for ( $i = 0; $i -lt $sections; $i++ )
	{
		$s = $table + ( $i * 40 )
		$virtualSize = [BitConverter]::ToUInt32( $Bytes, $s + 8 )
		$virtualAddress = [BitConverter]::ToUInt32( $Bytes, $s + 12 )
		$rawAddress = [BitConverter]::ToUInt32( $Bytes, $s + 20 )

		if ( $Rva -ge $virtualAddress -and $Rva -lt ( $virtualAddress + $virtualSize ) )
		{
			$offset = [int] ( $Rva - $virtualAddress + $rawAddress )
			return $offset
		}
	}

	throw "RVA 0x$( '{0:X}' -f $Rva ) is outside of all sections"
}

# x64 for PE32+, x86 for PE32
function Get-Arch( [byte[]] $Bytes )
{
	$pe = [BitConverter]::ToInt32( $Bytes, 0x3C )
	$magic = [BitConverter]::ToUInt16( $Bytes, $pe + 24 )
	if ( $magic -eq 0x20B )
	{
		return 'x64'
	}

	if ( $magic -eq 0x10B )
	{
		return 'x86'
	}

	throw "Unknown PE optional header 0x$( '{0:X}' -f $magic )"
}

# Import descriptors: name file offset and imported function names
function Get-Imports( [byte[]] $Bytes )
{
	$pe = [BitConverter]::ToInt32( $Bytes, 0x3C )
	$is64 = ( Get-Arch $Bytes ) -eq 'x64'

	# Data directory 1 (imports), after the 112 or 96 bytes of PE32+ or PE32 fields
	$directories = $pe + 24 + $( if ( $is64 ) { 112 } else { 96 } )
	$importRva = [BitConverter]::ToUInt32( $Bytes, $directories + 8 )
	$descriptor = Get-FileOffset $Bytes $importRva
	$thunkSize = $( if ( $is64 ) { 8 } else { 4 } )

	$imports = @()
	while ( $true )
	{
		$lookupRva = [BitConverter]::ToUInt32( $Bytes, $descriptor )
		$nameRva = [BitConverter]::ToUInt32( $Bytes, $descriptor + 12 )
		if ( $nameRva -eq 0 )
		{
			break
		}

		# No lookup table: the address table holds the names until bound
		if ( $lookupRva -eq 0 )
		{
			$lookupRva = [BitConverter]::ToUInt32( $Bytes, $descriptor + 16 )
		}

		$nameOffset = Get-FileOffset $Bytes $nameRva
		$functions = @()
		$thunk = Get-FileOffset $Bytes $lookupRva
		while ( $true )
		{
			if ( $is64 )
			{
				$entry = [BitConverter]::ToUInt64( $Bytes, $thunk )
				$byOrdinal = ( $entry -shr 63 ) -eq 1
			}
			else
			{
				$entry = [BitConverter]::ToUInt32( $Bytes, $thunk )
				$byOrdinal = ( $entry -shr 31 ) -eq 1
			}

			if ( $entry -eq 0 )
			{
				break
			}

			if ( $byOrdinal )
			{
				$functions += "#$( $entry -band 0xFFFF )"
			}
			else
			{
				$functions += Read-CString $Bytes ( ( Get-FileOffset $Bytes ( $entry -band 0x7FFFFFFF ) ) + 2 )
			}

			$thunk += $thunkSize
		}

		$imports += [pscustomobject]@{ Name = ( Read-CString $Bytes $nameOffset ); NameOffset = $nameOffset; Functions = $functions }
		$descriptor += 20
	}

	return $imports
}

# hammerplusplus.exe patched before 8873: the original exe back, its shim and settings moved out
function Restore-LegacyExe
{
	$legacyBackup = "$legacyExe.orig"
	if ( -not ( Test-Path $legacyBackup ) )
	{
		return
	}

	Copy-Item $legacyBackup $legacyExe -Force
	Remove-Item $legacyBackup
	foreach ( $name in $legacyShims )
	{
		$file = Join-Path $HammerDir $name
		if ( Test-Path $file )
		{
			Remove-Item $file
		}
	}

	$legacyIni = Join-Path $HammerDir 'hppxdark.ini'
	$ini = Join-Path $moduleDir 'hppxdark.ini'
	if ( ( Test-Path $legacyIni ) -and -not ( Test-Path $ini ) )
	{
		Move-Item $legacyIni $ini
	}

	Write-Host "Restored $legacyExe patched by an earlier version"
}

Restore-LegacyExe

if ( $Restore )
{
	if ( -not ( Test-Path $backup ) )
	{
		throw "No backup to restore: $backup"
	}

	Copy-Item $backup $module -Force
	Remove-Item $backup
	if ( Test-Path $shim )
	{
		Remove-Item $shim
	}

	Write-Host "Restored $module"
	exit 0
}

$bytes = [IO.File]::ReadAllBytes( $module )
$arch = Get-Arch $bytes
$builtShim = Join-Path $PSScriptRoot "bin\$arch\hppxdark.dll"
if ( -not ( Test-Path $builtShim ) )
{
	$platform = $( if ( $arch -eq 'x64' ) { 'x64' } else { 'Win32' } )
	throw "Build the $arch hppxdark.dll first: cmake -B build-$arch -A $platform && cmake --build build-$arch --config Release"
}

$imports = Get-Imports $bytes

if ( $imports | Where-Object { $_.Name -ieq $newName } )
{
	Copy-Item $builtShim $shim -Force
	Write-Host "Already patched, hppxdark.dll updated in $moduleDir"
	exit 0
}

$target = $imports | Where-Object { $_.Name -ieq $oldName }
if ( -not $target )
{
	throw "$oldName import is not found, this Hammer++ build is not supported"
}

$unknown = $target.Functions | Where-Object { $forwarded -notcontains $_ }
if ( $unknown )
{
	throw "Hammer++ imports from $oldName functions hppxdark.dll does not forward: $( $unknown -join ', ' ). Add them to src\hppxdark.def and `$forwarded"
}

# The new name with its terminator fits in the old one
$nameBytes = [Text.Encoding]::ASCII.GetBytes( $newName )
[Array]::Copy( $nameBytes, 0, $bytes, $target.NameOffset, $nameBytes.Length )
for ( $i = $nameBytes.Length; $i -le $oldName.Length; $i++ )
{
	$bytes[$target.NameOffset + $i] = 0
}

Copy-Item $module $backup -Force
[IO.File]::WriteAllBytes( $module, $bytes )
Copy-Item $builtShim $shim -Force

Write-Host "Patched $module ($arch), original saved as $( Split-Path $backup -Leaf )"
