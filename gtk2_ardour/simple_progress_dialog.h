/*
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_gtk_simpple_progress_dialog_h_
#define _ardour_gtk_simpple_progress_dialog_h_

#include <gtkmm/button.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/stock.h>

#include "ardour/types.h"

class SimpleProgressDialog : public Gtk::MessageDialog
{
public:
	SimpleProgressDialog (std::string title, const Glib::SignalProxy0< void >::SlotType & cancel)
		: MessageDialog (title, false, Gtk::MESSAGE_OTHER, Gtk::BUTTONS_NONE, true)
	{
		get_vbox()->set_size_request(400,-1);
		set_title (title);
		pbar = manage (new Gtk::ProgressBar());
		pbar->show();
		get_vbox()->pack_start (*pbar, Gtk::PACK_SHRINK, 4);

		Gtk::Button *cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		cancel_button->signal_clicked().connect (cancel);
		cancel_button->show();
		get_vbox()->pack_start (*cancel_button, Gtk::PACK_SHRINK);
	}

	void update_progress (samplecnt_t c, samplecnt_t t) {
		pbar->set_fraction ((float) c / (float) t);
		// see also ARDOUR_UI::gui_idle_handler();
		int timeout = 30;
		while (gtk_events_pending() && --timeout) {
			gtk_main_iteration ();
		}
	}
private:
	Gtk::ProgressBar *pbar;
};
#endif
