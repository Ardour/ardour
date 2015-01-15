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

#ifndef __ardour_interthread_progress_window_h__
#define __ardour_interthread_progress_window_h__

#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include "pbd/signals.h"
#include "ardour_dialog.h"
#include "waves_button.h"
#include "progress_dialog.h"

namespace ARDOUR {
	class InterThreadInfo;
	class ImportStatus;
}

/** A progress dialogue which gets its status from an
 *  ARDOUR::InterThreadInfo struct.  Displays a progress bar, which is
 *  automagically updated using a Glib timer, and a cancel button.
 */

class InterthreadProgressWindow
{
public:
	InterthreadProgressWindow (ARDOUR::InterThreadInfo *);
    virtual ~InterthreadProgressWindow ();
    void show ();

protected:

	virtual bool update ();
    ProgressDialog _progress_dialog;

private:
	void cancel_clicked ();
    
	ARDOUR::InterThreadInfo* _interthread_info;
    PBD::ScopedConnection _cancel_connection;
    sigc::connection _timeout_connection;
};

/** Progress dialogue for importing sound files */
class ImportProgressWindow : public InterthreadProgressWindow
{
public:
	ImportProgressWindow (ARDOUR::ImportStatus *);

private:
	bool update ();

	ARDOUR::ImportStatus* _import_status;
};

#endif
