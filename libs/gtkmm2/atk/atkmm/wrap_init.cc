
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <atkmm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "action.h"
#include "component.h"
#include "document.h"
#include "editabletext.h"
#include "hyperlink.h"
#include "hypertext.h"
#include "image.h"
#include "implementor.h"
#include "noopobject.h"
#include "object.h"
#include "objectaccessible.h"
#include "relation.h"
#include "relationset.h"
#include "selection.h"
#include "stateset.h"
#include "streamablecontent.h"
#include "table.h"
#include "text.h"
#include "value.h"

extern "C"
{

//Declarations of the *_get_type() functions:

GType atk_hyperlink_get_type(void);
GType atk_no_op_object_get_type(void);
GType atk_object_get_type(void);
GType atk_gobject_accessible_get_type(void);
GType atk_relation_get_type(void);
GType atk_relation_set_get_type(void);
GType atk_state_set_get_type(void);

//Declarations of the *_error_quark() functions:

} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Atk {  class Hyperlink_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Atk {  class NoOpObject_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Atk {  class Object_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Atk {  class ObjectAccessible_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Atk {  class Relation_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Atk {  class RelationSet_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Atk {  class StateSet_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }

namespace Atk { 

void wrap_init()
{
  // Register Error domains:

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(atk_hyperlink_get_type(), &Atk::Hyperlink_Class::wrap_new);
  Glib::wrap_register(atk_no_op_object_get_type(), &Atk::NoOpObject_Class::wrap_new);
  Glib::wrap_register(atk_object_get_type(), &Atk::Object_Class::wrap_new);
  Glib::wrap_register(atk_gobject_accessible_get_type(), &Atk::ObjectAccessible_Class::wrap_new);
  Glib::wrap_register(atk_relation_get_type(), &Atk::Relation_Class::wrap_new);
  Glib::wrap_register(atk_relation_set_get_type(), &Atk::RelationSet_Class::wrap_new);
  Glib::wrap_register(atk_state_set_get_type(), &Atk::StateSet_Class::wrap_new);

  // Register the gtkmm gtypes:
  Atk::Hyperlink::get_type();
  Atk::NoOpObject::get_type();
  Atk::Object::get_type();
  Atk::ObjectAccessible::get_type();
  Atk::Relation::get_type();
  Atk::RelationSet::get_type();
  Atk::StateSet::get_type();

} // wrap_init()

} //Atk


