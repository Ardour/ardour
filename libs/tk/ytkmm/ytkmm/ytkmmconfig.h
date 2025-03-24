#ifndef _GTKMM_CONFIG_H
#define _GTKMM_CONFIG_H

#include <ydkmm/ydkmmconfig.h>

/* Defined when the --enable-api-atkmm configure argument was given */
#define GTKMM_ATKMM_ENABLED 1

/* Define to omit deprecated API from gtkmm. */
/* #undef GTKMM_DISABLE_DEPRECATED */

/* Defined when the --enable-maemo-extensions configure argument was given */
/* #undef GTKMM_MAEMO_EXTENSIONS_ENABLED */

/* Define when building gtkmm as a static library */
/* #undef GTKMM_STATIC_LIB */

/* Enable DLL-specific stuff only when not building a static library */
# if (!defined(GTKMM_STATIC_LIB) && defined(_WIN32) && !defined(__CYGWIN__) && !defined(COMPILER_MINGW))
#  if !defined(GTKMM_DLL)
#   define GTKMM_DLL 1
#  endif
# endif

#ifdef GTKMM_DLL
# if defined(GTKMM_BUILD) && defined(_WINDLL)
   /* GTKMM_API was previously undefined here.  It was getting handled  */
   /* by 'gendef' on MSVC but 'gendef' stopped working a long time ago  */
#  define GTKMM_API __declspec(dllexport)
# elif !defined(GTKMM_BUILD)
#  define GTKMM_API __declspec(dllimport)
# else
   /* Build a static library */
#  define GTKMM_API
# endif /* GTKMM_BUILD - _WINDLL */
#else
# define GTKMM_API
#endif /* GTKMM_DLL */

#endif /* !_GTKMM_CONFIG_H */
