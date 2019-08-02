/*
 * Copyright (C) 2018-2019 Damien Zammit <damien@zamaudio.com>
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
#ifndef __pt_import_selector_h__
#define __pt_import_selector_h__

#include <string>
#include <stdio.h>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

#include "ptformat/ptformat.h"

#include "ardour_dialog.h"
#include "ardour/session.h"
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/filechooserwidget.h>
#include <gtkmm/textview.h>

class PTImportSelector : public ArdourDialog
{
public:
	PTImportSelector (PTFFormat& ptf);
	void update_ptf ();
	void set_session (ARDOUR::Session*);

private:
	PTFFormat* _ptf;
	uint32_t _session_rate;
	Gtk::FileChooserWidget ptimport_ptf_chooser;
	Gtk::TextView ptimport_info_text;
	Gtk::Button ptimport_import_button;
	Gtk::Button ptimport_cancel_button;
};

#endif
