/*
    Copyright (C) 2001 Paul Davis and others (see below)
    Code derived from various headers from the Linux kernel.
       Copyright attributions maintained where present.
    
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

    $Id$
*/

#ifndef __libpbd_atomic_h__
#define __libpbd_atomic_h__

#ifdef HAVE_SMP      /* a macro we control, to manage ... */
#define CONFIG_SMP   /* ... the macro the kernel headers use */
#endif

#if defined(__powerpc__) || defined(__ppc__)

/*
 * BK Id: SCCS/s.atomic.h 1.15 10/28/01 10:37:22 trini
 */
/*
 * PowerPC atomic operations
 */

#ifndef _ASM_PPC_ATOMIC_H_ 
#define _ASM_PPC_ATOMIC_H_

typedef struct { volatile int counter; } atomic_t;


#define ATOMIC_INIT(i)	{ (i) }

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		(((v)->counter) = (i))

extern void atomic_clear_mask(unsigned long mask, unsigned long *addr);
extern void atomic_set_mask(unsigned long mask, unsigned long *addr);

#ifdef CONFIG_SMP
#define SMP_ISYNC	"\n\tisync"
#else
#define SMP_ISYNC
#endif

static __inline__ void atomic_add(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%3\n\
	add	%0,%2,%0\n\
	stwcx.	%0,0,%3\n\
	bne-	1b"
	: "=&r" (t), "=m" (v->counter)
	: "r" (a), "r" (&v->counter), "m" (v->counter)
	: "cc");
}

static __inline__ int atomic_add_return(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%2\n\
	add	%0,%1,%0\n\
	stwcx.	%0,0,%2\n\
	bne-	1b"
	SMP_ISYNC
	: "=&r" (t)
	: "r" (a), "r" (&v->counter)
	: "cc", "memory");

	return t;
}

static __inline__ void atomic_sub(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%3\n\
	subf	%0,%2,%0\n\
	stwcx.	%0,0,%3\n\
	bne-	1b"
	: "=&r" (t), "=m" (v->counter)
	: "r" (a), "r" (&v->counter), "m" (v->counter)
	: "cc");
}

static __inline__ int atomic_sub_return(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%2\n\
	subf	%0,%1,%0\n\
	stwcx.	%0,0,%2\n\
	bne-	1b"
	SMP_ISYNC
	: "=&r" (t)
	: "r" (a), "r" (&v->counter)
	: "cc", "memory");

	return t;
}

static __inline__ void atomic_inc(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc");
}

static __inline__ int atomic_inc_return(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%1\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%1\n\
	bne-	1b"
	SMP_ISYNC
	: "=&r" (t)
	: "r" (&v->counter)
	: "cc", "memory");

	return t;
}

static __inline__ void atomic_dec(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,-1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc");
}

static __inline__ int atomic_dec_return(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%1\n\
	addic	%0,%0,-1\n\
	stwcx.	%0,0,%1\n\
	bne-	1b"
	SMP_ISYNC
	: "=&r" (t)
	: "r" (&v->counter)
	: "cc", "memory");

	return t;
}

#define atomic_sub_and_test(a, v)	(atomic_sub_return((a), (v)) == 0)
#define atomic_dec_and_test(v)		(atomic_dec_return((v)) == 0)

/*
 * Atomically test *v and decrement if it is greater than 0.
 * The function returns the old value of *v minus 1.
 */
static __inline__ int atomic_dec_if_positive(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%1\n\
	addic.	%0,%0,-1\n\
	blt-	2f\n\
	stwcx.	%0,0,%1\n\
	bne-	1b"
	SMP_ISYNC
	"\n\
2:"	: "=&r" (t)
	: "r" (&v->counter)
	: "cc", "memory");

	return t;
}

#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__after_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_inc()	smp_mb()

#endif /* _ASM_PPC_ATOMIC_H_ */

/***********************************************************************/

#  else  /* !PPC */

#if defined(__i386__) || defined(__x86_64__)

