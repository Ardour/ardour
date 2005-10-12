// -*- c++ -*-
#ifndef _LIBGNOMECANVASMM_SIMPLERECT_P_H
#define _LIBGNOMECANVASMM_SIMPLERECT_P_H

#include <glibmm/class.h>

namespace Gnome
{

namespace Canvas
{

class SimpleRect_Class : public Glib::Class
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef SimpleRect CppObjectType;
  typedef GnomeCanvasSimpleRect BaseObjectType;
  typedef GnomeCanvasSimpleRectClass BaseClassType;
  typedef Shape_Class CppClassParent;
  typedef GnomeCanvasItemClass BaseClassParent;

  friend class SimpleRect;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  const Glib::Class& init();

  static void class_init_function(void* g_class, void* class_data);

  static Glib::ObjectBase* wrap_new(GObject*);

protected:

  //Callbacks (default signal handlers):
  //These will call the *_impl member methods, which will then call the existing default signal callbacks, if any.
  //You could prevent the original default signal handlers being called by overriding the *_impl method.

  //Callbacks (virtual functions):
};


} // namespace Canvas

} // namespace Gnome

#endif /* _LIBGNOMECANVASMM_SIMPLERECT_P_H */

