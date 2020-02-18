/*
 * Copyright (C) 2005-2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2008 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#; void x86_sse_mix_buffers_with_gain (float *dst, float *src, long nframes, float gain);

.globl x86_sse_mix_buffers_with_gain
	.type	x86_sse_mix_buffers_with_gain,@function

x86_sse_mix_buffers_with_gain:
#; 8(%ebp)	= float	*dst 	= %edi
#; 12(%ebp) = float *src	= %esi
#; 16(%ebp) = long	nframes = %ecx
#; 20(%ebp) = float	gain    = st(0)

	pushl %ebp
	movl %esp, %ebp

	#; save the registers
#;	pushl %eax
	pushl %ebx
#;	pushl %ecx
	pushl %edi
	pushl %esi
	
	#; if nframes == 0, go to end
	movl 16(%ebp), %ecx #; nframes
	cmp	$0, %ecx
	je	.MBWG_END

	#; Check for alignment

	movl 8(%ebp), %edi  #; dst 
	movl 12(%ebp), %esi #; src

	movl %edi, %eax
	andl $12, %eax #; mask alignemnt offset

	movl %esi, %ebx
	andl $12, %ebx #; mask alignment offset

	cmp %eax, %ebx
	jne .MBWG_NONALIGN #; if not aligned, calculate manually

	#; if we are aligned
	cmp $0, %ebx
	jz .MBWG_SSE
	
	#; Pre-loop, we need to run 1-3 frames "manually" without
	#; SSE instructions

	movss 20(%ebp), %xmm1 #; xmm1

.MBWG_PRELOOP:
	
	movss (%esi), %xmm0
	mulss %xmm1, %xmm0
	addss (%edi), %xmm0
	movss %xmm0, (%edi)

	addl $4, %edi #; dst++
	addl $4, %esi #; src++
	decl %ecx 	  #; nframes--
	jz .MBWG_END

#;	cmp $0, %ecx
#;	je .MBWG_END #; if we run out of frames, go to end
	
	addl $4, %ebx
	
	cmp $16, %ebx #; test if we've reached 16 byte alignment
	jne .MBWG_PRELOOP


.MBWG_SSE:

	cmp $4, %ecx #; we know it's not zero, but if it's not >=4, then
	jnge .MBWG_NONALIGN #; we jump straight to the "normal" code

	#; copy gain to fill %xmm1
	movss   20(%ebp), %xmm1
    shufps  $0x00, %xmm1, %xmm1


.MBWG_SSELOOP:

	movaps	(%esi), %xmm0 #; source => xmm0
	mulps	%xmm1,  %xmm0 #; apply gain to source
	addps	(%edi), %xmm0 #; mix with destination
	movaps  %xmm0, (%edi) #; copy result to destination
	
	addl $16, %edi #; dst+=4
	addl $16, %esi #; src+=4

	subl $4, %ecx #; nframes-=4
	cmp $4, %ecx
	jge .MBWG_SSELOOP

	cmp $0, %ecx
	je .MBWG_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-3 frames.
	
.MBWG_NONALIGN:
	#; not aligned!

	movss 20(%ebp), %xmm1 #; gain => xmm1

.MBWG_NONALIGNLOOP:

	movss (%esi), %xmm0
	mulss %xmm1, %xmm0
	addss (%edi), %xmm0
	movss %xmm0, (%edi)
	
	addl $4, %edi
	addl $4, %esi
	
	decl %ecx
	jnz .MBWG_NONALIGNLOOP

.MBWG_END:

	popl %esi
	popl %edi
#;	popl %ecx
	popl %ebx
#;	popl %eax
	
	#; return
	leave
	ret

.size	x86_sse_mix_buffers_with_gain, .-x86_sse_mix_buffers_with_gain




#; void x86_sse_mix_buffers_no_gain (float *dst, float *src, long nframes);

.globl x86_sse_mix_buffers_no_gain
	.type	x86_sse_mix_buffers_no_gain,@function

x86_sse_mix_buffers_no_gain:
#; 8(%ebp)	= float	*dst 	= %edi
#; 12(%ebp) = float *src	= %esi
#; 16(%ebp) = long	nframes = %ecx

	pushl %ebp
	movl %esp, %ebp

	#; save the registers
#;	pushl %eax
	pushl %ebx
#;	pushl %ecx
	pushl %edi
	pushl %esi
	
	#; the real function

	#; if nframes == 0, go to end
	movl 16(%ebp), %ecx #; nframes
	cmp	$0, %ecx
	je	.MBNG_END

	#; Check for alignment

	movl 8(%ebp), %edi  #; dst 
	movl 12(%ebp), %esi #; src

	movl %edi, %eax
	andl $12, %eax #; mask alignemnt offset

	movl %esi, %ebx
	andl $12, %ebx #; mask alignment offset

	cmp %eax, %ebx
	jne .MBNG_NONALIGN #; if not aligned, calculate manually

	cmp $0, %ebx
	je .MBNG_SSE

	#; Pre-loop, we need to run 1-3 frames "manually" without
	#; SSE instructions

.MBNG_PRELOOP:
		
	movss (%esi), %xmm0
	addss (%edi), %xmm0
	movss %xmm0, (%edi)

	addl $4, %edi #; dst++
	addl $4, %esi #; src++
	decl %ecx 	  #; nframes--
	jz	.MBNG_END
	addl $4, %ebx
	
	cmp $16, %ebx #; test if we've reached 16 byte alignment
	jne .MBNG_PRELOOP

.MBNG_SSE:

	cmp $4, %ecx #; if there are frames left, but less than 4
	jnge .MBNG_NONALIGN #; we can't run SSE

.MBNG_SSELOOP:

	movaps	(%esi), %xmm0 #; source => xmm0
	addps	(%edi), %xmm0 #; mix with destination
	movaps  %xmm0, (%edi) #; copy result to destination
	
	addl $16, %edi #; dst+=4
	addl $16, %esi #; src+=4

	subl $4, %ecx #; nframes-=4
	cmp $4, %ecx
	jge .MBNG_SSELOOP

	cmp $0, %ecx
	je .MBNG_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-3 frames.
	
.MBNG_NONALIGN:
	#; not aligned!

	movss (%esi), %xmm0 #; src => xmm0
	addss (%edi), %xmm0 #; xmm0 += dst
	movss %xmm0, (%edi) #; xmm0 => dst
	
	addl $4, %edi
	addl $4, %esi
	
	decl %ecx
	jnz .MBNG_NONALIGN

.MBNG_END:

	popl %esi
	popl %edi
#;	popl %ecx
	popl %ebx
#;	popl %eax
	
	#; return
	leave
	ret

.size	x86_sse_mix_buffers_no_gain, .-x86_sse_mix_buffers_no_gain




#; void x86_sse_apply_gain_to_buffer (float *buf, long nframes, float gain);

.globl x86_sse_apply_gain_to_buffer
	.type	x86_sse_apply_gain_to_buffer,@function

x86_sse_apply_gain_to_buffer:
#; 8(%ebp)	= float	*buf 	= %edi
#; 12(%ebp) = long	nframes = %ecx
#; 16(%ebp) = float	gain    = st(0)

	pushl %ebp
	movl %esp, %ebp

	#; save %edi
	pushl %edi
	
	#; the real function

	#; if nframes == 0, go to end
	movl 12(%ebp), %ecx #; nframes
	cmp	$0, %ecx
	je	.AG_END

	#; create the gain buffer in %xmm1
	movss	16(%ebp), %xmm1
	shufps	$0x00, %xmm1, %xmm1
	
	#; Check for alignment

	movl 8(%ebp), %edi #; buf 
	movl %edi, %edx #; buf => %edx
	andl $12, %edx #; mask bits 1 & 2, result = 0, 4, 8 or 12
	jz	.AG_SSE #; if buffer IS aligned

	#; PRE-LOOP
	#; we iterate 1-3 times, doing normal x87 float comparison
	#; so we reach a 16 byte aligned "buf" (=%edi) value

.AGLP_START:

	#; Load next value from the buffer
	movss (%edi), %xmm0
	mulss %xmm1, %xmm0
	movss %xmm0, (%edi)

	#; increment buffer, decrement counter
	addl $4, %edi #; buf++;
	
	decl %ecx   #; nframes--
	jz	.AG_END #; if we run out of frames, we go to the end
	
	addl $4, %edx #; one non-aligned byte less
	cmp $16, %edx
	jne .AGLP_START #; if more non-aligned frames exist, we do a do-over

.AG_SSE:

	#; We have reached the 16 byte aligned "buf" ("edi") value

	#; Figure out how many loops we should do
	movl %ecx, %eax #; copy remaining nframes to %eax for division
	movl $0, %edx   #; 0 the edx register
	
	
	pushl %edi
	movl $4, %edi
	divl %edi #; %edx = remainder == 0
	popl %edi

	#; %eax = SSE iterations
	cmp $0, %eax
	je .AGPOST_START

	
.AGLP_SSE:

	movaps (%edi), %xmm0
	mulps %xmm1, %xmm0
	movaps %xmm0, (%edi)

	addl $16, %edi
#;	subl $4, %ecx   #; nframes-=4

	decl %eax
	jnz .AGLP_SSE

	#; Next we need to post-process all remaining frames
	#; the remaining frame count is in %ecx
	
	#; if no remaining frames, jump to the end
#;	cmp $0, %ecx
	andl $3, %ecx #; nframes % 4
	je .AG_END

.AGPOST_START:

	movss (%edi), %xmm0
	mulss %xmm1, %xmm0
	movss %xmm0, (%edi)

	#; increment buffer, decrement counter
	addl $4, %edi #; buf++;
	
	decl %ecx   #; nframes--
	jnz	.AGPOST_START #; if we run out of frames, we go to the end
	
.AG_END:


	popl %edi
	
	#; return
	leave
	ret

.size	x86_sse_apply_gain_to_buffer, .-x86_sse_apply_gain_to_buffer
#; end proc



#; float x86_sse_compute_peak(float *buf, long nframes, float current);

.globl x86_sse_compute_peak
	.type	x86_sse_compute_peak,@function

x86_sse_compute_peak:
#; 8(%ebp)	= float	*buf 	= %edi
#; 12(%ebp) = long	nframes = %ecx
#; 16(%ebp) = float	current = st(0)

	pushl %ebp
	movl %esp, %ebp

	#; save %edi
	pushl %edi
	
	#; the real function

	#; Load "current" in xmm0
	movss 16(%ebp), %xmm0

	#; if nframes == 0, go to end
	movl 12(%ebp), %ecx #; nframes
	cmp	$0, %ecx
	je	.CP_END

	#; create the "abs" mask in %xmm2
	pushl	$2147483647
	movss	(%esp), %xmm2
	addl    $4, %esp
	shufps	$0x00, %xmm2, %xmm2

	#; Check for alignment

	movl 8(%ebp), %edi #; buf 
	movl %edi, %edx #; buf => %edx
	andl $12, %edx #; mask bits 1 & 2, result = 0, 4, 8 or 12
	jz	.CP_SSE #; if buffer IS aligned

	#; PRE-LOOP
	#; we iterate 1-3 times, doing normal x87 float comparison
	#; so we reach a 16 byte aligned "buf" (=%edi) value

.LP_START:

	#; Load next value from the buffer
	movss (%edi), %xmm1
	andps %xmm2, %xmm1
	maxss %xmm1, %xmm0

	#; increment buffer, decrement counter
	addl $4, %edi #; buf++;
	
	decl %ecx   #; nframes--
	jz	.CP_END #; if we run out of frames, we go to the end
	
	addl $4, %edx #; one non-aligned byte less
	cmp $16, %edx
	jne .LP_START #; if more non-aligned frames exist, we do a do-over

.CP_SSE:

	#; We have reached the 16 byte aligned "buf" ("edi") value

	#; Figure out how many loops we should do
	movl %ecx, %eax #; copy remaining nframes to %eax for division

	shr $2,%eax #; unsigned divide by 4
	jz .POST_START

	#; %eax = SSE iterations

	#; current maximum is at %xmm0, but we need to ..
	shufps $0x00, %xmm0, %xmm0 #; shuffle "current" to all 4 FP's

	#;prefetcht0 16(%edi)

.LP_SSE:

	movaps (%edi), %xmm1
	andps %xmm2, %xmm1
	maxps %xmm1, %xmm0

	addl $16, %edi

	decl %eax
	jnz .LP_SSE

	#; Calculate the maximum value contained in the 4 FP's in %xmm0
	movaps %xmm0, %xmm1
	shufps $0x4e, %xmm1, %xmm1 #; shuffle left & right pairs (1234 => 3412)
	maxps  %xmm1, %xmm0 #; maximums of the two pairs
	movaps %xmm0, %xmm1
	shufps $0xb1, %xmm1, %xmm1 #; shuffle the floats inside the two pairs (1234 => 2143)
	maxps  %xmm1, %xmm0 

	#; now every float in %xmm0 is the same value, current maximum value
	
	#; Next we need to post-process all remaining frames
	#; the remaining frame count is in %ecx
	
	#; if no remaining frames, jump to the end

	andl $3, %ecx #; nframes % 4
	jz .CP_END

.POST_START:

	movss (%edi), %xmm1
	andps %xmm2, %xmm1
	maxss %xmm1, %xmm0
	
	addl $4, %edi 	#; buf++;
	
	decl %ecx		#; nframes--;
	jnz .POST_START

.CP_END:

	#; Load the value from xmm0 to the float stack for returning
	movss %xmm0, 16(%ebp)
	flds 16(%ebp)

	popl %edi
	
	#; return
	leave
	ret

.size	x86_sse_compute_peak, .-x86_sse_compute_peak
#; end proc

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif


