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

#endif /* __pbd_gstdio_compat_h__ */
