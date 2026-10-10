#============ Copyright 2026 Moon-6 Team, All rights reserved. ===============#
#
# Purpose: Points the ADVAPI32 import of hammerplusplus.exe at hppxdark.dll,
#			or restores the original exe with -Restore
#
#=============================================================================#

param(
	# The game bin folder or bin\x64 itself; asked for when not given
	[string] $HammerDir = '',
	[switch] $Restore
)

$ErrorActionPreference = 'Stop'

# bin or bin\x64: the folder that holds hammerplusplus.exe, empty if neither does
function Find-HammerDir( [string] $Path )
{
	foreach ( $candidate in @( $Path, ( Join-Path $Path 'x64' ) ) )
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
		throw "hammerplusplus.exe is not found in $HammerDir or $( Join-Path $HammerDir 'x64' )"
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
			Write-Host "hammerplusplus.exe is not found in $answer or $( Join-Path $answer 'x64' )"
		}
	}
}

$exe       = Join-Path $HammerDir 'hammerplusplus.exe'
$backup    = "$exe.orig"
$shim      = Join-Path $HammerDir 'hppxdark.dll'
$builtShim = Join-Path $PSScriptRoot 'bin\hppxdark.dll'

$oldName = 'ADVAPI32.dll'
$newName = 'hppxdark.dll'

# Registry functions forwarded by hppxdark.def
$forwarded = @( 'RegOpenKeyExA', 'RegQueryValueExA', 'RegCloseKey' )

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

function Get-FileOffset( [byte[]] $Bytes, [int] $Rva )
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
			$offset = $Rva - $virtualAddress + $rawAddress
			return $offset
		}
	}

	throw "RVA 0x$( '{0:X}' -f $Rva ) is outside of all sections"
}

# Import descriptors: name file offset and imported function names
function Get-Imports( [byte[]] $Bytes )
{
	$pe = [BitConverter]::ToInt32( $Bytes, 0x3C )
	if ( [BitConverter]::ToUInt16( $Bytes, $pe + 24 ) -ne 0x20B )
	{
		throw 'Not a 64-bit executable'
	}

	# PE32+ data directory 1 (imports)
	$importRva = [BitConverter]::ToUInt32( $Bytes, $pe + 24 + 112 + 8 )
	$descriptor = Get-FileOffset $Bytes $importRva

	$imports = @()
	while ( $true )
	{
		$lookupRva = [BitConverter]::ToUInt32( $Bytes, $descriptor )
		$nameRva = [BitConverter]::ToUInt32( $Bytes, $descriptor + 12 )
		if ( $nameRva -eq 0 )
		{
			break
		}

		$nameOffset = Get-FileOffset $Bytes $nameRva
		$functions = @()
		$thunk = Get-FileOffset $Bytes $lookupRva
		while ( $true )
		{
			$entry = [BitConverter]::ToUInt64( $Bytes, $thunk )
			if ( $entry -eq 0 )
			{
				break
			}

			# Top bit: import by ordinal
			if ( ( $entry -shr 63 ) -eq 1 )
			{
				$functions += "#$( $entry -band 0xFFFF )"
			}
			else
			{
				$functions += Read-CString $Bytes ( ( Get-FileOffset $Bytes ( [int] ( $entry -band 0x7FFFFFFF ) ) ) + 2 )
			}

			$thunk += 8
		}

		$imports += [pscustomobject]@{ Name = ( Read-CString $Bytes $nameOffset ); NameOffset = $nameOffset; Functions = $functions }
		$descriptor += 20
	}

	return $imports
}

if ( $Restore )
{
	if ( -not ( Test-Path $backup ) )
	{
		throw "No backup to restore: $backup"
	}

	Copy-Item $backup $exe -Force
	Remove-Item $backup
	if ( Test-Path $shim )
	{
		Remove-Item $shim
	}

	Write-Host "Restored $exe"
	exit 0
}

if ( -not ( Test-Path $builtShim ) )
{
	throw "Build hppxdark.dll first: cmake -B build -A x64 && cmake --build build --config Release"
}

$bytes = [IO.File]::ReadAllBytes( $exe )
$imports = Get-Imports $bytes

if ( $imports | Where-Object { $_.Name -ieq $newName } )
{
	Copy-Item $builtShim $shim -Force
	Write-Host "Already patched, hppxdark.dll updated in $HammerDir"
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
	throw "Hammer++ imports from $oldName functions hppxdark.dll does not forward: $( $unknown -join ', ' ). Add them to src\hppxdark.def"
}

# The new name with its terminator fits in the old one
$nameBytes = [Text.Encoding]::ASCII.GetBytes( $newName )
[Array]::Copy( $nameBytes, 0, $bytes, $target.NameOffset, $nameBytes.Length )
for ( $i = $nameBytes.Length; $i -le $oldName.Length; $i++ )
{
	$bytes[$target.NameOffset + $i] = 0
}

Copy-Item $exe $backup -Force
[IO.File]::WriteAllBytes( $exe, $bytes )
Copy-Item $builtShim $shim -Force

Write-Host "Patched $exe, original saved as $( Split-Path $backup -Leaf )"
