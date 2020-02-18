/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/lmath.h"
#include "selection_memento.h"
#include "editing.h"
#include "public_editor.h"

#include "pbd/i18n.h"

SelectionMemento::SelectionMemento ()
{
}

SelectionMemento::~SelectionMemento ()
{
}

XMLNode&
SelectionMemento::get_state () {

	XMLNode* node = new XMLNode ("SelectionMemento");
	PublicEditor& editor = PublicEditor::instance();

	node->set_property ("mouse-mode", enum2str(editor.current_mouse_mode()));
	node->set_property ("zoom", editor.get_current_zoom());
	node->set_property ("left-frame", editor.leftmost_sample());
	node->set_property ("y-origin", editor.get_y_origin());

	node->add_child_nocopy (editor.get_selection().get_state());
	return *node;
}

int
SelectionMemento::set_state (const XMLNode& node, int /*version*/) {

	PublicEditor& editor = PublicEditor::instance();
	if (node.name() != X_("SelectionMemento")) {
		return -1;
	}

	std::string str;
	if (node.get_property ("mouse-mode", str)) {
		Editing::MouseMode m = Editing::str2mousemode (str);
		editor.set_mouse_mode (m, true);
	}

	float zoom;
	if (node.get_property ("zoom", zoom)) {
		/* older versions of ardour used floating point samples_per_pixel */
		editor.reset_zoom (llrintf (zoom));
	}

	samplepos_t pos;
	if (node.get_property ("left-frame", pos)) {
		if (pos < 0) {
			pos = 0;
		}
		editor.reset_x_origin (pos);
	}

	double y_origin;
	if (node.get_property ("y-origin", y_origin)) {
		editor.reset_y_origin (y_origin);
	}

	XMLNodeList children = node.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		editor.get_selection().set_state (**i, Stateful::current_state_version);
	}

	return 0;
}
