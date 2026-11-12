cmake_minimum_required( VERSION 3.21 )

# MAJOR.MINOR by hand, BUILD.REVISION from the build time: see cmake/version.cmake
project( hppxdark VERSION 1.0 LANGUAGES CXX )

find_package( Git QUIET )
set( HPPXDARK_COMMIT "unknown" )
if ( GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git" )
	execute_process( COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE HPPXDARK_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET )

	# A new commit or checkout configures again, so the commit follows HEAD
	if ( EXISTS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD" )
		set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD" )
	endif ()
endif ()

# GitHub Actions builds are marked in the text version; the numeric one has no room for it
set( HPPXDARK_SUFFIX "" )
if ( "$ENV{GITHUB_ACTIONS}" STREQUAL "true" )
	set( HPPXDARK_SUFFIX "-git" )
endif ()

# hppxdark_version.h is written on every build; a nested build gets the outer one's,
# so the installer and both DLLs carry the same version
if ( NOT HPPXDARK_VERSION_DIR )
	set( HPPXDARK_VERSION_DIR "${CMAKE_BINARY_DIR}/version" )
	file( MAKE_DIRECTORY "${HPPXDARK_VERSION_DIR}" )
	add_custom_target( hppxdark_version ALL
		COMMAND "${CMAKE_COMMAND}"
			-DMAJOR=${PROJECT_VERSION_MAJOR}
			-DMINOR=${PROJECT_VERSION_MINOR}
			"-DSUFFIX=${HPPXDARK_SUFFIX}"
			"-DCOMMIT=${HPPXDARK_COMMIT}"
			"-DOUTPUT=${HPPXDARK_VERSION_DIR}/hppxdark_version.h"
			-P "${CMAKE_SOURCE_DIR}/cmake/version.cmake"
		BYPRODUCTS "${HPPXDARK_VERSION_DIR}/hppxdark_version.h"
		COMMENT "Version from the build time"
	)
endif ()

# Version resource of one target from src/version.rc.in
function( hppxdark_version_resource TARGET FILE_NAME FILE_TYPE DESCRIPTION )
	set( HPPXDARK_FILE_NAME "${FILE_NAME}" )
	set( HPPXDARK_FILE_TYPE "${FILE_TYPE}" )
	set( HPPXDARK_FILE_DESCRIPTION "${DESCRIPTION}" )
	configure_file( "${CMAKE_SOURCE_DIR}/src/version.rc.in" "${CMAKE_BINARY_DIR}/${TARGET}_version.rc" @ONLY )
	target_sources( ${TARGET} PRIVATE "${CMAKE_BINARY_DIR}/${TARGET}_version.rc" )
	target_include_directories( ${TARGET} PRIVATE "${HPPXDARK_VERSION_DIR}" )
	if ( TARGET hppxdark_version )
		add_dependencies( ${TARGET} hppxdark_version )
	endif ()
endfunction ()

# Hammer++ comes in both: x64 (-A x64) and x86 (-A Win32) builds of the proxy DLL
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
	src/core/iat_hook.cpp
	src/core/log.cpp
	src/theme/dark_theme.cpp
	src/theme/icons.cpp
	src/hppxdark.cpp
	src/hppxdark.def
)

target_include_directories( hppxdark PRIVATE src )

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
hppxdark_version_resource( hppxdark hppxdark.dll VFT_DLL "Hammer++ Extra Dark proxy DLL" )

if ( HPPXDARK_LOG )
	target_compile_definitions( hppxdark PRIVATE HPPXDARK_LOG )
endif ()

target_compile_features( hppxdark PRIVATE cxx_std_17 )
target_compile_options( hppxdark PRIVATE /W4 )
target_link_libraries( hppxdark PRIVATE advapi32 comctl32 dwmapi user32 uxtheme )

# bin\x64 and bin\x86: the installer packs both
set( HPPXDARK_OUTPUT "${CMAKE_SOURCE_DIR}/bin/${HPPXDARK_ARCH}" )
set_target_properties( hppxdark PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${HPPXDARK_OUTPUT}"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${HPPXDARK_OUTPUT}"
)

# Installer with both proxy DLLs inside: built by the x64 configuration, which builds the
# x86 DLL as a nested Win32 configuration of this project
if ( HPPXDARK_ARCH STREQUAL "x64" )
	include( ExternalProject )
	ExternalProject_Add( hppxdark_x86
		SOURCE_DIR "${CMAKE_SOURCE_DIR}"
		BINARY_DIR "${CMAKE_BINARY_DIR}/x86"
		CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		CMAKE_GENERATOR_PLATFORM Win32
		CMAKE_ARGS -DHPPXDARK_LOG=${HPPXDARK_LOG} "-DHPPXDARK_VERSION_DIR=${HPPXDARK_VERSION_DIR}"
		DEPENDS hppxdark_version
		BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target hppxdark
		INSTALL_COMMAND ""
		BUILD_ALWAYS TRUE
		BUILD_BYPRODUCTS "${CMAKE_SOURCE_DIR}/bin/x86/hppxdark.dll"
	)

	add_executable( hppxdark_setup WIN32
		src/installer/hammer_dir.cpp
		src/installer/patcher.cpp
		src/installer/version.cpp
		src/installer/main.cpp
	)

	file( GENERATE OUTPUT "${CMAKE_BINARY_DIR}/hppxdark_setup.rc" CONTENT
		"#include \"installer/resources.h\"\nIDI_APP ICON \"${CMAKE_SOURCE_DIR}/src/assets/hpp-extra-dark-app-icon.ico\"\nIDR_PROXY_X86 RCDATA \"${CMAKE_SOURCE_DIR}/bin/x86/hppxdark.dll\"\nIDR_PROXY_X64 RCDATA \"$<TARGET_FILE:hppxdark>\"\n"
	)
	target_sources( hppxdark_setup PRIVATE "${CMAKE_BINARY_DIR}/hppxdark_setup.rc" )
	hppxdark_version_resource( hppxdark_setup hppxdark_setup.exe VFT_APP "Hammer++ Extra Dark installer" )
	add_dependencies( hppxdark_setup hppxdark hppxdark_x86 )

	target_include_directories( hppxdark_setup PRIVATE src )
	target_compile_definitions( hppxdark_setup PRIVATE
		UNICODE _UNICODE
	)
	target_compile_features( hppxdark_setup PRIVATE cxx_std_17 )
	target_compile_options( hppxdark_setup PRIVATE /W4 /utf-8 )
	target_link_libraries( hppxdark_setup PRIVATE comctl32 dwmapi ole32 shell32 uxtheme version )

	set_target_properties( hppxdark_setup PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/bin"
	)
endif ()
