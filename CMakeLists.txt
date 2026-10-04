cmake_minimum_required(VERSION 3.26)

project(
	BGEDSortFix
	VERSION 1.0.0
	LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# CommonLibSSE-NG: git submodule (git submodule update --init).
set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "" FORCE)
add_subdirectory("${COMMONLIB_DIR}" CommonLibSSE EXCLUDE_FROM_ALL)
include("${COMMONLIB_DIR}/cmake/CommonLibSSE.cmake")

add_commonlibsse_plugin(
	${PROJECT_NAME}
	AUTHOR "nyxvel"
	SOURCES
		src/main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8 /Zc:preprocessor /wd4100 /wd4201)
	target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release,RelWithDebInfo>:/DEBUG:FULL;/OPT:REF;/OPT:ICF>")
endif()
