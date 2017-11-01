	.386p
	ifdef ??version
	if    ??version GT 500H
	.mmx
	endif
	endif
	model flat
	ifndef	??version
	?debug	macro
	endm
	endif
	?debug	S "crc32.c"
	?debug	T "crc32.c"
_TEXT	segment dword public use32 'CODE'
_TEXT	ends
_DATA	segment dword public use32 'DATA'
_DATA	ends
_BSS	segment dword public use32 'BSS'
_BSS	ends
$$BSYMS	segment byte public use32 'DEBSYM'
$$BSYMS	ends
$$BTYPES	segment byte public use32 'DEBTYP'
$$BTYPES	ends
$$BNAMES	segment byte public use32 'DEBNAM'
$$BNAMES	ends
$$BROWSE	segment byte public use32 'DEBSYM'
$$BROWSE	ends
$$BROWFILE	segment byte public use32 'DEBSYM'
$$BROWFILE	ends
DGROUP	group	_BSS,_DATA
_DATA	segment dword public use32 'DATA'
	align	4
_crc_table_empty	label	dword
	dd	1
$afnlfeia	label	byte
	db	0
	db	1
	db	2
	db	4
	db	5
	db	7
	db	8
	db	10
	db	11
	db	12
	db	16
	db	22
	db	23
	db	26
_DATA	ends
_TEXT	segment dword public use32 'CODE'
_make_crc_table	proc	near
?live1@0:
   ;	
   ;	static void make_crc_table()
   ;	
	?debug L 41
	push      ebp
	mov       ebp,esp
	add       esp,-16
   ;	
   ;	{
   ;	  unsigned long c;
   ;	  unsigned int n, k;
   ;	  unsigned long poly;            /* polynomial exclusive-or pattern */
   ;	  /* terms of polynomial defining this crc (except x^32): */
   ;	  static const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};
   ;	
   ;	  /* make exclusive-or pattern from polynomial (0xedb88320L) */
   ;	  poly = 0L;
   ;	
	?debug L 50
@1:
	xor       eax,eax
	mov       dword ptr [ebp-16],eax
   ;	
   ;	  for (n = 0; n < sizeof(p)/sizeof(unsigned char); n++)
   ;	
	?debug L 51
	xor       edx,edx
	mov       dword ptr [ebp-8],edx
   ;	
   ;	    poly |= 1L << (31 - p[n]);
   ;	
	?debug L 52
@2:
	mov       ecx,dword ptr [ebp-8]
	xor       eax,eax
	mov       al,byte ptr [ecx+$afnlfeia]
	mov       ecx,31
	sub       ecx,eax
	mov       edx,1
	shl       edx,cl
	or        dword ptr [ebp-16],edx
	inc       dword ptr [ebp-8]
	cmp       dword ptr [ebp-8],14
	jb        short @2
   ;	
   ;	 
   ;	  for (n = 0; n < 256; n++)
   ;	
	?debug L 54
	xor       eax,eax
	mov       dword ptr [ebp-8],eax
   ;	
   ;	  {
   ;	    c = (unsigned long)n;
   ;	
	?debug L 56
@5:
	mov       edx,dword ptr [ebp-8]
	mov       dword ptr [ebp-4],edx
   ;	
   ;	    for (k = 0; k < 8; k++)
   ;	
	?debug L 57
	xor       ecx,ecx
	mov       dword ptr [ebp-12],ecx
   ;	
   ;	      c = c & 1 ? poly ^ (c >> 1) : c >> 1;
   ;	
	?debug L 58
@7:
	test      byte ptr [ebp-4],1
	je        short @9
	mov       eax,dword ptr [ebp-4]
	shr       eax,1
	xor       eax,dword ptr [ebp-16]
	jmp       short @10
@9:
	mov       eax,dword ptr [ebp-4]
	shr       eax,1
@10:
	mov       dword ptr [ebp-4],eax
	inc       dword ptr [ebp-12]
	cmp       dword ptr [ebp-12],8
	jb        short @7
   ;	
   ;	    crc_table[n] = c;
   ;	
	?debug L 59
	mov       edx,dword ptr [ebp-8]
	mov       ecx,dword ptr [ebp-4]
	mov       dword ptr [_crc_table+4*edx],ecx
	inc       dword ptr [ebp-8]
	cmp       dword ptr [ebp-8],256
	jb        short @5
   ;	
   ;	  }
   ;	  crc_table_empty = 0;
   ;	
	?debug L 61
	xor       eax,eax
	mov       dword ptr [_crc_table_empty],eax
   ;	
   ;	}
   ;	
	?debug L 62
@13:
	mov       esp,ebp
	pop       ebp
	ret 
	?debug L 0
