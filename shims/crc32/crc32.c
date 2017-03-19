/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-1998 Mark Adler
 * Modified 2015-2017 by InvisibleUp
 * For conditions of distribution and use, see copyright notice in zlib.h (not included)
 */
#include "crc32.h"
#include "../../includes.h"
#include "../../funcproto.h"
 
#ifdef __unix__
#include <sys/mman.h>	//Map memory to file
#endif

#ifdef _WIN32
#include <windows.h>
#endif

static int crc_table_empty = 1;
static unsigned long crc_table[256];

/*
  Generate a table for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The table is simply the CRC of all possible eight bit values.  This is all
  the information needed to generate CRC's on data a byte at a time for all
  combinations of CRC register values and incoming bytes.
*/
static void make_crc_table()
{
  unsigned int n, k;
  unsigned long poly;            /* polynomial exclusive-or pattern */
  /* terms of polynomial defining this crc (except x^32): */
  static const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* make exclusive-or pattern from polynomial (0xedb88320L) */
  poly = 0L;
  for (n = 0; n < sizeof(p)/sizeof(unsigned char); n++)
    poly |= 1L << (31 - p[n]);
 
  for (n = 0; n < 256; n++)
  {
    unsigned long c = (unsigned long)n;
    for (k = 0; k < 8; k++)
      c = (c & 1) ? poly ^ (c >> 1) : c >> 1;
    crc_table[n] = c;
  }
  crc_table_empty = 0;
}

/* =========================================================================
 * This function can be used by asm versions of crc32()
 */
const unsigned long * get_crc_table()
{
  if (crc_table_empty) make_crc_table();
  return (const unsigned long *)crc_table;
}

/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned long len)
{
    if (buf == NULL) return 0L;
    if (crc_table_empty){
      make_crc_table();
	}
    crc = crc ^ 0xffffffffL;
    while (len >= 8){
      DO8(buf);
      len -= 8;
    }
    if (len) do {
      DO1(buf);
    } while (--len);
    return crc ^ 0xffffffffL;
}

/* Given file name, get a CRC */
unsigned long crc32File(const char *filename)
{
	unsigned long result = 0;
	unsigned long len = 0;  //Max file size is ~4GB. That's more than enough.
	unsigned char *input = NULL;
	
	#ifdef __unix__
        int fd = _open(filename, _O_RDONLY);
        if(fd == -1){
            ErrNo2ErrCode();
            return 0L;
        
        }
        
        len = filesize(filename);
        
        input = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
        if(input == MAP_FAILED){
            ErrNo2ErrCode();
            close(fd);
            return 0L;
        }
	#endif
	
	#ifdef _WIN32
		HANDLE File2CRC = INVALID_HANDLE_VALUE;
		HANDLE File2CRC_INVALID_FILE_SIZEMap = NULL;
	
		/* Open file */	
		File2CRC = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(File2CRC == INVALID_HANDLE_VALUE){return 0UL;}
		
		/* Get file size */
		len = filesize(File2CRC, NULL);
		if(len == INVALID_FILE_SIZE){CloseHandle(File2CRC); return 0UL;}
		
		/* Map to memory */
		File2CRC_Map = CreateFileMapping(File2CRC, NULL, PAGE_READONLY, 0, 0, NULL);
		if(File2CRC_Map == NULL){CloseHandle(File2CRC); return 0UL;}
			
		input = MapViewOfFile(File2CRC_Map, FILE_MAP_READ, 0, 0, 0);
		if(input == NULL){CloseHandle(File2CRC); CloseHandle(File2CRC_Map); return 0UL;}
	#endif
	
	/* Actually compute CRC */
	result = crc32(0, input, len);
	

	#ifdef __unix__
        munmap(input, len);
        close(fd);
	#endif
	
	#ifdef _WIN32
		/* Unmap and close files */
		UnmapViewOfFile(input);
		CloseHandle(File2CRC_Map);
		CloseHandle(File2CRC);
	#endif
	
	return result;
}
