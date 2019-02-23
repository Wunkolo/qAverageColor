cmake_minimum_required( VERSION 3.2.2 )
project( qAverageColor CXX )

### Standard
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS ON )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_VERBOSE_MAKEFILE ON )

# Generate 'compile_commands.json' for clang_complete
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Optimizations
if( MSVC )
	add_compile_options( /arch:AVX2 )
	add_compile_options( /W3 )
	add_compile_options( /Gv )
elseif( CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
	# Force colored diagnostic messages in Ninja's output
	if( CMAKE_GENERATOR STREQUAL "Ninja" )
		add_compile_options( -fdiagnostics-color=always )
	endif()
endif()

add_library(
	qAverageColor
	STATIC
	source/qAverageColor.cpp
)
target_include_directories(
	qAverageColor
	PUBLIC
	include
)

add_executable(
	AverageColor
	tests/AverageColor.cpp
)
target_link_libraries(
	AverageColor
	PRIVATE
	qAverageColor
)