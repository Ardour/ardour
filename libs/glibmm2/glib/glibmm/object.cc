// -*- c++ -*-
/* $Id: object.cc 369 2007-01-20 10:19:33Z daniel $ */

/* Copyright 1998-2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/object.h>
#include <glibmm/private/object_p.h>
#include <glibmm/property.h>

#include <glib.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include <cstdarg>
#include <cstring>

//Weak references:
//I'm not sure what the point of these are apart from being a hacky way out of circular references,
//but maybe we could make it easier to use them by making a Java Reference Object -style class like so:
// Glib::WeakRef<SomeDerivedObject> weakrefSomeObject(object1);
// ...
// if(weakrefSomeObject->isStillAlive())
// {
//   weakrefSomeObject->some_method();
// }
// else
// {
//   //Deal with it, maybe recreating the object.
// }
//
// Without this, the coder has to define his own signal handler which sets his own isStillAlive boolean.
// weakrefSomeObject<> could still have its own signal_destroyed signal so that coders can choose to deal
// with the destruction as soon as it happens instead of just checking later before they try to use it.


namespace Glib
{

ConstructParams::ConstructParams(const Glib::Class& glibmm_class_)
:
  glibmm_class (glibmm_class_),
  n_parameters (0),
  parameters   (0)
{}

/*
 * The implementation is mostly copied from gobject.c, with some minor tweaks.
 * Basically, it looks up each property name to get its GType, and then uses
 * G_VALUE_COLLECT() to store the varargs argument in a GValue of the correct
 * type.
 *
 * Note that the property name arguments are assumed to be static string
 * literals.  No attempt is made to copy the string content.  This is no
 * different from g_object_new().
 */
ConstructParams::ConstructParams(const Glib::Class& glibmm_class_,
                                 const char* first_property_name, ...)
:
  glibmm_class (glibmm_class_),
  n_parameters (0),
  parameters   (0)
{
  va_list var_args;
  va_start(var_args, first_property_name);

  GObjectClass *const g_class =
      static_cast<GObjectClass*>(g_type_class_ref(glibmm_class.get_type()));

  unsigned int n_alloced_params = 0;
  char*        collect_error    = 0; // output argument of G_VALUE_COLLECT()

  for(const char* name = first_property_name;
      name != 0;
      name = va_arg(var_args, char*))
  {
    GParamSpec *const pspec = g_object_class_find_property(g_class, name);

    if(!pspec)
    {
      g_warning("Glib::ConstructParams::ConstructParams(): "
                "object class \"%s\" has no property named \"%s\"",
                g_type_name(glibmm_class.get_type()), name);
      break;
    }

    if(n_parameters >= n_alloced_params)
      parameters = g_renew(GParameter, parameters, n_alloced_params += 8);

    GParameter& param = parameters[n_parameters];

    param.name = name;
    param.value.g_type = 0;

    // Fill the GValue with the current vararg, and move on to the next one.
    g_value_init(&param.value, G_PARAM_SPEC_VALUE_TYPE(pspec));
    G_VALUE_COLLECT(&param.value, var_args, 0, &collect_error);

    if(collect_error)
    {
      g_warning("Glib::ConstructParams::ConstructParams(): %s", collect_error);
      g_free(collect_error);
      g_value_unset(&param.value);
      break;
    }

    ++n_parameters;
  }

  g_type_class_unref(g_class);

  va_end(var_args);
}

ConstructParams::~ConstructParams()
{
  while(n_parameters > 0)
    g_value_unset(&parameters[--n_parameters].value);

  g_free(parameters);
}

/*
 * Some compilers require the existence of a copy constructor in certain
 * usage contexts.  This implementation is fully functional, but unlikely
 * to be ever actually called due to optimization.
 */
ConstructParams::ConstructParams(const ConstructParams& other)
:
  glibmm_class  (other.glibmm_class),
  n_parameters  (other.n_parameters),
  parameters    (g_new(GParameter, n_parameters))
{
  for(unsigned int i = 0; i < n_parameters; ++i)
  {
    parameters[i].name = other.parameters[i].name;
    parameters[i].value.g_type = 0;

    g_value_init(&parameters[i].value, G_VALUE_TYPE(&other.parameters[i].value));
    g_value_copy(&other.parameters[i].value, &parameters[i].value);
  }
}


/**** Glib::Object_Class ***************************************************/

