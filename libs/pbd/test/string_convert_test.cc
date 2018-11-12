#include "string_convert_test.h"

#include <stdio.h>
#include <string.h>

#include <limits>
#include <cassert>

#include <pthread.h>

#include <glib.h>

#include "pbd/string_convert.h"

using namespace PBD;
using namespace std;

CPPUNIT_TEST_SUITE_REGISTRATION (StringConvertTest);

namespace {

class LocaleGuard {
public:
	// RAII class that sets the global C locale and then resets it to its
	// previous setting when going out of scope
	LocaleGuard (const std::string& locale)
	{
		m_previous_locale = setlocale (LC_ALL, NULL);

		CPPUNIT_ASSERT (m_previous_locale != NULL);

		const char* new_locale = setlocale (LC_ALL, locale.c_str ());

		if (new_locale == NULL) {
			std::cerr << "Failed to set locale to : " << locale << std::endl;
		}

		CPPUNIT_ASSERT (new_locale != NULL);
	}

	~LocaleGuard ()
	{
		CPPUNIT_ASSERT (setlocale (LC_ALL, m_previous_locale) != NULL);
	}

private:
	const char* m_previous_locale;
};

static
bool
check_decimal_mark_is_comma ()
{
	char buf[32];
	double const dnum = 12345.67890;
	snprintf (buf, sizeof(buf), "%.12g", dnum);
	bool found = (strchr (buf, ',') != NULL);
	return found;
}

static std::vector<std::string> get_locale_list ()
{
	std::vector<std::string> locales;

	locales.push_back(""); // User preferred locale

#ifdef PLATFORM_WINDOWS
	locales.push_back("French_France.1252"); // must be first
	locales.push_back("Dutch_Netherlands.1252");
	locales.push_back("Italian_Italy.1252");
	locales.push_back("Farsi_Iran.1256");
	locales.push_back("Chinese_China.936");
	locales.push_back("Czech_Czech Republic.1250");
#else
	locales.push_back("fr_FR"); // French France
	locales.push_back("fr_FR.UTF-8");
	locales.push_back("de_DE"); // German Germany
	locales.push_back("de_DE.UTF-8");
	locales.push_back("nl_NL"); // Dutch - Netherlands
	locales.push_back("nl_NL.UTF-8");
	locales.push_back("it_IT"); // Italian
	locales.push_back("fa_IR"); // Farsi Iran
	locales.push_back("zh_CN"); // Chinese
	locales.push_back("cs_CZ"); // Czech
#endif
	return locales;
}

static std::vector<std::string> get_supported_locales ()
{
	std::vector<std::string> locales = get_locale_list ();
	std::vector<std::string> supported_locales;

	const char * m_orig_locale = setlocale (LC_ALL, NULL);

	CPPUNIT_ASSERT (m_orig_locale != NULL);

	std::cerr << std::endl << "Original locale: " << m_orig_locale << std::endl;

	for (std::vector<std::string>::const_iterator it = locales.begin(); it != locales.end(); ++it) {

		const char* locale = it->c_str();
		const char* new_locale = setlocale (LC_ALL, locale);

		if (new_locale == NULL) {
			std::cerr << "Unable to set locale : " << locale << ", may not be installed." << std::endl;
			continue;
		}

		if (*it != new_locale) {
			// Setting the locale may be successful but locale has a different
			// (or longer) name.
			if (it->empty()) {
				std::cerr << "User preferred locale is : " << new_locale << std::endl;
			} else {
				std::cerr << "locale : " << locale << ", has name : " << new_locale << std::endl;
			}
		}

		std::cerr << "Adding locale : " << new_locale << " to test locales" << std::endl;

		supported_locales.push_back (*it);
	}

	if (setlocale (LC_ALL, m_orig_locale) == NULL) {
		std::cerr << "ERROR: Unable to restore original locale " << m_orig_locale
		          << ", further tests may be invalid." << std::endl;
	}

	return supported_locales;
}

static std::vector<std::string> get_test_locales ()
{
	static std::vector<std::string> locales = get_supported_locales ();
	return locales;
}

static bool get_locale_with_comma_decimal_mark (std::string& locale_str) {
	std::vector<std::string> locales = get_test_locales ();

	for (std::vector<std::string>::const_iterator it = locales.begin(); it != locales.end(); ++it) {
		LocaleGuard guard (*it);
		if (check_decimal_mark_is_comma ()) {
			locale_str = *it;
			return true;
		}
	}
	return false;
}

} // anon namespace

