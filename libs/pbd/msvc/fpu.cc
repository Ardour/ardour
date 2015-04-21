// Added by JE - 05-12-2009. Inline assembler instructions
// have been changed to Intel format and (in the case of
// cpuid) was replaced by the equivalent VC++ system call).

#if defined (COMPILER_MSVC) || defined (COMPILER_MINGW)

#define _XOPEN_SOURCE 600
#include <cstdlib>
#include <stdint.h>
#include <intrin.h>  // Added by JE - 05-12-2009
#include <assert.h>

#include <pbd/fpu.h>
#include <pbd/error.h>

#include "i18n.h"

using namespace PBD;
using namespace std;

FPU::FPU ()
{
	unsigned long cpuflags_ECX = 0;
	unsigned long cpuflags_EDX = 0;

	_flags = (Flags)0;

#ifndef USE_X86_64_ASM
	return;
#endif

	// Get CPU lfags using Microsof function
	// It works for both 64 and 32 bit systems
	// no need to use assembler for getting info from register, this function does this for us
	int cpuInfo[4];
	__cpuid (cpuInfo, 1);
	cpuflags_ECX = cpuInfo[2]; // flags from ECX register
	cpuflags_EDX = cpuInfo[3]; // flags from EDX register

	if (cpuflags_ECX & (1<<28)) {
		_flags = Flags (_flags | (HasAVX) );
	}

	if (cpuflags_EDX & (1<<25)) {
		_flags = Flags (_flags | (HasSSE|HasFlushToZero) );
	}

	if (cpuflags_EDX & (1<<26)) {
		_flags = Flags (_flags | HasSSE2);
	}

	if (cpuflags_EDX & (1 << 24)) {
		char* fxbuf = 0;

		// allocate alligned buffer
		fxbuf = (char*)_aligned_malloc(512, 16);

		// Verify that fxbuf is correctly aligned
		unsigned long long buf_addr = (unsigned long long)(void*)fxbuf;
		if ((0 == buf_addr) || (buf_addr % 16))
			error << _("cannot allocate 16 byte aligned buffer for h/w feature detection") << endmsg;
		else
		{
			memset(fxbuf, 0, 512); // Initialize the buffer !!! Added by JE - 12-12-2009

#if defined (COMPILER_MINGW)
			asm volatile (
				"fxsave (%0)"
				:
				: "r" (fxbuf)
				: "memory"
				);

#elif defined (COMPILER_MSVC)
			__asm {
				mov eax, fxbuf
				fxsave   [eax]
			};
#endif
			uint32_t mxcsr_mask = *((uint32_t*) &fxbuf[28]);

			/* if the mask is zero, set its default value (from intel specs) */

			if (mxcsr_mask == 0) {
				mxcsr_mask = 0xffbf;
			}

			if (mxcsr_mask & (1<<6)) {
				_flags = Flags (_flags | HasDenormalsAreZero);
			}

			_aligned_free (fxbuf);
		}
	}
}

FPU::~FPU ()
{
}

#else  // !COMPILER_MSVC
	const char* pbd_fpu = "original pbd/fpu.cc takes precedence over this file";
#endif // COMPILER_MSVC
