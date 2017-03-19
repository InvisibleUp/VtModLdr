CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12 FATAL_ERROR)
PROJECT (SrModLdr C CXX)

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
IF(NOT "${CMAKE_BUILD_TYPE}" MATCHES "^(Debug|Release|RelWithDebInfo|MinSizeRel)\$")
	MESSAGE(FATAL_ERROR
		"Unknown keyword for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}\n"
		"Acceptable keywords: Debug, Release, RelWithDebInfo, MinSizeRel")
ENDIF(NOT "${CMAKE_BUILD_TYPE}" MATCHES "^(Debug|Release|RelWithDebInfo|MinSizeRel)\$")


# Set the user interface type
# You can choose following types:
#   win32          : Win32 GUI
#   test           : Unit testing stub

IF(NOT SRMODLDR_INTERFACE)
	SET(SRMODLDR_INTERFACE "win32" CACHE STRING "Interface" FORCE)
ENDIF(NOT SRMODLDR_INTERFACE)

# Set a value type to properly display SRMODLDR_INTERFACE on GUI if the
# value type is "UNINITIALIZED".
GET_PROPERTY(cached_type CACHE SRMODLDR_INTERFACE PROPERTY TYPE)
IF("${cached_type}" STREQUAL "UNINITIALIZED")
	SET(SRMODLDR_INTERFACE "${SRMODLDR_INTERFACE}" CACHE STRING "Interface" FORCE)
ENDIF("${cached_type}" STREQUAL "UNINITIALIZED")

# Check the Interface Type.
IF(NOT "${SRMODLDR_INTERFACE}" MATCHES "^(win32|test)\$")
	MESSAGE(FATAL_ERROR
		"Unknown keyword for SRMODLDR_INTERFACE: ${SRMODLDR_INTERFACE}\n"
		"Acceptable keywords: win32, test")
ENDIF(NOT "${SRMODLDR_INTERFACE}" MATCHES "^(win32|test)\$")

ADD_DEFINITIONS(-DSRMODLDR_INTERFACE_PATH="interface/${SRMODLDR_INTERFACE}/interface.h")
	  
# Initialize the state of the variables. This initialization is not
# necessary but this shows you what value the variables initially have.
#SET(CMAKE_REQUIRED_DEFINITIONS)
#SET(CMAKE_REQUIRED_INCLUDES)
#SET(CMAKE_REQUIRED_LIBRARIES)
#SET(CMAKE_REQUIRED_FLAGS)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake-deps/")

IF(WIN32 AND NOT CYGWIN)
	include_directories("build/include")
	link_directories("build/lib")
ENDIF(WIN32 AND NOT CYGWIN)

### Check libraries (broken on VS2010+)
#FIND_PACKAGE(SQLITE3 REQUIRED)
#FIND_PACKAGE(JANSSON REQUIRED)
#FIND_PACKAGE(ARCHIVE REQUIRED)

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
LA_CHECK_INCLUDE_FILE("io.h" HAVE_IO_H) # Ditto.
LA_CHECK_INCLUDE_FILE("sys/io.h" HAVE_SYS_IO_H)
LA_CHECK_INCLUDE_FILE("sys/param.h" HAVE_SYS_PARAM_H)
LA_CHECK_INCLUDE_FILE("sys/stat.h" HAVE_SYS_STAT_H)
LA_CHECK_INCLUDE_FILE("sys/sendfile.h" HAVE_SYS_SENDFILE_H)
LA_CHECK_INCLUDE_FILE("dir.h" HAVE_DIR_H)
LA_CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)
LA_CHECK_INCLUDE_FILE("sys/stat.h" HAVE_STAT_H)
LA_CHECK_INCLUDE_FILE("strings.h" HAVE_STRINGS_H)

# Check if libraries are installed
#LA_CHECK_INCLUDE_FILE_REQ("sqlite.h" HAVE_SQLITE_H)
#LA_CHECK_INCLUDE_FILE_REQ("jansson.h" HAVE_JANSSON_H)
#LA_CHECK_INCLUDE_FILE_REQ("archive.h" HAVE_ARCHIVE_H)

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
LA_CHECK_FUNCTION_EXISTS(mkdir HAVE_MKDIR)
LA_CHECK_FUNCTION_EXISTS(strcasecmp HAVE_STRCASECMP)

# Add to preprocessor
FOREACH(i ${FUNCTIONS})
	ADD_DEFINITIONS(-D${i})
ENDFOREACH(i)

### Set source files
SET(srmodldr_HEADERS
	errormsgs.h
	funcproto.h
	shims/crc32/crc32.h
	shims/zip/extract.c
	includes.h
)

SET(srmodldr_SOURCES
	main.c
	var.c
	space.c
	modop.c
	file.c
	json.c
	sql.c
	profile.c
	equation.cpp
	shims/crc32/crc32.c
	shims/zip/extract.h
)

IF(NOT HAVE_ASPRINTF)
	MESSAGE("Using asprintf shim")
	LIST(APPEND srmodldr_SOURCES shims/asprintf/asprintf.c)
	LIST(APPEND srmodldr_HEADERS shims/asprintf/asprintf.h)
ENDIF(NOT HAVE_ASPRINTF)

# Include interface-specific build files
include(interface/${SRMODLDR_INTERFACE}/CMakeLists.txt)

