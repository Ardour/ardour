
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <gdkmm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "bitmap.h"
#include "color.h"
#include "colormap.h"
#include "cursor.h"
#include "device.h"
#include "display.h"
#include "displaymanager.h"
#include "dragcontext.h"
#include "drawable.h"
#include "event.h"
#include "gc.h"
#include "image.h"
#include "pixbuf.h"
#include "pixbufanimation.h"
#include "pixbufanimationiter.h"
#include "pixmap.h"
#include "pixbufformat.h"
#include "pixbufloader.h"
#include "rectangle.h"
#include "region.h"
#include "rgbcmap.h"
#include "screen.h"
#include "types.h"
#include "visual.h"
#include "window.h"

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


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Gdk {  class Colormap_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Device_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Display_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class DisplayManager_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class DragContext_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Drawable_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class GC_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Image_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Pixbuf_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class PixbufAnimation_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class PixbufAnimationIter_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class PixbufLoader_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Pixmap_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Screen_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Visual_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gdk {  class Window_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }

namespace Gdk { 

void wrap_init()
{
  // Register Error domains:
  Glib::Error::register_domain(gdk_pixbuf_error_quark(), &Gdk::PixbufError::throw_func);

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(gdk_colormap_get_type(), &Gdk::Colormap_Class::wrap_new);
  Glib::wrap_register(gdk_device_get_type(), &Gdk::Device_Class::wrap_new);
  Glib::wrap_register(gdk_display_get_type(), &Gdk::Display_Class::wrap_new);
  Glib::wrap_register(gdk_display_manager_get_type(), &Gdk::DisplayManager_Class::wrap_new);
  Glib::wrap_register(gdk_drag_context_get_type(), &Gdk::DragContext_Class::wrap_new);
  Glib::wrap_register(gdk_drawable_get_type(), &Gdk::Drawable_Class::wrap_new);
  Glib::wrap_register(gdk_gc_get_type(), &Gdk::GC_Class::wrap_new);
  Glib::wrap_register(gdk_image_get_type(), &Gdk::Image_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_get_type(), &Gdk::Pixbuf_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_animation_get_type(), &Gdk::PixbufAnimation_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_animation_iter_get_type(), &Gdk::PixbufAnimationIter_Class::wrap_new);
  Glib::wrap_register(gdk_pixbuf_loader_get_type(), &Gdk::PixbufLoader_Class::wrap_new);
  Glib::wrap_register(gdk_pixmap_get_type(), &Gdk::Pixmap_Class::wrap_new);
  Glib::wrap_register(gdk_screen_get_type(), &Gdk::Screen_Class::wrap_new);
  Glib::wrap_register(gdk_visual_get_type(), &Gdk::Visual_Class::wrap_new);
  Glib::wrap_register(gdk_window_object_get_type(), &Gdk::Window_Class::wrap_new);

  // Register the gtkmm gtypes:
  Gdk::Colormap::get_type();
  Gdk::Device::get_type();
  Gdk::Display::get_type();
  Gdk::DisplayManager::get_type();
  Gdk::DragContext::get_type();
  Gdk::Drawable::get_type();
  Gdk::GC::get_type();
  Gdk::Image::get_type();
  Gdk::Pixbuf::get_type();
  Gdk::PixbufAnimation::get_type();
  Gdk::PixbufAnimationIter::get_type();
  Gdk::PixbufLoader::get_type();
  Gdk::Pixmap::get_type();
  Gdk::Screen::get_type();
  Gdk::Visual::get_type();
  Gdk::Window::get_type();

} // wrap_init()

} //Gdk