#ifndef __ARCH_I386_ATOMIC__
#define __ARCH_I386_ATOMIC__

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

#ifdef CONFIG_SMP
#define SMP_LOCK "lock ; "
#else
#define SMP_LOCK ""
#endif

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)	{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically reads the value of @v.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
#define atomic_read(v)		((v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 * 
 * Atomically sets the value of @v to @i.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
#define atomic_set(v,i)		(((v)->counter) = (i))

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 * 
 * Atomically adds @i to @v.  Note that the guaranteed useful range
 * of an atomic_t is only 24 bits.
 */
static __inline__ void atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__(
		SMP_LOCK "addl %1,%0"
		:"=m" (v->counter)
		:"ir" (i), "m" (v->counter));
}

/**
 * atomic_sub - subtract the atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
static __inline__ void atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__(
		SMP_LOCK "subl %1,%0"
		:"=m" (v->counter)
		:"ir" (i), "m" (v->counter));
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
static __inline__ int atomic_sub_and_test(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		SMP_LOCK "subl %2,%0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"ir" (i), "m" (v->counter) : "memory");
	return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
static __inline__ void atomic_inc(atomic_t *v)
{
	__asm__ __volatile__(
		SMP_LOCK "incl %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
static __inline__ void atomic_dec(atomic_t *v)
{
	__asm__ __volatile__(
		SMP_LOCK "decl %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
static __inline__ int atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		SMP_LOCK "decl %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return c != 0;
}

/**
 * atomic_inc_and_test - increment and test 
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
static __inline__ int atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		SMP_LOCK "incl %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return c != 0;
}

/**
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 * 
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */ 
static __inline__ int atomic_add_negative(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		SMP_LOCK "addl %2,%0; sets %1"
		:"=m" (v->counter), "=qm" (c)
		:"ir" (i), "m" (v->counter) : "memory");
	return c;
}

/* These are x86-specific, used by some header files */
#define atomic_clear_mask(mask, addr) \
__asm__ __volatile__(SMP_LOCK "andl %0,%1" \
: : "r" (~(mask)),"m" (*addr) : "memory")

#define atomic_set_mask(mask, addr) \
__asm__ __volatile__(SMP_LOCK "orl %0,%1" \
: : "r" (mask),"m" (*addr) : "memory")

/* Atomic operations are already serializing on x86 */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif /* __ARCH_I386_ATOMIC__ */

/***********************************************************************/

#else /* !PPC && !i386 */

#ifdef __sparc__

/* atomic.h: These still suck, but the I-cache hit rate is higher.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 2000 Anton Blanchard (anton@linuxcare.com.au)
 */

#ifndef __ARCH_SPARC_ATOMIC__
#define __ARCH_SPARC_ATOMIC__

typedef struct { volatile int counter; } atomic_t;

#ifndef CONFIG_SMP

#define ATOMIC_INIT(i)  { (i) }
#define atomic_read(v)          ((v)->counter)
#define atomic_set(v, i)        (((v)->counter) = i)

#else
/* We do the bulk of the actual work out of line in two common
 * routines in assembler, see arch/sparc/lib/atomic.S for the
 * "fun" details.
 *
 * For SMP the trick is you embed the spin lock byte within
 * the word, use the low byte so signedness is easily retained
 * via a quick arithmetic shift.  It looks like this:
 *
 *	----------------------------------------
 *	| signed 24-bit counter value |  lock  |  atomic_t
 *	----------------------------------------
 *	 31                          8 7      0
 */

#define ATOMIC_INIT(i)	{ (i << 8) }

static __inline__ int atomic_read(atomic_t *v)
{
	int ret = v->counter;

	while(ret & 0xff)
		ret = v->counter;

	return ret >> 8;
}

#define atomic_set(v, i)	(((v)->counter) = ((i) << 8))
#endif

