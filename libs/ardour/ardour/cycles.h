/*
    Copyright (C) 2001 Paul Davis
    Code derived from various headers from the Linux kernel

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

#ifndef __ardour_cycles_h__
#define __ardour_cycles_h__

#include <stdint.h>

#if defined(__i386__) || defined(__x86_64__)

/*
 * Standard way to access the cycle counter on i586+ CPUs.
 * Currently only used on SMP.
 *
 * If you really have a SMP machine with i486 chips or older,
 * compile for that, and this will just always return zero.
 * That's ok, it just means that the nicer scheduling heuristics
 * won't work for you.
 *
 * We only use the low 32 bits, and we'd simply better make sure
 * that we reschedule before that wraps. Scheduling at least every
 * four billion cycles just basically sounds like a good idea,
 * regardless of how fast the machine is.
 */
typedef uint64_t cycles_t;

extern cycles_t cacheflush_time;

#if defined(__x86_64__)

#define rdtscll(lo, hi)						\
	__asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi))

static inline cycles_t get_cycles (void)
{
	cycles_t lo, hi;

	rdtscll(lo, hi);
	return lo;
}

#else

#define rdtscll(val)				\
__asm__ __volatile__("rdtsc" : "=A" (val))

static inline cycles_t get_cycles (void)
{
	cycles_t ret;

	rdtscll(ret);
	return ret & 0xffffffff;
}
#endif

#elif defined(__powerpc__)

#define CPU_FTR_601			0x00000100

typedef uint32_t cycles_t;

/*
 * For the "cycle" counter we use the timebase lower half.
 * Currently only used on SMP.
 */

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles(void)
{
	cycles_t ret = 0;

	__asm__ __volatile__(
		"98:	mftb %0\n"
		"99:\n"
		".section __ftr_fixup,\"a\"\n"
		"	.long %1\n"
		"	.long 0\n"
		"	.long 98b\n"
		"	.long 99b\n"
		".previous"
		: "=r" (ret) : "i" (CPU_FTR_601));
	return ret;
}

#elif defined(__ia64__)
/* ia64 */

typedef uint32_t cycles_t;
static inline cycles_t
get_cycles (void)
{
	cycles_t ret;
	__asm__ __volatile__ ("mov %0=ar.itc" : "=r"(ret));
	return ret;
}

#elif defined(__alpha__)
/* alpha */

/*
 * Standard way to access the cycle counter.
 * Currently only used on SMP for scheduling.
 *
 * Only the low 32 bits are available as a continuously counting entity.
 * But this only means we'll force a reschedule every 8 seconds or so,
 * which isn't an evil thing.
 */

typedef uint32_t cycles_t;
static inline cycles_t get_cycles (void)
{
	cycles_t ret;
	__asm__ __volatile__ ("rpcc %0" : "=r"(ret));
	return ret;
}

#elif defined(__s390__)
/* s390 */

typedef uint32_t long cycles_t;
static inline cycles_t get_cycles(void)
{
	cycles_t cycles;
	__asm__("stck 0(%0)" : : "a" (&(cycles)) : "memory", "cc");
	return cycles >> 2;
}

#elif defined(__hppa__)
/* hppa/parisc */

#define mfctl(reg)      ({              \
	uint32_t cr;               \
	__asm__ __volatile__(           \
	        "mfctl " #reg ",%0" :   \
	         "=r" (cr)              \
	);                              \
	cr;                             \
})

typedef uint32_t cycles_t;
static inline cycles_t get_cycles (void)
{
	return mfctl(16);
}

#elif defined(__mips__)
/* mips/mipsel */

/*
 * Standard way to access the cycle counter.
 * Currently only used on SMP for scheduling.
 *
 * Only the low 32 bits are available as a continuously counting entity.
 * But this only means we'll force a reschedule every 8 seconds or so,
 * which isn't an evil thing.
 *
 * We know that all SMP capable CPUs have cycle counters.
 */

#define __read_32bit_c0_register(source, sel)               \
({ int __res;                                               \
	if (sel == 0)                                           \
		__asm__ __volatile__(                               \
			"mfc0\t%0, " #source "\n\t"                     \
			: "=r" (__res));                                \
	else                                                    \
		__asm__ __volatile__(                               \
			".set\tmips32\n\t"                              \
			"mfc0\t%0, " #source ", " #sel "\n\t"           \
			".set\tmips0\n\t"                               \
			: "=r" (__res));                                \
	__res;                                                  \
})

/* #define CP0_COUNT $9 */
#define read_c0_count()         __read_32bit_c0_register($9, 0)

typedef uint32_t cycles_t;
static inline cycles_t get_cycles (void)
{
	return read_c0_count();
}

/* begin mach */
#elif defined(__APPLE__)

#include <CoreAudio/HostTime.h>

typedef UInt64 cycles_t;
static inline cycles_t get_cycles (void)
{
	UInt64 time = AudioGetCurrentHostTime();
	return AudioConvertHostTimeToNanos(time);
}
/* end mach  */

#else

/* debian: sparc, arm, m68k */

#ifndef COMPILER_MSVC
/* GRRR... Annoyingly, #warning aborts the compilation for MSVC !!  */
#warning You are compiling libardour on a platform for which ardour/cycles.h needs work
#endif

#include <sys/time.h>

typedef long cycles_t;

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles(void)
{
	struct timeval tv;
	gettimeofday (&tv, NULL);

	return tv.tv_usec;
}

#endif

#endif /* __ardour_cycles_h__ */
