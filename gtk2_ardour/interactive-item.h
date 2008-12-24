/*
    Copyright (C) 2008 Paul Davis
    Author: Dave Robillard

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

#ifndef __ardour_interactive_item_h__
#define __ardour_interactive_item_h__

#include <libgnomecanvasmm/text.h>
#include "simplerect.h"

namespace Gnome {
namespace Canvas {


/** A canvas item that handles events.
 * This is required so ardour can custom deliver events to specific items
 * (e.g. to delineate scroll events) since Gnome::Canvas::Item::on_event
 * is protected.
 */
class InteractiveItem {
public:
	virtual bool on_event(GdkEvent* ev) = 0;
};

/** A canvas text that forwards events to its parent.
 */
class InteractiveText : public Text, public InteractiveItem {
public:
	InteractiveText(Group& parent, double x, double y, const Glib::ustring& text) 
		: Text(parent, x, y, text) 
	{
		_parent = dynamic_cast<InteractiveItem*>(&parent);
	}
	
	InteractiveText(Group& parent)
		: Text(parent) 
	{
		_parent = dynamic_cast<InteractiveItem*>(&parent);		
	}
	
	bool on_event(GdkEvent* ev) {
		if(_parent) {
			return _parent->on_event(ev);
		} else {
			return false;
		}
	}

protected:
	InteractiveItem* _parent;
};

class InteractiveRect: public SimpleRect, public InteractiveItem
{
public:
	InteractiveRect(Group& parent, double x1, double y1, double x2, double y2) 
		: SimpleRect(parent, x1, y1, x2, y2) {
		_parent = dynamic_cast<InteractiveItem*>(&parent);
	}
	
	bool on_event(GdkEvent* ev) {
		if(_parent) {
			return _parent->on_event(ev);
		} else {
			return false;
		}
	}

protected:
	InteractiveItem* _parent;
};

} /* namespace Canvas */
} /* namespace Gnome */

#endif /* __ardour_interactive_item_h__ */
