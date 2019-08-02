/*
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_interthread_progress_window_h__
#define __ardour_interthread_progress_window_h__

#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include "ardour_dialog.h"

namespace ARDOUR {
	class InterThreadInfo;
	class ImportStatus;
}

/** A progress dialogue which gets its status from an
 *  ARDOUR::InterThreadInfo struct.  Displays a progress bar, which is
 *  automagically updated using a Glib timer, and a cancel button.
 */

class InterthreadProgressWindow : public ArdourDialog
{
public:
	InterthreadProgressWindow (ARDOUR::InterThreadInfo *, std::string const &, std::string const &);

protected:

	virtual bool update ();
	virtual void on_hide ();

	Gtk::Button _cancel_button;
	Gtk::Label _cancel_label;
	Gtk::ProgressBar _bar;

private:
	void cancel_clicked ();

	ARDOUR::InterThreadInfo* _interthread_info;
};

/** Progress dialogue for importing sound files */
class ImportProgressWindow : public InterthreadProgressWindow
{
public:
	ImportProgressWindow (ARDOUR::ImportStatus *, std::string const &, std::string const &);

private:
	bool update ();

	Gtk::Label _label;
	ARDOUR::ImportStatus* _import_status;
};

#endif
