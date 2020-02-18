/*
 * Copyright (C) 2015 Paul Davis <paul@linuxaudiosystems.com>
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

#; Microsoft version of AVX sample processing functions

#; void x86_sse_avx_mix_buffers_with_gain (float *dst, float *src, unsigned int nframes, float gain);

.globl x86_sse_avx_mix_buffers_with_gain
	.def    x86_sse_avx_mix_buffers_with_gain; .scl    2;      .type   32;     
.endef

x86_sse_avx_mix_buffers_with_gain:

#; due to Microsoft calling convention
#; %rcx float *dst
#; %rdx float *src
#; %r8 unsigned int nframes
#; %xmm3 float	gain

	pushq %rbp
	movq %rsp, %rbp

	#; save the registers
	pushq %rbx #; must be preserved
	
	#; move current max to %xmm0 for convenience
	movss %xmm3, %xmm0

	#; if nframes == 0, go to end
	cmp	$0, %r8
	je	.MBWG_END

	#; Check for alignment

	movq %rcx, %rax
	andq $28, %rax #; mask alignment offset

	movq %rdx, %rbx
	andq $28, %rbx #; mask alignment offset

	cmp %rax, %rbx
	jne .MBWG_NONALIGN #; if buffer are not aligned between each other, calculate manually

	#; if we are aligned
	cmp $0, %rbx
	jz .MBWG_AVX
	
	#; Pre-loop, we need to run 1-7 frames "manually" without
	#; SSE instructions

.MBWG_PRELOOP:
	
	#; gain is already in %xmm0
	movss (%rdx), %xmm1
	mulss %xmm0, %xmm1
	addss (%rcx), %xmm1
	movss %xmm1, (%rcx)

	addq $4, %rcx #; dst++
	addq $4, %rdx #; src++
	decq %r8 	  #; nframes--
	jz .MBWG_END

	addq $4, %rbx
	
	cmp $32, %rbx #; test if we've reached 32 byte alignment
	jne .MBWG_PRELOOP

.MBWG_AVX:

	cmp $8, %r8 #; we know it's not zero, but if it's not >=4, then
	jl .MBWG_NONALIGN #; we jump straight to the "normal" code

	#; set up the gain buffer (gain is already in %xmm0)
	vshufps $0x00, %ymm0, %ymm0, %ymm0 #; spread single float value to the first 128 bits of ymm0 register
	vperm2f128 $0x00, %ymm0, %ymm0, %ymm0 #; extend the first 128 bits of ymm0 register to higher 128 bits

.MBWG_AVXLOOP:

	vmovaps	(%rdx), %ymm1        #; source => xmm0
	vmulps	%ymm0,  %ymm1, %ymm2 #; apply gain to source
	vaddps	(%rcx), %ymm2, %ymm1 #; mix with destination
	vmovaps  %ymm1, (%rcx)        #; copy result to destination
	
	addq $32, %rcx #; dst+=8
	addq $32, %rdx #; src+=8

	subq $8, %r8 #; nframes-=8
	cmp $8, %r8
	jge .MBWG_AVXLOOP

	#; zero upper 128 bits of all ymm registers to proceed with SSE operations without penalties
	vzeroupper

	cmp $0, %r8
	je .MBWG_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-7 frames.
	
.MBWG_NONALIGN:
	#; not aligned!

	#; gain is already in %xmm0

.MBWG_NONALIGNLOOP:

	movss (%rdx), %xmm1
	mulss %xmm0, %xmm1
	addss (%rcx), %xmm1
	movss %xmm1, (%rcx)
	
	addq $4, %rcx
	addq $4, %rdx
	
	decq %r8
	jnz .MBWG_NONALIGNLOOP

.MBWG_END:

	popq %rbx

	#; return
	leave
	ret


#; void x86_sse_avx_mix_buffers_no_gain (float *dst, float *src, unsigned int nframes);

.globl x86_sse_avx_mix_buffers_no_gain
	.def	x86_sse_avx_mix_buffers_no_gain; .scl    2;   .type   32;
.endef

x86_sse_avx_mix_buffers_no_gain:

#; due to Microsoft calling convention
#; %rcx float *dst
#; %rdx float *src
#; %r8 unsigned int nframes

	pushq %rbp
	movq %rsp, %rbp

	#; save the registers
	pushq %rbx #; must be preserved

	#; the real function

	#; if nframes == 0, go to end
	cmp	$0, %r8
	je	.MBNG_END

	#; Check for alignment

	movq %rcx, %rax
	andq $28, %rax #; mask alignment offset

	movq %rdx, %rbx
	andq $28, %rbx #; mask alignment offset

	cmp %rax, %rbx
	jne .MBNG_NONALIGN #; if not buffers are not aligned btween each other, calculate manually

	cmp $0, %rbx
	je .MBNG_AVX #; aligned at 32, rpoceed to AVX

	#; Pre-loop, we need to run 1-7 frames "manually" without
	#; AVX instructions

.MBNG_PRELOOP:

	movss (%rdx), %xmm0
	addss (%rcx), %xmm0
	movss %xmm0, (%rcx)

	addq $4, %rcx #; dst++
	addq $4, %rdx #; src++

	decq %r8 	  #; nframes--
	jz	.MBNG_END
	
	addq $4, %rbx #; one non-aligned byte less
	
	cmp $32, %rbx #; test if we've reached 32 byte alignment
	jne .MBNG_PRELOOP

.MBNG_AVX:

	cmp $8, %r8 #; if there are frames left, but less than 8
	jl .MBNG_NONALIGN #; we can't run AVX

.MBNG_AVXLOOP:

	vmovaps	(%rdx), %ymm0        #; source => xmm0
	vaddps	(%rcx), %ymm0, %ymm1 #; mix with destination
	vmovaps  %ymm1, (%rcx)       #; copy result to destination
	
	addq $32, %rcx #; dst+=8
	addq $32, %rdx #; src+=8

	subq $8, %r8 #; nframes-=8
	cmp $8, %r8
	jge .MBNG_AVXLOOP

	#; zero upper 128 bits of all ymm registers to proceed with SSE operations without penalties
	vzeroupper

	cmp $0, %r8
	je .MBNG_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-7 frames.
	
.MBNG_NONALIGN:
	#; not aligned!
	#; 

	movss (%rdx), %xmm0 #; src => xmm0
	addss (%rcx), %xmm0 #; xmm0 += dst
	movss %xmm0, (%rcx) #; xmm0 => dst
	
	addq $4, %rcx
	addq $4, %rdx
	
	decq %r8
	jnz .MBNG_NONALIGN

.MBNG_END:

	popq %rbx

	#; return
	leave
	ret


#; void x86_sse_avx_copy_vector (float *dst, float *src, unsigned int nframes);

.globl x86_sse_avx_copy_vector
	.def	x86_sse_avx_copy_vector; .scl    2;   .type   32;
.endef

x86_sse_avx_copy_vector:

#; due to Microsoft calling convention
#; %rcx float *dst
#; %rdx float *src
#; %r8 unsigned int nframes

	pushq %rbp
	movq %rsp, %rbp

	#; save the registers
	pushq %rbx #; must be preserved

	#; the real function

	#; if nframes == 0, go to end
	cmp	$0, %r8
	je	.CB_END

	#; Check for alignment

	movq %rcx, %rax
	andq $28, %rax #; mask alignment offset

	movq %rdx, %rbx
	andq $28, %rbx #; mask alignment offset

	cmp %rax, %rbx
	jne .CB_NONALIGN #; if not buffers are not aligned btween each other, calculate manually

	cmp $0, %rbx
	je .CB_AVX #; aligned at 32, rpoceed to AVX

	#; Pre-loop, we need to run 1-7 frames "manually" without
	#; AVX instructions

.CB_PRELOOP:

	movss (%rdx), %xmm0
	movss %xmm0, (%rcx)

	addq $4, %rcx #; dst++
	addq $4, %rdx #; src++

	decq %r8 	  #; nframes--
	jz	.CB_END
	
	addq $4, %rbx #; one non-aligned byte less
	
	cmp $32, %rbx #; test if we've reached 32 byte alignment
	jne .CB_PRELOOP

.CB_AVX:

	cmp $8, %r8 #; if there are frames left, but less than 8
	jl .CB_NONALIGN #; we can't run AVX

.CB_AVXLOOP:

	vmovaps	(%rdx), %ymm0        #; source => xmm0
	vmovaps  %ymm0, (%rcx)       #; copy result to destination
	
	addq $32, %rcx #; dst+=8
	addq $32, %rdx #; src+=8

	subq $8, %r8 #; nframes-=8
	cmp $8, %r8
	jge .CB_AVXLOOP

	#; zero upper 128 bits of all ymm registers to proceed with SSE operations without penalties
	vzeroupper

	cmp $0, %r8
	je .CB_END

	#; if there are remaining frames, the nonalign code will do nicely
	#; for the rest 1-7 frames.
	
.CB_NONALIGN:
	#; not aligned!
	#; 

	movss (%rdx), %xmm0 #; src => xmm0
	movss %xmm0, (%rcx) #; xmm0 => dst
	
	addq $4, %rcx
	addq $4, %rdx
	
	decq %r8
	jnz .CB_NONALIGN

.CB_END:

	popq %rbx

	#; return
	leave
	ret


#; void x86_sse_avx_apply_gain_to_buffer (float *buf, unsigned int nframes, float gain);

.globl x86_sse_avx_apply_gain_to_buffer
	.def	x86_sse_avx_apply_gain_to_buffer; .scl    2;   .type   32;
.endef

x86_sse_avx_apply_gain_to_buffer:

#; due to Microsoft calling convention
#; %rcx float 			*buf	32(%rbp)
#; %rdx unsigned int 	nframes
#; %xmm2 float			gain			avx specific register

	pushq %rbp
	movq %rsp, %rbp
	
	#; move current max to %xmm0 for convenience
	movss %xmm2, %xmm0

	#; the real function	

	#; if nframes == 0, go to end
	cmp	$0, %rdx
	je	.AG_END
	
	#; Check for alignment

	movq %rcx, %r8 #; buf => %rdx
	andq $28, %r8 #; check alignment with mask 11100
	jz	.AG_AVX #; if buffer IS aligned

	#; PRE-LOOP
	#; we iterate 1-7 times, doing normal x87 float comparison
	#; so we reach a 32 byte aligned "buf" (=%rdi) value

.AGLP_START:

	#; Load next value from the buffer into %xmm1
	movss (%rcx), %xmm1
	mulss %xmm0, %xmm1
	movss %xmm1, (%rcx)

	#; increment buffer, decrement counter
	addq $4, %rcx #; buf++;
	
	decq %rdx   #; nframes--
	jz	.AG_END #; if we run out of frames, we go to the end

	addq $4, %r8 #; one non-aligned byte less
	cmp $16, %r8
	jne .AGLP_START #; if more non-aligned frames exist, we do a do-over

.AG_AVX:

	#; We have reached the 32 byte aligned "buf" ("rcx") value
	#; use AVX instructions

	#; Figure out how many loops we should do
	movq %rdx, %rax #; copy remaining nframes to %rax for division

	shr $3, %rax #; unsigned divide by 8

	#; %rax = AVX iterations
	cmp $0, %rax
	je .AGPOST_START

	#; set up the gain buffer (gain is already in %xmm0)
	vshufps $0x00, %ymm0, %ymm0, %ymm0 #; spread single float value to the first 128 bits of ymm0 register
	vperm2f128 $0x00, %ymm0, %ymm0, %ymm0 #; extend the first 128 bits of ymm0 register to higher 128 bits

.AGLP_AVX:

	vmovaps (%rcx), %ymm1
	vmulps %ymm0, %ymm1, %ymm2
	vmovaps %ymm2, (%rcx)

	addq $32, %rcx  #; buf + 8
	subq $8, %rdx   #; nframes-=8

	decq %rax
	jnz .AGLP_AVX

	#; zero upper 128 bits of all ymm registers to proceed with SSE operations without penalties
	vzeroupper

	#; Next we need to post-process all remaining frames
	#; the remaining frame count is in %rcx
	cmpq $0, %rdx #;
	jz .AG_END

.AGPOST_START:

	movss (%rcx), %xmm1
	mulss %xmm0, %xmm1
	movss %xmm1, (%rcx)

	#; increment buffer, decrement counter
	addq $4, %rcx #; buf++;
	
	decq %rdx   #; nframes--
	jnz	.AGPOST_START #; if we run out of frames, we go to the end
	
.AG_END:

	#; return
	leave
	ret

#; end proc


#; float x86_sse_avx_compute_peak(float *buf, long nframes, float current);

.globl x86_sse_avx_compute_peak
	.def	x86_sse_avx_compute_peak; .scl    2;   .type   32;
.endef

x86_sse_avx_compute_peak:

#; due to Microsoft calling convention
#; %rcx float*          buf	32(%rbp)
#; %rdx unsigned int 	nframes
#; %xmm2 float			current

	pushq %rbp
	movq %rsp, %rbp

	#; move current max to %xmm0 for convenience
	movss %xmm2, %xmm0

	#; if nframes == 0, go to end
	cmp	$0, %rdx
	je	.CP_END

	#; create the "abs" mask in %xmm3
	#; if will be used to discard sign bit
	pushq   $2147483647
	movss	(%rsp), %xmm3
	addq    $8, %rsp

	#; Check for alignment 
	movq %rcx, %r8 #; buf => %rdx
	andq $28, %r8 #; mask bits 1 & 2
	jz	.CP_AVX #; if buffer IS aligned

	#; PRE-LOOP
	#; we iterate 1-7 times, doing normal x87 float comparison
	#; so we reach a 32 byte aligned "buf" (=%rcx) value

.LP_START:

	#; Load next value from the buffer
	movss (%rcx), %xmm1
	andps %xmm3, %xmm1	#; mask out sign bit
	maxss %xmm1, %xmm0

	#; increment buffer, decrement counter
	addq $4, %rcx #; buf++;

	decq %rdx   #; nframes--
	jz	.CP_END #; if we run out of frames, we go to the end

	addq $4, %r8 #; one non-aligned byte less
	cmp $32, %r8
	jne .LP_START #; if more non-aligned frames exist, we do a do-over

.CP_AVX:

	#; We have reached the 32 byte aligned "buf" ("rdi") value

	#; Figure out how many loops we should do
	movq %rdx, %rax #; copy remaining nframes to %rax for division

	shr $3, %rax #; unsigned divide by 8
	jz .POST_START

	#; %rax = AVX iterations

	#; current maximum is at %xmm0, but we need to broadcast it to the whole ymm0 register..
	vshufps $0x00, %ymm0, %ymm0, %ymm0 #; spread single float value to the all 128 bits of xmm0 register
	vperm2f128 $0x00, %ymm0, %ymm0, %ymm0 #; extend the first 128 bits of ymm0 register to higher 128 bits

	#; broadcast sign mask to the whole ymm3 register
	vshufps $0x00, %ymm3, %ymm3, %ymm3 #; spread single float value to the all 128 bits of xmm3 register
	vperm2f128 $0x00, %ymm3, %ymm3, %ymm3 #; extend the first 128 bits of ymm3 register to higher 128 bits

.LP_AVX:

	vmovaps (%rcx), %ymm1
	vandps %ymm3, %ymm1, %ymm1	#; mask out sign bit
	vmaxps %ymm1, %ymm0, %ymm0

	addq $32, %rcx #; buf+=8
	subq $8, %rdx #; nframes-=8

	decq %rax
	jnz .LP_AVX

	#; Calculate the maximum value contained in the 4 FP's in %ymm0
	vshufps $0x4e, %ymm0, %ymm0, %ymm1     #; shuffle left & right pairs (1234 => 3412) in each 128 bit half
	vmaxps  %ymm1, %ymm0, %ymm0            #; maximums of the four pairs, if each of 8 elements was unique, 4 unique elements left now
	vshufps $0xb1, %ymm0, %ymm0, %ymm1     #; shuffle the floats inside pairs (1234 => 2143) in each 128 bit half
	vmaxps  %ymm1, %ymm0, %ymm0			   #; maximums of the four pairs, we had up to 4 unique elements was unique, 2 unique elements left now
	vperm2f128 $0x01, %ymm0, %ymm0, %ymm1  #; swap 128 bit halfs
	vmaxps  %ymm1, %ymm0, %ymm0			   #; the result will be - all 8 elemens are maximums

	#; now every float in %ymm0 is the same value, current maximum value

	#; Next we need to post-process all remaining frames
	#; the remaining frame count is in %rcx
	
	#; zero upper 128 bits of all ymm registers to proceed with SSE operations without penalties
	vzeroupper

	#; if no remaining frames, jump to the end
	cmp $0, %rdx
	je .CP_END

.POST_START:

	movss (%rcx), %xmm1
	andps %xmm3, %xmm1	#; mask out sign bit
	maxss %xmm1, %xmm0
	
	addq $4, %rcx 	#; buf++;
	
	decq %rdx		#; nframes--;
	jnz .POST_START

.CP_END:

	#; return value is in xmm0

	#; return
	leave
	ret

#; end proc