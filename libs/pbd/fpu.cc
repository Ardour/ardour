/*
 * Copyright (C) 2007-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include "libpbd-config.h"

#define _XOPEN_SOURCE 600
#include <cstring> // for memset
#include <cstdlib>
#include <stdint.h>
#include <assert.h>

#ifdef PLATFORM_WINDOWS
#include <intrin.h>
#endif

#ifdef ARM_NEON_SUPPORT
/* Needed for ARM NEON detection */
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

#include "pbd/compose.h"
#include "pbd/fpu.h"
#include "pbd/error.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace std;

FPU* FPU::_instance (0);

#if ( (defined __x86_64__) || (defined __i386__) || (defined _M_X64) || (defined _M_IX86) ) // ARCH_X86
#ifndef PLATFORM_WINDOWS

/* use __cpuid() as the name to match the MSVC/mingw intrinsic */

static void
__cpuid(int regs[4], int cpuid_leaf)
{
	asm volatile (
#if defined(__i386__)
			"pushl %%ebx;\n\t"
#endif
			"cpuid;\n\t"
			"movl %%eax, (%1);\n\t"
			"movl %%ebx, 4(%1);\n\t"
			"movl %%ecx, 8(%1);\n\t"
			"movl %%edx, 12(%1);\n\t"
#if defined(__i386__)
			"popl %%ebx;\n\t"
#endif
			:"=a" (cpuid_leaf) /* %eax clobbered by CPUID */
			:"S" (regs), "a" (cpuid_leaf)
			:
#if !defined(__i386__)
			"%ebx",
#endif
			"%ecx", "%edx", "memory");
}

#endif /* !PLATFORM_WINDOWS */

#ifndef HAVE_XGETBV // Allow definition by build system
	#if defined(__MINGW32__) && defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR >= 5
		#define HAVE_XGETBV
	#elif defined(_MSC_VER) && _MSC_VER >= 1600
		// '_xgetbv()' was only available from VC10 onwards
		#define HAVE_XGETBV
	#endif
#endif

#ifndef HAVE_XGETBV

#ifdef COMPILER_MSVC

// '_xgetbv()' was only available from VC10 onwards
__declspec(noinline) static uint64_t
_xgetbv (uint32_t xcr)
{
	return 0;

	// N.B.  The following would probably work for a pre-VC10 build,
	// although it might suffer from optimization issues.  We'd need
	// to place this function into its own (unoptimized) source file.
	__asm {
			 mov ecx, [xcr]
			 __asm _emit 0x0f __asm _emit 0x01 __asm _emit 0xd0 /*xgetbv*/
	}
}

#else

static uint64_t
_xgetbv (uint32_t xcr)
{
#ifdef __APPLE__
	/* it would be nice to make this work on OS X but as long we use veclib,
	   we don't really need to know about SSE/AVX on that platform.
	*/
	return 0;
#else
	uint32_t eax, edx;
	__asm__ volatile ("xgetbv" : "=a" (eax), "=d" (edx) : "c" (xcr));
	return (static_cast<uint64_t>(edx) << 32) | eax;
#endif
}

#endif /* !COMPILER_MSVC */
#endif /* !HAVE_XGETBV */
#endif /* ARCH_X86 */

#ifndef _XCR_XFEATURE_ENABLED_MASK
#define _XCR_XFEATURE_ENABLED_MASK 0
#endif

FPU*
FPU::instance()
{
	if (!_instance) {
		_instance = new FPU;
	}

	return _instance;
}

void
FPU::destroy ()
{
	delete _instance;
	_instance = 0;
}

