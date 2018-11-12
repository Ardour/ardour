/* glib mingw64/win32 compatibility
 *
 * see http://pidgin.im/pipermail/devel/2014-April/023475.html
 * and https://pidgin.im/pipermail/commits/2014-April/025031.html
 * http://tracker.ardour.org/view.php?id=6575
 */

#ifndef __pbd_gstdio_compat_h__
#define __pbd_gstdio_compat_h__

#include <glib/gstdio.h>

/* glib's definition of g_stat+GStatBuf is broken for mingw64-w32
 * (and possibly other 32-bit windows)
 */
#if defined(_WIN32) && !defined(_MSC_VER) && !defined(_WIN64)
typedef struct _stat GStatBufW32;
static inline int
pbd_g_stat(const gchar *filename, GStatBufW32 *buf)
{
	return g_stat(filename, (GStatBuf*)buf);
}
#  define GStatBuf GStatBufW32
#  define g_stat pbd_g_stat
#  define g_lstat pbd_g_stat
#endif

/* 64bit mingw -- use _mingw_stat64.h
 *
 * glib-2.42.0 wrongly uses _wstat() with 'struct stat' (only MSVC is special cased),
 * while the windows API is
 *   int _wstat(const wchar_t*, struct _stat*)
 * note that  struct _stat != struct stat;
 */
#if defined(_WIN32) && !defined(_MSC_VER) && defined(_WIN64)
#include <windows.h>
#include <errno.h>
#include <wchar.h>

typedef struct _stat GStatBufW64;
static inline int
pbd_g_stat(const gchar* filename, GStatBufW64* buf)
{
	gunichar2* wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
	if (wfilename == NULL) {
		errno = EINVAL;
		return -1;
	}

	int len = wcslen ((wchar_t*)wfilename);
	while (len > 0 && G_IS_DIR_SEPARATOR (wfilename[len-1])) {
		--len;
	}
	if (len > 0 && (!g_path_is_absolute (filename) || len > g_path_skip_root (filename) - filename)) {
		wfilename[len] = '\0';
	}

	int retval = _wstat ((wchar_t*)wfilename, buf);
	int save_errno = errno;
	g_free (wfilename);
	errno = save_errno;
	return retval;
}
#  define GStatBuf GStatBufW64
#  define g_stat pbd_g_stat
#  define g_lstat pbd_g_stat
#endif

#endif /* __pbd_gstdio_compat_h__ */
