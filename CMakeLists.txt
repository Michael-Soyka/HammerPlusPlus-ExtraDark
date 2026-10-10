cmake_minimum_required( VERSION 3.21 )

project( hppxdark LANGUAGES CXX )

if ( NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
	message( FATAL_ERROR "Hammer++ is 64-bit, configure for x64" )
endif ()

# On: Hammer++ runs on the On The Moon DXVK build from ..\..\otm\bin\x64 (OTM shaders in the
# editor). Off: on Valve's DXVK from bin\x64, for Hammer++ outside On The Moon
option( OTM_BUILD "Build for On The Moon: load its DXVK build" OFF )

# Static CRT: the shim loads before anything else in hammerplusplus.exe
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

if ( OTM_BUILD )
	target_compile_definitions( hppxdark PRIVATE OTM_BUILD )
endif ()

target_compile_features( hppxdark PRIVATE cxx_std_17 )
target_compile_options( hppxdark PRIVATE /W4 )
target_link_libraries( hppxdark PRIVATE advapi32 comctl32 dwmapi user32 uxtheme )

# patch.ps1 takes the DLL from bin
set_target_properties( hppxdark PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/bin"
)
