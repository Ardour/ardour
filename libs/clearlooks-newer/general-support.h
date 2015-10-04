
#include <gmodule.h>
#include <glib.h>

/* macros to make sure that things are sane ... */

#define CHECK_DETAIL(detail, value) ((detail) && (!strcmp(value, detail)))

#define CHECK_ARGS					\
  g_return_if_fail (window != NULL);			\
  g_return_if_fail (style != NULL);

#define SANITIZE_SIZE					\
  g_return_if_fail (width  >= -1);			\
  g_return_if_fail (height >= -1);			\
                                                        \
  if ((width == -1) && (height == -1))			\
    gdk_drawable_get_size (window, &width, &height);	\
  else if (width == -1)					\
    gdk_drawable_get_size (window, &width, NULL);	\
  else if (height == -1)				\
    gdk_drawable_get_size (window, NULL, &height);

#define GE_EXPORT	G_MODULE_EXPORT
#define GE_INTERNAL	G_GNUC_INTERNAL

/* explicitly export with ggc, G_MODULE_EXPORT does not do this, this should
 * make it possible to compile with -fvisibility=hidden */
#ifdef G_HAVE_GNUC_VISIBILITY
# undef GE_EXPORT
# define GE_EXPORT	__attribute__((__visibility__("default")))
#endif

#if defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
# undef GE_EXPORT
# undef GE_INTERNAL
# define GE_EXPORT      __global
# define GE_INTERNAL    __hidden
#endif
