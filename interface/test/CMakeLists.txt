### Test for presence of Win32 API
message("Using unit testing interface")

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

# Win32 libraries (still dependent on this...)
IF(WIN32)
	LA_CHECK_INCLUDE_FILE_REQ("windows.h" HAVE_WINDOWS_H)
ENDIF(WIN32)

# Add to preprocessor
FOREACH(i ${INCLUDES})
	ADD_DEFINITIONS(-D${i})
ENDFOREACH(i)


### Add files
LIST(APPEND srmodldr_HEADERS 
	${CMAKE_CURRENT_LIST_DIR}/interface.h 
)
LIST(APPEND srmodldr_SOURCES 
	${CMAKE_CURRENT_LIST_DIR}/interface.c
	${CMAKE_CURRENT_LIST_DIR}/proto_checksum.c
	${CMAKE_CURRENT_LIST_DIR}/proto_dbase_ok.c
	${CMAKE_CURRENT_LIST_DIR}/proto_dbase_tojson.c
	${CMAKE_CURRENT_LIST_DIR}/proto_json_checkoutput.c
)

# Add everything in the /tests/ folder
FILE(GLOB_RECURSE test_src "${CMAKE_SOURCE_DIR}/tests/*.c" "${CMAKE_SOURCE_DIR}/tests/*.h" "${CMAKE_SOURCE_DIR}/tests/*.cpp")
LIST(APPEND srmodldr_SOURCES ${test_src})

