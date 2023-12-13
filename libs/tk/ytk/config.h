/* always defined to indicate that i18n is enabled */
/* #undef ENABLE_NLS */

/* Define the location where the catalogs will be installed */
#define GTK_LOCALEDIR "/ardour/share/locale"

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#ifndef __APPLE__
#define HAVE_BIND_TEXTDOMAIN_CODESET 1
#endif

/* Is the wctype implementation broken */
#ifdef __APPLE__
#define HAVE_BROKEN_WCTYPE 1
#endif

/* Define to 1 if you have the <ftw.h> header file. */
#define HAVE_FTW_H 1

/* Define to 1 if you have the `getresuid' function. */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_GETRESUID 1
#endif

#ifndef __APPLE__
/* Have GNU ftw */
#define HAVE_GNU_FTW 1
#endif

/* Define to 1 if ipc.h is available */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_IPC_H 1
#endif

#ifndef PLATFORM_WINDOWS
/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1
#endif

#ifndef PLATFORM_WINDOWS
#define HAVE_PWD_H 1
#endif

/* Have the Xrandr extension library */
/* #undef HAVE_RANDR */

/* Define to 1 if shm.h is available */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_SHM_H 1
#endif

/* Define to 1 if solaris xinerama is available */
/* #undef HAVE_SOLARIS_XINERAMA */

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if sys/sysinfo.h is available */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_SYS_SYSINFO_H 1
#endif

/* Define to 1 if sys/systeminfo.h is available */
/* #undef HAVE_SYS_SYSTEMINFO_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Have wchar.h include file */
#define HAVE_WCHAR_H 1

/* Have wctype.h include file */
#define HAVE_WCTYPE_H 1

/* Define if we have X11R6 */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_X11R6 1
#endif

/* Have the XCOMPOSITE X extension */
/* #undef HAVE_XCOMPOSITE */

/* Define to 1 if you have the `XConvertCase' function. */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_XCONVERTCASE 1
#endif

/* Have the Xcursor library */
/* #undef HAVE_XCURSOR */

/* Have the XDAMAGE X extension */
/* #undef HAVE_XDAMAGE */

/* Have the XFIXES X extension */
/* #undef HAVE_XFIXES */

/* Define to 1 if XFree Xinerama is available */
/* #undef HAVE_XFREE_XINERAMA */

/* Define to 1 if you have the `XInternAtoms' function. */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define HAVE_XINTERNATOMS 1

/* Define to use XKB extension */
#define HAVE_XKB 1

/* Define to 1 if xshm.h is available */
#define HAVE_XSHM_H 1

/* Have the SYNC extension library */
#define HAVE_XSYNC 1
#endif

/* Define if <X11/extensions/XIproto.h> needed for xReply */
/* #undef NEED_XIPROTO_H_FOR_XREPLY */

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.24.23"

/* Define to 1 if medialib is available and should be used */
/* #undef USE_MEDIALIB */

/* Define to 1 if medialib 2.5 is available */
/* #undef USE_MEDIALIB25 */

/* Define to 1 if no XInput should be used */
#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
#define XINPUT_NONE 1
#endif

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifdef __APPLE__
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif
#endif

#ifdef PLATFORM_WINDOWS
/* Define to `int' if <sys/types.h> doesn't define. */
#define gid_t int

/* Define to `int' if <sys/types.h> doesn't define. */
#define uid_t int
#endif
