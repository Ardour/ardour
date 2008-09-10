
AC_INIT(RubberBand, 0.1, cannam@all-day-breakfast.com)

AC_CONFIG_SRCDIR(src/StretcherImpl.h)
AC_PROG_CXX
AC_HEADER_STDC
AC_C_BIGENDIAN

PKG_CHECK_MODULES([SRC],[samplerate])
AC_SUBST(SRC_CFLAGS)
AC_SUBST(SRC_LIBS)

PKG_CHECK_MODULES([SNDFILE],[sndfile])
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

PKG_CHECK_MODULES([FFTW],[fftw3])
AC_SUBST(FFTW_CFLAGS)
AC_SUBST(FFTW_LIBS)

AC_CHECK_HEADERS(ladspa.h)
AC_CHECK_HEADERS(pthread.h)

PKG_CHECK_MODULES([Vamp],[vamp-sdk])
AC_SUBST(Vamp_CFLAGS)
AC_SUBST(Vamp_LIBS)

changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CXXFLAGS " in
    *[\ \	]-fPIC\ -Wall[\ \	]*) ;;
    *) CXXFLAGS="$CXXFLAGS -fPIC -Wall" ;;
  esac
fi
changequote([,])dnl

AC_OUTPUT([Makefile])

