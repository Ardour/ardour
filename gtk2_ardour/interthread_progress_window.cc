/*
 * Copyright (C) 2010-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include <glibmm/main.h>
#include <gtkmm/stock.h>
#include "gtkmm2ext/utils.h"
#include "ardour/import_status.h"
#include "interthread_progress_window.h"
#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;

/** @param i Status information.
 *  @param t Window title.
 *  @param c Label to use for Cancel button.
 */
InterthreadProgressWindow::InterthreadProgressWindow (ARDOUR::InterThreadInfo* i, string const & t, string const & c)
	: ArdourDialog (t, true)
	, _interthread_info (i)
{
	_interthread_info->cancel = false;

	_bar.set_orientation (Gtk::PROGRESS_LEFT_TO_RIGHT);

	set_border_width (12);
	get_vbox()->set_spacing (6);

	get_vbox()->pack_start (_bar, false, false);

	Button* b = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	b->signal_clicked().connect (sigc::mem_fun (*this, &InterthreadProgressWindow::cancel_clicked));

	_cancel_label.set_text (c);
	_cancel_button.add (_cancel_label);

	set_default_size (200, 100);
	show_all ();

	Glib::signal_timeout().connect (sigc::mem_fun (*this, &InterthreadProgressWindow::update), 100);
}

void
InterthreadProgressWindow::on_hide ()
{
	if (_interthread_info && !_interthread_info->done) {
		//catch user pressing 'esc' or WM close
		_interthread_info->cancel = true;
	}
}

void
InterthreadProgressWindow::cancel_clicked ()
{
	_interthread_info->cancel = true;
}

bool
InterthreadProgressWindow::update ()
{
	_bar.set_fraction (_interthread_info->progress);
	return !(_interthread_info->done || _interthread_info->cancel);
}

/** @param i Status information.
 *  @param t Window title.
 *  @param c Label to use for Cancel button.
 */
ImportProgressWindow::ImportProgressWindow (ARDOUR::ImportStatus* s, string const & t, string const & c)
	: InterthreadProgressWindow (s, t, c)
	, _import_status (s)
{
	_label.set_alignment (0, 0.5);
	_label.set_use_markup (true);

	get_vbox()->pack_start (_label, false, false);

	_label.show ();
}

bool
ImportProgressWindow::update ()
{
	_cancel_button.set_sensitive (!_import_status->freeze);
	_label.set_markup ("<i>" + Gtkmm2ext::markup_escape_text (_import_status->doing_what) + "</i>");

	/* use overall progress for the bar, rather than that for individual files */
	_bar.set_fraction ((_import_status->current - 1 + _import_status->progress) / _import_status->total);

	/* some of the code which sets up _import_status->current may briefly increment it too far
	   at the end of an import, so check for that to avoid a visual glitch
	*/
	uint32_t c = _import_status->current;
	if (c > _import_status->total) {
		c = _import_status->total;
	}

	_bar.set_text (string_compose (_("Importing file: %1 of %2"), c, _import_status->total));

	return !(_import_status->all_done || _import_status->cancel);
}
