
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <libglademm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "xml.h"

extern "C"
{

//Declarations of the *_get_type() functions:

GType glade_xml_get_type(void);

//Declarations of the *_error_quark() functions:

} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Gnome { namespace Glade {  class Xml_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  } }

namespace Gnome { namespace Glade { 

void wrap_init()
{
  // Register Error domains:

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(glade_xml_get_type(), &Gnome::Glade::Xml_Class::wrap_new);

  // Register the gtkmm gtypes:
  Gnome::Glade::Xml::get_type();

} // wrap_init()

} //Glade
} //Gnome