void
StringConvertTest::test_required_locales ()
{
	std::string locale_str;
	CPPUNIT_ASSERT(get_locale_with_comma_decimal_mark(locale_str));
}

static const std::string MAX_INT16_STR ("32767");
static const std::string MIN_INT16_STR ("-32768");

typedef void (*TestFunctionType)(void);

void
test_function_for_locales (TestFunctionType test_func)
{
	const std::vector<std::string> locales = get_test_locales();

	for (std::vector<std::string>::const_iterator ci = locales.begin ();
	     ci != locales.end ();
	     ++ci) {
		LocaleGuard guard (*ci);
		test_func ();
	}
}

void
_test_int16_conversion ()
{
	string str;
	CPPUNIT_ASSERT (int16_to_string (numeric_limits<int16_t>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_INT16_STR, str);

	int16_t val = 0;
	CPPUNIT_ASSERT (string_to_int16 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<int16_t>::max (), val);

	CPPUNIT_ASSERT (int16_to_string (numeric_limits<int16_t>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_INT16_STR, str);

	CPPUNIT_ASSERT (string_to_int16 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<int16_t>::min (), val);

	// test the string_to/to_string templates
	int16_t max = numeric_limits<int16_t>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<int16_t>(to_string (max)));

	int16_t min = numeric_limits<int16_t>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<int16_t>(to_string (min)));
}

void
StringConvertTest::test_int16_conversion ()
{
	test_function_for_locales(&_test_int16_conversion);
}

static const std::string MAX_UINT16_STR("65535");
static const std::string MIN_UINT16_STR("0");

void
_test_uint16_conversion ()
{
	string str;
	CPPUNIT_ASSERT (uint16_to_string (numeric_limits<uint16_t>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_UINT16_STR, str);

	uint16_t val = 0;
	CPPUNIT_ASSERT (string_to_uint16 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<uint16_t>::max (), val);

	CPPUNIT_ASSERT (uint16_to_string (numeric_limits<uint16_t>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_UINT16_STR, str);

	CPPUNIT_ASSERT (string_to_uint16 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<uint16_t>::min (), val);

	// test the string_to/to_string templates
	uint16_t max = numeric_limits<uint16_t>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<uint16_t>(to_string (max)));

	uint16_t min = numeric_limits<uint16_t>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<uint16_t>(to_string (min)));
}

void
StringConvertTest::test_uint16_conversion ()
{
	test_function_for_locales(&_test_uint16_conversion);
}

static const std::string MAX_INT32_STR ("2147483647");
static const std::string MIN_INT32_STR ("-2147483648");

void
_test_int32_conversion ()
{
	string str;
	CPPUNIT_ASSERT (int32_to_string (numeric_limits<int32_t>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_INT32_STR, str);

	int32_t val = 0;
	CPPUNIT_ASSERT (string_to_int32 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<int32_t>::max (), val);

	CPPUNIT_ASSERT (int32_to_string (numeric_limits<int32_t>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_INT32_STR, str);

	CPPUNIT_ASSERT (string_to_int32 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<int32_t>::min (), val);

	// test the string_to/to_string templates
	int32_t max = numeric_limits<int32_t>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<int32_t>(to_string (max)));

	int32_t min = numeric_limits<int32_t>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<int32_t>(to_string (min)));
}

void
StringConvertTest::test_int32_conversion ()
{
	test_function_for_locales(&_test_int32_conversion);
}

