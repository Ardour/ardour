#ifndef _GTKMM_CONFIG_H
#define _GTKMM_CONFIG_H

#include <gdkmmconfig.h>

/* Defined when the --enable-api-atkmm configure argument was given */
#define GTKMM_ATKMM_ENABLED 1

/* Define to omit deprecated API from gtkmm. */
/* #undef GTKMM_DISABLE_DEPRECATED */

/* Defined when the --enable-maemo-extensions configure argument was given */
/* #undef GTKMM_MAEMO_EXTENSIONS_ENABLED */

#ifdef GTKMM_DLL
# if defined(GTKMM_BUILD) && defined(_WINDLL)
   /* Do not dllexport as it is handled by gendef on MSVC */
#  define GTKMM_API
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
