#include "libpbd-config.h"

#define _XOPEN_SOURCE 600
#include <cstring> // for memset
#include <cstdlib>
#include <stdint.h>
#include <assert.h>

#include "pbd/fpu.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace PBD;
using namespace std;

FPU::FPU ()
{
	unsigned long cpuflags = 0;

	_flags = Flags (0);

#if !( (defined __x86_64__) || (defined __i386__) ) // !ARCH_X86
	return;
#else

	
#ifndef _LP64 //USE_X86_64_ASM
	asm volatile (
		"mov $1, %%eax\n"
		"pushl %%ebx\n"
		"cpuid\n"
		"movl %%edx, %0\n"
		"popl %%ebx\n"
		: "=r" (cpuflags)
		: 
		: "%eax", "%ecx", "%edx"
		);
	
#else
	
	/* asm notes: although we explicitly save&restore rbx, we must tell
	   gcc that ebx,rbx is clobbered so that it doesn't try to use it as an intermediate
	   register when storing rbx. gcc 4.3 didn't make this "mistake", but gcc 4.4
	   does, at least on x86_64.
	*/

	asm volatile (
		"pushq %%rbx\n"
		"movq $1, %%rax\n"
		"cpuid\n"
		"movq %%rdx, %0\n"
		"popq %%rbx\n"
		: "=r" (cpuflags)
		: 
		: "%rax", "%rbx", "%rcx", "%rdx"
		);

#endif /* USE_X86_64_ASM */

	if (cpuflags & (1<<25)) {
		_flags = Flags (_flags | (HasSSE|HasFlushToZero));
	}

	if (cpuflags & (1<<26)) {
		_flags = Flags (_flags | HasSSE2);
	}

	if (cpuflags & (1 << 24)) {
		
		char** fxbuf = 0;
		
		/* DAZ wasn't available in the first version of SSE. Since
		   setting a reserved bit in MXCSR causes a general protection
		   fault, we need to be able to check the availability of this
		   feature without causing problems. To do this, one needs to
		   set up a 512-byte area of memory to save the SSE state to,
		   using fxsave, and then one needs to inspect bytes 28 through
		   31 for the MXCSR_MASK value. If bit 6 is set, DAZ is
		   supported, otherwise, it isn't.
		*/
		
#ifdef NO_POSIX_MEMALIGN
		fxbuf = (char **) malloc (sizeof (char *));
		assert (fxbuf);
		*fxbuf = (char *) malloc (512);
		assert (*fxbuf);
#else
		posix_memalign ((void **) &fxbuf, 16, sizeof (char *));
		assert (fxbuf);
		posix_memalign ((void **) fxbuf, 16, 512);
		assert (*fxbuf);
#endif			
		
		memset (*fxbuf, 0, 512);
		
		asm volatile (
			"fxsave (%0)"
			:
			: "r" (*fxbuf)
			: "memory"
			);
		
		uint32_t mxcsr_mask = *((uint32_t*) &((*fxbuf)[28]));
		
		/* if the mask is zero, set its default value (from intel specs) */
		
		if (mxcsr_mask == 0) {
			mxcsr_mask = 0xffbf;
		}
		
		if (mxcsr_mask & (1<<6)) {
			_flags = Flags (_flags | HasDenormalsAreZero);
		} 
		
		free (*fxbuf);
		free (fxbuf);
	}
#endif
}			

FPU::~FPU ()
{
}
