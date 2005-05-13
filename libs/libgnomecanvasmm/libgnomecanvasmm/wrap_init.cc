
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <libgnomecanvasmm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "canvas.h"
#include "ellipse.h"
#include "rect.h"
#include "group.h"
#include "item.h"
#include "line.h"
#include "polygon.h"
#include "rect-ellipse.h"
#include "shape.h"
#include "pixbuf.h"
#include "rich-text.h"
#include "text.h"
#include "widget.h"
#include "path-def.h"
#include "bpath.h"

extern "C"
{

//Declarations of the *_get_type() functions:

GType gnome_canvas_bpath_get_type(void);
GType gnome_canvas_get_type(void);
GType gnome_canvas_ellipse_get_type(void);
GType gnome_canvas_group_get_type(void);
GType gnome_canvas_item_get_type(void);
GType gnome_canvas_line_get_type(void);
GType gnome_canvas_pixbuf_get_type(void);
GType gnome_canvas_polygon_get_type(void);
GType gnome_canvas_rect_get_type(void);
GType gnome_canvas_re_get_type(void);
GType gnome_canvas_rich_text_get_type(void);
GType gnome_canvas_shape_get_type(void);
GType gnome_canvas_text_get_type(void);
GType gnome_canvas_widget_get_type(void);

//Declarations of the *_error_quark() functions:

} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Gnome { namespace Canvas {  class Bpath_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Canvas_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Ellipse_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Group_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Item_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Line_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Pixbuf_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Polygon_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Rect_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class RectEllipse_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class RichText_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Shape_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Text_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }
namespace Gnome { namespace Canvas {  class Widget_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }

namespace Gnome { namespace Canvas { 

void wrap_init()
{
  // Register Error domains:

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(gnome_canvas_bpath_get_type(), &Gnome::Canvas::Bpath_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_get_type(), &Gnome::Canvas::Canvas_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_ellipse_get_type(), &Gnome::Canvas::Ellipse_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_group_get_type(), &Gnome::Canvas::Group_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_item_get_type(), &Gnome::Canvas::Item_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_line_get_type(), &Gnome::Canvas::Line_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_pixbuf_get_type(), &Gnome::Canvas::Pixbuf_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_polygon_get_type(), &Gnome::Canvas::Polygon_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_rect_get_type(), &Gnome::Canvas::Rect_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_re_get_type(), &Gnome::Canvas::RectEllipse_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_rich_text_get_type(), &Gnome::Canvas::RichText_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_shape_get_type(), &Gnome::Canvas::Shape_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_text_get_type(), &Gnome::Canvas::Text_Class::wrap_new);
  Glib::wrap_register(gnome_canvas_widget_get_type(), &Gnome::Canvas::Widget_Class::wrap_new);

  // Register the gtkmm gtypes:
  Gnome::Canvas::Bpath::get_type();
  Gnome::Canvas::Canvas::get_type();
  Gnome::Canvas::Ellipse::get_type();
  Gnome::Canvas::Group::get_type();
  Gnome::Canvas::Item::get_type();
  Gnome::Canvas::Line::get_type();
  Gnome::Canvas::Pixbuf::get_type();
  Gnome::Canvas::Polygon::get_type();
  Gnome::Canvas::Rect::get_type();
  Gnome::Canvas::RectEllipse::get_type();
  Gnome::Canvas::RichText::get_type();
  Gnome::Canvas::Shape::get_type();
  Gnome::Canvas::Text::get_type();
  Gnome::Canvas::Widget::get_type();

} // wrap_init()

} //Canvas
} //Gnome


