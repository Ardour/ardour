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
	.file	"composite_line_color_22_4a4_mmx.S"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16

#if !defined(__MINGW32__) && !defined(__CYGWIN__) && !defined(__INTERIX)	

/* Magic indicating no need for an executable stack */
#if !defined __powerpc64__ && !defined __ia64__
.section .note.GNU-stack;  .previous
#endif
	
.globl _pixops_composite_line_color_22_4a4_mmx
	.type	 _pixops_composite_line_color_22_4a4_mmx,@function
_pixops_composite_line_color_22_4a4_mmx:

#else

.globl __pixops_composite_line_color_22_4a4_mmx
__pixops_composite_line_color_22_4a4_mmx:
	
#endif
/*
 * Arguments
 *		
 * weights:	 8(%ebp)
 * p:	        12(%ebp)	%esi
 * q1:	        16(%ebp)	
 * q2:	        20(%ebp)	
 * xstep:       24(%ebp)	
 * p_end:       28(%ebp)
 * xinit:       32(%ebp)
 * dest_x:	36(%ebp)
 * check_shift:	40(%ebp)
 * colors:	44(%ebp)
 *	
*/

/*
 * Function call entry
 */
	pushl %ebp
	movl %esp,%ebp
	subl $28,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
/* Locals:	
 * int x                      %ebx
 * int x_scaled             -24(%ebp)
 */

/*
 * Setup
 */
/* Initialize variables */	
	movl 32(%ebp),%ebx
	movl 32(%ebp),%edx
	sarl $16,%edx
	movl 12(%ebp),%esi

	movl %edx,-24(%ebp)

	cmpl 28(%ebp),%esi
	jnb  .out

/* Load initial values into %mm1, %mm3 */
	shll $2, %edx

	pxor %mm4, %mm4

	movl 16(%ebp),%edi
	movl (%edi, %edx), %eax
	movd (%edi, %edx), %mm5
	punpcklbw %mm4, %mm5
	shrl $24, %eax
	movl $0x010101, %ecx
	mull %ecx
	orl  $0xff000000, %eax
	movd %eax, %mm1
	punpcklbw %mm4, %mm1
	pmullw %mm5,%mm1

/* 
 *	mull destroyed %edx, need to reconstitute 
 */
	movl -24(%ebp),%edx
	shll $2, %edx
		
	movl 20(%ebp),%edi
	movl (%edi, %edx), %eax
	movd (%edi, %edx), %mm5
	punpcklbw %mm4, %mm5
	shrl $24, %eax
	movl $0x010101, %ecx
	mull %ecx
	orl  $0xff000000, %eax
	movd %eax, %mm3
	punpcklbw %mm4, %mm3
	pmullw %mm5,%mm3

	psrlw $8,%mm1
	psrlw $8,%mm3

	addl $65536,%ebx
	movl %ebx,%edx
	sarl $16,%edx

	jmp .newx
	.p2align 4,,7
.loop:
/* int x_index = (x & 0xf000) >> 12 */
	movl %ebx,%eax
	andl $0xf000,%eax
	shrl $7,%eax

	movq (%edi,%eax),%mm4
	pmullw %mm0,%mm4
	movq 8(%edi,%eax),%mm5
	pmullw %mm1,%mm5
	movq 16(%edi,%eax),%mm6
	movq 24(%edi,%eax),%mm7
	pmullw %mm2,%mm6
	pmullw %mm3,%mm7
	paddw %mm4, %mm5
	paddw %mm6, %mm7
	paddw %mm5, %mm7

	movl $0xffff,%ecx
	movd %ecx,%mm4
	psllq $48,%mm4
	movq %mm4,%mm6
	psubw %mm7,%mm4
	pand %mm6,%mm4
	
	movq %mm4,%mm5
	psrlq $16,%mm4
	por %mm4,%mm5
	psrlq $32,%mm5
	por %mm4,%mm5
	
	psrlw $8,%mm5

	movl 36(%ebp),%eax
	incl 36(%ebp)

	movl 40(%ebp),%ecx
	shrl %cl,%eax
	andl $1,%eax

	movl 44(%ebp),%ecx
	movq (%ecx,%eax,8),%mm6

	pmullw %mm6,%mm5

/* x += x_step; */
	addl 24(%ebp),%ebx
/* x_scale = x >> 16; */
	movl %ebx,%edx
	sarl $16,%edx

	paddw %mm5,%mm7

	psrlw $8,%mm7
	packuswb %mm7, %mm7 
	movd %mm7,(%esi)

	addl $4, %esi
		
	cmpl %esi,28(%ebp)
	je   .out

	cmpl %edx,-24(%ebp)
	je   .loop

.newx:
	movl %edx,-24(%ebp)
/*
 * Load the two new values into %mm1, %mm3, move old values into %mm0, %mm2
 */
	movq %mm1, %mm0
	movq %mm3, %mm2

	shll $2, %edx

	pxor %mm4, %mm4

	movl 16(%ebp),%edi
	movl (%edi, %edx), %eax
	movd (%edi, %edx), %mm5
	punpcklbw %mm4, %mm5
	shrl $24, %eax
	movl $0x010101, %ecx
	mull %ecx
/* 
 *	mull destroyed %edx, need to reconstitute 
 */
	movl -24(%ebp),%edx
	shll $2, %edx

	orl  $0xff000000, %eax
	movd %eax, %mm1
	punpcklbw %mm4, %mm1
	pmullw %mm5,%mm1
		
	movl 20(%ebp),%edi
	movl (%edi, %edx), %eax
	movd (%edi, %edx), %mm5
	punpcklbw %mm4, %mm5
	shrl $24, %eax
	movl $0x010101, %ecx
	mull %ecx
	orl  $0xff000000, %eax
	movd %eax, %mm3
	punpcklbw %mm4, %mm3
	pmullw %mm5,%mm3
	
	psrlw $8,%mm1
	psrlw $8,%mm3

	movl 8(%ebp),%edi
	
	jmp .loop

.out:
	movl %esi,%eax
	emms
	leal -40(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
