cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(prefix-search-vector
	LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
	# default build type
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE RelWithDebInfo)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_EXTRA_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -pedantic" CACHE STRING "Extra flags used by the compiler")
	set(CMAKE_EXTRA_EXE_LINKER_FLAGS "-Wl,--as-needed" CACHE STRING "Extra flags used by the linker")
else()
	set(CMAKE_EXTRA_CXX_FLAGS "" CACHE STRING "Extra flags used by the compiler")
	set(CMAKE_EXTRA_EXE_LINKER_FLAGS "" CACHE STRING "Extra flags used by the linker")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_EXTRA_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXTRA_EXE_LINKER_FLAGS}")

add_executable(test_prefix_vector
	prefix_vector.cpp
	prefix_vector.hpp

	test_prefix_vector.cpp
	)
