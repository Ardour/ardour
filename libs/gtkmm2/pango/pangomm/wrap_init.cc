
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <pangomm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "attributes.h"
#include "attriter.h"
#include "attrlist.h"
#include "cairofontmap.h"
#include "color.h"
#include "context.h"
#include "coverage.h"
#include "font.h"
#include "fontdescription.h"
#include "fontface.h"
#include "fontfamily.h"
#include "fontmap.h"
#include "fontmetrics.h"
#include "fontset.h"
#include "glyph.h"
#include "glyphstring.h"
#include "item.h"
#include "language.h"
#include "layout.h"
#include "layoutiter.h"
#include "layoutline.h"
#include "layoutrun.h"
#include "rectangle.h"
#include "renderer.h"
#include "tabarray.h"

extern "C"
{

//Declarations of the *_get_type() functions:

GType pango_context_get_type(void);
GType pango_font_get_type(void);
GType pango_font_face_get_type(void);
GType pango_font_family_get_type(void);
GType pango_font_map_get_type(void);
GType pango_fontset_get_type(void);
GType pango_layout_get_type(void);
GType pango_renderer_get_type(void);

//Declarations of the *_error_quark() functions:

} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Pango {  class Context_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class Font_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class FontFace_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class FontFamily_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class FontMap_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class Fontset_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class Layout_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Pango {  class Renderer_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }

namespace Pango { 

void wrap_init()
{
  // Register Error domains:

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(pango_context_get_type(), &Pango::Context_Class::wrap_new);
  Glib::wrap_register(pango_font_get_type(), &Pango::Font_Class::wrap_new);
  Glib::wrap_register(pango_font_face_get_type(), &Pango::FontFace_Class::wrap_new);
  Glib::wrap_register(pango_font_family_get_type(), &Pango::FontFamily_Class::wrap_new);
  Glib::wrap_register(pango_font_map_get_type(), &Pango::FontMap_Class::wrap_new);
  Glib::wrap_register(pango_fontset_get_type(), &Pango::Fontset_Class::wrap_new);
  Glib::wrap_register(pango_layout_get_type(), &Pango::Layout_Class::wrap_new);
  Glib::wrap_register(pango_renderer_get_type(), &Pango::Renderer_Class::wrap_new);

  // Register the gtkmm gtypes:
  Pango::Context::get_type();
  Pango::Font::get_type();
  Pango::FontFace::get_type();
  Pango::FontFamily::get_type();
  Pango::FontMap::get_type();
  Pango::Fontset::get_type();
  Pango::Layout::get_type();
  Pango::Renderer::get_type();

} // wrap_init()

} //Pango


