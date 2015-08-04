#include <stdio.h>
#include <string.h>
#include "sha1.c"
#include "sha1_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (Sha1Test);

void
Sha1Test::basicTest ()
{
	uint32_t a;
	char hash[41];
	Sha1Digest s;

	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) "abc", 3);
	sha1_result_hash (&s, hash);
	printf ("\nSha1: FIPS 180-2 C.1 and RFC3174 7.3 TEST1\n");
	printf ("Expect: a9993e364706816aba3e25717850c26c9cd0d89d\n");
	printf ("Result: %s\n", hash);
	CPPUNIT_ASSERT_MESSAGE ("Sha1: FIPS 180-2 C.1 and RFC3174 7.3 TEST1",
			!strcmp ("a9993e364706816aba3e25717850c26c9cd0d89d", hash));


	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
	sha1_result_hash (&s, hash);
	printf ("\nSha1: FIPS 180-2 C.2 and RFC3174 7.3 TEST2\n");
	printf ("Expect: 84983e441c3bd26ebaae4aa1f95129e5e54670f1\n");
	printf ("Result: %s\n", hash);
	CPPUNIT_ASSERT_MESSAGE ("Sha1: FIPS 180-2 C.2 and RFC3174 7.3 TEST2",
			!strcmp ("84983e441c3bd26ebaae4aa1f95129e5e54670f1", hash));


	sha1_init (&s);
	for (a = 0; a < 80; ++a) sha1_write (&s, (const uint8_t *) "01234567", 8);
	sha1_result_hash (&s, hash);
	printf ("\nSha1: RFC3174 7.3 TEST4\n");
	printf ("Expect: dea356a2cddd90c7a7ecedc5ebb563934f460452\n");
	printf ("Result: %s\n", hash);
	CPPUNIT_ASSERT_MESSAGE ("Sha1: RFC3174 7.3 TEST4",
			!strcmp ("dea356a2cddd90c7a7ecedc5ebb563934f460452", hash));


	sha1_init (&s);
	for (a = 0; a < 1000000; ++a) sha1_writebyte (&s, 'a');
	sha1_result_hash (&s, hash);
	printf ("\nSha1: Sha1: FIPS 180-2 C.3 and RFC3174 7.3 TEST3\n");
	printf ("Expect:34aa973cd4c4daa4f61eeb2bdbad27316534016f\n");
	printf ("Result:%s\n", hash);
	CPPUNIT_ASSERT_MESSAGE ("Sha1: FIPS 180-2 C.3 and RFC3174 7.3 TEST3",
			!strcmp ("34aa973cd4c4daa4f61eeb2bdbad27316534016f", hash));
}
