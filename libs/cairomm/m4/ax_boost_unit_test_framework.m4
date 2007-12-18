##### http://autoconf-archive.cryp.to/ax_boost_unit_test_framework.html
#
# SYNOPSIS
#
#   AX_BOOST_UNIT_TEST_FRAMEWORK
#
# DESCRIPTION
#
#   Test for Unit_Test_Framework library from the Boost C++ libraries.
#   The macro requires a preceding call to AX_BOOST_BASE. Further
#   documentation is available at
#   <http://randspringer.de/boost/index.html>.
#
#   This macro calls:
#
#     AC_SUBST(BOOST_UNIT_TEST_FRAMEWORK_LIB)
#
#   And sets:
#
#     HAVE_BOOST_UNIT_TEST_FRAMEWORK
#
# LAST MODIFICATION
#
#   2006-12-28
#
# COPYLEFT
#
#   Copyright (c) 2006 Thomas Porschberg <thomas@randspringer.de>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([AX_BOOST_UNIT_TEST_FRAMEWORK],
[
	AC_ARG_WITH([boost-unit-test-framework],
	AS_HELP_STRING([--with-boost-unit-test-framework@<:@=special-lib@:>@],
                   [use the Unit_Test_Framework library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-unit-test-framework=boost_unit_test_framework-gcc ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_unit_test_framework_lib=""
        else
		    want_boost="yes"
        	ax_boost_user_unit_test_framework_lib="$withval"
		fi
        ],
        [want_boost="yes"]
	)

	if test "x$want_boost" = "xyes"; then
        AC_REQUIRE([AC_PROG_CC])
		CPPFLAGS_SAVED="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
		export CPPFLAGS

		LDFLAGS_SAVED="$LDFLAGS"
		LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
		export LDFLAGS

        AC_CACHE_CHECK(whether the Boost::Unit_Test_Framework library is available,
					   ax_cv_boost_unit_test_framework,
        [AC_LANG_PUSH([C++])
			 AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[@%:@include <boost/test/unit_test.hpp>]],
                                    [[using boost::unit_test::test_suite;
					                 test_suite* test= BOOST_TEST_SUITE( "Unit test example 1" ); return 0;]]),
                   ax_cv_boost_unit_test_framework=yes, ax_cv_boost_unit_test_framework=no)
         AC_LANG_POP([C++])
		])
		if test "x$ax_cv_boost_unit_test_framework" = "xyes"; then
			AC_DEFINE(HAVE_BOOST_UNIT_TEST_FRAMEWORK,,[define if the Boost::Unit_Test_Framework library is available])
			BN=boost_unit_test_framework
            if test "x$ax_boost_user_unit_test_framework_lib" = "x"; then
         		saved_ldflags="${LDFLAGS}"
		    	for ax_lib in $BN $BN-$CC $BN-$CC-mt $BN-$CC-mt-s $BN-$CC-s \
                             lib$BN lib$BN-$CC lib$BN-$CC-mt lib$BN-$CC-mt-s lib$BN-$CC-s \
                             $BN-mgw $BN-mgw $BN-mgw-mt $BN-mgw-mt-s $BN-mgw-s ; do
                   LDFLAGS="${LDFLAGS} -l$ax_lib"
    			   AC_CACHE_CHECK(Boost::UnitTestFramework library linkage,
	      			    		   ax_cv_boost_unit_test_framework_link,
						  [AC_LANG_PUSH([C++])
                   AC_LINK_IFELSE([AC_LANG_PROGRAM([[@%:@include <boost/test/unit_test.hpp>
                                                     using boost::unit_test::test_suite;
                                                     test_suite* init_unit_test_suite( int argc, char * argv[] ) {
                                                     test_suite* test= BOOST_TEST_SUITE( "Unit test example 1" );
                                                     return test;
                                                     }
                                                   ]],
                                 [[ return 0;]])],
                                 link_unit_test_framework="yes",link_unit_test_framework="no")
			      AC_LANG_POP([C++])
                  ])
                  LDFLAGS="${saved_ldflags}"

			      if test "x$link_unit_test_framework" = "xyes"; then
                      BOOST_UNIT_TEST_FRAMEWORK_LIB="-l$ax_lib"
                      AC_SUBST(BOOST_UNIT_TEST_FRAMEWORK_LIB)
					  break
				  fi
                done
            else
         		saved_ldflags="${LDFLAGS}"
               for ax_lib in $ax_boost_user_unit_test_framework_lib $BN-$ax_boost_user_unit_test_framework_lib; do
                   LDFLAGS="${LDFLAGS} -l$ax_lib"
              			   AC_CACHE_CHECK(Boost::UnitTestFramework library linkage,
	      			    		   ax_cv_boost_unit_test_framework_link,
						  [AC_LANG_PUSH([C++])
                           AC_LINK_IFELSE([AC_LANG_PROGRAM([[@%:@include <boost/test/unit_test.hpp>
                                                        using boost::unit_test::test_suite;
                                                        test_suite* init_unit_test_suite( int argc, char * argv[] ) {
                                                        test_suite* test= BOOST_TEST_SUITE( "Unit test example 1" );
                                                        return test;
                                                        }
                                                   ]],
                                 [[ return 0;]])],
                                 link_unit_test_framework="yes",link_unit_test_framework="no")
			      AC_LANG_POP([C++])
                  ])
                  LDFLAGS="${saved_ldflags}"
			      if test "x$link_unit_test_framework" = "xyes"; then
                      BOOST_UNIT_TEST_FRAMEWORK_LIB="-l$ax_lib"
                      AC_SUBST(BOOST_UNIT_TEST_FRAMEWORK_LIB)
					  break
				  fi
               done
            fi
			if test "x$link_unit_test_framework" = "xno"; then
				AC_MSG_ERROR(Could not link against $ax_lib !)
			fi
		fi

		CPPFLAGS="$CPPFLAGS_SAVED"
    	LDFLAGS="$LDFLAGS_SAVED"
	fi
])