const Glib::Class& Object_Class::init()
{
  if(!gtype_)
  {
    class_init_func_ = &Object_Class::class_init_function;
    register_derived_type(G_TYPE_OBJECT);
  }

  return *this;
}

void Object_Class::class_init_function(void*, void*)
{}

Object* Object_Class::wrap_new(GObject* object)
{
  return new Object(object);
}


/**** Glib::Object *********************************************************/

// static data
Object::CppClassType Object::object_class_;

Object::Object()
{
  // This constructor is ONLY for derived classes that are NOT wrappers of
  // derived C objects.  For instance, Gtk::Object should NOT use this
  // constructor.

  //g_warning("Object::Object(): Did you really mean to call this?");

  // If Glib::ObjectBase has been constructed with a custom typeid, we derive
  // a new GType on the fly.  This works because ObjectBase is a virtual base
  // class, therefore its constructor is always executed first.

  GType object_type = G_TYPE_OBJECT; // the default -- not very useful

  if(custom_type_name_ && !is_anonymous_custom_())
  {
    object_class_.init();
    // This creates a type that is derived (indirectly) from GObject.
    object_type = object_class_.clone_custom_type(custom_type_name_);
  }

  void *const new_object = g_object_newv(object_type, 0, 0);

  // Connect the GObject and Glib::Object instances.
  ObjectBase::initialize(static_cast<GObject*>(new_object));

}

Object::Object(const Glib::ConstructParams& construct_params)
{
  GType object_type = construct_params.glibmm_class.get_type();

  // If Glib::ObjectBase has been constructed with a custom typeid, we derive
  // a new GType on the fly.  This works because ObjectBase is a virtual base
  // class, therefore its constructor is always executed first.

  if(custom_type_name_ && !is_anonymous_custom_())
    object_type = construct_params.glibmm_class.clone_custom_type(custom_type_name_);

  // Create a new GObject with the specified array of construct properties.
  // This works with custom types too, since those inherit the properties of
  // their base class.

  void *const new_object = g_object_newv(
      object_type, construct_params.n_parameters, construct_params.parameters);

  // Connect the GObject and Glib::Object instances.
  ObjectBase::initialize(static_cast<GObject*>(new_object));
}

Object::Object(GObject* castitem)
{
  //I disabled this check because libglademm really does need to do this.
  //(actually it tells libglade to instantiate "gtkmm_" types.
  //The 2nd instance bug will be caught elsewhere anyway.
/*
  static const char gtkmm_prefix[] = "gtkmm__";
  const char *const type_name = G_OBJECT_TYPE_NAME(castitem);

  if(strncmp(type_name, gtkmm_prefix, sizeof(gtkmm_prefix) - 1) == 0)
  {
    g_warning("Glib::Object::Object(GObject*): "
              "An object of type '%s' was created directly via g_object_new(). "
              "The Object::Object(const Glib::ConstructParams&) constructor "
              "should be used instead.\n"
              "This could happen if the C instance lived longer than the C++ instance, so that "
              "a second C++ instance was created automatically to wrap it. That would be a gtkmm bug that you should report.",
               type_name);
  }
*/

  // Connect the GObject and Glib::Object instances.
  ObjectBase::initialize(castitem);
}

Object::~Object()
{
  cpp_destruction_in_progress_ = true;
}

/*
RefPtr<Object> Object::create()
{
  // Derived classes will actually return RefPtr<>s that contain useful instances.
  return RefPtr<Object>();
}
*/

GType Object::get_type()
{
  return object_class_.init().get_type();
}

GType Object::get_base_type()
{
  return G_TYPE_OBJECT;
}

// Data services
void* Object::get_data(const QueryQuark& id)
{
  return g_object_get_qdata(gobj(),id);
}

void Object::set_data(const Quark& id, void* data)
{
  g_object_set_qdata(gobj(),id,data);
}

void Object::set_data(const Quark& id, void* data, DestroyNotify destroy)
{
  g_object_set_qdata_full(gobj(), id, data, destroy);
}

void Object::remove_data(const QueryQuark& id)
{
  // missing in glib??
  g_return_if_fail(id.id() > 0);
  g_datalist_id_remove_data(&gobj()->qdata, id);
}

void* Object::steal_data(const QueryQuark& id)
{
  return g_object_steal_qdata(gobj(), id);
}

} // namespace Glib

