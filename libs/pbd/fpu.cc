#define _XOPEN_SOURCE 600
#include <cstdlib>
#include <stdint.h>

#include <pbd/fpu.h>
#include <pbd/error.h>

#include "i18n.h"

using namespace PBD;
using namespace std;

FPU::FPU ()
{
	unsigned long cpuflags = 0;

	_flags = Flags (0);

#ifndef ARCH_X86
	return;
#endif
	
#ifndef USE_X86_64_ASM
	asm volatile (
		"mov $1, %%eax\n"
		"pushl %%ebx\n"
		"cpuid\n"
		"movl %%edx, %0\n"
		"popl %%ebx\n"
		: "=r" (cpuflags)
		: 
		: "%eax", "%ecx", "%edx", "memory"
		);
	
#else
	
	asm volatile (
		"pushq %%rbx\n"
		"movq $1, %%rax\n"
		"cpuid\n"
		"movq %%rdx, %0\n"
		"popq %%rbx\n"
		: "=r" (cpuflags)
		: 
		: "%rax", "%rcx", "%rdx", "memory"
		);

#endif /* USE_X86_64_ASM */
	
	if (cpuflags & (1<<25)) {
		_flags = Flags (_flags | (HasSSE|HasFlushToZero));
	}

	if (cpuflags & (1<<26)) {
		_flags = Flags (_flags | HasSSE2);
	}

	if (cpuflags & (1 << 24)) {
		
		char* fxbuf = 0;
		
#ifdef NO_POSIX_MEMALIGN
		if ((fxbuf = (char *) malloc(512)) == 0)
#else
		if (posix_memalign ((void**)&fxbuf, 16, 512)) 
#endif			
		{
			error << _("cannot allocate 16 byte aligned buffer for h/w feature detection") << endmsg;
		} else {
			
			asm volatile (
				"fxsave (%0)"
				:
				: "r" (fxbuf)
				: "memory"
				);
			
			uint32_t mxcsr_mask = *((uint32_t*) &fxbuf[28]);
			
			/* if the mask is zero, set its default value (from intel specs) */
			
			if (mxcsr_mask == 0) {
				mxcsr_mask = 0xffbf;
			}
			
			if (mxcsr_mask & (1<<6)) {
				_flags = Flags (_flags | HasDenormalsAreZero);
			} 

			free (fxbuf);
		}
	}
}			

FPU::~FPU ()
{
}
