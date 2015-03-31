/*
    Copyright (C) 2014 Paul Davis

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

#include "ardour/lmath.h"
#include "selection_memento.h"
#include "editing.h"
#include "public_editor.h"

#include "i18n.h"

SelectionMemento::SelectionMemento ()
{
}

SelectionMemento::~SelectionMemento ()
{
}

XMLNode&
SelectionMemento::get_state () {

	XMLNode* node = new XMLNode ("SelectionMemento");
	char buf[32];
	PublicEditor& editor = PublicEditor::instance();

	node->add_property ("mouse-mode", enum2str(editor.current_mouse_mode()));
	snprintf (buf, sizeof(buf), "%" PRId64, editor.get_current_zoom());
	node->add_property ("zoom", buf);
	snprintf (buf, sizeof (buf), "%" PRIi64, editor.leftmost_sample());
	node->add_property ("left-frame", buf);
	snprintf (buf, sizeof (buf), "%f", editor.get_y_origin());
	node->add_property ("y-origin", buf);

	node->add_child_nocopy (editor.get_selection().get_state());
	return *node;
}

int
SelectionMemento::set_state (const XMLNode& node, int /*version*/) {

	const XMLProperty* prop;
	PublicEditor& editor = PublicEditor::instance();
	if (node.name() != X_("SelectionMemento")) {
		return -1;
	}

	if ((prop = node.property ("mouse-mode"))) {
		Editing::MouseMode m = Editing::str2mousemode(prop->value());
		editor.set_mouse_mode (m, true);
	}

	if ((prop = node.property ("zoom"))) {
		/* older versions of ardour used floating point samples_per_pixel */
		double f = PBD::atof (prop->value());
		editor.reset_zoom (llrintf (f));
	}

	if ((prop = node.property ("left-frame")) != 0) {
		framepos_t pos;
		if (sscanf (prop->value().c_str(), "%" PRId64, &pos) == 1) {
			if (pos < 0) {
				pos = 0;
			}
			editor.reset_x_origin (pos);
		}
	}

	if ((prop = node.property ("y-origin")) != 0) {
		editor.reset_y_origin (atof (prop->value ().c_str()));
	}

	XMLNodeList children = node.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		editor.get_selection().set_state (**i, Stateful::current_state_version);
	}

	return 0;
}