_make_crc_table	endp
_TEXT	ends
$$BSYMS	segment byte public use32 'DEBSYM'
	db	2
	db	0
	db	0
	db	0
	dw	46
	dw	516
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	?patch1
	dd	?patch2
	dd	?patch3
	df	_make_crc_table
	dw	0
	dw	4096
	dw	0
	dw	1
	dw	0
	dw	0
	dw	0
	dw	22
	dw	513
	df	$afnlfeia
	dw	0
	dw	4098
	dw	0
	dw	2
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65520
	dw	65535
	dw	34
	dw	0
	dw	3
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65524
	dw	65535
	dw	117
	dw	0
	dw	4
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65528
	dw	65535
	dw	117
	dw	0
	dw	5
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65532
	dw	65535
	dw	34
	dw	0
	dw	6
	dw	0
	dw	0
	dw	0
?patch1	equ	@13-_make_crc_table+4
?patch2	equ	0
?patch3	equ	@13-_make_crc_table
	dw	2
	dw	6
	dw	4
	dw	531
	dw	0
$$BSYMS	ends
_TEXT	segment dword public use32 'CODE'
_get_crc_table	proc	near
?live1@176:
   ;	
   ;	const unsigned long * get_crc_table()
   ;	
	?debug L 67
	push      ebp
	mov       ebp,esp
   ;	
   ;	{
   ;	  if (crc_table_empty) make_crc_table();
   ;	
	?debug L 69
@14:
	cmp       dword ptr [_crc_table_empty],0
	je        short @15
	call      _make_crc_table
   ;	
   ;	  return (const unsigned long *)crc_table;
   ;	
	?debug L 70
@15:
	mov       eax,offset _crc_table
   ;	
   ;	}
   ;	
	?debug L 71
@17:
@16:
	pop       ebp
	ret 
	?debug L 0
_get_crc_table	endp
_TEXT	ends
$$BSYMS	segment byte public use32 'DEBSYM'
	dw	61
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	?patch4
	dd	?patch5
	dd	?patch6
	df	_get_crc_table
	dw	0
	dw	4100
	dw	0
	dw	7
	dw	0
	dw	0
	dw	0
	db	14
	db	95
	db	103
	db	101
	db	116
	db	95
	db	99
	db	114
	db	99
	db	95
	db	116
	db	97
	db	98
	db	108
	db	101
?patch4	equ	@17-_get_crc_table+2
?patch5	equ	0
?patch6	equ	@17-_get_crc_table
	dw	2
	dw	6