### Actually build
IF(WIN32 AND NOT CYGWIN)
	add_executable(
		SrModLdr WIN32 ${srmodldr_HEADERS} ${srmodldr_SOURCES}
	)
ELSE(WIN32 AND NOT CYGWIN)
	add_executable(SrModLdr ${srmodldr_HEADERS} ${srmodldr_SOURCES})
ENDIF(WIN32 AND NOT CYGWIN)

### Link libraries
#IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
	#target_link_libraries(SrModLdr ${JANSSON_LIBRARY_DEBUG})
	#target_link_libraries(SrModLdr ${SQLITE_LIBRARY_DEBUG})
	#target_link_libraries(SrModLdr ${ARCHIVE_LIBRARY_DEBUG})
#ELSE()
	#target_link_libraries(SrModLdr ${JANSSON_LIBRARY_RELEASE})
	#target_link_libraries(SrModLdr ${SQLITE_LIBRARY_RELEASE})
	#target_link_libraries(SrModLdr ${ARCHIVE_LIBRARY_RELEASE})
#ENDIF()
target_link_libraries(SrModLdr "jansson")
target_link_libraries(SrModLdr "sqlite3")
target_link_libraries(SrModLdr "archive")

### Copy contents of include/ to bin/
add_custom_command(
	TARGET SrModLdr POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/include ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

### Borland C++ linker haxx (resource file compatibility)
if(BORLAND AND RESOURCE_FILES)
	ADD_CUSTOM_COMMAND(
		TARGET SrModLdr PRE_LINK
		COMMAND brcc32
		ARGS -fo${RESOURCE_FILES} ${RESOURCE_FILES_SRC}
	)

	# Linking... with the linker! What a concept.
	set(CMAKE_C_LINK_EXECUTABLE
	    "ilink32 -aa -V4.0 -c -Gn -x <OBJECTS> c0w32.obj, <TARGET>, ,<LINK_LIBRARIES> import32.lib cw32.lib, ,\"${RESOURCE_FILES}\""
	)
endif()

### Visual Studio compiler haxx
if(MSVC)
	if(${SRMODLDR_INTERFACE} STREQUAL "test")
		set_target_properties(SrModLdr PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
	endif()
endif()

## Tests?
if(${SRMODLDR_INTERFACE} STREQUAL "test")
	FILE(GLOB_RECURSE test_src "${CMAKE_SOURCE_DIR}/tests/*.c" "${CMAKE_SOURCE_DIR}/tests/*.cpp")
	LIST(APPEND srmodldr_SOURCES ${test_src})

	ENABLE_TESTING()

	FOREACH(test ${test_src})
		GET_FILENAME_COMPONENT(testname ${test} NAME)
		IF(${testname} STREQUAL "Test_Caller.c")
			break()
		ENDIF()
		ADD_TEST(
			NAME ${testname}
			COMMAND SrModLdr ${testname} 
			WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		)
	ENDFOREACH(test)
	
	# Add post-build Test_Caller reparser
#	add_custom_command(
#		TARGET SrModLdr
#		PRE_BUILD
#		COMMAND ${CMAKE_SOURCE_DIR}/tests/genmapping_c_wrapper.bat
#		COMMAND ${CMAKE_SOURCE_DIR}/tests/genmapping_h_wrapper.bat
#		WORKING_DIRECTORY  ${CMAKE_SOURCE_DIR}/tests/
#		VERBATIM
#	)

endif()

### Create installer (because why not?)
install (TARGETS SrModLdr DESTINATION .)
install (FILES include\\\\games\\\\sonicr.json DESTINATION games)
if(WIN32)
install (FILES include\\\\jansson.dll include\\\\sqlite3.dll include\\\\archive.dll include\\\\zlib.dll DESTINATION .)
endif(WIN32)
# Dummy preinstall target (seems to be some sort of bug?)

include (InstallRequiredSystemLibraries)
set (CPACK_GENERATOR NSIS ZIP)
set (CPACK_PACKAGE_INSTALL_DIRECTORY SrModLdr)
set (CPACK_PACKAGE_INSTALL_REGISTRY_KEY SrModLdr)

set (CPACK_PACKAGE_NAME "Sonic R Mod Loader")
set (CPACK_PACKAGE_VENDOR InvisibleUp)
set (CPACK_PACKAGE_VERSION_MAJOR 1)
set (CPACK_PACKAGE_VERSION_MINOR 0)
set (CPACK_PACKAGE_VERSION_PATCH 0)
set (CPACK_PACKAGE_FILE_NAME SrModLdr-v1.0.0-Win32)

# Installer is edited to add single shortcut to start menu root.
# You know. Like how you're supposed to.
set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set (CPACK_NSIS_CREATE_ICONS "CreateShortCut '\$SMPROGRAMS\\\\${CPACK_PACKAGE_NAME}.lnk' '\$INSTDIR\\\\${PROJECT_NAME}.exe'") 
set (CPACK_NSIS_DELETE_ICONS "DELETE '\$SMPROGRAMS\\\\${CPACK_PACKAGE_NAME}.lnk'") 
set (CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/interface/win32/newicon.ico")

set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set (CPACK_NSIS_URL_INFO_ABOUT "http://invisibleup.neocities.org")
set (CPACK_NSIS_HELP_LINK "http://github.com/invisibleup/SrModLdr")

include (CPack)
