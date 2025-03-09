/* gdkconfig.h
 *
 * This is a generated file.  Please modify `configure.in'
 */

#ifndef GDKCONFIG_H
#define GDKCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef GSEAL
/* introduce GSEAL() here for all of Gdk and Gtk+ without the need to modify GLib */
#  ifdef GSEAL_ENABLE
#    define GSEAL(ident)      _g_sealed__ ## ident
#  else
#    define GSEAL(ident)      ident
#  endif
#endif /* !GSEAL */


#ifdef __APPLE__
# define GDK_WINDOWING_QUARTZ
# define GDK_HAVE_BROKEN_WCTYPE 1
# define GDK_NATIVE_WINDOW_POINTER
#elif defined PLATFORM_WINDOWS
# define GDK_WINDOWING_WIN32
# define GDK_NATIVE_WINDOW_POINTER
#else
# define GDK_WINDOWING_X11
#endif

#define GDK_HAVE_WCHAR_H 1
#define GDK_HAVE_WCTYPE_H 1

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GDKCONFIG_H */
