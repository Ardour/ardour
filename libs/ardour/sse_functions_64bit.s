/*
 * Copyright (C) 2006 Sampo Savolainen <v2@iki.fi>
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


#; void x86_sse_mix_buffers_with_gain (float *dst, float *src, unsigned int nframes, float gain);

.globl x86_sse_mix_buffers_with_gain
	.type	x86_sse_mix_buffers_with_gain,@function

x86_sse_mix_buffers_with_gain:

#; %rdi float	*dst
#; %rsi float	*src	
#; %rdx unsigned int nframes
#; %xmm0 float	gain

	pushq %rbp
	movq %rsp, %rbp

	#; save the registers
	pushq %rbx
	pushq %rdi
	pushq %rsi
	
	#; if nframes == 0, go to end
	cmp	$0, %rdx
	je	.MBWG_END

	#; Check for alignment

	movq %rdi, %rax
	andq $12, %rax #; mask alignment offset

	movq %rsi, %rbx
	andq $12, %rbx #; mask alignment offset

	cmp %rax, %rbx
	jne .MBWG_NONALIGN #; if not aligned, calculate manually

	#; if we are aligned
	cmp $0, %rbx
	jz .MBWG_SSE
	
	#; Pre-loop, we need to run 1-3 frames "manually" without
	#; SSE instructions

.MBWG_PRELOOP:
	
	#; gain is already in %xmm0
	movss (%rsi), %xmm1
	mulss %xmm0, %xmm1
	addss (%rdi), %xmm1
	movss %xmm1, (%rdi)

	addq $4, %rdi #; dst++
	addq $4, %rsi #; src++
	decq %rdx 	  #; nframes--
	jz .MBWG_END

	addq $4, %rbx
	
	cmp $16, %rbx #; test if we've reached 16 byte alignment
	jne .MBWG_PRELOOP


.MBWG_SSE:

	cmp $4, %rdx #; we know it's not zero, but if it's not >=4, then
	jnge .MBWG_NONALIGN #; we jump straight to the "normal" code

	#; gain is already in %xmm0
	shufps  $0x00, %xmm0, %xmm0


.MBWG_SSELOOP:

	movaps	(%rsi), %xmm1 #; source => xmm0
	mulps	%xmm0,  %xmm1 #; apply gain to source
	addps	(%rdi), %xmm1 #; mix with destination
	movaps  %xmm1, (%rdi) #; copy result to destination
	
	addq $16, %rdi #; dst+=4
	addq $16, %rsi #; src+=4

	subq $4, %rdx #; nframes-=4
	cmp $4, %rdx
	jge .MBWG_SSELOOP

	cmp $0, %rdx
	je .MBWG_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-3 frames.
	
.MBWG_NONALIGN:
	#; not aligned!

	#; gain is already in %xmm0

.MBWG_NONALIGNLOOP:

	movss (%rsi), %xmm1
	mulss %xmm0, %xmm1
	addss (%rdi), %xmm1
	movss %xmm1, (%rdi)
	
	addq $4, %rdi
	addq $4, %rsi
	
	decq %rdx
	jnz .MBWG_NONALIGNLOOP

.MBWG_END:

	popq %rsi
	popq %rdi
	popq %rbx
	
	#; return
	leave
	ret

.size	x86_sse_mix_buffers_with_gain, .-x86_sse_mix_buffers_with_gain


#; void x86_sse_mix_buffers_no_gain (float *dst, float *src, unsigned int nframes);

.globl x86_sse_mix_buffers_no_gain
	.type	x86_sse_mix_buffers_no_gain,@function

x86_sse_mix_buffers_no_gain:

#; %rdi float *dst
#; %rsi float *src
#; %rdx unsigned int nframes

	pushq %rbp
	movq %rsp, %rbp

	#; save the registers
	pushq %rbx
	pushq %rdi
	pushq %rsi
	
	#; the real function

	#; if nframes == 0, go to end
	cmp	$0, %rdx
	je	.MBNG_END

	#; Check for alignment

	movq %rdi, %rax
	andq $12, %rax #; mask alignment offset

	movq %rsi, %rbx
	andq $12, %rbx #; mask alignment offset

	cmp %rax, %rbx
	jne .MBNG_NONALIGN #; if not aligned, calculate manually

	cmp $0, %rbx
	je .MBNG_SSE

	#; Pre-loop, we need to run 1-3 frames "manually" without
	#; SSE instructions

.MBNG_PRELOOP:
		
	movss (%rsi), %xmm0
	addss (%rdi), %xmm0
	movss %xmm0, (%rdi)

	addq $4, %rdi #; dst++
	addq $4, %rsi #; src++
	decq %rdx 	  #; nframes--
	jz	.MBNG_END
	addq $4, %rbx
	
	cmp $16, %rbx #; test if we've reached 16 byte alignment
	jne .MBNG_PRELOOP

.MBNG_SSE:

	cmp $4, %rdx #; if there are frames left, but less than 4
	jnge .MBNG_NONALIGN #; we can't run SSE

.MBNG_SSELOOP:

	movaps	(%rsi), %xmm0 #; source => xmm0
	addps	(%rdi), %xmm0 #; mix with destination
	movaps  %xmm0, (%rdi) #; copy result to destination
	
	addq $16, %rdi #; dst+=4
	addq $16, %rsi #; src+=4

	subq $4, %rdx #; nframes-=4
	cmp $4, %rdx
	jge .MBNG_SSELOOP

	cmp $0, %rdx
	je .MBNG_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-3 frames.
	
.MBNG_NONALIGN:
	#; not aligned!

	movss (%rsi), %xmm0 #; src => xmm0
	addss (%rdi), %xmm0 #; xmm0 += dst
	movss %xmm0, (%rdi) #; xmm0 => dst
	
	addq $4, %rdi
	addq $4, %rsi
	
	decq %rdx
	jnz .MBNG_NONALIGN

.MBNG_END:

	popq %rsi
	popq %rdi
	popq %rbx
	
	#; return
	leave
	ret

.size	x86_sse_mix_buffers_no_gain, .-x86_sse_mix_buffers_no_gain


#; void x86_sse_apply_gain_to_buffer (float *buf, unsigned int nframes, float gain);

.globl x86_sse_apply_gain_to_buffer
	.type	x86_sse_apply_gain_to_buffer,@function

x86_sse_apply_gain_to_buffer:

#; %rdi	 float 		*buf	32(%rbp)
#; %rsi  unsigned int 	nframes
#; %xmm0 float 		gain
#; %xmm1 float		buf[0]

	pushq %rbp
	movq %rsp, %rbp

	#; save %rdi
	pushq %rdi
	
	#; the real function

	#; if nframes == 0, go to end
	movq %rsi, %rcx #; nframes
	cmp	$0, %rcx
	je	.AG_END

	#; set up the gain buffer (gain is already in %xmm0)
	shufps	$0x00, %xmm0, %xmm0
	
	#; Check for alignment

	movq %rdi, %rdx #; buf => %rdx
	andq $12, %rdx #; mask bits 1 & 2, result = 0, 4, 8 or 12
	jz	.AG_SSE #; if buffer IS aligned

	#; PRE-LOOP
	#; we iterate 1-3 times, doing normal x87 float comparison
	#; so we reach a 16 byte aligned "buf" (=%rdi) value

.AGLP_START:

	#; Load next value from the buffer into %xmm1
	movss (%rdi), %xmm1
	mulss %xmm0, %xmm1
	movss %xmm1, (%rdi)

	#; increment buffer, decrement counter
	addq $4, %rdi #; buf++;
	
	decq %rcx   #; nframes--
	jz	.AG_END #; if we run out of frames, we go to the end
	
	addq $4, %rdx #; one non-aligned byte less
	cmp $16, %rdx
	jne .AGLP_START #; if more non-aligned frames exist, we do a do-over

.AG_SSE:

	#; We have reached the 16 byte aligned "buf" ("rdi") value

	#; Figure out how many loops we should do
	movq %rcx, %rax #; copy remaining nframes to %rax for division
	movq $0, %rdx   #; 0 the edx register
	
	
	pushq %rdi
	movq $4, %rdi
	divq %rdi #; %rdx = remainder == 0
	popq %rdi

	#; %rax = SSE iterations
	cmp $0, %rax
	je .AGPOST_START

	
.AGLP_SSE:

	movaps (%rdi), %xmm1
	mulps %xmm0, %xmm1
	movaps %xmm1, (%rdi)

	addq $16, %rdi
	subq $4, %rcx   #; nframes-=4

	decq %rax
	jnz .AGLP_SSE

	#; Next we need to post-process all remaining frames
	#; the remaining frame count is in %rcx
	
	#; if no remaining frames, jump to the end
	cmp $0, %rcx
	andq $3, %rcx #; nframes % 4
	je .AG_END

.AGPOST_START:

	movss (%rdi), %xmm1
	mulss %xmm0, %xmm1
	movss %xmm1, (%rdi)

	#; increment buffer, decrement counter
	addq $4, %rdi #; buf++;
	
	decq %rcx   #; nframes--
	jnz	.AGPOST_START #; if we run out of frames, we go to the end
	
.AG_END:


	popq %rdi
	
	#; return
	leave
	ret

.size	x86_sse_apply_gain_to_buffer, .-x86_sse_apply_gain_to_buffer
#; end proc


#; x86_sse_apply_gain_vector(float *buf, float *gain_vector, unsigned int nframes)

.globl x86_sse_apply_gain_vector
        .type   x86_sse_apply_gain_vector,@function

x86_sse_apply_gain_vector:

#; %rdi float *buf
#; %rsi float *gain_vector
#; %rdx unsigned int nframes

	pushq %rbp
	movq %rsp, %rbp

	#; Save registers
	pushq %rdi
	pushq %rsi
	pushq %rbx

	#; if nframes == 0 go to end
	cmp $0, %rdx
	je .AGA_END
		
	#; Check alignment
	movq %rdi, %rax
	andq $12, %rax
		
	movq %rsi, %rbx
	andq $12, %rbx

	cmp %rax,%rbx
	jne .AGA_ENDLOOP

	cmp $0, %rax
	jz .AGA_SSE #; if buffers are aligned, jump to the SSE loop

#; Buffers aren't 16 byte aligned, but they are unaligned by the same amount
.AGA_ALIGNLOOP:
		
	movss (%rdi), %xmm0 #; buf => xmm0
	movss (%rsi), %xmm1 #; gain value => xmm1
	mulss %xmm1, %xmm0  #; xmm1 * xmm0 => xmm0
	movss %xmm0, (%rdi) #; signal with gain => buf

	decq %rdx
	jz .AGA_END

	addq $4, %rdi #; buf++
	addq $4, %rsi #; gab++
	
	addq $4, %rax
	cmp $16, %rax
	jne .AGA_ALIGNLOOP
	
#; There are frames left for sure, as that is checked in the beginning
#; and within the previous loop. BUT, there might be less than 4 frames
#; to process

.AGA_SSE:
	movq %rdx, %rax #; nframes => %rax
	shr $2, %rax #; unsigned divide by 4

	cmp $0, %rax  #; Jos toimii ilman tätä, niin kiva
	je .AGA_ENDLOOP

.AGA_SSELOOP:
	movaps (%rdi), %xmm0
	movaps (%rsi), %xmm1
	mulps %xmm1, %xmm0
	movaps %xmm0, (%rdi)

	addq $16, %rdi
	addq $16, %rsi

	decq %rax
	jnz .AGA_SSELOOP

	andq $3, %rdx #; Remaining frames are nframes & 3
	jz .AGA_END


#; Inside this loop, we know there are frames left to process
#; but because either there are < 4 frames left, or the buffers
#; are not aligned, we can't use the parallel SSE ops
.AGA_ENDLOOP:
	movss (%rdi), %xmm0 #; buf => xmm0
	movss (%rsi), %xmm1 #; gain value => xmm1
	mulss %xmm1, %xmm0  #; xmm1 * xmm0 => xmm0
	movss %xmm0, (%rdi) #; signal with gain => buf

	addq $4,%rdi
	addq $4,%rsi
	decq %rdx #; nframes--
	jnz .AGA_ENDLOOP

.AGA_END:

	popq %rbx
	popq %rsi
	popq %rdi

	leave
	ret

.size	x86_sse_apply_gain_vector, .-x86_sse_apply_gain_vector
#; end proc


#; float x86_sse_compute_peak(float *buf, long nframes, float current);

.globl x86_sse_compute_peak
	.type	x86_sse_compute_peak,@function

	
x86_sse_compute_peak:

#; %rdi	 float 		*buf	32(%rbp)
#; %rsi	 unsigned int 	nframes
#; %xmm0 float		current
#; %xmm1 float		buf[0]

	pushq %rbp
	movq %rsp, %rbp

	#; save %rdi
	pushq %rdi
	
	#; if nframes == 0, go to end
	movq %rsi, %rcx #; nframes
	cmp	$0, %rcx
	je	.CP_END

	#; create the "abs" mask in %xmm2
	pushq   $2147483647
	movss	(%rsp), %xmm2
	addq    $8, %rsp
	shufps	$0x00, %xmm2, %xmm2

	#; Check for alignment

	#;movq 8(%rbp), %rdi #; buf 
	movq %rdi, %rdx #; buf => %rdx
	andq $12, %rdx #; mask bits 1 & 2, result = 0, 4, 8 or 12
	jz	.CP_SSE #; if buffer IS aligned

	#; PRE-LOOP
	#; we iterate 1-3 times, doing normal x87 float comparison
	#; so we reach a 16 byte aligned "buf" (=%rdi) value

.LP_START:

	#; Load next value from the buffer
	movss (%rdi), %xmm1
	andps %xmm2, %xmm1
	maxss %xmm1, %xmm0

	#; increment buffer, decrement counter
	addq $4, %rdi #; buf++;
	
	decq %rcx   #; nframes--
	jz	.CP_END #; if we run out of frames, we go to the end
	
	addq $4, %rdx #; one non-aligned byte less
	cmp $16, %rdx
	jne .LP_START #; if more non-aligned frames exist, we do a do-over

.CP_SSE:

	#; We have reached the 16 byte aligned "buf" ("rdi") value

	#; Figure out how many loops we should do
	movq %rcx, %rax #; copy remaining nframes to %rax for division

	shr $2,%rax #; unsigned divide by 4
	jz .POST_START

	#; %rax = SSE iterations

	#; current maximum is at %xmm0, but we need to ..
	shufps $0x00, %xmm0, %xmm0 #; shuffle "current" to all 4 FP's

	#;prefetcht0 16(%rdi)

.LP_SSE:

	movaps (%rdi), %xmm1
	andps %xmm2, %xmm1
	maxps %xmm1, %xmm0

	addq $16, %rdi

	decq %rax
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
	#; the remaining frame count is in %rcx
	
	#; if no remaining frames, jump to the end

	andq $3, %rcx #; nframes % 4
	jz .CP_END

.POST_START:

	movss (%rdi), %xmm1
	andps %xmm2, %xmm1
	maxss %xmm1, %xmm0
	
	addq $4, %rdi 	#; buf++;
	
	decq %rcx		#; nframes--;
	jnz .POST_START

.CP_END:

	popq %rdi
	
	#; return
	leave
	ret

.size	x86_sse_compute_peak, .-x86_sse_compute_peak
#; end proc

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif

