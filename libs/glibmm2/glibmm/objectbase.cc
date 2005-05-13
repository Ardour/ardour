// -*- c++ -*-
/* $Id$ */

/* Copyright 2002 The gtkmm Development Team
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

#include <glib-object.h>

#include <glibmm/quark.h>
#include <glibmm/objectbase.h>


namespace
{

// Used by the Glib::ObjectBase default ctor.  Using an explicitly defined
// char array rather than a string literal allows for fast pointer comparison,
// which is otherwise not guaranteed to work.

const char anonymous_custom_type_name[] = "gtkmm__anonymous_custom_type";

} // anonymous namespace


namespace Glib
{

/**** Glib::ObjectBase *****************************************************/

ObjectBase::ObjectBase()
:
  gobject_                      (0),
  custom_type_name_             (anonymous_custom_type_name),
  cpp_destruction_in_progress_  (false)
{}

ObjectBase::ObjectBase(const char* custom_type_name)
:
  gobject_                      (0),
  custom_type_name_             (custom_type_name),
  cpp_destruction_in_progress_  (false)
{}

ObjectBase::ObjectBase(const std::type_info& custom_type_info)
:
  gobject_                      (0),
  custom_type_name_             (custom_type_info.name()),
  cpp_destruction_in_progress_  (false)
{}

// initialize() actually initializes the wrapper.  Glib::ObjectBase is used
// as virtual base class, which means the most-derived class' ctor invokes
// the Glib::ObjectBase ctor -- thus it's useless for Glib::Object.
//
void ObjectBase::initialize(GObject* castitem)
{
  if(gobject_)
  {
    // initialize() might be called twice when used with MI, e.g. by the ctors
    // of Glib::Object and Glib::Interface.  However, they must both refer to
    // the same underlying GObject instance.
    //
    g_assert(gobject_ == castitem);

    // TODO: Think about it.  Will this really be called twice?
    g_printerr("ObjectBase::initialize() called twice for the same GObject\n");

    return; // Don't initialize the wrapper twice.
  }

  //g_print("%s : %s\n", G_GNUC_PRETTY_FUNCTION, G_OBJECT_TYPE_NAME(castitem));

  gobject_ = castitem;
  _set_current_wrapper(castitem);
}

ObjectBase::~ObjectBase()
{
  // Normally, gobject_ should always be 0 at this point, because:
  //
  // a) Gtk::Object handles memory management on its own and always resets
  //    the gobject_ pointer in its destructor.
  //
  // b) Glib::Object instances that aren't Gtk::Objects will always be
  //    deleted by the destroy_notify_() virtual method.  Calling delete
  //    on a Glib::Object is a programming error.
  //
  // The *only* situation where gobject_ is validly not 0 at this point
  // happens if a derived class's ctor throws an exception.  In that case
  // we have to call g_object_unref() on our own.
  //
  if(GObject *const gobject = gobject_)
  {
#ifdef GLIBMM_DEBUG_REFCOUNTING
    g_warning("(Glib::ObjectBase::~ObjectBase): gobject_ = %p", (void*) gobject_);
#endif

    gobject_ = 0;

#ifdef GLIBMM_DEBUG_REFCOUNTING
    g_warning("(Glib::ObjectBase::~ObjectBase): before g_object_steal_qdata()");
#endif

    // Remove the pointer to the wrapper from the underlying instance.
    // This does _not_ cause invocation of the destroy_notify callback.
    g_object_steal_qdata(gobject, quark_);

#ifdef GLIBMM_DEBUG_REFCOUNTING
    g_warning("(Glib::ObjectBase::~ObjectBase): calling g_object_unref()");
#endif

    g_object_unref(gobject);
  }
}

void ObjectBase::reference() const
{
  GLIBMM_DEBUG_REFERENCE(this, gobject_);
  g_object_ref(gobject_);
}

void ObjectBase::unreference() const
{
  GLIBMM_DEBUG_UNREFERENCE(this, gobject_);
  g_object_unref(gobject_);
}

GObject* ObjectBase::gobj_copy() const
{
  reference();
  return gobject_;
}

void ObjectBase::_set_current_wrapper(GObject* object)
{
  // Store a pointer to this wrapper in the underlying instance, so that we
  // never create a second wrapper for the same underlying instance.  Also,
  // specify a callback that will tell us when it's time to delete this C++
  // wrapper instance:

  if(object)
  {
    if(!g_object_get_qdata(object, Glib::quark_))
    {
      g_object_set_qdata_full(object, Glib::quark_, this, &destroy_notify_callback_);
    }
    else
    {
      g_warning("This object, of type %s, already has a wrapper.\n"
                "You should use wrap() instead of a constructor.",
                G_OBJECT_TYPE_NAME(object));
    }
  }
}

// static
ObjectBase* ObjectBase::_get_current_wrapper(GObject* object)
{
  if(object)
    return static_cast<ObjectBase*>(g_object_get_qdata(object, Glib::quark_));
  else
    return 0;
}

// static
void ObjectBase::destroy_notify_callback_(void* data)
{
  //GLIBMM_LIFECYCLE

  // This method is called (indirectly) from g_object_run_dispose().
  // Get the C++ instance associated with the C instance:
  ObjectBase* cppObject = static_cast<ObjectBase*>(data); //Previously set with g_object_set_qdata_full().

#ifdef GLIBMM_DEBUG_REFCOUNTING
  g_warning("ObjectBase::destroy_notify_callback_: cppObject = %p, gobject_ = %p, gtypename = %s",
            (void*) cppObject, (void*) cppObject->gobject_, cppObject->gobject_);
#endif

  if(cppObject) //This will be 0 if the C++ destructor has already run.
  {
    cppObject->destroy_notify_(); //Virtual - it does different things for GObject and GtkObject.
  }
}

void ObjectBase::destroy_notify_()
{
  // The C instance is about to be disposed, making it unusable.  Now is a
  // good time to delete the C++ wrapper of the C instance.  There is no way
  // to force the disposal of the GObject (though GtkObject  has
  // gtk_object_destroy()), So this is the *only* place where we delete the
  // C++ wrapper.
  //
  // This will only happen after the last unreference(), which will be done by
  // the RefPtr<> destructor.  There should be no way to access the wrapper or
  // the undobjecterlying instance after that, so it's OK to delete this.

#ifdef GLIBMM_DEBUG_REFCOUNTING
  g_warning("Glib::ObjectBase::destroy_notify_: gobject_ = %p", (void*) gobject_);
#endif

  gobject_ = 0; // Make sure we don't unref it again in the dtor.

  delete this;
}

bool ObjectBase::is_anonymous_custom_() const
{
  // Doing high-speed pointer comparison is OK here.
  return (custom_type_name_ == anonymous_custom_type_name);
}

bool ObjectBase::is_derived_() const
{
  // gtkmmproc-generated classes initialize this to 0 by default.
  return (custom_type_name_ != 0);
}

void ObjectBase::set_manage()
{
  // This is a private method and Gtk::manage() is a template function.
  // Thus this will probably never run, unless you do something like:
  //
  // manage(static_cast<Gtk::Object*>(refptr.operator->()));

  g_error("Glib::ObjectBase::set_manage(): "
          "only Gtk::Object instances can be managed");
}

bool ObjectBase::_cpp_destruction_is_in_progress() const
{
  return cpp_destruction_in_progress_;
}

void ObjectBase::set_property_value(const Glib::ustring& property_name, const Glib::ValueBase& value)
{
  g_object_set_property(gobj(), property_name.c_str(), value.gobj());
}

void ObjectBase::get_property_value(const Glib::ustring& property_name, Glib::ValueBase& value) const
{
  g_object_get_property(const_cast<GObject*>(gobj()), property_name.c_str(), value.gobj());
}


bool _gobject_cppinstance_already_deleted(GObject* gobject)
{
  //This function is used to prevent calling wrap() on a GTK+ instance whose gtkmm instance has been deleted.

  if(gobject)
    return (bool)g_object_get_qdata(gobject, Glib::quark_cpp_wrapper_deleted_); //true means that something is odd.
  else
    return false; //Nothing is particularly wrong.
}


} // namespace Glib