FPU::FPU ()
	: _flags ((Flags) 0)
{
	if (_instance) {
		error << _("FPU object instantiated more than once") << endmsg;
	}

	if (getenv("ARDOUR_FPU_FLAGS")) {
		_flags = Flags (atoi (getenv("ARDOUR_FPU_FLAGS")));
		return;
	}

#ifdef ARM_NEON_SUPPORT
# ifdef __aarch64__
	/* all armv8+ features NEON used in arm_neon_functions.cc */
	_flags = Flags(_flags | HasNEON);
# elif defined __arm__
	if (getauxval(AT_HWCAP) & HWCAP_NEON) {
		_flags = Flags(_flags | HasNEON);
	}
# endif
#endif

#if !( (defined __x86_64__) || (defined __i386__) || (defined _M_X64) || (defined _M_IX86) ) // !ARCH_X86
	/* Non-Intel architecture, nothing to do here */
	return;
#else

	/* Get the CPU vendor just for kicks
	 *
	 * __cpuid with an InfoType argument of 0 returns the number of
	 * valid Ids in CPUInfo[0] and the CPU identification string in
	 * the other three array elements. The CPU identification string is
	 * not in linear order. The code below arranges the information
	 * in a human readable form. The human readable order is CPUInfo[1] |
	 * CPUInfo[3] | CPUInfo[2]. CPUInfo[2] and CPUInfo[3] are swapped
	 * before using memcpy to copy these three array elements to cpu_string.
	 */

	int cpu_info[4];
	char cpu_string[48];
	string cpu_vendor;

	__cpuid (cpu_info, 0);

	int num_ids = cpu_info[0];
	std::swap(cpu_info[2], cpu_info[3]);
	memcpy(cpu_string, &cpu_info[1], 3 * sizeof(cpu_info[1]));
	cpu_vendor.assign(cpu_string, 3 * sizeof(cpu_info[1]));

	info << string_compose (_("CPU vendor: %1"), cpu_vendor) << endmsg;

	if (num_ids > 0) {

		/* Now get CPU/FPU flags */

		__cpuid (cpu_info, 1);

		if ((cpu_info[2] & (1<<27)) /* OSXSAVE */ &&
		    (cpu_info[2] & (1<<28) /* AVX */) &&
		    ((_xgetbv (_XCR_XFEATURE_ENABLED_MASK) & 0x6) == 0x6)) { /* OS really supports XSAVE */
			info << _("AVX-capable processor") << endmsg;
			_flags = Flags (_flags | (HasAVX));
		}

		if (cpu_info[2] & (1<<12) /* FMA */) {
			info << _("AVX with FMA capable processor") << endmsg;
			_flags = Flags (_flags | (HasFMA));
		}

		if (cpu_info[3] & (1<<25)) {
			_flags = Flags (_flags | (HasSSE|HasFlushToZero));
		}

		if (cpu_info[3] & (1<<26)) {
			_flags = Flags (_flags | HasSSE2);
		}

		/* Figure out CPU/FPU denormal handling capabilities */

		if (cpu_info[3] & (1 << 24)) {

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
			char* buf = *fxbuf;
#ifdef _WIN64
			/* For 64-bit compilation, MSVC doesn't support inline assembly !!
			   ( https://docs.microsoft.com/en-us/cpp/assembler/inline/inline-assembler?view=msvc-160 ) */

			/* but instead, it uses something called 'x64 intrinsics'
			   1: ( https://docs.microsoft.com/en-us/cpp/intrinsics/x64-amd64-intrinsics-list?view=msvc-160 )
			   2: ( https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_fxsave ) */
			_fxsave (buf);
#else
			__asm {
				mov eax, buf
					fxsave   [eax]
					};
#endif
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

		/* finally get the CPU brand */

		__cpuid (cpu_info, 0x80000000);

		const int parameter_end = 0x80000004;
		string cpu_brand;

		if (cpu_info[0] >= parameter_end) {
			char* cpu_string_ptr = cpu_string;

			for (int parameter = 0x80000002; parameter <= parameter_end &&
				     cpu_string_ptr < &cpu_string[sizeof(cpu_string)]; parameter++) {
				__cpuid(cpu_info, parameter);
				memcpy(cpu_string_ptr, cpu_info, sizeof(cpu_info));
				cpu_string_ptr += sizeof(cpu_info);
			}
			cpu_brand.assign(cpu_string, cpu_string_ptr - cpu_string);
			info << string_compose (_("CPU brand: %1"), cpu_brand) << endmsg;
		}
	}
#endif /* !ARCH_X86 */
}

FPU::~FPU ()
{
}
