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

#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace Gtk {
	class RadioButton;
	class SpinButton;
	class ProgressBar;
}

class NormalizeDialog : public ArdourDialog, public ProgressReporter
{
public:
	NormalizeDialog (bool);

	bool normalize_individually () const;
	double target () const;
	int run ();

private:
	void update_progress_gui (float);
	void button_clicked (int);

	Gtk::RadioButton* _normalize_individually;
	Gtk::SpinButton* _spin;
	Gtk::ProgressBar* _progress_bar;

	static double _last_normalization_value;
	static bool _last_normalize_individually;
};
