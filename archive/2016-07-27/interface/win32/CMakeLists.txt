### Test for presence of Win32 API
message("Using Win32 interface")

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

# Win32 libraries
LA_CHECK_INCLUDE_FILE_REQ("windows.h" HAVE_WINDOWS_H)
LA_CHECK_INCLUDE_FILE("COMMCTRL.H" HAVE_COMMCTRL_H)
LA_CHECK_INCLUDE_FILE_REQ("Shlobj.h" HAVE_SHLOBJ_H)

# Add to preprocessor
FOREACH(i ${INCLUDES})
	ADD_DEFINITIONS(-D${i})
ENDFOREACH(i)

### Add files
LIST(APPEND srmodldr_HEADERS 
	${CMAKE_CURRENT_LIST_DIR}/resource.h 
	${CMAKE_CURRENT_LIST_DIR}/interface.h 
	${CMAKE_CURRENT_LIST_DIR}/win32helper.h
	${CMAKE_CURRENT_LIST_DIR}/win32global.h
)
LIST(APPEND srmodldr_SOURCES 
	${CMAKE_CURRENT_LIST_DIR}/resources.rc 
	${CMAKE_CURRENT_LIST_DIR}/io.c 
	${CMAKE_CURRENT_LIST_DIR}/dpi.c 
	${CMAKE_CURRENT_LIST_DIR}/filesel.c 
	${CMAKE_CURRENT_LIST_DIR}/winmanage.c
	${CMAKE_CURRENT_LIST_DIR}/dialogs.c
	${CMAKE_CURRENT_LIST_DIR}/interface.c
	${CMAKE_CURRENT_LIST_DIR}/win32global.c
)