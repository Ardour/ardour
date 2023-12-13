/*
 * Copyright (C) 2000 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
	.file	"have_mmx.S"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16

#if !defined(__MINGW32__) && !defined(__CYGWIN__) && !defined(__INTERIX)

/* Magic indicating no need for an executable stack */
#if !defined __powerpc64__ && !defined __ia64__
.section .note.GNU-stack;  .previous
#endif
	
.globl _pixops_have_mmx
	.type	 _pixops_have_mmx,@function
_pixops_have_mmx:

#else

.globl __pixops_have_mmx
__pixops_have_mmx:

#endif
	
	push	%ebx

/* # Check if bit 21 in flags word is writeable */

	pushfl	
	popl	%eax
	movl	%eax,%ebx
	xorl	$0x00200000, %eax
	pushl   %eax
	popfl
	pushfl
	popl	%eax

	cmpl	%eax, %ebx

	je .notfound

/* # OK, we have CPUID */

	movl	$1, %eax
	cpuid
	
	test	$0x00800000, %edx
	jz	.notfound

	movl	$1, %eax
	jmp	.out

.notfound:
	movl  	$0, %eax
.out:	
	popl	%ebx
	ret

