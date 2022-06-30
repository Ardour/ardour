/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_startup_h__
#define __gtk2_ardour_startup_h__

#include <string>

#include <gtkmm/assistant.h>
#include <gtkmm/label.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/comboboxtext.h>

#include "ardour/utils.h"

class EngineControl;

class NewUserWizard : public Gtk::Assistant
{
public:
	NewUserWizard ();
	~NewUserWizard ();

	static bool required ();

	/* It's not a dialog so we have to fake this to make it behave like a
	 * dialog. This allows the StartupFSM to treat everything similarly.
	 */

	sigc::signal1<void,int>& signal_response() { return _signal_response; }

protected:
	void on_show ();
	void on_unmap ();
	void pop_splash ();
	void push_splash ();

	bool _splash_pushed;

private:
	bool config_modified;
	bool new_user;

	void on_apply ();
	void on_cancel ();
	bool on_delete_event (GdkEventAny*);

	Glib::RefPtr<Gdk::Pixbuf> icon_pixbuf;

	void setup_prerelease_page ();
	void setup_new_user_page ();
	Glib::RefPtr<Gdk::Pixbuf> splash_pixbuf;

	void setup_first_time_config_page ();

	/* Welcome */
	Gtk::ComboBoxText ui_font_scale;
	void rescale_ui ();
	void guess_default_ui_scale ();

	/* first page */
	Gtk::FileChooserButton* default_dir_chooser;
	void default_dir_changed();
	void setup_first_page ();
	Gtk::FileChooserButton new_folder_chooser;

	/* final page */
	void setup_final_page ();
	void move_along_now ();

	/* always there */
	Glib::RefPtr<Pango::Layout> layout;


	sigc::signal1<void,int> _signal_response;
};

#endif /* __gtk2_ardour_startup_h__ */
