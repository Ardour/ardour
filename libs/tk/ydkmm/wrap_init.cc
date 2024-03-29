// Generated by generate_wrap_init.pl -- DO NOT MODIFY!

#define GLIBMM_INCLUDED_FROM_WRAP_INIT_CC
#include <glibmm.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <gdkmm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:
#include "gdkmm/bitmap.h"
#include "gdkmm/color.h"
#include "gdkmm/colormap.h"
#include "gdkmm/cursor.h"
#include "gdkmm/device.h"
#include "gdkmm/display.h"
#include "gdkmm/displaymanager.h"
#include "gdkmm/dragcontext.h"
#include "gdkmm/drawable.h"
#include "gdkmm/event.h"
#include "gdkmm/gc.h"
#include "gdkmm/image.h"
#include "gdkmm/pixbuf.h"
#include "gdkmm/pixbufanimation.h"
#include "gdkmm/pixbufanimationiter.h"
#include "gdkmm/pixbufformat.h"
#include "gdkmm/pixbufloader.h"
#include "gdkmm/pixmap.h"
#include "gdkmm/rectangle.h"
#include "gdkmm/region.h"
#include "gdkmm/rgbcmap.h"
#include "gdkmm/screen.h"
#include "gdkmm/types.h"
#include "gdkmm/visual.h"
#include "gdkmm/window.h"

extern "C"
{
//Declarations of the *_get_type() functions:

GType gdk_colormap_get_type(void);
GType gdk_device_get_type(void);
GType gdk_display_get_type(void);
GType gdk_display_manager_get_type(void);
GType gdk_drag_context_get_type(void);
GType gdk_drawable_get_type(void);
GType gdk_gc_get_type(void);
GType gdk_image_get_type(void);
GType gdk_pixbuf_get_type(void);
GType gdk_pixbuf_animation_get_type(void);
GType gdk_pixbuf_animation_iter_get_type(void);
GType gdk_pixbuf_loader_get_type(void);
GType gdk_pixmap_get_type(void);
GType gdk_screen_get_type(void);
GType gdk_visual_get_type(void);
GType gdk_window_object_get_type(void);

//Declarations of the *_error_quark() functions:

GQuark gdk_pixbuf_error_quark(void);
} // extern "C"

namespace Gdk {

//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

class Colormap_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Device_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Display_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class DisplayManager_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class DragContext_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Drawable_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class GC_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Image_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Pixbuf_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class PixbufAnimation_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class PixbufAnimationIter_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class PixbufLoader_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Pixmap_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Screen_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Visual_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };
class Window_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };

void wrap_init()
{
  // Register Error domains in the main namespace:
  Glib::Error::register_domain(gdk_pixbuf_error_quark(), &PixbufError::throw_func);

  // Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(gdk_colormap_get_type(), &Colormap_Class::wrap_new);
  Glib::wrap_register(gdk_device_get_type(), &Device_Class::wrap_new);
  Glib::wrap_register(gdk_display_get_type(), &Display_Class::wrap_new);
  Glib::wrap_register(gdk_display_manager_get_type(), &DisplayManager_Class::wrap_new);
  Glib::wrap_register(gdk_drag_context_get_type(), &DragContext_Class::wrap_new);
  Glib::wrap_register(gdk_drawable_get_type(), &Drawable_Class::wrap_new);
  Glib::wrap_register(gdk_gc_get_type(), &GC_Class::wrap_new);
  Glib::wrap_register(gdk_image_get_type(), &Image_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_get_type(), &Pixbuf_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_animation_get_type(), &PixbufAnimation_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_animation_iter_get_type(), &PixbufAnimationIter_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_loader_get_type(), &PixbufLoader_Class::wrap_new);
  Glib::wrap_register(gdk_pixmap_get_type(), &Pixmap_Class::wrap_new);
  Glib::wrap_register(gdk_screen_get_type(), &Screen_Class::wrap_new);
  Glib::wrap_register(gdk_visual_get_type(), &Visual_Class::wrap_new);
  Glib::wrap_register(gdk_window_object_get_type(), &Window_Class::wrap_new);

  // Register the gtkmm gtypes:
  Colormap::get_type();
  Device::get_type();
  Display::get_type();
  DisplayManager::get_type();
  DragContext::get_type();
  Drawable::get_type();
  GC::get_type();
  Image::get_type();
  Pixbuf::get_type();
  PixbufAnimation::get_type();
  PixbufAnimationIter::get_type();
  PixbufLoader::get_type();
  Pixmap::get_type();
  Screen::get_type();
  Visual::get_type();
  Window::get_type();

} // wrap_init()

} // Gdk