$$BSYMS	ends
_TEXT	segment dword public use32 'CODE'
_crc32	proc	near
?live1@240:
   ;	
   ;	unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned long len)
   ;	
	?debug L 80
	push      ebp
	mov       ebp,esp
   ;	
   ;	{
   ;	    if (buf == NULL) return 0L;
   ;	
	?debug L 82
@18:
	cmp       dword ptr [ebp+12],0
	jne       short @19
	xor       eax,eax
@29:
	pop       ebp
	ret 
   ;	
   ;	    if (crc_table_empty){
   ;	
	?debug L 83
@19:
	cmp       dword ptr [_crc_table_empty],0
	je        short @21
   ;	
   ;	      make_crc_table();
   ;	
	?debug L 84
	call      _make_crc_table
   ;	
   ;		}
   ;	    crc = crc ^ 0xffffffffL;
   ;	
	?debug L 86
@21:
	xor       dword ptr [ebp+8],-1
   ;	
   ;	    while (len >= 8){
   ;	
	?debug L 87
	cmp       dword ptr [ebp+16],8
	jb        @23
   ;	
   ;	      DO8(buf);
   ;	
	?debug L 88
@22:
	mov       edx,dword ptr [ebp+12]
	xor       ecx,ecx
	mov       cl,byte ptr [edx]
	xor       ecx,dword ptr [ebp+8]
	and       ecx,255
	mov       eax,dword ptr [_crc_table+4*ecx]
	mov       edx,dword ptr [ebp+8]
	shr       edx,8
	xor       eax,edx
	mov       dword ptr [ebp+8],eax
	inc       dword ptr [ebp+12]
	mov       ecx,dword ptr [ebp+12]
	xor       eax,eax
	mov       al,byte ptr [ecx]
	xor       eax,dword ptr [ebp+8]
	and       eax,255
	mov       edx,dword ptr [_crc_table+4*eax]
	mov       ecx,dword ptr [ebp+8]
	shr       ecx,8
	xor       edx,ecx
	mov       dword ptr [ebp+8],edx
	inc       dword ptr [ebp+12]
	mov       eax,dword ptr [ebp+12]
	xor       edx,edx
	mov       dl,byte ptr [eax]
	xor       edx,dword ptr [ebp+8]
	and       edx,255
	mov       ecx,dword ptr [_crc_table+4*edx]
	mov       eax,dword ptr [ebp+8]
	shr       eax,8
	xor       ecx,eax
	mov       dword ptr [ebp+8],ecx
	inc       dword ptr [ebp+12]
	mov       edx,dword ptr [ebp+12]
	xor       ecx,ecx
	mov       cl,byte ptr [edx]
	xor       ecx,dword ptr [ebp+8]
	and       ecx,255
	mov       eax,dword ptr [_crc_table+4*ecx]
	mov       edx,dword ptr [ebp+8]
	shr       edx,8
	xor       eax,edx
	mov       dword ptr [ebp+8],eax
	inc       dword ptr [ebp+12]
	mov       ecx,dword ptr [ebp+12]
	xor       eax,eax
	mov       al,byte ptr [ecx]
	xor       eax,dword ptr [ebp+8]
	and       eax,255
	mov       edx,dword ptr [_crc_table+4*eax]
	mov       ecx,dword ptr [ebp+8]
	shr       ecx,8
	xor       edx,ecx
	mov       dword ptr [ebp+8],edx
	inc       dword ptr [ebp+12]
	mov       eax,dword ptr [ebp+12]
	xor       edx,edx
	mov       dl,byte ptr [eax]
	xor       edx,dword ptr [ebp+8]
	and       edx,255
	mov       ecx,dword ptr [_crc_table+4*edx]
	mov       eax,dword ptr [ebp+8]
	shr       eax,8
	xor       ecx,eax
	mov       dword ptr [ebp+8],ecx
	inc       dword ptr [ebp+12]
	mov       edx,dword ptr [ebp+12]
	xor       ecx,ecx
	mov       cl,byte ptr [edx]
	xor       ecx,dword ptr [ebp+8]
	and       ecx,255
	mov       eax,dword ptr [_crc_table+4*ecx]
	mov       edx,dword ptr [ebp+8]
	shr       edx,8
	xor       eax,edx
	mov       dword ptr [ebp+8],eax
	inc       dword ptr [ebp+12]
	mov       ecx,dword ptr [ebp+12]
	xor       eax,eax
	mov       al,byte ptr [ecx]
	xor       eax,dword ptr [ebp+8]
	and       eax,255
	mov       edx,dword ptr [_crc_table+4*eax]
	mov       ecx,dword ptr [ebp+8]
	shr       ecx,8
	xor       edx,ecx
	mov       dword ptr [ebp+8],edx
	inc       dword ptr [ebp+12]
   ;	
   ;	      len -= 8;
   ;	
	?debug L 89
	sub       dword ptr [ebp+16],8
	?debug L 87
	cmp       dword ptr [ebp+16],8
	jae       @22
   ;	
   ;	    }
   ;	    if (len) do {
   ;	
	?debug L 91
@23:
	cmp       dword ptr [ebp+16],0
	je        short @25
   ;	
   ;	      DO1(buf);
   ;	
	?debug L 92
@26:
	mov       eax,dword ptr [ebp+12]
	xor       edx,edx
	mov       dl,byte ptr [eax]
	xor       edx,dword ptr [ebp+8]
	and       edx,255
	mov       ecx,dword ptr [_crc_table+4*edx]
	mov       eax,dword ptr [ebp+8]
	shr       eax,8
	xor       ecx,eax
	mov       dword ptr [ebp+8],ecx
	inc       dword ptr [ebp+12]
   ;	
   ;	    } while (--len);
   ;	
	?debug L 93
	dec       dword ptr [ebp+16]
	jne       short @26
   ;	
   ;	    return crc ^ 0xffffffffL;
   ;	
	?debug L 94
@25:
	mov       eax,dword ptr [ebp+8]
	xor       eax,-1
   ;	
   ;	}
   ;	
	?debug L 95
@28:
@20:
	pop       ebp
	ret 
	?debug L 0
_crc32	endp
_TEXT	ends
$$BSYMS	segment byte public use32 'DEBSYM'
	dw	53
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	?patch7
	dd	?patch8
	dd	?patch9
	df	_crc32
	dw	0
	dw	4104
	dw	0
	dw	8
	dw	0
	dw	0
	dw	0
	db	6
	db	95
	db	99
	db	114
	db	99
	db	51
	db	50
	dw	18
	dw	512
	dw	8
	dw	0
	dw	34
	dw	0
	dw	9
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	12
	dw	0
	dw	4105
	dw	0
	dw	10
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	16
	dw	0
	dw	34
	dw	0
	dw	11
	dw	0
	dw	0
	dw	0
	dw	8
	dw	530
	dd	@29-_crc32
	dw	2
?patch7	equ	@28-_crc32+2
?patch8	equ	0
?patch9	equ	@28-_crc32
	dw	2
	dw	6
