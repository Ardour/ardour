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

#include <glibmm/main.h>
#include <gtkmm/stock.h>
#include "ardour/import_status.h"
#include "interthread_progress_window.h"
#include "i18n.h"
#include "progress_dialog.h"
#include "gtkmm2ext/gui_thread.h"

using namespace std;
using namespace Gtk;

/** @param i Status information.

 */

InterthreadProgressWindow::InterthreadProgressWindow (ARDOUR::InterThreadInfo* i)
	: _interthread_info (i)
{
    _timeout_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &InterthreadProgressWindow::update), 100);
    _progress_dialog.CancelClicked.connect (_cancel_connection, MISSING_INVALIDATOR, boost::bind (&InterthreadProgressWindow::cancel_clicked, this), gui_context());
}

InterthreadProgressWindow::~InterthreadProgressWindow ()
{
    _timeout_connection.disconnect ();
    _cancel_connection.disconnect ();
    _progress_dialog.hide_cancel_button ();
    _progress_dialog.hide_pd ();
}

void
InterthreadProgressWindow::cancel_clicked ()
{
    _interthread_info->cancel = true;
}

void
InterthreadProgressWindow::show ()
{
    _progress_dialog.show_pd ();
    _progress_dialog.show_cancel_button ();
}

bool
InterthreadProgressWindow::update ()
{
    _progress_dialog.set_progress (_interthread_info->progress);
 	return !(_interthread_info->done || _interthread_info->cancel);
}

/** @param s Status information.
  */

ImportProgressWindow::ImportProgressWindow (ARDOUR::ImportStatus* s)
: InterthreadProgressWindow (s)
, _import_status (s)
{
}

bool
ImportProgressWindow::update ()
{
    _progress_dialog.set_cancel_button_sensitive (!_import_status->freeze);

 	/* use overall progress for the bar, rather than that for individual files */
    double fraction = (_import_status->current - 1 + _import_status->progress) / _import_status->total;
    
 	/* some of the code which sets up _import_status->current may briefly increment it too far
     at the end of an import, so check for that to avoid a visual glitch */

    uint32_t c = _import_status->current;
	if (c > _import_status->total) {
		c = _import_status->total;
	}
     
     _progress_dialog.update_info ( fraction,
                                    _("Importing files"),
                                    (string_compose (_("Importing file: %1 of %2"), c, _import_status->total)).c_str(),
                                    (_import_status->doing_what).c_str() );
     
     return !(_import_status->all_done || _import_status->cancel);
}
