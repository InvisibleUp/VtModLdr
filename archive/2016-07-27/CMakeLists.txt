CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12 FATAL_ERROR)
PROJECT (SrModLdr C)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SrModLdr_BINARY_DIR}/bin)
endif()

# Set the Build type for make based generators.
# You can choose following types:
#   Debug          : Debug build
#   Release        : Release build
#   RelWithDebInfo : Release build with Debug Info
#   MinSizeRel     : Release Min Size build
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)
# Set a value type to properly display CMAKE_BUILD_TYPE on GUI if the
# value type is "UNINITIALIZED".
GET_PROPERTY(cached_type CACHE CMAKE_BUILD_TYPE PROPERTY TYPE)
IF("${cached_type}" STREQUAL "UNINITIALIZED")
  SET(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "Build Type" FORCE)
ENDIF("${cached_type}" STREQUAL "UNINITIALIZED")
# Check the Build Type.
IF(NOT "${CMAKE_BUILD_TYPE}"
       MATCHES "^(Debug|Release|RelWithDebInfo|MinSizeRel)\$")
  MESSAGE(FATAL_ERROR
          "Unknown keyword for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}\n"
          "Acceptable keywords: Debug,Release,RelWithDebInfo,MinSizeRel")
ENDIF(NOT "${CMAKE_BUILD_TYPE}"
          MATCHES "^(Debug|Release|RelWithDebInfo|MinSizeRel)\$")
	  
# Initialize the state of the variables. This initialization is not
# necessary but this shows you what value the variables initially have.
#SET(CMAKE_REQUIRED_DEFINITIONS)
#SET(CMAKE_REQUIRED_INCLUDES)
#SET(CMAKE_REQUIRED_LIBRARIES)
#SET(CMAKE_REQUIRED_FLAGS)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake-deps/")
IF(WIN32 AND NOT CYGWIN)
	# Because for some dumb reason compiler directories aren't system directories
	# Also I'm honestly surprised this works at all.
	SET(CMAKE_PREFIX_PATH "${CMAKE_C_COMPILER}/../../")
	
ENDIF(WIN32 AND NOT CYGWIN)

### Check libraries
FIND_PACKAGE(SQLITE3 REQUIRED)
FIND_PACKAGE(JANSSON REQUIRED)
FIND_PACKAGE(ZLIB REQUIRED)
FIND_PACKAGE(ARCHIVE REQUIRED)

### Check headers
INCLUDE (CheckIncludeFiles)

# Define preprocessor macros when found
# (Taken from libarchive source and modified)
SET(INCLUDES "")
MACRO (LA_CHECK_INCLUDE_FILE header var)
	CHECK_INCLUDE_FILES("${header}" ${var})
	IF (${var})
		LIST(APPEND INCLUDES ${var})
	ENDIF (${var})
ENDMACRO (LA_CHECK_INCLUDE_FILE)

MACRO (LA_CHECK_INCLUDE_FILE_REQ header var)
	CHECK_INCLUDE_FILES("${header}" ${var})
	IF (NOT ${var})
		MESSAGE(FATAL_ERROR "Required header ${header} missing!")
	ELSE ()
		LIST(APPEND INCLUDES ${var})
	ENDIF ()
ENDMACRO (LA_CHECK_INCLUDE_FILE_REQ)

# C99 Core libaries (must exist)
LA_CHECK_INCLUDE_FILE_REQ("stdlib.h" HAVE_STDLIB_H)
LA_CHECK_INCLUDE_FILE_REQ("errno.h" HAVE_ERRNO_H)
LA_CHECK_INCLUDE_FILE_REQ("stdarg.h" HAVE_STDARG_H)
LA_CHECK_INCLUDE_FILE_REQ("math.h" HAVE_MATH_H)
LA_CHECK_INCLUDE_FILE_REQ("stdint.h" HAVE_STDINT_H)