$$BSYMS	ends
_TEXT	segment dword public use32 'CODE'
_crc32File	proc	near
?live1@464:
   ;	
   ;	unsigned long crc32File(const char *filename)
   ;	
	?debug L 98
	push      ebp
	mov       ebp,esp
	add       esp,-20
   ;	
   ;	{
   ;		unsigned long result = 0;
   ;	
	?debug L 100
@30:
	xor       eax,eax
	mov       dword ptr [ebp-4],eax
   ;	
   ;		unsigned long len = 0;  //Max file size is ~4GB. That's more than enough.
   ;	
	?debug L 101
	xor       edx,edx
	mov       dword ptr [ebp-8],edx
   ;	
   ;		unsigned char *input = NULL;
   ;	
	?debug L 102
	xor       ecx,ecx
	mov       dword ptr [ebp-12],ecx
   ;	
   ;		
   ;		#ifdef __unix__
   ;		/* TODO */
   ;		#endif
   ;		
   ;		#ifdef _WIN32
   ;			HANDLE File2CRC = INVALID_HANDLE_VALUE;
   ;	
	?debug L 109
	mov       dword ptr [ebp-16],-1
   ;	
   ;			HANDLE File2CRC_Map = NULL;
   ;	
	?debug L 110
	xor       eax,eax
	mov       dword ptr [ebp-20],eax
   ;	
   ;		
   ;			/* Open file */	
   ;			File2CRC = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   ;	
	?debug L 113
	push      0
	push      128
	push      3
	push      0
	push      0
	push      -2147483648
	push      dword ptr [ebp+8]
	call      CreateFileA
	mov       dword ptr [ebp-16],eax
   ;	
   ;			if(File2CRC == INVALID_HANDLE_VALUE){return 0UL;}
   ;	
	?debug L 114
	cmp       dword ptr [ebp-16],-1
	jne       short @31
	xor       eax,eax
	jmp       @32
   ;	
   ;			
   ;			/* Get file size */
   ;			len = GetFileSize(File2CRC, NULL);
   ;	
	?debug L 117
@31:
	push      0
	push      dword ptr [ebp-16]
	call      GetFileSize
	mov       dword ptr [ebp-8],eax
   ;	
   ;			if(len == INVALID_FILE_SIZE){CloseHandle(File2CRC); return 0UL;}
   ;	
	?debug L 118
	cmp       dword ptr [ebp-8],-1
	jne       short @33
	push      dword ptr [ebp-16]
	call      CloseHandle
	xor       eax,eax
	jmp       @32
   ;	
   ;			
   ;			/* Map to memory */
   ;			File2CRC_Map = CreateFileMapping(File2CRC, NULL, PAGE_READONLY, 0, 0, NULL);
   ;	
	?debug L 121
@33:
	push      0
	push      0
	push      0
	push      2
	push      0
	push      dword ptr [ebp-16]
	call      CreateFileMappingA
	mov       dword ptr [ebp-20],eax
   ;	
   ;			if(File2CRC == NULL){CloseHandle(File2CRC); return 0UL;}
   ;	
	?debug L 122
	cmp       dword ptr [ebp-16],0
	jne       short @34
	push      dword ptr [ebp-16]
	call      CloseHandle
	xor       eax,eax
	jmp       short @32
   ;	
   ;				
   ;			input = MapViewOfFile(File2CRC_Map, FILE_MAP_READ, 0, 0, 0);
   ;	
	?debug L 124
@34:
	push      0
	push      0
	push      0
	push      4
	push      dword ptr [ebp-20]
	call      MapViewOfFile
	mov       dword ptr [ebp-12],eax
   ;	
   ;			if(input == NULL){CloseHandle(File2CRC); CloseHandle(File2CRC_Map); return 0UL;}
   ;	
	?debug L 125
	cmp       dword ptr [ebp-12],0
	jne       short @35
	push      dword ptr [ebp-16]
	call      CloseHandle
	push      dword ptr [ebp-20]
	call      CloseHandle
	xor       eax,eax
	jmp       short @32
   ;	
   ;		#endif
   ;		
   ;		/* Actually compute CRC */
   ;		result = crc32(0, input, len);
   ;	
	?debug L 129
@35:
	push      dword ptr [ebp-8]
	push      dword ptr [ebp-12]
	push      0
	call      _crc32
	add       esp,12
	mov       dword ptr [ebp-4],eax
   ;	
   ;		
   ;	
   ;		#ifdef __unix__
   ;		/* TODO */
   ;		#endif
   ;		
   ;		#ifdef _WIN32
   ;			/* Unmap and close files */
   ;			UnmapViewOfFile(input);
   ;	
	?debug L 138
	push      dword ptr [ebp-12]
	call      UnmapViewOfFile
   ;	
   ;			CloseHandle(File2CRC_Map);
   ;	
	?debug L 139
	push      dword ptr [ebp-20]
	call      CloseHandle
   ;	
   ;			CloseHandle(File2CRC);
   ;	
	?debug L 140
	push      dword ptr [ebp-16]
	call      CloseHandle
   ;	
   ;		#endif
   ;		
   ;		return result;
   ;	
	?debug L 143
	mov       eax,dword ptr [ebp-4]
   ;	
   ;	}
   ;	
	?debug L 144
@36:
@32:
	mov       esp,ebp
	pop       ebp
	ret 
	?debug L 0
