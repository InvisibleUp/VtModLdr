//Windows defines
#ifndef WINVER
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define _WIN32_IE 0x0000 //No IE requirement
#endif

//Includes and macros
#include <stdlib.h>                // Malloc and other useful things
#include <errno.h>                 // Catching and explaining errors
#include <stdarg.h>                // Indeterminate argument support
#include <math.h>                  // HUGE_VAL
#include <fcntl.h>                 // Flags for open() (O_RDWR, O_RDONLY)
#include <io.h>                    // Standardized I/O library. (Portable!)
#include <stdint.h>                // Variable type sizes
#include "funcproto.h"             // Prototypes for all cross-plat functions
#include "shims/crc32/crc32.h"     // CRC32 function. Not actually a shim yet.

#include <jansson.h>               // EXTERNAL (http://www.digip.org/jansson/)
                                   // JSON parser. for loading patches, etc.
                                   // and parsing SQL results
#include <sqlite3.h>               // EXTERNAL (http://sqlite.org)
                                   // Embedded database. Used for quick lookups
                                   // of existing mods, dependencies, etc.
#include <archive.h>               // EXTERNAL (http://libarchive.org)
                                   // Archive packer/extractor. Used for
				   // storing mods for transport across the
				   // internet.
				   
/// Macro and shim declarations
#define safe_free(ptr) if(ptr){free(ptr); ptr = NULL;}

// Switch and manipulate directories (chdir and getcwd)
#if defined(HAVE_CHDIR) && defined(HAVE_GETCWD) && defined(HAVE_UNISTD_H)
	#include <unistd.h>
#elif defined(HAVE_CHDIR) && defined(HAVE_GETCWD) && defined(HAVE_DIR_H)
	#include <dir.h>
#elif defined(HAVE_WINDOWS_H)
	#define chdir SetCurrentDirectory
	#define getcwd(x, y) GetCurrentDirectory(y, x)
#else
	#error "No functions for directory switching found. Cannot compile."
#endif

//asprintf shim
#if !defined(HAVE_ASPRINTF)
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