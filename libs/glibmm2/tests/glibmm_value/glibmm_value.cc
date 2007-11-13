
#include <glibmm.h>

struct Foo
{
  int bar;
};

namespace Gtk
{
class Widget;
}

void some_method()
{
// custom copyable
  Glib::Value<Foo> value_foo;

  // custom pointer
  Glib::Value<Foo*> value_foo_pointer;
  Glib::Value<const Foo*> value_foo_const_pointer;

// Glib::Object pointer
  Glib::Value<Gtk::Widget*> value_widget_pointer;
  Glib::Value<const Gtk::Widget*> value_widget_const_pointer;
}

// Glib::Object RefPtr<>

//template Glib::Value< Glib::RefPtr<Gdk::Pixbuf> >;
//template Glib::Value< Glib::RefPtr<const Gdk::Pixbuf> >;