static const std::string MAX_UINT32_STR("4294967295");
static const std::string MIN_UINT32_STR("0");

void
_test_uint32_conversion ()
{
	string str;
	CPPUNIT_ASSERT (uint32_to_string (numeric_limits<uint32_t>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_UINT32_STR, str);

	uint32_t val = 0;
	CPPUNIT_ASSERT (string_to_uint32 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<uint32_t>::max (), val);

	CPPUNIT_ASSERT (uint32_to_string (numeric_limits<uint32_t>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_UINT32_STR, str);

	CPPUNIT_ASSERT (string_to_uint32 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<uint32_t>::min (), val);

	// test the string_to/to_string templates
	uint32_t max = numeric_limits<uint32_t>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<uint32_t>(to_string (max)));

	uint32_t min = numeric_limits<uint32_t>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<uint32_t>(to_string (min)));
}

void
StringConvertTest::test_uint32_conversion ()
{
	test_function_for_locales(&_test_uint32_conversion);
}

static const std::string MAX_INT64_STR ("9223372036854775807");
static const std::string MIN_INT64_STR ("-9223372036854775808");

void
_test_int64_conversion ()
{
	string str;
	CPPUNIT_ASSERT (int64_to_string (numeric_limits<int64_t>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_INT64_STR, str);

	int64_t val = 0;
	CPPUNIT_ASSERT (string_to_int64 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<int64_t>::max (), val);

	CPPUNIT_ASSERT (int64_to_string (numeric_limits<int64_t>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_INT64_STR, str);

	CPPUNIT_ASSERT (string_to_int64 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<int64_t>::min (), val);

	// test the string_to/to_string templates
	int64_t max = numeric_limits<int64_t>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<int64_t>(to_string (max)));

	int64_t min = numeric_limits<int64_t>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<int64_t>(to_string (min)));
}

void
StringConvertTest::test_int64_conversion ()
{
	test_function_for_locales(&_test_int64_conversion);
}

static const std::string MAX_UINT64_STR ("18446744073709551615");
static const std::string MIN_UINT64_STR ("0");

void
_test_uint64_conversion ()
{
	string str;
	CPPUNIT_ASSERT (uint64_to_string (numeric_limits<uint64_t>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_UINT64_STR, str);

	uint64_t val = 0;
	CPPUNIT_ASSERT (string_to_uint64 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<uint64_t>::max (), val);

	CPPUNIT_ASSERT (uint64_to_string (numeric_limits<uint64_t>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_UINT64_STR, str);

	CPPUNIT_ASSERT (string_to_uint64 (str, val));
	CPPUNIT_ASSERT_EQUAL (numeric_limits<uint64_t>::min (), val);

	// test the string_to/to_string templates
	uint64_t max = numeric_limits<uint64_t>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<uint64_t>(to_string (max)));

	uint64_t min = numeric_limits<uint64_t>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<uint64_t>(to_string (min)));
}

void
StringConvertTest::test_uint64_conversion ()
{
	test_function_for_locales(&_test_uint64_conversion);
}

static const std::string POS_INFINITY_STR ("infinity");
static const std::string NEG_INFINITY_STR ("-infinity");
static const std::string POS_INFINITY_CAPS_STR ("INFINITY");
static const std::string NEG_INFINITY_CAPS_STR ("-INFINITY");
static const std::string POS_INF_STR ("inf");
static const std::string NEG_INF_STR ("-inf");
static const std::string POS_INF_CAPS_STR ("INF");
static const std::string NEG_INF_CAPS_STR ("-INF");

static
std::vector<std::string>
_pos_infinity_strings ()
{
	std::vector<std::string> vec;
	vec.push_back (POS_INFINITY_STR);
	vec.push_back (POS_INFINITY_CAPS_STR);
	vec.push_back (POS_INF_STR);
	vec.push_back (POS_INF_CAPS_STR);
	return vec;
}

