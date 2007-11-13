// -*- c++ -*-
/* $Id: interface.cc,v 1.3 2005/03/07 15:42:20 murrayc Exp $ */

/* Copyright (C) 2002 The gtkmm Development Team
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

#include <glibmm/interface.h>
#include <glibmm/private/interface_p.h>


namespace Glib
{

/**** Glib::Interface_Class ************************************************/

void Interface_Class::add_interface(GType instance_type) const
{
  //This check is distabled, because it checks whether any of the types's bases implement the interface, not just the specific type.
  //if( !g_type_is_a(instance_type, gtype_) ) //For convenience, don't complain about calling this twice.
  //{
    const GInterfaceInfo interface_info =
    {
      class_init_func_,
      0, // interface_finalize
      0, // interface_data
    };

    g_type_add_interface_static(instance_type, gtype_, &interface_info);
  //}
}


/**** Interface Glib::Interface ********************************************/

Interface::Interface(const Interface_Class& interface_class)
{
  //gobject_ will be set in the Object constructor.
  //Any instantiable class that derives from Interface should also inherit from Object.

  // If I understand it correctly, gobject_ shouldn't be 0 now.  daniel.
  // TODO: Make this a g_assert() if the assumption above is correct.

  g_return_if_fail(gobject_ != 0);

  if(custom_type_name_ && !is_anonymous_custom_())
  {
    void *const instance_class = G_OBJECT_GET_CLASS(gobject_);

    if(!g_type_interface_peek(instance_class, interface_class.get_type()))
    {
      interface_class.add_interface(G_OBJECT_CLASS_TYPE(instance_class));
    }
  }
}

Interface::Interface(GObject* castitem)
{
  // Connect GObject and wrapper instances.
  ObjectBase::initialize(castitem);
}

Interface::~Interface()
{}

GType Interface::get_type()
{
  return G_TYPE_INTERFACE;
}

GType Interface::get_base_type()
{
  return G_TYPE_INTERFACE;
}

RefPtr<ObjectBase> wrap_interface(GObject* object, bool take_copy)
{
  return Glib::RefPtr<ObjectBase>( wrap_auto(object, take_copy) );
}

} // namespace Glib

