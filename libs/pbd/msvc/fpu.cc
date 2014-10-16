#ifdef COMPILER_MSVC  // Added by JE - 05-12-2009. Inline assembler instructions
                      // have been changed to Intel format and (in the case of
                      // cpuid) was replaced by the equivalent VC++ system call).
#define _XOPEN_SOURCE 600
#include <cstdlib>
#include <stdint.h>
#include <intrin.h>  // Added by JE - 05-12-2009

#include <pbd/fpu.h>
#include <pbd/error.h>

#include "i18n.h"

using namespace PBD;
using namespace std;

FPU::FPU ()
{
	unsigned long cpuflags = 0;

	_flags = (Flags)0;

#ifndef ARCH_X86
	return;

#else

#ifndef USE_X86_64_ASM
int cpuInfo[4];

	__cpuid (cpuInfo, 1);
	cpuflags = cpuInfo[3];
/*
	__asm {  // This is how the original section would look if converted to Intel syntax.
             // However, I have grave doubts about whether it's doing the right thing.
             // It seems as if the intention was to retrieve feature information from
             // the processor. However, feature information is returned in the ebx register
             // (if you believe Wikipedia) or in edx (if you believe Microsoft). Unfortunately,
             // both registers get ignored in the original code!! Confused?? Join the club!!
		mov   eax, 1
		push  ebx
		cpuid
		mov   edx, 0
		pop   ebx
		mov   cpuflags, ecx // This can't be right, surely???
	}; */
#else
// Note that this syntax is currently still in AT&T format !
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
		bool  aligned_malloc = false; // Added by JE - 05-12-2009
		char* fxbuf = 0;
// This section changed by JE - 05-12-2009
#ifdef NO_POSIX_MEMALIGN
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)       // All of these support '_aligned_malloc()'
		fxbuf = (char *) _aligned_malloc(512, 16);  // (note that they all need at least MSVC runtime 7.0)
		aligned_malloc = true;
#else
		fxbuf = (char *) malloc(512);
#endif
#else
		fxbuf = posix_memalign ((void**)&fxbuf, 16, 512);
#endif
		// Verify that fxbuf is correctly aligned
		unsigned long buf_addr = (unsigned long)(void*)fxbuf;
		if ((0 == buf_addr) || (buf_addr % 16))
			error << _("cannot allocate 16 byte aligned buffer for h/w feature detection") << endmsg;
		else
		{
			memset(fxbuf, 0, 512); // Initialize the buffer !!! Added by JE - 12-12-2009

			__asm {
				mov eax, fxbuf
				fxsave   [eax]
			};

			uint32_t mxcsr_mask = *((uint32_t*) &fxbuf[28]);

			/* if the mask is zero, set its default value (from intel specs) */

			if (mxcsr_mask == 0) {
				mxcsr_mask = 0xffbf;
			}

			if (mxcsr_mask & (1<<6)) {
				_flags = Flags (_flags | HasDenormalsAreZero);
			}

			if (aligned_malloc)
				_aligned_free (fxbuf);
			else
				free (fxbuf);
		}
	}
#endif  // ARCH_X86
}

FPU::~FPU ()
{
}

#else  // !COMPILER_MSVC
	const char* pbd_fpu = "original pbd/fpu.cc takes precedence over this file";
#endif // COMPILER_MSVC