static
std::vector<std::string>
_neg_infinity_strings ()
{
	std::vector<std::string> vec;
	vec.push_back (NEG_INFINITY_STR);
	vec.push_back (NEG_INFINITY_CAPS_STR);
	vec.push_back (NEG_INF_STR);
	vec.push_back (NEG_INF_CAPS_STR);
	return vec;
}

template <class FloatType>
void
_test_infinity_conversion ()
{
	const FloatType pos_infinity = numeric_limits<FloatType>::infinity ();
	const FloatType neg_infinity = -numeric_limits<FloatType>::infinity ();

	// Check float -> string
	string str;
	CPPUNIT_ASSERT (to_string<FloatType> (pos_infinity, str));
	CPPUNIT_ASSERT_EQUAL (POS_INF_STR, str);

	CPPUNIT_ASSERT (to_string<FloatType> (neg_infinity, str));
	CPPUNIT_ASSERT_EQUAL (NEG_INF_STR, str);

	// Check string -> float for all supported string representations of "INFINITY"
	std::vector<std::string> pos_inf_strings = _pos_infinity_strings ();

	for (std::vector<std::string>::const_iterator i = pos_inf_strings.begin ();
	     i != pos_inf_strings.end (); ++i) {
		FloatType pos_infinity_arg;
		CPPUNIT_ASSERT (string_to<FloatType> (*i, pos_infinity_arg));
		CPPUNIT_ASSERT_EQUAL (pos_infinity_arg, pos_infinity);
	}

	// Check string -> float for all supported string representations of "-INFINITY"
	std::vector<std::string> neg_inf_strings = _neg_infinity_strings ();

	for (std::vector<std::string>::const_iterator i = neg_inf_strings.begin ();
	     i != neg_inf_strings.end (); ++i) {
		FloatType neg_infinity_arg;
		CPPUNIT_ASSERT (string_to<FloatType> (*i, neg_infinity_arg));
		CPPUNIT_ASSERT_EQUAL (neg_infinity_arg, neg_infinity);
	}

	// Check round-trip equality
	CPPUNIT_ASSERT_EQUAL (pos_infinity, string_to<FloatType> (to_string<FloatType> (pos_infinity)));
	CPPUNIT_ASSERT_EQUAL (neg_infinity, string_to<FloatType> (to_string<FloatType> (neg_infinity)));
}

static const std::string MAX_FLOAT_WIN ("3.4028234663852886e+038");
static const std::string MIN_FLOAT_WIN ("1.1754943508222875e-038");
static const std::string MAX_FLOAT_STR ("3.4028234663852886e+38");
static const std::string MIN_FLOAT_STR ("1.1754943508222875e-38");

