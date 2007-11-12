/* gtk/gtkmmconfig.h.  Generated from gtkmmconfig.h.in by configure.  */
#ifndef _GTKMM_CONFIG_H
#define _GTKMM_CONFIG_H 1

#include <gdkmmconfig.h>

/* version numbers */
#define GTKMM_MAJOR_VERSION 2
#define GTKMM_MINOR_VERSION 10
#define GTKMM_MICRO_VERSION 8

#define GTKMM_ATKMM_ENABLED 1
/* #undef GTKMM_MAEMO_EXTENSIONS_ENABLED */


#ifdef GLIBMM_CONFIGURE
/* compiler feature tests that are used during compile time and run-time
   by gtkmm only. */

/* SUN Forte, AIX, and Tru64 have the problem with flockfile and
   funlockfile - configure finds it but the compiler can not find it
   while compiling demowindow.cc. undef HAVE_FLOCKFILE and
   HAVE_FUNLOCKFILE for now, so that it builds on those platforms. */
	
#define HAVE_FLOCKFILE 1
#define HAVE_FUNLOCKFILE 1
#define HAVE_MKFIFO 1
/* #undef GETC_UNLOCKED */

#endif /* GLIBMM_CONFIGURE */


#ifdef GLIBMM_DLL
  #if defined(GTKMM_BUILD) && defined(_WINDLL)
    // Do not dllexport as it is handled by gendef on MSVC
    #define GTKMM_API 
  #elif !defined(GTKMM_BUILD)
    #define GTKMM_API __declspec(dllimport)
  #else
    /* Build a static library */
    #define GTKMM_API
  #endif /* GTKMM_BUILD - _WINDLL */
#else
  #define GTKMM_API
#endif /* GLIBMM_DLL */

#endif /* _GTKMM_CONFIG_H */

