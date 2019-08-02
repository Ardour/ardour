/*
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_mono_panner_editor_h__
#define __gtk_ardour_mono_panner_editor_h__

#include <gtkmm/spinbutton.h>
#include "panner_editor.h"

class MonoPanner;

/** Editor dialog for the mono panner */
class MonoPannerEditor : public PannerEditor
{
public:
	MonoPannerEditor (MonoPanner *);

private:
	void panner_going_away ();
	void update_editor ();
	void left_changed ();
	void right_changed ();

	MonoPanner* _panner;
	Gtk::SpinButton _left;
	Gtk::SpinButton _right;
	bool _ignore_changes;

	PBD::ScopedConnectionList _connections;
};

#endif // __gtk_ardour_mono_panner_editor_h__