void
_test_float_conversion ()
{
	// check float to string and back again for min and max float values
	string str;
	CPPUNIT_ASSERT (float_to_string (numeric_limits<float>::max (), str));
#ifdef PLATFORM_WINDOWS
	CPPUNIT_ASSERT_EQUAL (MAX_FLOAT_WIN, str);
#else
	CPPUNIT_ASSERT_EQUAL (MAX_FLOAT_STR, str);
#endif

	float val = 0.0f;
	CPPUNIT_ASSERT (string_to_float (str, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<float>::max (), val, numeric_limits<float>::epsilon ());

	CPPUNIT_ASSERT (float_to_string (numeric_limits<float>::min (), str));
#ifdef PLATFORM_WINDOWS
	CPPUNIT_ASSERT_EQUAL (MIN_FLOAT_WIN, str);
#else
	CPPUNIT_ASSERT_EQUAL (MIN_FLOAT_STR, str);
#endif

	CPPUNIT_ASSERT (string_to_float (str, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<float>::min (), val, numeric_limits<float>::epsilon ());

	// test the string_to/to_string templates
	float max = numeric_limits<float>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<float>(to_string<float> (max)));

	float min = numeric_limits<float>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<float>(to_string<float> (min)));

// check that parsing the windows float string representation with the
// difference in the exponent part parses correctly on other platforms
// and vice versa
#ifdef PLATFORM_WINDOWS
	CPPUNIT_ASSERT (string_to_float (MAX_FLOAT_STR, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<float>::max (), val, numeric_limits<float>::epsilon ());

	CPPUNIT_ASSERT (string_to_float (MIN_FLOAT_STR, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<float>::min (), val, numeric_limits<float>::epsilon ());
#else
	CPPUNIT_ASSERT (string_to_float (MAX_FLOAT_WIN, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<float>::max (), val, numeric_limits<float>::epsilon ());

	CPPUNIT_ASSERT (string_to_float (MIN_FLOAT_WIN, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<float>::min (), val, numeric_limits<float>::epsilon ());
#endif

	_test_infinity_conversion<float>();
}

void
StringConvertTest::test_float_conversion ()
{
	test_function_for_locales(&_test_float_conversion);
}

static const std::string MAX_DOUBLE_STR ("1.7976931348623157e+308");
static const std::string MIN_DOUBLE_STR ("2.2250738585072014e-308");

void
_test_double_conversion ()
{
	string str;
	CPPUNIT_ASSERT (double_to_string (numeric_limits<double>::max (), str));
	CPPUNIT_ASSERT_EQUAL (MAX_DOUBLE_STR, str);

	double val = 0.0;
	CPPUNIT_ASSERT (string_to_double (str, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<double>::max (), val, numeric_limits<double>::epsilon ());

	CPPUNIT_ASSERT (double_to_string (numeric_limits<double>::min (), str));
	CPPUNIT_ASSERT_EQUAL (MIN_DOUBLE_STR, str);

	CPPUNIT_ASSERT (string_to_double (str, val));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (
	    numeric_limits<double>::min (), val, numeric_limits<double>::epsilon ());

	// test that overflow fails
	CPPUNIT_ASSERT (!string_to_double ("1.8e+308", val));
	// test that underflow fails
	CPPUNIT_ASSERT (!string_to_double ("2.4e-310", val));

	// test the string_to/to_string templates
	double max = numeric_limits<double>::max ();
	CPPUNIT_ASSERT_EQUAL (max, string_to<double>(to_string<double> (max)));

	double min = numeric_limits<double>::min ();
	CPPUNIT_ASSERT_EQUAL (min, string_to<double>(to_string<double> (min)));

	_test_infinity_conversion<double>();
}

void
StringConvertTest::test_double_conversion ()
{
	test_function_for_locales(&_test_double_conversion);
}

// we have to use these as CPPUNIT_ASSERT_EQUAL won't accept char arrays
static const std::string BOOL_TRUE_STR ("1");
static const std::string BOOL_FALSE_STR ("0");

void
StringConvertTest::test_bool_conversion ()
{
	string str;

	// check the normal case for true/false
	CPPUNIT_ASSERT (bool_to_string (true, str));
	CPPUNIT_ASSERT_EQUAL (BOOL_TRUE_STR, str);

	bool val = false;
	CPPUNIT_ASSERT (string_to_bool (str, val));
	CPPUNIT_ASSERT_EQUAL (val, true);

	CPPUNIT_ASSERT (bool_to_string (false, str));
	CPPUNIT_ASSERT_EQUAL (BOOL_FALSE_STR, str);

	val = true;
	CPPUNIT_ASSERT (string_to_bool (str, val));
	CPPUNIT_ASSERT_EQUAL (val, false);

	// now check the other accepted values for true and false
	// when converting from a string to bool

	val = false;
	CPPUNIT_ASSERT (string_to_bool ("1", val));
	CPPUNIT_ASSERT_EQUAL (val, true);

	val = true;
	CPPUNIT_ASSERT (string_to_bool ("0", val));
	CPPUNIT_ASSERT_EQUAL (val, false);

	val = false;
	CPPUNIT_ASSERT (string_to_bool ("Y", val));
	CPPUNIT_ASSERT_EQUAL (val, true);

	val = true;
	CPPUNIT_ASSERT (string_to_bool ("N", val));
	CPPUNIT_ASSERT_EQUAL (val, false);

	val = false;
	CPPUNIT_ASSERT (string_to_bool ("y", val));
	CPPUNIT_ASSERT_EQUAL (val, true);

	val = true;
	CPPUNIT_ASSERT (string_to_bool ("n", val));
	CPPUNIT_ASSERT_EQUAL (val, false);

	// test some junk
	CPPUNIT_ASSERT (!string_to_bool ("01234someYNtrueyesno junk0123", val));

	// test the string_to/to_string templates
	CPPUNIT_ASSERT_EQUAL (true, string_to<bool>(to_string (true)));

	CPPUNIT_ASSERT_EQUAL (false, string_to<bool>(to_string (false)));
}

namespace {

bool
check_int_convert ()
{
	int32_t num = g_random_int ();
	return (num == string_to<int32_t>(to_string (num)));
}

bool
check_float_convert ()
{
	float num = (float)g_random_double ();
	return (num == string_to<float>(to_string<float> (num)));
}

bool
check_double_convert ()
{
	double num = g_random_double ();
	return (num == string_to<double>(to_string<double> (num)));
}

static const int s_iter_count = 1000000;

void*
check_int_convert_thread(void*)
{
	for (int n = 0; n < s_iter_count; n++) {
		assert (check_int_convert ());
	}
	return NULL;
}

void*
check_float_convert_thread(void*)
{
	for (int n = 0; n < s_iter_count; n++) {
		assert (check_float_convert ());
	}
	return NULL;
}

void*
check_double_convert_thread(void*)
{
	for (int n = 0; n < s_iter_count; n++) {
		assert (check_double_convert ());
	}
	return NULL;
}

static const double s_test_double = 31459.265359;

void*
check_decimal_mark_is_comma_thread (void*)
{
	for (int n = 0; n < s_iter_count; n++) {
		assert (check_decimal_mark_is_comma ());
	}

	return NULL;
}

} // anon namespace

// Perform the test in the French locale as the format for decimals is
// different and a comma is used as a decimal point. Test that this has no
// impact on the string conversions which are expected to be the same as in the
// C locale.
void
StringConvertTest::test_convert_thread_safety ()
{
	std::string locale_str;

	CPPUNIT_ASSERT(get_locale_with_comma_decimal_mark(locale_str));

	LocaleGuard guard (locale_str);

	CPPUNIT_ASSERT (check_int_convert ());
	CPPUNIT_ASSERT (check_float_convert ());
	CPPUNIT_ASSERT (check_double_convert ());
	CPPUNIT_ASSERT (check_decimal_mark_is_comma ());

	pthread_t convert_int_thread;
	pthread_t convert_float_thread;
	pthread_t convert_double_thread;
	pthread_t fr_printf_thread;

	CPPUNIT_ASSERT (
	    pthread_create (
	        &convert_int_thread, NULL, check_int_convert_thread, NULL) == 0);
	CPPUNIT_ASSERT (
	    pthread_create (
	        &convert_float_thread, NULL, check_float_convert_thread, NULL) == 0);
	CPPUNIT_ASSERT (
	    pthread_create (
	        &convert_double_thread, NULL, check_double_convert_thread, NULL) == 0);
	CPPUNIT_ASSERT (
	    pthread_create (&fr_printf_thread, NULL, check_decimal_mark_is_comma_thread, NULL) ==
	    0);

	void* return_value;

	CPPUNIT_ASSERT (pthread_join (convert_int_thread, &return_value) == 0);
	CPPUNIT_ASSERT (pthread_join (convert_float_thread, &return_value) == 0);
	CPPUNIT_ASSERT (pthread_join (convert_double_thread, &return_value) == 0);
	CPPUNIT_ASSERT (pthread_join (fr_printf_thread, &return_value) == 0);
}
