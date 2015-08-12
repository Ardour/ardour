/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "libpbd-config.h"

#define _XOPEN_SOURCE 600
#include <cstring> // for memset
#include <cstdlib>
#include <stdint.h>
#include <assert.h>

#ifdef PLATFORM_WINDOWS
#include <intrin.h>
#endif

#include "pbd/fpu.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace PBD;
using namespace std;

FPU::FPU ()
{
	unsigned long cpuflags = 0;

	_flags = Flags (0);

#if !( (defined __x86_64__) || (defined __i386__) || (defined _M_X64) || (defined _M_IX86) ) // !ARCH_X86
	return;
#else

#ifdef PLATFORM_WINDOWS

	// Get CPU flags using Microsoft function
	// It works for both 64 and 32 bit systems
	// no need to use assembler for getting info from register, this function does this for us
	int cpuInfo[4];
	__cpuid (cpuInfo, 1);
	cpuflags = cpuInfo[3];

#else	

#ifndef _LP64 /* *nix; 32 bit version. This odd macro constant is required because we need something that identifies this as a 32 bit
                 build on Linux and on OS X. Anything that serves this purpose will do, but this is the best thing we've identified
                 so far.
              */
	
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
	
#else /* *nix; 64 bit version */
	
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

#endif /* _LP64 */
#endif /* PLATFORM_WINDOWS */

#ifndef __APPLE__
	/* must check for both AVX and OSXSAVE support in cpuflags before
	 * attempting to use AVX related instructions.
	 */
	if ((cpuflags & (1<<27)) /* AVX */ && (cpuflags & (1<<28) /* (OS)XSAVE */)) {

		std::cerr << "Looks like AVX\n";
		
		/* now check if YMM resters state is saved: which means OS does
		 * know about new YMM registers and saves them during context
		 * switches it's true for most cases, but we must be sure
		 *
		 * giving 0 as the argument to _xgetbv() fetches the 
		 * XCR_XFEATURE_ENABLED_MASK, which we need to check for 
		 * the 2nd and 3rd bits, indicating correct register save/restore.
		 */

		uint64_t xcrFeatureMask = 0;

#if __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 4
		unsigned int eax, edx, index = 0;
		asm volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
		xcrFeatureMask = ((unsigned long long)edx << 32) | eax;
#elif defined (COMPILER_MSVC)
		xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
#endif
		if (xcrFeatureMask & 0x6) {
			std::cerr << "Definitely AVX\n";
			_flags = Flags (_flags | (HasAVX) );
		}
	}
#endif /* !__APPLE__ */ 

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

#ifndef HAVE_POSIX_MEMALIGN
#  ifdef PLATFORM_WINDOWS
		fxbuf = (char **) _aligned_malloc (sizeof (char *), 16);
		assert (fxbuf);
		*fxbuf = (char *) _aligned_malloc (512, 16);
		assert (*fxbuf);
#  else
#  warning using default malloc for aligned memory
		fxbuf = (char **) malloc (sizeof (char *));
		assert (fxbuf);
		*fxbuf = (char *) malloc (512);
		assert (*fxbuf);
#  endif
#else
		(void) posix_memalign ((void **) &fxbuf, 16, sizeof (char *));
		assert (fxbuf);
		(void) posix_memalign ((void **) fxbuf, 16, 512);
		assert (*fxbuf);
#endif			
		
		memset (*fxbuf, 0, 512);
		
#ifdef COMPILER_MSVC
		char *buf = *fxbuf;
		__asm {
			mov eax, buf
			fxsave   [eax]
		};
#else
		asm volatile (
			"fxsave (%0)"
			:
			: "r" (*fxbuf)
			: "memory"
			);
#endif
		
		uint32_t mxcsr_mask = *((uint32_t*) &((*fxbuf)[28]));
		
		/* if the mask is zero, set its default value (from intel specs) */
		
		if (mxcsr_mask == 0) {
			mxcsr_mask = 0xffbf;
		}
		
		if (mxcsr_mask & (1<<6)) {
			_flags = Flags (_flags | HasDenormalsAreZero);
		} 
		
#if !defined HAVE_POSIX_MEMALIGN && defined PLATFORM_WINDOWS
		_aligned_free (*fxbuf);
		_aligned_free (fxbuf);
#else
		free (*fxbuf);
		free (fxbuf);
#endif
	}
#endif
}			

FPU::~FPU ()
{
}