_crc32File	endp
_TEXT	ends
$$BSYMS	segment byte public use32 'DEBSYM'
	dw	57
	dw	517
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dd	?patch10
	dd	?patch11
	dd	?patch12
	df	_crc32File
	dw	0
	dw	4108
	dw	0
	dw	12
	dw	0
	dw	0
	dw	0
	db	10
	db	95
	db	99
	db	114
	db	99
	db	51
	db	50
	db	70
	db	105
	db	108
	db	101
	dw	18
	dw	512
	dw	8
	dw	0
	dw	4109
	dw	0
	dw	13
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65516
	dw	65535
	dw	1027
	dw	0
	dw	14
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65520
	dw	65535
	dw	1027
	dw	0
	dw	15
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65524
	dw	65535
	dw	1056
	dw	0
	dw	16
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65528
	dw	65535
	dw	34
	dw	0
	dw	17
	dw	0
	dw	0
	dw	0
	dw	18
	dw	512
	dw	65532
	dw	65535
	dw	34
	dw	0
	dw	18
	dw	0
	dw	0
	dw	0
?patch10	equ	@36-_crc32File+4
?patch11	equ	0
?patch12	equ	@36-_crc32File
	dw	2
	dw	6
	dw	4
	dw	531
	dw	0
$$BSYMS	ends
_BSS	segment dword public use32 'BSS'
	align	4
_crc_table	label	dword
	db	1024	dup(?)
_BSS	ends
_TEXT	segment dword public use32 'CODE'
_TEXT	ends
 ?debug  C FB040B43726561746546696C654102000000
 ?debug  C FB040B47657446696C6553697A6502000000
 ?debug  C FB040B436C6F736548616E646C6502000000
 ?debug  C FB041243726561746546696C654D617070696E674102000000
 ?debug  C FB040D4D6170566965774F6646696C6502000000
 ?debug  C FB040F556E6D6170566965774F6646696C6502000000
	public	_get_crc_table
	public	_crc32
	public	_crc32File
 extrn   CreateFileA:near
 extrn   GetFileSize:near
 extrn   CloseHandle:near
 extrn   CreateFileMappingA:near
 extrn   MapViewOfFile:near
 extrn   UnmapViewOfFile:near
 ?debug  C 9F757569642E6C6962
 ?debug  C 9F757569642E6C6962
$$BSYMS	segment byte public use32 'DEBSYM'
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	23
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	24
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	25
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	26
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	27
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	28
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	29
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	30
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	31
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	32
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	33
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	34
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	35
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	36
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	37
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	64
	dw	0
	dw	0
	dw	38
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	39
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	40
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	41
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	42
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	43
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4127
	dw	0
	dw	0
	dw	44
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	45
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	46
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	47
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	48
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	49
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	50
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	51
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	52
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	53
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	54
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	55
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	56
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	57
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	58
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	17
	dw	0
	dw	0
	dw	59
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	60
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	61
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	62
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	63
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	64
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	65
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	118
	dw	0
	dw	0
	dw	66
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	118
	dw	0
	dw	0
	dw	67
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	119
	dw	0
	dw	0
	dw	68
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	119
	dw	0
	dw	0
	dw	69
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	119
	dw	0
	dw	0
	dw	70
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	71
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	72
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	73
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	17
	dw	0
	dw	0
	dw	74
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	75
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	76
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4109
	dw	0
	dw	0
	dw	77
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4109
	dw	0
	dw	0
	dw	78
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4109
	dw	0
	dw	0
	dw	79
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4109
	dw	0
	dw	0
	dw	80
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	81
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	82
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	83
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4109
	dw	0
	dw	0
	dw	84
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4109
	dw	0
	dw	0
	dw	85
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	86
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	87
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	88
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	89
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	90
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	16
	dw	0
	dw	0
	dw	91
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	92
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	93
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	94
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	118
	dw	0
	dw	0
	dw	95
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	119
	dw	0
	dw	0
	dw	96
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	118
	dw	0
	dw	0
	dw	97
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	119
	dw	0
	dw	0
	dw	98
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	99
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	100
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	101
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	102
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	103
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	104
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	105
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	106
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	107
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	108
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	109
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	110
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	111
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	112
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	113
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	114
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	115
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	116
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	117
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	118
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	119
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	120
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	121
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	122
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	116
	dw	0
	dw	0
	dw	123
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	124
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	125
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4115
	dw	0
	dw	1
	dw	126
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4115
	dw	0
	dw	0
	dw	127
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4114
	dw	0
	dw	0
	dw	128
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4114
	dw	0
	dw	0
	dw	129
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	130
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	131
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	132
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	133
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	134
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	135
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	136
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	137
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	138
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	3
	dw	0
	dw	0
	dw	139
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	140
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	141
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	142
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	143
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	144
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	145
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	146
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	147
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	148
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	149
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	150
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	151
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	152
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	153
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	154
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	155
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	156
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	157
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	158
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	159
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	160
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	161
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	162
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	163
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	164
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	165
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	166
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	167
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	168
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	169
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	170
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	171
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	172
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	173
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	174
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	117
	dw	0
	dw	0
	dw	175
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	176
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	177
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	178
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	179
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	180
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	181
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	182
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	183
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	184
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	185
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	186
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	187
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	32
	dw	0
	dw	0
	dw	188
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	189
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	190
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1056
	dw	0
	dw	0
	dw	191
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	192
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4105
	dw	0
	dw	0
	dw	193
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	194
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	195
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	196
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	65
	dw	0
	dw	0
	dw	197
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	198
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	199
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	200
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	201
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	65
	dw	0
	dw	0
	dw	202
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	17
	dw	0
	dw	0
	dw	203
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	204
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	205
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	206
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	207
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	4105
	dw	0
	dw	0
	dw	208
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	209
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	210
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	211
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	212
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	213
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1058
	dw	0
	dw	0
	dw	214
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	215
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	216
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	217
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	18
	dw	0
	dw	0
	dw	218
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	34
	dw	0
	dw	0
	dw	219
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	220
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	221
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	33
	dw	0
	dw	0
	dw	222
	dw	0
	dw	0
	dw	0
	dw	16
	dw	4
	dw	1027
	dw	0
	dw	0
	dw	223
	dw	0
	dw	0
	dw	0
	dw	22
	dw	513
	df	_crc_table_empty
	dw	0
	dw	116
	dw	0
	dw	224
	dw	0
	dw	0
	dw	0
	dw	22
	dw	513
	df	_crc_table
	dw	0
	dw	4112
	dw	0
	dw	225
	dw	0
	dw	0
	dw	0
	dw	?patch13
	dw	1
	db	3
	db	0
	db	0
	db	24
	db	11
	db	66
	db	67
	db	67
	db	51
	db	50
	db	32
	db	53
	db	46
	db	53
	db	46
	db	49