# Weird platform-specific libraries that may or may not exist
LA_CHECK_INCLUDE_FILE_REQ("fcntl.h" HAVE_FCNTL_H) # TODO: Find alternatives 
LA_CHECK_INCLUDE_FILE_REQ("io.h" HAVE_IO_H) # Ditto.
LA_CHECK_INCLUDE_FILE("dir.h" HAVE_DIR_H)
LA_CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)

# Add to preprocessor
FOREACH(i ${INCLUDES})
	ADD_DEFINITIONS(-D${i})
ENDFOREACH(i)

### Check functions
include(CheckFunctionExists)
SET(FUNCTIONS "")
MACRO (LA_CHECK_FUNCTION_EXISTS function var)
	CHECK_FUNCTION_EXISTS("${function}" ${var})
	IF (${var})
		LIST(APPEND FUNCTIONS ${var})
	ENDIF (${var})
ENDMACRO (LA_CHECK_FUNCTION_EXISTS)

# Printing functions
LA_CHECK_FUNCTION_EXISTS(snprintf HAVE_SNPRINTF)
LA_CHECK_FUNCTION_EXISTS(_snprintf HAVE_MS_SNPRINTF)
LA_CHECK_FUNCTION_EXISTS(vsnprintf HAVE_VSNPRINTF)
LA_CHECK_FUNCTION_EXISTS(_vsnprintf HAVE_MS_VSNPRINTF)
LA_CHECK_FUNCTION_EXISTS(asprintf HAVE_ASPRINTF)

# Directory functions
LA_CHECK_FUNCTION_EXISTS(chdir HAVE_CHDIR)
LA_CHECK_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
LA_CHECK_FUNCTION_EXISTS(filesize HAVE_FILESIZE)

# Add to preprocessor
FOREACH(i ${FUNCTIONS})
	ADD_DEFINITIONS(-D${i})
ENDFOREACH(i)

# Add to preprocessor
FOREACH(i ${SYMBOLS})
	ADD_DEFINITIONS(-D${i})
ENDFOREACH(i)

### Set source files
SET(srmodldr_HEADERS
	errormsgs.h
	funcproto.h
	shims/crc32/crc32.h
)

SET(srmodldr_SOURCES
	main.c
	shims/crc32/crc32.c
)

# Platform-specific headers & sources
IF(WIN32 AND NOT CYGWIN)

ENDIF(WIN32 AND NOT CYGWIN)

IF(NOT HAVE_ASPRINTF)
	MESSAGE("Using asprintf shim")
	LIST(APPEND srmodldr_SOURCES shims/asprintf/asprintf.c)
	LIST(APPEND srmodldr_HEADERS shims/asprintf/asprintf.h)
ENDIF(NOT HAVE_ASPRINTF)

# If using Win32 interface
include(interface/win32/CMakeLists.txt)

### Actually build
IF(WIN32 AND NOT CYGWIN)
	add_executable(
		SrModLdr WIN32 ${srmodldr_HEADERS} ${srmodldr_SOURCES}
	)
ELSE(WIN32 AND NOT CYGWIN)
	add_executable(SrModLdr ${srmodldr_HEADERS} ${srmodldr_SOURCE})
ENDIF(WIN32 AND NOT CYGWIN)

### Link libraries
IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_link_libraries(SrModLdr ${JANSSON_LIBRARY_DEBUG})
	target_link_libraries(SrModLdr ${SQLITE_LIBRARY_DEBUG})
ELSE()
	target_link_libraries(SrModLdr ${JANSSON_LIBRARY_RELEASE})
	target_link_libraries(SrModLdr ${SQLITE_LIBRARY_RELEASE})
ENDIF()

### Create installed (because why not?)
include (InstallRequiredSystemLibraries)
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.TXT")
set (CPACK_NSIS_URL_INFO_ABOUT "http://invisibleup.neocities.org")
set (CPACK_NSIS_HELP_LINK "http://github.com/invisibleup/SrModLdr")

include (CPack)
