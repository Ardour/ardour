// -*- c++ -*-
/* $Id$ */

/* propertyproxy_base.h
 *
 * Copyright 2002 The gtkmm Development Team
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

#include <glibmm/propertyproxy_base.h>
#include <glibmm/signalproxy_connectionnode.h>
#include <glibmm/object.h>
#include <glibmm/private/object_p.h>

namespace Glib
{

/** PropertyProxyConnectionNode is a connection node for use with SignalProxyProperty.
  * It's like ProxyConnectionNode, but it contains the property name too.
  */
class PropertyProxyConnectionNode : public SignalProxyConnectionNode
{
public:
  friend class SignalProxyProperty;

  PropertyProxyConnectionNode(const sigc::slot_base& slot, GObject* gobject, const gchar* property_name);

protected:
  //This will be examined in the callback.
  //Should be a static string literal.
  const gchar* property_name_;
};

PropertyProxyConnectionNode::PropertyProxyConnectionNode(const sigc::slot_base& slot, GObject* gobject, const gchar* property_name)
: SignalProxyConnectionNode(slot, gobject),
  property_name_(property_name)
{
}

//SignalProxyProperty implementation:

SignalProxyProperty::SignalProxyProperty(Glib::ObjectBase* obj, const gchar* property_name)
: SignalProxyBase(obj),
  property_name_(property_name)
{
}

SignalProxyProperty::~SignalProxyProperty()
{
}

sigc::connection SignalProxyProperty::connect(const SlotType& sl)
{
  // create a proxy to hold our connection info
  PropertyProxyConnectionNode* pConnectionNode = new PropertyProxyConnectionNode(sl, obj_->gobj(), property_name_ );

  // connect it to gtk+
  // pConnectionNode will be passed as the data argument to the callback.
  // The callback will then call the virtual Object::property_change_notify() method,
  // which will contain a switch/case statement which will examine the property name.
  pConnectionNode->connection_id_ = g_signal_connect_data(obj_->gobj(),
         "notify", (GCallback)(&callback), pConnectionNode,
         &PropertyProxyConnectionNode::destroy_notify_handler,
         G_CONNECT_AFTER);

  return sigc::connection(pConnectionNode->slot_);
}

void SignalProxyProperty::callback(GObject*, GParamSpec* pspec, gpointer data) //static
{
  if(pspec && data)
  {
    //Get the name of the property that has changed:
    const char* property_name_changed = pspec->name;

    //Get the name of the property that we are waiting for:
    PropertyProxyConnectionNode* conn = static_cast<PropertyProxyConnectionNode*>(data);
    const char* property_name_monitored = conn->property_name_;

    //If it's the correct property, then call the signal handler:
    if(strcmp(property_name_changed, property_name_monitored) == 0)
    {
      if(sigc::slot_base *const slot = data_to_slot(data))
        (*static_cast<sigc::slot<void>*>(slot))();
    }
  }
}


//PropertyProxy_Base implementation:

PropertyProxy_Base::PropertyProxy_Base(ObjectBase* obj, const char* property_name)
:
  obj_           (obj),
  property_name_ (property_name)
{}

PropertyProxy_Base::PropertyProxy_Base(const PropertyProxy_Base& other)
:
  obj_           (other.obj_),
  property_name_ (other.property_name_)
{}

SignalProxyProperty PropertyProxy_Base::signal_changed()
{
  return SignalProxyProperty(obj_, property_name_);
}

void PropertyProxy_Base::set_property_(const Glib::ValueBase& value)
{
  g_object_set_property(obj_->gobj(), property_name_, value.gobj());
}

void PropertyProxy_Base::get_property_(Glib::ValueBase& value) const
{
  g_object_get_property(obj_->gobj(), property_name_, value.gobj());
}

void PropertyProxy_Base::reset_property_()
{
  // Get information about the parameter:
  const GParamSpec *const pParamSpec =
      g_object_class_find_property(G_OBJECT_GET_CLASS(obj_->gobj()), property_name_);

  g_return_if_fail(pParamSpec != 0);

  Glib::ValueBase value;
  value.init(G_PARAM_SPEC_VALUE_TYPE(pParamSpec));

  // An explicit reset is not needed, because ValueBase:init()
  // has already initialized it to the default value for this type.
  // value.reset();

  g_object_set_property(obj_->gobj(), property_name_, value.gobj());
}

} // namespace Glib