static __inline__ int __atomic_add(int i, atomic_t *v)
{
	register volatile int *ptr asm("g1");
	register int increment asm("g2");

	ptr = &v->counter;
	increment = i;

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___atomic_add\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (increment)
	: "0" (increment), "r" (ptr)
	: "g3", "g4", "g7", "memory", "cc");

	return increment;
}

static __inline__ int __atomic_sub(int i, atomic_t *v)
{
	register volatile int *ptr asm("g1");
	register int increment asm("g2");

	ptr = &v->counter;
	increment = i;

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___atomic_sub\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (increment)
	: "0" (increment), "r" (ptr)
	: "g3", "g4", "g7", "memory", "cc");

	return increment;
}

#define atomic_add(i, v) ((void)__atomic_add((i), (v)))
#define atomic_sub(i, v) ((void)__atomic_sub((i), (v)))

#define atomic_dec_return(v) __atomic_sub(1, (v))
#define atomic_inc_return(v) __atomic_add(1, (v))

#define atomic_sub_and_test(i, v) (__atomic_sub((i), (v)) == 0)
#define atomic_dec_and_test(v) (__atomic_sub(1, (v)) == 0)

#define atomic_inc(v) ((void)__atomic_add(1, (v)))
#define atomic_dec(v) ((void)__atomic_sub(1, (v)))

#define atomic_add_negative(i, v) (__atomic_add((i), (v)) < 0)

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()


#endif /* !(__ARCH_SPARC_ATOMIC__) */

/***********************************************************************/

#else

#ifdef __ia64__

#ifndef __ARCH_IA64_ATOMIC__
#define __ARCH_IA64_ATOMIC__

typedef volatile int atomic_t;

inline
int
atomic_read (const atomic_t * a)
{
	return *a;
}

inline
void
atomic_set(atomic_t *a, int v)
{
	*a = v;
}

inline
void
atomic_inc (atomic_t *v)
{
	int old, r;

	do {
		old = atomic_read(v);
		__asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO" (old));
		__asm__ __volatile__ ("cmpxchg4.acq %0=[%1],%2,ar.ccv"
				      : "=r"(r) : "r"(v), "r"(old + 1)
				      : "memory");
	} while (r != old);
}

inline
void
atomic_dec (atomic_t *v)
{
	int old, r;

	do {
		old = atomic_read(v);
		__asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO" (old));
		__asm__ __volatile__ ("cmpxchg4.acq %0=[%1],%2,ar.ccv"
				      : "=r"(r) : "r"(v), "r"(old - 1)
				      : "memory");
	} while (r != old);
}

inline
int
atomic_dec_and_test (atomic_t *v)
{
	int old, r;

	do {
		old = atomic_read(v);
		__asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO" (old));
		__asm__ __volatile__ ("cmpxchg4.acq %0=[%1],%2,ar.ccv"
				      : "=r"(r) : "r"(v), "r"(old - 1)
				      : "memory");
	} while (r != old);
	return old != 1;
}

#endif /* !(__ARCH_IA64_ATOMIC__) */

#else

#ifdef __alpha__

#ifndef _ALPHA_ATOMIC_H
#define _ALPHA_ATOMIC_H

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc...
 *
 * But use these as seldom as possible since they are much slower
 * than regular operations.
 */


/*
 * Counter is volatile to make sure gcc doesn't try to be clever
 * and move things around on us. We need to use _exactly_ the address
 * the user gave us, not some alias that contains the same information.
 */
typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)	( (atomic_t) { (i) } )

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		((v)->counter = (i))

/*
 * To get proper branch prediction for the main line, we must branch
 * forward to code at the end of this object's .text section, then
 * branch back to restart the operation.
 */

static __inline__ void atomic_add(int i, atomic_t * v)
{
	unsigned long temp;
	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	addl %0,%2,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,2f\n"
	".subsection 2\n"
	"2:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (v->counter)
	:"Ir" (i), "m" (v->counter));
}

