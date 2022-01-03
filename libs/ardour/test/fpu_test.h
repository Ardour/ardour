#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "ardour/runtime_functions.h"

class FPUTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (FPUTest);
#if defined(ARCH_X86) && defined(BUILD_SSE_OPTIMIZATIONS)
	CPPUNIT_TEST (sseTest);
	CPPUNIT_TEST (avxTest);
	CPPUNIT_TEST (avxFmaTest);
#elif defined ARM_NEON_SUPPORT
	CPPUNIT_TEST (neonTest);
#elif defined(__APPLE__) && defined(BUILD_VECLIB_OPTIMIZATIONS)
	CPPUNIT_TEST (veclibTest);
#else
	CPPUNIT_TEST (noTest);
#endif
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

#if defined(ARCH_X86) && defined(BUILD_SSE_OPTIMIZATIONS)
	void avxFmaTest ();
	void avxTest ();
	void sseTest ();
#elif defined ARM_NEON_SUPPORT
	void neonTest ();
#elif defined(__APPLE__) && defined(BUILD_VECLIB_OPTIMIZATIONS)
	void veclibTest ();
#else
	void noTest ();
#endif

private:
	void run (size_t, float const max_diff = 0);
	void compare (std::string, size_t, float const max_diff = 0);

	ARDOUR::compute_peak_t          compute_peak;
	ARDOUR::find_peaks_t            find_peaks;
	ARDOUR::apply_gain_to_buffer_t  apply_gain_to_buffer;
	ARDOUR::mix_buffers_with_gain_t mix_buffers_with_gain;
	ARDOUR::mix_buffers_no_gain_t   mix_buffers_no_gain;
	ARDOUR::copy_vector_t           copy_vector;

	size_t _size;

	float* _test1;
	float* _test2;
	float* _comp1;
	float* _comp2;
};
