/* start.s -- probe.c's entry and its system call stub: "lcall $7,$0", the
 * C stub's return address on top of the arguments, as NetBSD 1.0's libc
 * calls; scerr is 1 when the carry flag came back set. */
	.text
	.globl _start, sc, sc2, scerr
_start:	pushl %esp
	call start_c
1:	jmp 1b
sc:
sc2:	popl %ecx		/* return address */
	popl %eax		/* code */
	pushl %ecx		/* the stub's return address, as libc's */
	movl $0, scerr
	lcall $7,$0
	jnc 2f
	movl $1, scerr
2:	popl %ecx
	pushl %eax		/* restore the slot for the caller's cleanup */
	pushl %ecx
	ret
	.data
scerr:	.long 0
	.section .note.GNU-stack,"",@progbits