static __inline__ void atomic_sub(int i, atomic_t * v)
{
	unsigned long temp;
	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	subl %0,%2,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,2f\n"
	".subsection 2\n"
	"2:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (v->counter)
	:"Ir" (i), "m" (v->counter));
}

/*
 * Same as above, but return the result value
 */
static __inline__ long atomic_add_return(int i, atomic_t * v)
{
	long temp, result;
	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	addl %0,%3,%2\n"
	"	addl %0,%3,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,2f\n"
	"	mb\n"
	".subsection 2\n"
	"2:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (v->counter), "=&r" (result)
	:"Ir" (i), "m" (v->counter) : "memory");
	return result;
}

static __inline__ long atomic_sub_return(int i, atomic_t * v)
{
	long temp, result;
	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	subl %0,%3,%2\n"
	"	subl %0,%3,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,2f\n"
	"	mb\n"
	".subsection 2\n"
	"2:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (v->counter), "=&r" (result)
	:"Ir" (i), "m" (v->counter) : "memory");
	return result;
}

#define atomic_dec_return(v) atomic_sub_return(1,(v))
#define atomic_inc_return(v) atomic_add_return(1,(v))

#define atomic_sub_and_test(i,v) (atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v) (atomic_sub_return(1, (v)) == 0)

#define atomic_inc(v) atomic_add(1,(v))
#define atomic_dec(v) atomic_sub(1,(v))

#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__after_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_inc()	smp_mb()

#endif /* _ALPHA_ATOMIC_H */

#else

#ifdef __s390__

#ifndef __ARCH_S390_ATOMIC__
#define __ARCH_S390_ATOMIC__

/*
 *  include/asm-s390/atomic.h
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Denis Joseph Barrow
 *
 *  Derived from "include/asm-i386/bitops.h"
 *    Copyright (C) 1992, Linus Torvalds
 *
 */

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 * S390 uses 'Compare And Swap' for atomicity in SMP enviroment
 */

typedef struct { volatile int counter; } __attribute__ ((aligned (4))) atomic_t;
#define ATOMIC_INIT(i)  { (i) }

#define atomic_eieio()          __asm__ __volatile__ ("BCR 15,0")

#define __CS_LOOP(old_val, new_val, ptr, op_val, op_string)		\
        __asm__ __volatile__("   l     %0,0(%2)\n"			\
                             "0: lr    %1,%0\n"				\
                             op_string "  %1,%3\n"			\
                             "   cs    %0,%1,0(%2)\n"			\
                             "   jl    0b"				\
                             : "=&d" (old_val), "=&d" (new_val)		\
			     : "a" (ptr), "d" (op_val) : "cc" );

#define atomic_read(v)          ((v)->counter)
#define atomic_set(v,i)         (((v)->counter) = (i))

static __inline__ void atomic_add(int i, atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, i, "ar");
}

static __inline__ int atomic_add_return (int i, atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, i, "ar");
	return new_val;
}

static __inline__ int atomic_add_negative(int i, atomic_t *v)
{
	int old_val, new_val;
        __CS_LOOP(old_val, new_val, v, i, "ar");
        return new_val < 0;
}

static __inline__ void atomic_sub(int i, atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, i, "sr");
}

static __inline__ void atomic_inc(volatile atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, 1, "ar");
}

static __inline__ int atomic_inc_return(volatile atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, 1, "ar");
        return new_val;
}

static __inline__ int atomic_inc_and_test(volatile atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, 1, "ar");
	return new_val != 0;
}

static __inline__ void atomic_dec(volatile atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, 1, "sr");
}

static __inline__ int atomic_dec_return(volatile atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, 1, "sr");
        return new_val;
}

static __inline__ int atomic_dec_and_test(volatile atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, 1, "sr");
        return new_val == 0;
}

static __inline__ void atomic_clear_mask(unsigned long mask, atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, ~mask, "nr");
}

static __inline__ void atomic_set_mask(unsigned long mask, atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, mask, "or");
}

