/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "pbd/debug.h"
#include "pbd/compose.h"
#include "pbd/property_list.h"
#include "pbd/xml++.h"

using namespace PBD;

PropertyList::PropertyList() 
        : _property_owner (true) 
{
}

PropertyList::~PropertyList ()
{
        if (_property_owner) {
                for (iterator i = begin (); i != end (); ++i) {
                        delete i->second;
                }
        }
}

void
PropertyList::add_history_state (XMLNode* history_node)
{
        for (const_iterator i = begin(); i != end(); ++i) {
                DEBUG_TRACE (DEBUG::Properties, string_compose ("Add before/after to %1 for %2\n",
                                                                history_node->name(), 
                                                                i->second->property_name()));
                i->second->add_history_state (history_node);
        }
}

bool
PropertyList::add (PropertyBase* prop)
{
        return insert (value_type (prop->property_id(), prop)).second;
}        

OwnedPropertyList::OwnedPropertyList ()
{
        _property_owner = false;
}

bool
OwnedPropertyList::add (PropertyBase& p)
{
        return insert (value_type (p.property_id (), &p)).second;
}


