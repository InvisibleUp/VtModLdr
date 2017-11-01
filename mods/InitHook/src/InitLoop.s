	.file	"InitLoop.c"
	.intel_syntax noprefix
	.text
	.globl	InitLoop
	.type	InitLoop, @function
InitLoop:
.LFB4:
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	push	ebx
	push	edx
	.cfi_offset 3, -12
	xor	ebx, ebx
.L2:
	cmp	ebx, OFFSET FLAT:HookCount
	jge	.L6
	mov	eax, DWORD PTR HookAddr
	lea	eax, [eax+ebx*4]
	inc	ebx
	call	eax
	jmp	.L2
.L6:
	pop	eax
	pop	ebx
	.cfi_restore 3
	pop	ebp
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE4:
	.size	InitLoop, .-InitLoop
	.ident	"GCC: (SUSE Linux) 7.2.1 20171005 [gcc-7-branch revision 253439]"
	.section	.note.GNU-stack,"",@progbits
