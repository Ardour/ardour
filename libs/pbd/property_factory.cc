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

#include <stdint.h>
#include <cstdio>

#include "pbd/properties.h"
#include "pbd/xml++.h"

#include "i18n.h"

using namespace PBD;

PropertyBase*
PropertyFactory::create (const XMLNode& node)
{
        const XMLProperty* prop_type = node.property (X_("property-type"));
        const XMLProperty* prop_id = node.property (X_("id"));
        const XMLProperty* prop_val = node.property (X_("val"));

        if (!prop_type || !prop_id || !prop_val) {
                return 0;
        }

        PropertyID id;
        sscanf (prop_id->value().c_str(), "%u", &id);        

        if (prop_type->value() == typeid (Property<bool>).name()) {

                PropertyDescriptor<bool> pd (id);
                Property<bool>* p = new Property<bool> (pd);
                p->set (p->from_string (prop_val->value()));
                return p;
        }

        return 0;
}
