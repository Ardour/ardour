/*
 *  Akupara/threading/atomic_ops_gcc_x86.hpp
 *  
 *
 *  Created by Udi Barzilai on 06/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */
#if !defined(_AKUPARA_THREADING_ATOMIC_OPS_GCC_X86_HPP__INCLUDED_)
#	define _AKUPARA_THREADING_ATOMIC_OPS_GCC_X86_HPP__INCLUDED_
#	if defined(__GNUC__) && (defined(__i386__) ||  defined(__x86_64__))

namespace Akupara
{
	namespace threading
	{
		namespace atomic
		{
			namespace machine
			{
				const unsigned int k_bytes_per_cache_line = 64;  // this is true for P4 & K8
	

				// Flags for operations supported by this machine
				//-------------------------------------
				template<> struct implements_load         <4> : public true_type {};
				template<> struct implements_store        <4> : public true_type {};
				template<> struct implements_CAS          <4> : public true_type {};
				template<> struct implements_CAS          <8> : public true_type {};
				template<> struct implements_add          <4> : public true_type {};
				template<> struct implements_fetch_and_add<4> : public true_type {};
				//-------------------------------------
		


				// CAS
				//--------------------------------------------------------------------------------
				template<>
				inline bool compare_and_store<int64_t>(volatile int64_t * p, const int64_t & x, const int64_t & y)
				{
					register int32_t evh=int32_t(x>>32), evl=int32_t(x);
					register const int32_t nvh=int32_t(y>>32), nvl=int32_t(y);
					register bool result;
					__asm__ __volatile__ (
							"# CAS64\n"
							"	lock		           \n"
							"	cmpxchg8b %[location]  \n"
							"	sete %[result]         \n"
							: [location] "+m" (*p), [result] "=qm" (result), [expected_value_high] "+d" (evh), [expected_value_low] "+a" (evl)
							: [new_value_high] "c" (nvh), [new_value_low] "b" (nvl)
							: "cc"
					);
					return result;
				}
				//--------------------------------------------------------------------------------
				template<>
				inline bool compare_and_store<int32_t>(volatile int32_t *p, const int32_t & x, const int32_t & y)
				{
					register int32_t expected_value = x;
					register bool result;
					__asm__	__volatile__ (
							"# CAS32\n"
							"	lock                             \n"
							"	cmpxchgl %[new_value],%[operand] \n"
							"	sete %[result]                   \n"
							: [operand] "+m" (*p), [result] "=qm" (result), [expected_value] "+a" (expected_value)
							: [new_value] "r" (y)
							: "cc"
					);
					return result;
				}
				//--------------------------------------------------------------------------------




				// Atomic add/sub
				//--------------------------------------------------------------------------------
				inline void increment(volatile int32_t * operand_address)
				{
					__asm__ __volatile__ (
					"# atomic_increment_32\n"
					"	lock;             \n"
					"	incl %[operand];  \n"
					: [operand] "+m" (*operand_address)
					:
					: "cc"
					);
				}
				//--------------------------------------------------------------------------------
				inline void decrement(volatile int32_t * operand_address)
				{
					__asm__ __volatile__ (
					"# atomic_decrement_32\n"
					"	lock;             \n"
					"	decl %[operand];  \n"
					: [operand] "+m" (*operand_address)
					:
					: "cc"
					);
				}
				//--------------------------------------------------------------------------------
				template<>
				inline void add<int32_t>(volatile int32_t * operand_address, const int32_t & addend)
				{
					if (__builtin_constant_p(addend) && addend==1)
						increment(operand_address);
					else if (__builtin_constant_p(addend) && addend==-1)
						decrement(operand_address);
					else
						__asm__ __volatile__ (
						"# atomic_add_32               \n"
						"	lock                       \n"
						"	addl %[addend], %[operand] \n"
						: [operand] "+m" (*operand_address)
						: [addend] "ir" (addend)
						: "cc"
						);
				}
				//--------------------------------------------------------------------------------
				template<>
				inline void subtract<int32_t>(volatile int32_t * operand_address, const int32_t & subtrahend)
				{
					if (__builtin_constant_p(subtrahend) && subtrahend==1)
						decrement(operand_address);
					else if (__builtin_constant_p(subtrahend) && subtrahend==-1)
						increment(operand_address);
					else
						__asm__ __volatile__ (
						"# atomic_subtract_32              \n"
						"	lock                           \n"
						"	subl %[subtrahend], %[operand] \n"
						: [operand] "+m" (*operand_address)
						: [subtrahend] "ir" (subtrahend)
						: "cc"
						);
				}
				//--------------------------------------------------------------------------------



				// Atomic fetch and add/sub
				//--------------------------------------------------------------------------------
				template<>
				inline int32_t fetch_and_add<int32_t>(volatile int32_t * operand_address, const int32_t & addend)
				{
					register int32_t addend_and_fetched = addend;
					__asm__ __volatile__ (
					"# atomic_fetch_and_add_32       \n"
					"	lock;                        \n"
					"	xaddl %[addend], %[operand]; \n"
					: [operand] "+m" (*operand_address), [addend] "+r" (addend_and_fetched)
					:
					: "cc"
					);
					return addend_and_fetched;
				}
				//--------------------------------------------------------------------------------
				template<>
				inline int32_t fetch_and_subtract<int32_t>(volatile int32_t * operand_address, const int32_t & subtrahend)
				{
					return fetch_and_add(operand_address, -subtrahend);
				}
				//--------------------------------------------------------------------------------
		


			
				// Memory barriers
				//--------------------------------------------------------------------------------
				inline void memory_barrier_readwrite()
				{
				#if _AKUPARA_X86_SSE_NOT_AVAILABLE
					__asm__ __volatile__ ("	lock; addl $0,0(%%esp); # memory_barrier_readwrite" : : : "memory");
				#else
					__asm__ __volatile__ ("	mfence;   # memory_barrier_readwrite" : : : "memory");
				#endif // _LOCKFREE_ATOMIC_OPS_X86_LFENCE_NOT_AVAILABLE
				}
				//--------------------------------------------------------------------------------
				inline void memory_barrier_read()
				{
				#if _AKUPARA_X86_SSE_NOT_AVAILABLE
					__asm__ __volatile__ ("	lock; addl $0,0(%%esp); # memory_barrier_read" : : : "memory");
				#else
					__asm__ __volatile__ ("	lfence;  # memory_barrier_read" : : : "memory");
				#endif // _LOCKFREE_ATOMIC_OPS_X86_LFENCE_NOT_AVAILABLE
				}
				//--------------------------------------------------------------------------------
				inline void memory_barrier_write()
				{
					__asm__ __volatile__ ("	sfence;  # memory_barrier_write" : : : "memory");
				}
				//--------------------------------------------------------------------------------

			} // namespace machine
		} // namespace atomic
	} // namespace threading
} // namespace Akupara

#	endif // defined(__GNUC__) && defined(__i386__)
#endif // _AKUPARA_THREADING_ATOMIC_OPS_GCC_X86_HPP__INCLUDED_