/*
  returns 0  if expected_oldval==value in *v ( swap was successful )
  returns 1  if unsuccessful.
*/
static __inline__ int
atomic_compare_and_swap(int expected_oldval,int new_val,atomic_t *v)
{
        int retval;

        __asm__ __volatile__(
                "  lr   0,%2\n"
                "  cs   0,%3,0(%1)\n"
                "  ipm  %0\n"
                "  srl  %0,28\n"
                "0:"
                : "=&d" (retval)
                : "a" (v), "d" (expected_oldval) , "d" (new_val)
                : "0", "cc");
        return retval;
}

/*
  Spin till *v = expected_oldval then swap with newval.
 */
static __inline__ void
atomic_compare_and_swap_spin(int expected_oldval,int new_val,atomic_t *v)
{
        __asm__ __volatile__(
                "0: lr  0,%1\n"
                "   cs  0,%2,0(%0)\n"
                "   jl  0b\n"
                : : "a" (v), "d" (expected_oldval) , "d" (new_val)
                : "cc", "0" );
}

#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__after_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_inc()	smp_mb()

#endif                                 /* __ARCH_S390_ATOMIC __            */

#else

#ifdef __mips__

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 *
 * But use these as seldom as possible since they are much more slower
 * than regular operations.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 2000 by Ralf Baechle
 */
#ifndef __ASM_ATOMIC_H
#define __ASM_ATOMIC_H

typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)    { (i) }

/*
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_read(v)	((v)->counter)

/*
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_set(v,i)	((v)->counter = (i))

/*
 * ... while for MIPS II and better we can use ll/sc instruction.  This
 * implementation is SMP safe ...
 */

/*
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v.  Note that the guaranteed useful range
 * of an atomic_t is only 24 bits.
 */
extern __inline__ void atomic_add(int i, atomic_t * v)
{
	unsigned long temp;

	__asm__ __volatile__(
		".set push                # atomic_add\n"
		".set mips2                           \n"
		"1:   ll      %0, %1                  \n"
		"     addu    %0, %2                  \n"
		"     sc      %0, %1                  \n"
		"     beqz    %0, 1b                  \n"
		".set pop                             \n"
		: "=&r" (temp), "=m" (v->counter)
		: "Ir" (i), "m" (v->counter));
}

/*
 * atomic_sub - subtract the atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
extern __inline__ void atomic_sub(int i, atomic_t * v)
{
	unsigned long temp;

	__asm__ __volatile__(
		".set push                # atomic_sub\n"
		".set mips2                           \n"
		"1:   ll      %0, %1                  \n"
		"     subu    %0, %2                  \n"
		"     sc      %0, %1                  \n"
		"     beqz    %0, 1b                  \n"
		".set pop                             \n"
		: "=&r" (temp), "=m" (v->counter)
		: "Ir" (i), "m" (v->counter));
}

/*
 * Same as above, but return the result value
 */
extern __inline__ int atomic_add_return(int i, atomic_t * v)
{
	unsigned long temp, result;

	__asm__ __volatile__(
		".set push               # atomic_add_return\n"
		".set mips2                                 \n"
		".set noreorder                             \n"
		"1:   ll      %1, %2                        \n"
		"     addu    %0, %1, %3                    \n"
		"     sc      %0, %2                        \n"
		"     beqz    %0, 1b                        \n"
		"     addu    %0, %1, %3                    \n"
		"     sync                                  \n"
		".set pop                                   \n"
		: "=&r" (result), "=&r" (temp), "=m" (v->counter)
		: "Ir" (i), "m" (v->counter)
		: "memory");

	return result;
}

extern __inline__ int atomic_sub_return(int i, atomic_t * v)
{
	unsigned long temp, result;

	__asm__ __volatile__(
		".set push                # atomic_sub_return\n"
		".set mips2                                  \n"
		".set noreorder                              \n"
		"1:   ll    %1, %2                           \n"
		"     subu  %0, %1, %3                       \n"
		"     sc    %0, %2                           \n"
		"     beqz  %0, 1b                           \n"
		"     subu  %0, %1, %3                       \n"
		"     sync                                   \n"
		".set pop                                    \n"
		: "=&r" (result), "=&r" (temp), "=m" (v->counter)
		: "Ir" (i), "m" (v->counter)
		: "memory");

	return result;
}

