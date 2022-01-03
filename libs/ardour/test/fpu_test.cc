#include <cassert>
#include "pbd/compose.h"
#include "pbd/fpu.h"
#include "pbd/malign.h"
#include "libs/ardour/ardour/mix.h"
#include "fpu_test.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

CPPUNIT_TEST_SUITE_REGISTRATION(FPUTest);

void
FPUTest::setUp ()
{
	_size = 1024;
	cache_aligned_malloc ((void**) &_test1, sizeof (float) * _size);
	cache_aligned_malloc ((void**) &_test2, sizeof (float) * _size);
	cache_aligned_malloc ((void**) &_comp1, sizeof (float) * _size);
	cache_aligned_malloc ((void**) &_comp2, sizeof (float) * _size);

	for (size_t i = 0; i < _size; ++i) {
		_test1[i] = _comp1[i] = 3.0 / (i + 1.0);
		_test2[i] = _comp2[i] = 2.5 / (i + 1.0);
	}

}

void
FPUTest::tearDown ()
{
	cache_aligned_free (_comp1);
	cache_aligned_free (_comp2);
	cache_aligned_free (_test1);
	cache_aligned_free (_test2);
}

void
FPUTest::run (size_t align_max, float const max_diff)
{
	apply_gain_to_buffer (_test1, _size, 1.33);
	default_apply_gain_to_buffer (_comp1, _size, 1.33);
	compare ("Apply Gain", _size);

	for (size_t off = 0; off < align_max; ++off) {
		for (size_t cnt = 1; cnt < align_max; ++cnt) {
			/* apply gain */
			apply_gain_to_buffer (&_test1[off], cnt, 0.99);
			default_apply_gain_to_buffer (&_comp1[off], cnt, 0.99);
			compare (string_compose ("Apply Gain not aligned off: %1 cnt: %2", off, cnt), cnt);

			/* compute peak */
			float pk_test = 0;
			float pk_comp = 0;
			pk_test = compute_peak (&_test1[off], cnt, pk_test);
			pk_comp = default_compute_peak (&_comp1[off], cnt, pk_comp);

			CPPUNIT_ASSERT_MESSAGE (string_compose ("Compute peak not aligned off: %1 cnt: %2", off, cnt), fabsf (pk_test - pk_comp) < 1e-6);

			/* mix buffers w/o gain */
			mix_buffers_no_gain (&_test1[off], &_test2[off], cnt);
			default_mix_buffers_no_gain (&_comp1[off], &_comp2[off], cnt);
			compare (string_compose ("Mix Buffers no gain not aligned off: %1 cnt: %2", off, cnt), cnt);

			/* mix buffers w/gain */
			mix_buffers_with_gain (&_test1[off], &_test2[off], cnt, 0.45);
			default_mix_buffers_with_gain (&_comp1[off], &_comp2[off], cnt, 0.45);
			compare (string_compose ("Mix Buffers w/gain not aligned off: %1 cnt: %2", off, cnt), cnt, max_diff);

			/* copy vector */
			copy_vector (&_test1[off], &_test2[off], cnt);
			default_copy_vector (&_comp1[off], &_comp2[off], cnt);
			compare (string_compose ("Copy Vector not aligned off: %1 cnt: %2", off, cnt), cnt);

			/* find_peaks */
			pk_test = _test1[off];
			pk_comp = _comp1[off];
			float pk_test_max = _test1[off];
			float pk_comp_max = _comp1[off];
			find_peaks (&_test1[off], cnt, &pk_test, &pk_test_max);
			default_find_peaks (&_comp1[off], cnt, &pk_comp, &pk_comp_max);
			CPPUNIT_ASSERT_MESSAGE (string_compose ("Find peaks not aligned off: %1 cnt: %2", off, cnt), fabsf (pk_test - pk_comp) < 2e-6 && fabsf (pk_test_max - pk_comp_max) < 2e-6);
		}
	}
}

void
FPUTest::compare (std::string msg, size_t cnt, float max_diff)
{
	size_t err = 0;
	for (size_t i = 0; i < cnt; ++i) {
		if (fabsf (_test1[i] - _comp1[i]) > max_diff) {
			++err;
		}
	}
	CPPUNIT_ASSERT_MESSAGE (msg, err == 0);
}

#if defined(ARCH_X86) && defined(BUILD_SSE_OPTIMIZATIONS)