?patch13	equ	18
$$BSYMS	ends
$$BTYPES	segment byte public use32 'DEBTYP'
	db        2,0,0,0,14,0,8,0,3,0,0,0,0,0,0,0
	db        1,16,0,0,4,0,1,2,0,0,8,0,1,0,1,0
	db        3,16,0,0,18,0,3,0,32,0,0,0,17,0,0,0
	db        0,0,0,0,14,0,14,0,14,0,8,0,5,16,0,0
	db        0,0,0,0,7,16,0,0,8,0,2,0,10,0,6,16
	db        0,0,8,0,1,0,1,0,34,0,0,0,4,0,1,2
	db        0,0,14,0,8,0,34,0,0,0,0,0,3,0,11,16
	db        0,0,8,0,2,0,10,0,10,16,0,0,8,0,1,0
	db        1,0,32,0,0,0,16,0,1,2,3,0,34,0,0,0
	db        9,16,0,0,34,0,0,0,14,0,8,0,34,0,0,0
	db        0,0,1,0,15,16,0,0,8,0,2,0,10,0,14,16
	db        0,0,8,0,1,0,1,0,16,0,0,0,8,0,1,2
	db        1,0,13,16,0,0,18,0,3,0,34,0,0,0,17,0
	db        0,0,0,0,0,0,0,4,0,1,14,0,8,0,3,4
	db        0,0,7,0,7,0,21,16,0,0,8,0,2,0,10,0
	db        19,16,0,0,28,0,5,0,3,0,20,16,0,0,0,0
	db        0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0
	db        12,0,60,0,4,2,6,4,34,0,0,0,0,0,20,0
	db        0,0,0,0,0,0,0,0,242,241,6,4,3,4,0,0
	db        0,0,21,0,0,0,0,0,0,0,4,0,242,241,6,4
	db        116,0,0,0,0,0,22,0,0,0,0,0,0,0,8,0
	db        32,0,1,2,7,0,13,16,0,0,34,0,0,0,34,0
	db        0,0,18,16,0,0,34,0,0,0,34,0,0,0,3,4
	db        0,0,14,0,8,0,34,0,0,0,7,0,2,0,23,16
	db        0,0,12,0,1,2,2,0,3,4,0,0,34,4,0,0
	db        14,0,8,0,116,0,0,0,7,0,1,0,25,16,0,0
	db        8,0,1,2,1,0,3,4,0,0,14,0,8,0,3,4
	db        0,0,7,0,6,0,27,16,0,0,28,0,1,2,6,0
	db        3,4,0,0,18,16,0,0,34,0,0,0,34,0,0,0
	db        34,0,0,0,13,16,0,0,14,0,8,0,3,4,0,0
	db        7,0,5,0,29,16,0,0,24,0,1,2,5,0,3,4
	db        0,0,34,0,0,0,34,0,0,0,34,0,0,0,34,0
	db        0,0,14,0,8,0,116,0,0,0,7,0,1,0,33,16
	db        0,0,8,0,2,0,10,0,32,16,0,0,8,0,1,0
	db        1,0,3,0,0,0,8,0,1,2,1,0,31,16,0,0
