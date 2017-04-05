#pragma once
// Windows defines
#ifdef WINVER
	#undef WINVER
	#undef _WIN32_IE

	#define WINVER 0x0400
	#define _WIN32_IE 0x0000 //No IE requirement
#endif

// Linux defines
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

// Includes and macros
#include <stdlib.h>                // Malloc and other useful things
#include <errno.h>                 // Catching and explaining errors
#include <fcntl.h>                 // Flags for open() (O_RDWR, O_RDONLY)
#include <stdint.h>                // Variable type sizes
#include <string.h>                // String length/copy/etc.
#include <ctype.h>                 // isdigit(), etc.
#include <stdio.h>                 // sscanf, asprintf, etc.
#include <limits.h>                // LONG_MAX, etc.
#include "funcproto.h"             // Prototypes for all cross-plat functions
#include "shims/crc32/crc32.h"     // CRC32 function.
#include "shims/zip/extract.h"     // Funct. to extract a ZIP
#include SRMODLDR_INTERFACE_PATH

#include <jansson.h>               // EXTERNAL (http://www.digip.org/jansson/)
                                   // JSON parser. for loading patches, etc.
                                   // and parsing SQL results
#include <sqlite3.h>               // EXTERNAL (http://sqlite.org)
                                   // Embedded database. Used for quick lookups
                                   // of existing mods, dependencies, etc.
#include <archive.h>               // EXTERNAL (http://libarchive.org)
                                   // Archive packer/extractor. Used for
                                   // storing mods for sharing
#include <archive_entry.h>         // Additional functions for libarchive
				   
/// Macro and shim declarations
#define safe_free(ptr) if(ptr){free(ptr); ptr = NULL;}
#define streq(str1, str2) (str1 && str2 && (strcmp(str1, str2) == 0))
#define strneq(str1, str2) (str1 && str2 && (strcmp(str1, str2) != 0))
#define strndef(str1) (!str1 || (strcmp(str1, "") == 0))

// Switch and manipulate directories (chdir and getcwd)
#if defined(HAVE_CHDIR) && defined(HAVE_GETCWD) && defined(HAVE_UNISTD_H)
	#include <unistd.h>
#elif defined(HAVE_CHDIR) && defined(HAVE_GETCWD) && defined(HAVE_MKDIR) && defined(HAVE_DIR_H)
	#include <dir.h>
#elif defined(HAVE_WINDOWS_H)
	#define chdir SetCurrentDirectory
	#define getcwd(x, y) GetCurrentDirectory(y, x)
#else
	#error "No functions for directory switching found. Cannot compile."
#endif

// Linux IO compatibility
/*#ifdef HAVE_IO_H
	#include <io.h>                    // Standardized I/O library. (Portable!)
#elif HAVE_SYS_IO_H
	#include <sys/io.h>
#else
	#error "No functions for io.h style file IO found. Cannot compile."
#endif*/

#ifdef HAVE_SYS_SENDFILE_H
#include <sys/sendfile.h>
#endif

#ifndef O_BINARY
	#define O_BINARY 0
	#define O_TEXT 0
#endif

#ifndef _O_RDWR
	#define _O_BINARY O_BINARY
	#define _O_RDWR O_RDWR
	#define _O_RDONLY O_RDONLY
	#define _O_WRONLY O_WRONLY
#endif

#ifndef _open
	#define _open open
	#define _creat creat
	#define _tell tell
#endif

#ifndef tell
	#define tell(fd) lseek(fd, 0, SEEK_CUR)
#endif


#ifndef MIN
#ifdef HAVE_SYS_PARAM_H
	#include <sys/param.h>
#endif
#endif
	
// Define Win32-style booleans if not present
#ifndef HAVE_STDBOOL_H
	#include <stdbool.h>
	#define BOOL bool
	#define TRUE true
	#define FALSE false
#else
    #define BOOL int
    #define TRUE 1
    #define FALSE 0
#endif

// Define a MAX_PATH for non-Win32 systems
#ifndef MAX_PATH
	#define MAX_PATH 255
#endif

// Make directories
#ifndef mkdir
	#if defined(HAVE_MKDIR) && defined(HAVE_WINDOWS_H)
		#define mkdir(x) CreateDirectory(x, NULL)
	#elif defined(HAVE_MKDIR) && defined(HAVE_STAT_H)
		#include <sys/stat.h>
		#include <sys/types.h>
		#define mkdir(x) mkdir(x, S_IRWXU)
	#else
		#error "No funtions for directory creation found. Cannot compile."
	#endif
#endif

// Case-insensitive string comparsion
#if defined(HAVE_STRINGS_H) && defined(HAVE_STRCASECMP)
	#include <strings.h>
	#define strieq(str1, str2) (str1 && str2 && (strcasecmp(str1, str2) == 0))
#elif defined(HAVE_WINDOWS_H)
	#define strieq(str1, str2) (str1 && str2 && (lstrcmpi(str1, str2) == 0))
#endif

//asprintf shim
#ifndef HAVE_ASPRINTF
#include "shims/asprintf/asprintf.h"
#endif

// These are for access() in io.h. They're defined somewhere I'm sure
// but I couldn't tell you where.
#if !defined(F_OK) && !defined(_MSC_VER) // Tuned for Borland / Digital Mars
	#define F_OK 0	// Check for existence
	#define X_OK 1	// Check for execute permission
	#define W_OK 2	// Check for write permission
	#define R_OK 4	// Check for read permission
#elif !defined(F_OK) 
	#define F_OK 00	// Check for existence
	#define X_OK 04	// Check for execute permission (not applicable)
	#define W_OK 02	// Check for write permission
	#define R_OK 04	// Check for read permission
#endif

//Visual Studio 6.0 compatibility hacks
#if !defined(HAVE_VSNPRINTF) && defined(HAVE_MS_VSNPRINTF)
#define vsnprintf _vsnprintf
#endif

#if !defined(HAVE_SNPRINTF) && defined(HAVE_MS_SNPRINTF)
#define snprintf _snprintf
#endif

//Dingos at MS made vsnprintf (a C99 standard!) return -1 on oversized buffers.
//They didn't fix this until the year 2015.
#if _MSC_VER < 1900
	#define VSNPRINTF_BROKEN
	#define SNPRINTF_BROKEN
#endif

//"deprecated" POSIX-compliant functions cause instant crashes in debug mode
//i'm not kidding.
#ifdef _MSC_VER
	#define strdup _strdup
	#define access _access
	#define open _open
	#define lseek _lseek
	#define close _close
	#define write _write
	#define read _read
#endif
