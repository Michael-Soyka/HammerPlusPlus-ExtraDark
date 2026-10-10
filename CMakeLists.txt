cmake_minimum_required( VERSION 3.21 )

project( hppxdark LANGUAGES CXX )

if ( NOT CMAKE_SIZEOF_VOID_P EQUAL 8 )
	message( FATAL_ERROR "Hammer++ is 64-bit, configure for x64" )
endif ()

# Static CRT: the shim loads before anything else in hammerplusplus.exe
set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

add_library( hppxdark SHARED
	src/iat_hook.cpp
	src/hppxdark.cpp
	src/hppxdark.def
)

target_compile_features( hppxdark PRIVATE cxx_std_17 )
target_compile_options( hppxdark PRIVATE /W4 )
target_link_libraries( hppxdark PRIVATE advapi32 user32 )

# patch.ps1 takes the DLL from bin
set_target_properties( hppxdark PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/bin"
)