$$BTYPES	ends
$$BNAMES	segment byte public use32 'DEBNAM'
	db	14,'make_crc_table'
	db	1,'p'
	db	4,'poly'
	db	1,'k'
	db	1,'n'
	db	1,'c'
	db	13,'get_crc_table'
	db	5,'crc32'
	db	3,'crc'
	db	3,'buf'
	db	3,'len'
	db	9,'crc32File'
	db	8,'filename'
	db	12,'File2CRC_Map'
	db	8,'File2CRC'
	db	5,'input'
	db	3,'len'
	db	6,'result'
	db	20,'_SECURITY_ATTRIBUTES'
	db	7,'nLength'
	db	20,'lpSecurityDescriptor'
	db	14,'bInheritHandle'
	db	6,'size_t'
	db	9,'ptrdiff_t'
	db	7,'wchar_t'
	db	6,'wint_t'
	db	8,'wctype_t'
	db	7,'va_list'
	db	5,'ULONG'
	db	6,'PULONG'
	db	6,'USHORT'
	db	5,'UCHAR'
	db	6,'PUCHAR'
	db	5,'DWORD'
	db	4,'BOOL'
	db	4,'BYTE'
	db	4,'WORD'
	db	5,'FLOAT'
	db	5,'PBYTE'
	db	6,'LPBYTE'
	db	6,'PDWORD'
	db	7,'LPDWORD'
	db	6,'LPVOID'
	db	7,'LPCVOID'
	db	3,'INT'
	db	4,'UINT'
	db	14,'POINTER_64_INT'
	db	6,'LONG32'
	db	5,'INT32'
	db	7,'ULONG32'
	db	7,'DWORD32'
	db	6,'UINT32'
	db	7,'INT_PTR'
	db	8,'UINT_PTR'
	db	8,'LONG_PTR'
	db	9,'ULONG_PTR'
	db	10,'PULONG_PTR'
	db	9,'UHALF_PTR'
	db	8,'HALF_PTR'
	db	10,'HANDLE_PTR'
	db	6,'SIZE_T'
	db	7,'PSIZE_T'
	db	7,'SSIZE_T'
	db	9,'DWORD_PTR'
	db	10,'PDWORD_PTR'
	db	6,'LONG64'
	db	5,'INT64'
	db	7,'ULONG64'
	db	7,'DWORD64'
	db	6,'UINT64'
	db	5,'PVOID'
	db	7,'PVOID64'
	db	4,'CHAR'
	db	5,'SHORT'
	db	4,'LONG'
	db	5,'WCHAR'
	db	5,'LPCCH'
	db	4,'PCCH'
	db	6,'LPCSTR'
	db	5,'PCSTR'
	db	5,'TCHAR'
	db	5,'TBYTE'
	db	6,'PTBYTE'
	db	6,'PCTSTR'
	db	7,'LPCTSTR'
	db	6,'HANDLE'
	db	5,'FCHAR'
	db	6,'FSHORT'
	db	5,'FLONG'
	db	7,'HRESULT'
	db	5,'CCHAR'
	db	4,'LCID'
	db	5,'PLCID'
	db	6,'LANGID'
	db	8,'LONGLONG'
	db	9,'ULONGLONG'
	db	3,'USN'
	db	9,'DWORDLONG'
	db	7,'BOOLEAN'
	db	8,'PBOOLEAN'
	db	10,'KSPIN_LOCK'
	db	11,'PKSPIN_LOCK'
	db	13,'PACCESS_TOKEN'
	db	20,'PSECURITY_DESCRIPTOR'
	db	4,'PSID'
	db	11,'ACCESS_MASK'
	db	12,'PACCESS_MASK'
	db	27,'SECURITY_DESCRIPTOR_CONTROL'
	db	30,'SECURITY_CONTEXT_TRACKING_MODE'
	db	31,'PSECURITY_CONTEXT_TRACKING_MODE'
	db	20,'SECURITY_INFORMATION'
	db	21,'PSECURITY_INFORMATION'
	db	15,'EXECUTION_STATE'
	db	6,'WPARAM'
	db	6,'LPARAM'
	db	7,'LRESULT'
	db	4,'ATOM'
	db	7,'HGLOBAL'
	db	6,'HLOCAL'
	db	12,'GLOBALHANDLE'
	db	11,'LOCALHANDLE'
	db	7,'HGDIOBJ'
	db	5,'HFILE'
	db	8,'COLORREF'
	db	10,'LPCOLORREF'
	db	20,'_SECURITY_ATTRIBUTES'
	db	19,'SECURITY_ATTRIBUTES'
	db	20,'PSECURITY_ATTRIBUTES'
	db	21,'LPSECURITY_ATTRIBUTES'
	db	9,'LCSCSTYPE'
	db	13,'LCSGAMUTMATCH'
	db	11,'FXPT16DOT16'
	db	10,'FXPT2DOT30'
	db	5,'BCHAR'
	db	7,'COLOR16'
	db	4,'HDWP'
	db	13,'MENUTEMPLATEA'
	db	13,'MENUTEMPLATEW'
	db	12,'MENUTEMPLATE'
	db	15,'LPMENUTEMPLATEA'
	db	15,'LPMENUTEMPLATEW'
	db	14,'LPMENUTEMPLATE'
	db	10,'HDEVNOTIFY'
	db	8,'HELPPOLY'
	db	6,'LGRPID'
	db	6,'LCTYPE'
	db	7,'CALTYPE'
	db	5,'CALID'
	db	6,'REGSAM'
	db	9,'MMVERSION'
	db	8,'MMRESULT'
	db	6,'FOURCC'
	db	8,'MCIERROR'
	db	11,'MCIDEVICEID'
	db	12,'I_RPC_HANDLE'
	db	10,'RPC_STATUS'
	db	18,'RPC_BINDING_HANDLE'
	db	8,'handle_t'
	db	13,'RPC_IF_HANDLE'
	db	24,'RPC_AUTH_IDENTITY_HANDLE'
	db	16,'RPC_AUTHZ_HANDLE'
	db	11,'I_RPC_MUTEX'
	db	13,'RPC_NS_HANDLE'
	db	12,'FILEOP_FLAGS'
	db	15,'PRINTEROP_FLAGS'
	db	6,'u_char'
	db	7,'u_short'
	db	5,'u_int'
	db	6,'u_long'
	db	6,'SOCKET'
	db	8,'WSAEVENT'
	db	10,'LPWSAEVENT'
	db	11,'SERVICETYPE'
	db	5,'GROUP'
	db	6,'ALG_ID'
	db	10,'HCRYPTPROV'
	db	9,'HCRYPTKEY'
	db	10,'HCRYPTHASH'
	db	16,'HCRYPTOIDFUNCSET'
	db	17,'HCRYPTOIDFUNCADDR'
	db	9,'HCRYPTMSG'
	db	10,'HCERTSTORE'
	db	14,'HCERTSTOREPROV'
	db	20,'HCRYPTDEFAULTCONTEXT'
	db	11,'HCRYPTASYNC'
	db	16,'HCERTCHAINENGINE'
	db	4,'byte'
	db	7,'boolean'
	db	12,'NDR_CCONTEXT'
	db	14,'error_status_t'
	db	10,'RPC_BUFPTR'
	db	10,'RPC_LENGTH'
	db	14,'PFORMAT_STRING'
	db	15,'PMIDL_XMIT_TYPE'
	db	20,'RPC_SS_THREAD_HANDLE'
	db	7,'OLECHAR'
	db	6,'DOUBLE'
	db	5,'SCODE'
	db	8,'HCONTEXT'
	db	10,'CLIPFORMAT'
	db	13,'HMETAFILEPICT'
	db	4,'DATE'
	db	12,'VARIANT_BOOL'
	db	7,'VARTYPE'
	db	6,'PROPID'
	db	16,'BAD_TRACK_NUMBER'
	db	5,'UWORD'
	db	7,'LPCBYTE'
	db	12,'SCARDCONTEXT'
	db	13,'PSCARDCONTEXT'
	db	14,'LPSCARDCONTEXT'
	db	11,'SCARDHANDLE'
	db	12,'PSCARDHANDLE'
	db	13,'LPSCARDHANDLE'
	db	13,'RPCOLEDATAREP'
	db	8,'HOLEMENU'
	db	6,'DISPID'
	db	8,'MEMBERID'
	db	8,'HREFTYPE'
	db	12,'PROPVAR_PAD1'
	db	12,'PROPVAR_PAD2'
	db	12,'PROPVAR_PAD3'
	db	7,'SC_LOCK'
	db	15,'crc_table_empty'
	db	9,'crc_table'