#define atomic_dec_return(v) atomic_sub_return(1,(v))
#define atomic_inc_return(v) atomic_add_return(1,(v))

/*
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_sub_and_test(i,v) (atomic_sub_return((i), (v)) == 0)

/*
 * atomic_inc_and_test - increment and test
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_inc_and_test(v) (atomic_inc_return(1, (v)) == 0)

/*
 * atomic_dec_and_test - decrement by 1 and test
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_dec_and_test(v) (atomic_sub_return(1, (v)) == 0)

/*
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_inc(v) atomic_add(1,(v))

/*
 * atomic_dec - decrement and test
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
#define atomic_dec(v) atomic_sub(1,(v))

/*
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 *
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 *
 * Currently not implemented for MIPS.
 */

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__after_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_inc()	smp_mb()

#endif /* __ASM_ATOMIC_H */

#else

#if defined(__m68k__)

#ifndef __ARCH_M68K_ATOMIC__
#define __ARCH_M68K_ATOMIC__

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * We do not have SMP m68k systems, so we don't have to deal with that.
 */

typedef struct { int counter; } atomic_t;
#define ATOMIC_INIT(i)	{ (i) }

#define atomic_read(v)		((v)->counter)
#define atomic_set(v, i)	(((v)->counter) = i)

static __inline__ void atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__("addl %1,%0" : "=m" (*v) : "id" (i), "0" (*v));
}

static __inline__ void atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__("subl %1,%0" : "=m" (*v) : "id" (i), "0" (*v));
}

static __inline__ void atomic_inc(volatile atomic_t *v)
{
	__asm__ __volatile__("addql #1,%0" : "=m" (*v): "0" (*v));
}

static __inline__ void atomic_dec(volatile atomic_t *v)
{
	__asm__ __volatile__("subql #1,%0" : "=m" (*v): "0" (*v));
}

static __inline__ int atomic_dec_and_test(volatile atomic_t *v)
{
	char c;
	__asm__ __volatile__("subql #1,%1; seq %0" : "=d" (c), "=m" (*v): "1" (*v));
	return c != 0;
}

#define atomic_clear_mask(mask, v) \
	__asm__ __volatile__("andl %1,%0" : "=m" (*v) : "id" (~(mask)),"0"(*v))

#define atomic_set_mask(mask, v) \
	__asm__ __volatile__("orl %1,%0" : "=m" (*v) : "id" (mask),"0"(*v))

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif /* __ARCH_M68K_ATOMIC __ */

#else

#warning libs/pbd has no implementation of strictly atomic operations for your hardware.

#define __NO_STRICT_ATOMIC
#ifdef __NO_STRICT_ATOMIC
	
/* 
 * Because the implementations from the kernel (where all these come
 * from) use cli and spinlocks for hppa and arm...
 */

typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)	( (atomic_t) { (i) } )

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		((v)->counter = (i))

static __inline__ void atomic_inc(atomic_t *v)
{
	v->counter++;
}

static __inline__ void atomic_dec(atomic_t *v)
{
	v->counter--;
}
    
static __inline__ int atomic_dec_and_test(atomic_t *v)
{
	int res;
	v->counter--;
	res = v->counter;
	return res == 0;
}
    
static __inline__ int atomic_inc_and_test(atomic_t *v)
{
	int res;
	v->counter++;
	res = v->counter;
	return res == 0;
}

#  endif /* __NO_STRICT_ATOMIC */
#  endif /* m68k */
#  endif /* mips */
#  endif /* s390 */
#  endif /* alpha */
#  endif /* ia64 */
#  endif /* sparc */
#  endif /* i386 */
#  endif /* ppc */

#endif /* __libpbd_atomic_h__ */

