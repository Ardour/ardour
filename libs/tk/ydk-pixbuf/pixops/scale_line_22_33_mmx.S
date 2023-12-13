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
	.file	"scale_line_22_33_mmx.S"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16

#if !defined(__MINGW32__) && !defined(__CYGWIN__) && !defined(__INTERIX)
	
/* Magic indicating no need for an executable stack */
#if !defined __powerpc64__ && !defined __ia64__
.section .note.GNU-stack;  .previous
#endif
	
.globl _pixops_scale_line_22_33_mmx
	.type	 _pixops_scale_line_22_33_mmx,@function
_pixops_scale_line_22_33_mmx:
	
#else
	
.globl __pixops_scale_line_22_33_mmx
__pixops_scale_line_22_33_mmx:
	
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

	cmpl 28(%ebp),%esi
	jnb  .out

/* For the body of this loop, %mm01, %mm1, %mm2, %mm3 hold the 4 adjoining
 * points we are interpolating between, as:
 *
 *  000000BB00GG00RR
 */	
	
/* Load initial values into %mm1, %mm3 */
	leal (%edx,%edx,2),%edx  # Multiply by 3

	movl 16(%ebp),%edi
	pxor %mm4, %mm4
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm1
	punpcklbw %mm4, %mm1

	movl 20(%ebp),%edi
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm3
	punpcklbw %mm4, %mm3

	addl $65536,%ebx
	movl %ebx,%edx
	sarl $16,%edx

	jmp .newx
	.p2align 4,,7
.loop:
/* short *pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y
 *                                             16             4                  0xf            2     2
 */
	movl %ebx,%eax
	andl $0xf000,%eax
	shrl $7,%eax

/* At this point, %edi holds weights. Load the 4 weights into %mm4,%mm5,%mm6,%mm7, multiply and
 * accumulate.
 */
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

/* %mm7	holds the accumulated sum. Compute (C + 0x80) / 256
 */
	pxor %mm4, %mm4
	movl $8421504, %eax  # 0x00808080
	movd %eax, %mm6  
	punpcklbw %mm4, %mm6
	paddw %mm6, %mm7
	psrlw $8, %mm7

/* Pack into %eax and store result
 */	
	packuswb %mm7, %mm7
	movd %mm7, %eax
	
	movb %al, (%esi)
	shrl $8, %eax
	movw %ax, 1(%esi)
	addl $3, %esi
		
	cmpl %esi,28(%ebp)
	je   .out

/* x += x_step; */
	addl 24(%ebp),%ebx
/* x_scaled = x >> 16; */
	movl %ebx,%edx
	sarl $16,%edx

	cmpl %edx,-24(%ebp)
	je   .loop

.newx:
	movl %edx,-24(%ebp)
/*
 * Load the two new values into %mm1, %mm3, move old values into %mm0, %mm2
 */
	movq %mm1, %mm0
	movq %mm3, %mm2
	
	leal (%edx,%edx,2),%edx  # Multiply by 3

	movl 16(%ebp),%edi
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm1
	punpcklbw %mm4, %mm1

	movl 20(%ebp),%edi
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm3
	punpcklbw %mm4, %mm3
	
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