$$BNAMES	ends
	?debug	D "C:\WINPROG\Borland\Include\imm.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\mcx.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winsvc.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\commdlg.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\oleauto.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\propidl.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\oaidl.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\msxml.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\servprov.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\oleidl.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\urlmon.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\cguid.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\objidl.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\unknwn.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\search.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\stdlib.h" 10502 43040
	?debug	D "C:\WINPROG\Borland\Include\objbase.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\ole2.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\prsht.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winspool.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winsmcrd.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winioctl.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcnsip.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcndr.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\wtypes.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winscard.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winefs.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\wincrypt.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\qos.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winsock2.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winperf.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\shellapi.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcasync.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcnterr.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcnsi.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcdcep.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpcdce.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\rpc.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\nb30.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\mmsystem.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\lzexpand.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\dlgs.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\ddeml.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\dde.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\cderr.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winnetwk.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winreg.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winver.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\wincon.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winnls.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\tvout.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winuser.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\pshpack1.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\wingdi.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winerror.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winbase.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\pshpack8.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\pshpack2.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\poppack.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\pshpack4.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\mem.h" 10502 43040
	?debug	D "C:\WINPROG\Borland\Include\_loc.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\locale.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\_str.h" 10502 43040
	?debug	D "C:\WINPROG\Borland\Include\string.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\guiddef.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\basetsd.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\mbctype.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\ctype.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\winnt.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\windef.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\_null.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\_defs.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\_stddef.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\stdarg.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\excpt.h" 10458 43040
	?debug	D "C:\WINPROG\Borland\Include\windows.h" 10458 43040
	?debug	D "crc32.c" 18468 45213
	end
