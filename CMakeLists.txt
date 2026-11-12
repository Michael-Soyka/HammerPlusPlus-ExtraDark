cmake_minimum_required( VERSION 3.21 )

project( hppxdark LANGUAGES CXX )

# Hammer++ comes in both: x64 (-A x64) and x86 (-A Win32) builds of the shim
if ( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	set( HPPXDARK_ARCH x64 )
else ()
	set( HPPXDARK_ARCH x86 )
endif ()

# On: hppxdark.log next to the DLL. Off: logging is compiled out
option( HPPXDARK_LOG "Write hppxdark.log next to the DLL" OFF )

# Static CRT: no runtime DLLs next to hammerplusplus_dll.dll
set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

add_library( hppxdark SHARED
	src/dark_theme.cpp
	src/iat_hook.cpp
	src/icons.cpp
	src/log.cpp
	src/hppxdark.cpp
	src/hppxdark.def
)

# src/assets/Bitmap<id>.bmp become bitmap resources <id>, replacing the Hammer++ ones
file( GLOB HPPXDARK_ASSETS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/assets/Bitmap*.bmp" )
set( HPPXDARK_RC "" )
foreach( ASSET ${HPPXDARK_ASSETS} )
	get_filename_component( ASSET_NAME "${ASSET}" NAME_WE )
	string( REGEX REPLACE "^Bitmap" "" ASSET_ID "${ASSET_NAME}" )
	string( APPEND HPPXDARK_RC "${ASSET_ID} BITMAP \"${ASSET}\"\n" )
endforeach ()

file( GENERATE OUTPUT "${CMAKE_BINARY_DIR}/hppxdark_assets.rc" CONTENT "${HPPXDARK_RC}" )
target_sources( hppxdark PRIVATE "${CMAKE_BINARY_DIR}/hppxdark_assets.rc" )

if ( HPPXDARK_LOG )
	target_compile_definitions( hppxdark PRIVATE HPPXDARK_LOG )
endif ()

target_compile_features( hppxdark PRIVATE cxx_std_17 )
target_compile_options( hppxdark PRIVATE /W4 )
target_link_libraries( hppxdark PRIVATE advapi32 comctl32 dwmapi user32 uxtheme )

# patch.ps1 takes the DLL from bin\x64 or bin\x86
set( HPPXDARK_OUTPUT "${CMAKE_SOURCE_DIR}/bin/${HPPXDARK_ARCH}" )
set_target_properties( hppxdark PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${HPPXDARK_OUTPUT}"
)