void
FPUTest::avxFmaTest ()
{
	PBD::FPU* fpu = PBD::FPU::instance ();
	if (!(fpu->has_avx () && fpu->has_fma ())) {
		printf ("AVX and FMA is not available at run-time\n");
		return;
	}

#if ( defined(__x86_64__) || defined(_M_X64) )
	size_t align_max = 64;
#else
	size_t align_max = 16;
#endif
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test1) % align_max) == 0);
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test2) % align_max) == 0);

	compute_peak          = x86_sse_avx_compute_peak;
	find_peaks            = x86_sse_avx_find_peaks;
	apply_gain_to_buffer  = x86_sse_avx_apply_gain_to_buffer;
	mix_buffers_with_gain = x86_fma_mix_buffers_with_gain;
	mix_buffers_no_gain   = x86_sse_avx_mix_buffers_no_gain;
	copy_vector           = x86_sse_avx_copy_vector;

	run (align_max, FLT_EPSILON);
}

void
FPUTest::avxTest ()
{
	PBD::FPU* fpu = PBD::FPU::instance ();
	if (!fpu->has_avx ()) {
		printf ("AVX is not available at run-time\n");
		return;
	}

#if ( defined(__x86_64__) || defined(_M_X64) )
	size_t align_max = 64;
#else
	size_t align_max = 16;
#endif
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test1) % align_max) == 0);
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test2) % align_max) == 0);

	compute_peak          = x86_sse_avx_compute_peak;
	find_peaks            = x86_sse_avx_find_peaks;
	apply_gain_to_buffer  = x86_sse_avx_apply_gain_to_buffer;
	mix_buffers_with_gain = x86_sse_avx_mix_buffers_with_gain;
	mix_buffers_no_gain   = x86_sse_avx_mix_buffers_no_gain;
	copy_vector           = x86_sse_avx_copy_vector;

	run (align_max);
}

void
FPUTest::sseTest ()
{
	PBD::FPU* fpu = PBD::FPU::instance ();
	if (!fpu->has_sse ()) {
		printf ("SSE is not available at run-time\n");
		return;
	}

#if ( defined(__x86_64__) || defined(_M_X64) )
	size_t align_max = 64;
#else
	size_t align_max = 16;
#endif
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test1) % align_max) == 0);
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test2) % align_max) == 0);

	compute_peak          = x86_sse_compute_peak;
	find_peaks            = x86_sse_find_peaks;
	apply_gain_to_buffer  = x86_sse_apply_gain_to_buffer;
	mix_buffers_with_gain = x86_sse_mix_buffers_with_gain;
	mix_buffers_no_gain   = x86_sse_mix_buffers_no_gain;
	copy_vector           = default_copy_vector;

	run (align_max);
}

#elif defined ARM_NEON_SUPPORT

void
FPUTest::neonTest ()
{
	PBD::FPU* fpu = PBD::FPU::instance ();
	if (!fpu->has_neon ()) {
		printf ("NEON is not available at run-time\n");
		return;
	}

	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test1) % 128) == 0);
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test2) % 128) == 0);

	compute_peak          = arm_neon_compute_peak;
	find_peaks            = arm_neon_find_peaks;
	apply_gain_to_buffer  = arm_neon_apply_gain_to_buffer;
	mix_buffers_with_gain = arm_neon_mix_buffers_with_gain;
	mix_buffers_no_gain   = arm_neon_mix_buffers_no_gain;
	copy_vector           = arm_neon_copy_vector;

	run (128);
}

#elif defined(__APPLE__) && defined(BUILD_VECLIB_OPTIMIZATIONS)

void
FPUTest::veclibTest ()
{
	if (floor (kCFCoreFoundationVersionNumber) <= kCFCoreFoundationVersionNumber10_4) {
		printf ("veclib is not available at run-time\n");
		return;
	}

	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test1) % 16) == 0);
	CPPUNIT_ASSERT_MESSAGE ("Aligned Malloc", (((intptr_t)_test2) % 16) == 0);


	compute_peak          = veclib_compute_peak;
	find_peaks            = veclib_find_peaks;
	apply_gain_to_buffer  = veclib_apply_gain_to_buffer;
	mix_buffers_with_gain = veclib_mix_buffers_with_gain;
	mix_buffers_no_gain   = veclib_mix_buffers_no_gain;
	copy_vector           = default_copy_vector;

	run (16);
}

#else

void
FPUTest::noTest ()
{
	printf ("HW acceleration is disabled at compile-time\n");
}
#endif
