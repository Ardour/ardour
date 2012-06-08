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
#include "ardour/panner.h"
#include "pbd/controllable.h"
#include "stereo_panner_editor.h"
#include "stereo_panner.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

StereoPannerEditor::StereoPannerEditor (StereoPanner* p)
	: PannerEditor (_("Stereo Panner"))
	, _panner (p)
	, _ignore_changes (false)
{
	Table* t = manage (new Table (2, 3));
	t->set_spacings (6);

	int n = 0;
	
	t->attach (*manage (left_aligned_label (_("Position"))), 0, 1, n, n + 1);
	t->attach (_position, 1, 2, n, n + 1);
	t->attach (*manage (left_aligned_label (_("%"))), 2, 3, n, n + 1);
	++n;
	
	t->attach (*manage (left_aligned_label (_("Width"))), 0, 1, n, n + 1);
	t->attach (_width, 1, 2, n, n + 1);
	t->attach (*manage (left_aligned_label (_("%"))), 2, 3, n, n + 1);
	++n;

	get_vbox()->pack_start (*manage (t));
	get_vbox()->set_spacing (6);

	_position.set_increments (1, 10);
	_width.set_increments (1, 10);
	set_position_range ();
	set_width_range ();

	_panner->get_position_controllable()->Changed.connect (
		_connections, invalidator (*this), boost::bind (&StereoPannerEditor::update_editor, this), gui_context ()
		);

	_panner->get_width_controllable()->Changed.connect (
		_connections, invalidator (*this), boost::bind (&StereoPannerEditor::update_editor, this), gui_context ()
		);
	
	_panner->DropReferences.connect (_connections, invalidator (*this), boost::bind (&StereoPannerEditor::panner_going_away, this), gui_context ());
	_position.signal_value_changed().connect (sigc::mem_fun (*this, &StereoPannerEditor::position_changed));
	_width.signal_value_changed().connect (sigc::mem_fun (*this, &StereoPannerEditor::width_changed));

	show_all ();
	update_editor ();
}

void
StereoPannerEditor::panner_going_away ()
{
	_panner = 0;
}

void
StereoPannerEditor::update_editor ()
{
	if (!_panner) {
		return;
	}
	
	_ignore_changes = true;
	_position.set_value (100 * _panner->get_position_controllable()->get_value ());
	_width.set_value (100 * _panner->get_width_controllable()->get_value ());
	_ignore_changes = false;
}

void
StereoPannerEditor::position_changed ()
{
	if (_ignore_changes || !_panner) {
		return;
	}

	_ignore_changes = true;
	double const v = _position.get_value() / 100;
	_panner->get_position_controllable()->set_value (v);
	set_width_range ();
	_ignore_changes = false;
}

void
StereoPannerEditor::width_changed ()
{
	if (_ignore_changes || !_panner) {
		return;
	}

	_ignore_changes = true;
	double const v = _width.get_value() / 100;
	_panner->get_width_controllable()->set_value (v);
	set_position_range ();
	_ignore_changes = false;
}

void
StereoPannerEditor::set_position_range ()
{
	pair<double, double> const pr = _panner->panner()->position_range ();
	_position.set_range (pr.first * 100, pr.second * 100);
}

void
StereoPannerEditor::set_width_range ()
{
	pair<double, double> const wr = _panner->panner()->width_range ();
	_width.set_range (wr.first * 100, wr.second * 100);
}
	
