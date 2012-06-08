/*
    Copyright (C) 2012 Paul Davis

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

#include <gtkmm.h>
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "pbd/controllable.h"
#include "mono_panner_editor.h"
#include "mono_panner.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;

MonoPannerEditor::MonoPannerEditor (MonoPanner* p)
	: PannerEditor (_("Mono Panner"))
	, _panner (p)
	, _ignore_changes (false)
{
	Table* t = manage (new Table (2, 3));
	t->set_spacings (6);

	int n = 0;
	
	t->attach (*manage (left_aligned_label (_("Left"))), 0, 1, n, n + 1);
	t->attach (_left, 1, 2, n, n + 1);
	t->attach (*manage (left_aligned_label (_("%"))), 2, 3, n, n + 1);
	++n;
	
	t->attach (*manage (left_aligned_label (_("Right"))), 0, 1, n, n + 1);
	t->attach (_right, 1, 2, n, n + 1);
	t->attach (*manage (left_aligned_label (_("%"))), 2, 3, n, n + 1);
	++n;

	get_vbox()->pack_start (*manage (t));
	get_vbox()->set_spacing (6);

	_left.set_increments (1, 10);
	_left.set_range (0, 100);
	_right.set_increments (1, 10);
	_right.set_range (0, 100);

	_panner->get_controllable()->Changed.connect (_connections, invalidator (*this), boost::bind (&MonoPannerEditor::update_editor, this), gui_context ());
	_panner->DropReferences.connect (_connections, invalidator (*this), boost::bind (&MonoPannerEditor::panner_going_away, this), gui_context ());
	_left.signal_value_changed().connect (sigc::mem_fun (*this, &MonoPannerEditor::left_changed));
	_right.signal_value_changed().connect (sigc::mem_fun (*this, &MonoPannerEditor::right_changed));

	show_all ();
	update_editor ();
}

void
MonoPannerEditor::panner_going_away ()
{
	_panner = 0;
}

void
MonoPannerEditor::update_editor ()
{
	if (!_panner) {
		return;
	}
	
	float const v = _panner->get_controllable()->get_value();

	_ignore_changes = true;
	_left.set_value (100 * (1 - v));
	_right.set_value (100 * v);
	_ignore_changes = false;
}

void
MonoPannerEditor::left_changed ()
{
	if (_ignore_changes || !_panner) {
		return;
	}

	float const v = 1 - _left.get_value () / 100;

	_ignore_changes = true;
	_right.set_value (100 * v);
	_panner->get_controllable()->set_value (v);
	_ignore_changes = false;
}

void
MonoPannerEditor::right_changed ()
{
	if (_ignore_changes || !_panner) {
		return;
	}

	float const v = _right.get_value () / 100;
	
	_ignore_changes = true;
	_left.set_value (100 * (1 - v));
	_panner->get_controllable()->set_value (v);
	_ignore_changes = false;
}

