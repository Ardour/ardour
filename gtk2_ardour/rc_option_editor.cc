/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2018 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cairo/cairo.h>

#include <boost/algorithm/string.hpp>

#include <gtkmm/liststore.h>
#include <gtkmm/stock.h>
#include <gtkmm/scale.h>

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/window_title.h"

#include "pbd/file_utils.h"
#include "pbd/fpu.h"
#include "pbd/cpus.h"
#include "pbd/unwind.h"
#include "pbd/i18n.h"

#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/clip_library.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/dB.h"
#include "ardour/port_manager.h"
#include "ardour/plugin_manager.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/transport_master_manager.h"

#include "control_protocol/control_protocol.h"

#include "waveview/wave_view.h"

#include "widgets/paths_dialog.h"
#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "ardour_window.h"
#include "color_theme_manager.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "meter_patterns.h"
#include "midi_tracer.h"
#include "plugin_scan_dialog.h"
#include "rc_option_editor.h"
#include "sfdb_ui.h"
#include "transport_masters_dialog.h"
#include "ui_config.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace ArdourWidgets;

class ClickOptions : public OptionEditorMiniPage
{
public:
	ClickOptions (RCConfiguration* c)
		: _rc_config (c)
		, _click_browse_button (_("Browse..."))
		, _click_emphasis_browse_button (_("Browse..."))
	{
		// TODO get rid of GTK -> use OptionEditor Widgets
		Table* t = &table;
		Label* l;
		int row = 0;

		l = manage (left_aligned_label (_("Emphasis on first beat")));
		_use_emphasis_on_click_check_button.add (*l);
		t->attach (_use_emphasis_on_click_check_button, 1, 3, row, row + 1, FILL);
		_use_emphasis_on_click_check_button.signal_toggled().connect (
		    sigc::mem_fun (*this, &ClickOptions::use_emphasis_on_click_toggled));
		++row;

		l = manage (left_aligned_label (_("Use built-in default sounds")));
		_use_default_click_check_button.add (*l);
		t->attach (_use_default_click_check_button, 1, 3, row, row + 1, FILL);
		_use_default_click_check_button.signal_toggled().connect (
		    sigc::mem_fun (*this, &ClickOptions::use_default_click_toggled));
		++row;

		l = manage (left_aligned_label (_("Audio file:")));
		t->attach (*l, 1, 2, row, row + 1, FILL);
		t->attach (_click_path_entry, 2, 3, row, row + 1, FILL);
		_click_browse_button.signal_clicked ().connect (
		    sigc::mem_fun (*this, &ClickOptions::click_browse_clicked));
		t->attach (_click_browse_button, 3, 4, row, row + 1, FILL);
		++row;

		l = manage (left_aligned_label (_("Emphasis audio file:")));
		t->attach (*l, 1, 2, row, row + 1, FILL);
		t->attach (_click_emphasis_path_entry, 2, 3, row, row + 1, FILL);
		_click_emphasis_browse_button.signal_clicked ().connect (
		    sigc::mem_fun (*this, &ClickOptions::click_emphasis_browse_clicked));
		t->attach (_click_emphasis_browse_button, 3, 4, row, row + 1, FILL);
		++row;

		_click_fader = new FaderOption (
				"click-gain",
				_("Gain level"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_click_gain),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_click_gain)
				);

		_click_fader->add_to_page (this);
		_click_fader->set_state_from_config ();

		_click_path_entry.signal_activate().connect (sigc::mem_fun (*this, &ClickOptions::click_changed));
		_click_emphasis_path_entry.signal_activate().connect (sigc::mem_fun (*this, &ClickOptions::click_emphasis_changed));

		if (_rc_config->get_click_sound ().empty() &&
		    _rc_config->get_click_emphasis_sound().empty()) {
			_use_default_click_check_button.set_active (true);
			_use_emphasis_on_click_check_button.set_active (_rc_config->get_use_click_emphasis ());

		} else {
			_use_default_click_check_button.set_active (false);
			_use_emphasis_on_click_check_button.set_active (false);
		}
	}

	void parameter_changed (string const & p)
	{
		if (p == "click-sound") {
			_click_path_entry.set_text (_rc_config->get_click_sound());
		} else if (p == "click-emphasis-sound") {
			_click_emphasis_path_entry.set_text (_rc_config->get_click_emphasis_sound());
		} else if (p == "use-click-emphasis") {
			bool x = _rc_config->get_use_click_emphasis ();
			_use_emphasis_on_click_check_button.set_active (x);
		} else if (p == "click-gain") {
			_click_fader->set_state_from_config ();
		}
	}

	void set_state_from_config ()
	{
		parameter_changed ("click-sound");
		parameter_changed ("click-emphasis-sound");
		parameter_changed ("use-click-emphasis");
	}

private:

	void click_browse_clicked ()
	{
		SoundFileChooser sfdb (_("Choose Click"));

		sfdb.show_all ();
		sfdb.present ();

		if (sfdb.run () == RESPONSE_OK) {
			click_chosen (sfdb.get_filename());
		}
	}

	void click_chosen (string const & path)
	{
		_click_path_entry.set_text (path);
		_rc_config->set_click_sound (path);
	}

	void click_changed ()
	{
		click_chosen (_click_path_entry.get_text ());
	}

	void click_emphasis_browse_clicked ()
	{
		SoundFileChooser sfdb (_("Choose Click Emphasis"));

		sfdb.show_all ();
		sfdb.present ();

		if (sfdb.run () == RESPONSE_OK) {
			click_emphasis_chosen (sfdb.get_filename());
		}
	}

	void click_emphasis_chosen (string const & path)
	{
		_click_emphasis_path_entry.set_text (path);
		_rc_config->set_click_emphasis_sound (path);
	}

	void click_emphasis_changed ()
	{
		click_emphasis_chosen (_click_emphasis_path_entry.get_text ());
	}

	void use_default_click_toggled ()
	{
		if (_use_default_click_check_button.get_active ()) {
			_rc_config->set_click_sound ("");
			_rc_config->set_click_emphasis_sound ("");
			_click_path_entry.set_sensitive (false);
			_click_emphasis_path_entry.set_sensitive (false);
			_click_browse_button.set_sensitive (false);
			_click_emphasis_browse_button.set_sensitive (false);
		} else {
			_click_path_entry.set_sensitive (true);
			_click_emphasis_path_entry.set_sensitive (true);
			_click_browse_button.set_sensitive (true);
			_click_emphasis_browse_button.set_sensitive (true);
		}
	}

	void use_emphasis_on_click_toggled ()
	{
		if (_use_emphasis_on_click_check_button.get_active ()) {
			_rc_config->set_use_click_emphasis(true);
		} else {
			_rc_config->set_use_click_emphasis(false);
		}
	}

	RCConfiguration* _rc_config;
	CheckButton _use_default_click_check_button;
	CheckButton _use_emphasis_on_click_check_button;
	Entry _click_path_entry;
	Entry _click_emphasis_path_entry;
	Button _click_browse_button;
	Button _click_emphasis_browse_button;
	FaderOption* _click_fader;
};

class UndoOptions : public OptionEditorComponent
{
public:
	UndoOptions (RCConfiguration* c) :
		_rc_config (c),
		_limit_undo_button (_("Limit undo history to")),
		_save_undo_button (_("Save undo history of"))
	{
		// TODO get rid of GTK -> use OptionEditor SpinOption
		_limit_undo_spin.set_range (0, 512);
		_limit_undo_spin.set_increments (1, 10);

		_save_undo_spin.set_range (0, 512);
		_save_undo_spin.set_increments (1, 10);

		_limit_undo_button.signal_toggled().connect (sigc::mem_fun (*this, &UndoOptions::limit_undo_toggled));
		_limit_undo_spin.signal_value_changed().connect (sigc::mem_fun (*this, &UndoOptions::limit_undo_changed));
		_save_undo_button.signal_toggled().connect (sigc::mem_fun (*this, &UndoOptions::save_undo_toggled));
		_save_undo_spin.signal_value_changed().connect (sigc::mem_fun (*this, &UndoOptions::save_undo_changed));
	}

	void parameter_changed (string const & p)
	{
		if (p == "history-depth") {
			int32_t const d = _rc_config->get_history_depth();
			_limit_undo_button.set_active (d != 0);
			_limit_undo_spin.set_sensitive (d != 0);
			_limit_undo_spin.set_value (d);
		} else if (p == "save-history") {
			bool const x = _rc_config->get_save_history ();
			_save_undo_button.set_active (x);
			_save_undo_spin.set_sensitive (x);
		} else if (p == "save-history-depth") {
			_save_undo_spin.set_value (_rc_config->get_saved_history_depth());
		}
	}

	void set_state_from_config ()
	{
		parameter_changed ("save-history");
		parameter_changed ("history-depth");
		parameter_changed ("save-history-depth");
	}

	void limit_undo_toggled ()
	{
		bool const x = _limit_undo_button.get_active ();
		_limit_undo_spin.set_sensitive (x);
		int32_t const n = x ? 16 : 0;
		_limit_undo_spin.set_value (n);
		_rc_config->set_history_depth (n);
	}

	void limit_undo_changed ()
	{
		_rc_config->set_history_depth (_limit_undo_spin.get_value_as_int ());
	}

	void save_undo_toggled ()
	{
		bool const x = _save_undo_button.get_active ();
		_rc_config->set_save_history (x);
	}

	void save_undo_changed ()
	{
		_rc_config->set_saved_history_depth (_save_undo_spin.get_value_as_int ());
	}

	void add_to_page (OptionEditorPage* p)
	{
		int const n = p->table.property_n_rows();
		Table* t = & p->table;

		t->resize (n + 2, 3);

		Label* l = manage (left_aligned_label (_("commands")));
		HBox* box = manage (new HBox());
		box->set_spacing (4);
		box->pack_start (_limit_undo_spin, false, false);
		box->pack_start (*l, true, true);
		t->attach (_limit_undo_button, 1, 2, n, n +1, FILL);
		t->attach (*box, 2, 3, n, n + 1, FILL | EXPAND);

		l = manage (left_aligned_label (_("commands")));
		box = manage (new HBox());
		box->set_spacing (4);
		box->pack_start (_save_undo_spin, false, false);
		box->pack_start (*l, true, true);
		t->attach (_save_undo_button, 1, 2, n + 1, n + 2, FILL);
		t->attach (*box, 2, 3, n + 1, n + 2, FILL | EXPAND);
	}

	Gtk::Widget& tip_widget() {
		return _limit_undo_button; // unused
	}

private:
	RCConfiguration* _rc_config;
	CheckButton _limit_undo_button;
	SpinButton _limit_undo_spin;
	CheckButton _save_undo_button;
	SpinButton _save_undo_spin;
};


static const struct {
	const char *name;
	guint modifier;
} modifiers[] = {

	{ "Unmodified", 0 },

#ifdef __APPLE__

	/* Command = Meta
	   Option/Alt = Mod1
	*/
	{ "Key|Shift", GDK_SHIFT_MASK },
	{ "Command", GDK_MOD2_MASK },
	{ "Control", GDK_CONTROL_MASK },
	{ "Option", GDK_MOD1_MASK },
	{ "Command-Shift", GDK_MOD2_MASK|GDK_SHIFT_MASK },
	{ "Command-Option", GDK_MOD2_MASK|GDK_MOD1_MASK },
	{ "Command-Control", GDK_MOD2_MASK|GDK_CONTROL_MASK },
	{ "Command-Option-Control", GDK_MOD2_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK },
	{ "Option-Control", GDK_MOD1_MASK|GDK_CONTROL_MASK },
	{ "Option-Shift", GDK_MOD1_MASK|GDK_SHIFT_MASK },
	{ "Control-Shift", GDK_CONTROL_MASK|GDK_SHIFT_MASK },
	{ "Shift-Command-Option", GDK_MOD5_MASK|GDK_SHIFT_MASK|GDK_MOD2_MASK },

#else
	{ "Key|Shift", GDK_SHIFT_MASK },
	{ "Control", GDK_CONTROL_MASK },
	{ "Alt", GDK_MOD1_MASK },
	{ "Control-Shift", GDK_CONTROL_MASK|GDK_SHIFT_MASK },
	{ "Control-Alt", GDK_CONTROL_MASK|GDK_MOD1_MASK },
	{ "Control-Windows", GDK_CONTROL_MASK|GDK_MOD4_MASK },
	{ "Control-Shift-Alt", GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_MOD1_MASK },
	{ "Alt-Windows", GDK_MOD1_MASK|GDK_MOD4_MASK },
	{ "Alt-Shift", GDK_MOD1_MASK|GDK_SHIFT_MASK },
	{ "Alt-Shift-Windows", GDK_MOD1_MASK|GDK_SHIFT_MASK|GDK_MOD4_MASK },
	{ "Mod2", GDK_MOD2_MASK },
	{ "Mod3", GDK_MOD3_MASK },
	{ "Windows", GDK_MOD4_MASK },
	{ "Mod5", GDK_MOD5_MASK },
#endif
	{ 0, 0 }
};


class KeyboardOptions : public OptionEditorMiniPage
{
public:
	KeyboardOptions ()
		: _delete_button_adjustment (3, 1, 12)
		, _delete_button_spin (_delete_button_adjustment)
		, _edit_button_adjustment (3, 1, 5)
		, _edit_button_spin (_edit_button_adjustment)
		, _insert_note_button_adjustment (3, 1, 5)
		, _insert_note_button_spin (_insert_note_button_adjustment)
	{
		// TODO get rid of GTK -> use OptionEditor Widgets

		const std::string restart_msg = _("\nChanges to this setting will only persist after your project has been saved.");
		/* internationalize and prepare for use with combos */

		vector<string> dumb;
		for (int i = 0; modifiers[i].name; ++i) {
			dumb.push_back (S_(modifiers[i].name));
		}

		set_popdown_strings (_edit_modifier_combo, dumb);
		_edit_modifier_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::edit_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_edit_modifier_combo,
				(string_compose (_("<b>Recommended Setting: %1 + button 3 (right mouse button)</b>%2"),  Keyboard::primary_modifier_name (), restart_msg)));

		Table* t = &table;

		int row = 0;
		int col = 0;

		Label* l = manage (left_aligned_label (_("Select Keyboard layout:")));
		l->set_name ("OptionsLabel");

		vector<string> strs;

		for (map<string,string>::iterator bf = Keyboard::binding_files.begin(); bf != Keyboard::binding_files.end(); ++bf) {
			strs.push_back (bf->first);
		}

		set_popdown_strings (_keyboard_layout_selector, strs);
		_keyboard_layout_selector.set_active_text (Keyboard::current_binding_name());
		_keyboard_layout_selector.signal_changed().connect (sigc::mem_fun (*this, &KeyboardOptions::bindings_changed));

		t->attach (*l, col + 1, col + 2, row, row + 1, FILL, FILL);
		t->attach (_keyboard_layout_selector, col + 2, col + 3, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 0;

		l = manage (left_aligned_label (string_compose ("<b>%1</b>", _("When Clicking:"))));
		l->set_name ("OptionEditorHeading");
		l->set_use_markup (true);
		t->attach (*l, col, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		l = manage (left_aligned_label (_("Edit using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_edit_modifier_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		l = manage (new Label (_("+ button")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col + 3, col + 4, row, row + 1, FILL, FILL);
		t->attach (_edit_button_spin, col + 4, col + 5, row, row + 1, SHRINK , FILL);

		_edit_button_spin.set_name ("OptionsEntry");
		_edit_button_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::edit_button_changed));

		++row;
		col = 1;

		set_popdown_strings (_delete_modifier_combo, dumb);
		_delete_modifier_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::delete_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_delete_modifier_combo,
				(string_compose (_("<b>Recommended Setting: %1 + button 3 (right mouse button)</b>%2"), Keyboard::tertiary_modifier_name (), restart_msg)));

		l = manage (left_aligned_label (_("Delete using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_delete_modifier_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		l = manage (new Label (_("+ button")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col + 3, col + 4, row, row + 1, FILL, FILL);
		t->attach (_delete_button_spin, col + 4, col + 5, row, row + 1, SHRINK, FILL);

		_delete_button_spin.set_name ("OptionsEntry");
		_delete_button_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::delete_button_changed));

		++row;
		col = 1;

		set_popdown_strings (_insert_note_modifier_combo, dumb);
		_insert_note_modifier_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::insert_note_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_insert_note_modifier_combo,
				(string_compose (_("<b>Recommended Setting: %1 + button 1 (left mouse button)</b>%2"), Keyboard::primary_modifier_name (), restart_msg)));

		l = manage (left_aligned_label (_("Insert note using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_insert_note_modifier_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		l = manage (new Label (_("+ button")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col + 3, col + 4, row, row + 1, FILL, FILL);
		t->attach (_insert_note_button_spin, col + 4, col + 5, row, row + 1, SHRINK, FILL);

		_insert_note_button_spin.set_name ("OptionsEntry");
		_insert_note_button_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::insert_note_button_changed));

		++row;

		l = manage (left_aligned_label (string_compose ("<b>%1</b>", _("When Beginning a Drag:"))));
		l->set_name ("OptionEditorHeading");
		l->set_use_markup (true);
		t->attach (*l, 0, 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* copy modifier */
		set_popdown_strings (_copy_modifier_combo, dumb);
		_copy_modifier_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::copy_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_copy_modifier_combo,
						    (string_compose (_("<b>Recommended Setting: %1</b>%2"),
#ifdef __APPLE__
								     Keyboard::secondary_modifier_name (),
#else
								     Keyboard::primary_modifier_name (),
#endif
								     restart_msg)));

		l = manage (left_aligned_label (_("Copy items using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_copy_modifier_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

				++row;
		col = 1;

		/* slip_contents */
		set_popdown_strings (_slip_contents_combo, dumb);
		_slip_contents_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::slip_contents_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_slip_contents_combo,
				(string_compose (_("<b>Recommended Setting: %1-%2</b>%3"), Keyboard::primary_modifier_name (), Keyboard::tertiary_modifier_name (), restart_msg)));

		l = manage (left_aligned_label (_("Slip Contents using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_slip_contents_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* constraint modifier */
		set_popdown_strings (_constraint_modifier_combo, dumb);
		_constraint_modifier_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::constraint_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_constraint_modifier_combo,
						    (string_compose (_("<b>Recommended Setting: %1</b>%2"),
#ifdef __APPLE__
								     Keyboard::primary_modifier_name (),
#else
								     Keyboard::tertiary_modifier_name (),
#endif
								     restart_msg)));

		l = manage (left_aligned_label (_("Constrain drag using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_constraint_modifier_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* push points */
		set_popdown_strings (_push_points_combo, dumb);
		_push_points_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::push_points_modifier_chosen));

		std::string mod_str = string_compose (X_("%1-%2"), Keyboard::primary_modifier_name (), Keyboard::level4_modifier_name ());
		Gtkmm2ext::UI::instance()->set_tip (_push_points_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), mod_str, restart_msg)));

		l = manage (left_aligned_label (_("Push points using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_push_points_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;

		l = manage (left_aligned_label (string_compose ("<b>%1</b>", _("When Beginning a Trim:"))));
		l->set_name ("OptionEditorHeading");
		l->set_use_markup (true);
		t->attach (*l, 0, 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* anchored trim */
		set_popdown_strings (_trim_anchored_combo, dumb);
		_trim_anchored_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::trim_anchored_modifier_chosen));

		mod_str = string_compose (X_("%1-%2"), Keyboard::primary_modifier_name (), Keyboard::tertiary_modifier_name ());
		Gtkmm2ext::UI::instance()->set_tip (_trim_anchored_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), mod_str, restart_msg)));

		l = manage (left_aligned_label (_("Anchored trim using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		++col;
		t->attach (_trim_anchored_combo, col, col + 1, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* jump trim disabled for now
		set_popdown_strings (_trim_jump_combo, dumb);
		_trim_jump_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::trim_jump_modifier_chosen));

		l = manage (left_aligned_label (_("Jump after trim using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		++col;
		t->attach (_trim_jump_combo, col, col + 1, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;
		*/

		/* note resize relative */
		set_popdown_strings (_note_size_relative_combo, dumb);
		_note_size_relative_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::note_size_relative_modifier_chosen));
		Gtkmm2ext::UI::instance()->set_tip (_note_size_relative_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), Keyboard::tertiary_modifier_name (), restart_msg))); // XXX 2ndary

		l = manage (left_aligned_label (_("Resize notes relatively using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		++col;
		t->attach (_note_size_relative_combo, col, col + 1, row, row + 1, FILL | EXPAND, FILL);

		++row;

		l = manage (left_aligned_label (string_compose ("<b>%1</b>", _("While Dragging:"))));
		l->set_name ("OptionEditorHeading");
		l->set_use_markup (true);
		t->attach (*l, 0, 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* ignore snap */
		set_popdown_strings (_snap_modifier_combo, dumb);
		_snap_modifier_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::snap_modifier_chosen));
#ifdef __APPLE__
		mod_str = string_compose (X_("%1-%2"), Keyboard::level4_modifier_name (), Keyboard::tertiary_modifier_name ());
#else
		mod_str = Keyboard::secondary_modifier_name();
#endif
		Gtkmm2ext::UI::instance()->set_tip (_snap_modifier_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), mod_str, restart_msg)));

		l = manage (left_aligned_label (_("Ignore snap using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_snap_modifier_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* snap delta */
		set_popdown_strings (_snap_delta_combo, dumb);
		_snap_delta_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::snap_delta_modifier_chosen));
#ifdef __APPLE__
		mod_str = Keyboard::level4_modifier_name ();
#else
		mod_str = string_compose (X_("%1-%2"), Keyboard::secondary_modifier_name (), Keyboard::level4_modifier_name ());
#endif
		Gtkmm2ext::UI::instance()->set_tip (_snap_delta_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), mod_str, restart_msg)));

		l = manage (left_aligned_label (_("Snap relatively using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_snap_delta_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;

		l = manage (left_aligned_label (string_compose ("<b>%1</b>", _("While Trimming:"))));
		l->set_name ("OptionEditorHeading");
		l->set_use_markup (true);
		t->attach (*l, 0, 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* trim_overlap */
		set_popdown_strings (_trim_overlap_combo, dumb);
		_trim_overlap_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::trim_overlap_modifier_chosen));

		Gtkmm2ext::UI::instance()->set_tip (_trim_overlap_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), Keyboard::tertiary_modifier_name (), restart_msg)));

		l = manage (left_aligned_label (_("Resize overlapped regions using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_trim_overlap_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		++row;

		l = manage (left_aligned_label (string_compose ("<b>%1</b>", _("While Dragging Control Points:"))));
		l->set_name ("OptionEditorHeading");
		l->set_use_markup (true);
		t->attach (*l, 0, 2, row, row + 1, FILL | EXPAND, FILL);

		++row;
		col = 1;

		/* fine adjust */
		set_popdown_strings (_fine_adjust_combo, dumb);
		_fine_adjust_combo.signal_changed().connect (sigc::mem_fun(*this, &KeyboardOptions::fine_adjust_modifier_chosen));

		mod_str = string_compose (X_("%1-%2"), Keyboard::primary_modifier_name (), Keyboard::secondary_modifier_name ()); // XXX just 2ndary ?!
		Gtkmm2ext::UI::instance()->set_tip (_fine_adjust_combo,
				(string_compose (_("<b>Recommended Setting: %1</b>%2"), mod_str, restart_msg)));

		l = manage (left_aligned_label (_("Fine adjust using:")));
		l->set_name ("OptionsLabel");

		t->attach (*l, col, col + 1, row, row + 1, FILL, FILL);
		t->attach (_fine_adjust_combo, col + 1, col + 2, row, row + 1, FILL | EXPAND, FILL);

		OptionEditorHeading* h = new OptionEditorHeading (_("Reset"));
		h->add_to_page (this);

		RcActionButton* rb = new RcActionButton (_("Reset to recommended defaults"),
				sigc::mem_fun (*this, &KeyboardOptions::reset_to_defaults));
		rb->add_to_page (this);

		set_state_from_config ();
	}

	void parameter_changed (string const &)
	{
		/* XXX: these aren't really config options... */
	}

	void set_state_from_config ()
	{
		_delete_button_adjustment.set_value (Keyboard::delete_button());
		_insert_note_button_adjustment.set_value (Keyboard::insert_note_button());
		_edit_button_adjustment.set_value (Keyboard::edit_button());

		for (int x = 0; modifiers[x].name; ++x) {
			if (modifiers[x].modifier == (guint) ArdourKeyboard::trim_overlap_modifier ()) {
				_trim_overlap_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == Keyboard::delete_modifier ()) {
				_delete_modifier_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == Keyboard::edit_modifier ()) {
				_edit_modifier_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == Keyboard::insert_note_modifier ()) {
				_insert_note_modifier_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) Keyboard::CopyModifier) {
				_copy_modifier_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) ArdourKeyboard::constraint_modifier ()) {
				_constraint_modifier_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) ArdourKeyboard::push_points_modifier ()) {
				_push_points_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) ArdourKeyboard::slip_contents_modifier ()) {
				_slip_contents_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) ArdourKeyboard::trim_anchored_modifier ()) {
				_trim_anchored_combo.set_active_text (S_(modifiers[x].name));
			}
#if 0
			if (modifiers[x].modifier == (guint) Keyboard::trim_jump_modifier ()) {
				_trim_jump_combo.set_active_text (S_(modifiers[x].name));
			}
#endif
			if (modifiers[x].modifier == (guint) ArdourKeyboard::note_size_relative_modifier ()) {
				_note_size_relative_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) Keyboard::snap_modifier ()) {
				_snap_modifier_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) Keyboard::snap_delta_modifier ()) {
				_snap_delta_combo.set_active_text (S_(modifiers[x].name));
			}
			if (modifiers[x].modifier == (guint) ArdourKeyboard::fine_adjust_modifier ()) {
				_fine_adjust_combo.set_active_text (S_(modifiers[x].name));
			}
		}
	}

	void add_to_page (OptionEditorPage* p)
	{
		int const n = p->table.property_n_rows();
		p->table.resize (n + 1, 3);
		p->table.attach (box, 1, 3, n, n + 1, FILL | EXPAND, SHRINK, 0, 0);
	}

private:

	void bindings_changed ()
	{
		string const txt = _keyboard_layout_selector.get_active_text();

		/* XXX: config...?  for all this keyboard stuff */

		for (map<string,string>::iterator i = Keyboard::binding_files.begin(); i != Keyboard::binding_files.end(); ++i) {
			if (txt == i->first) {
				if (Keyboard::load_keybindings (i->second)) {
					Keyboard::save_keybindings ();
				}
			}
		}
	}

	void edit_modifier_chosen ()
	{
		string const txt = _edit_modifier_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				Keyboard::set_edit_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void delete_modifier_chosen ()
	{
		string const txt = _delete_modifier_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				Keyboard::set_delete_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void copy_modifier_chosen ()
	{
		string const txt = _copy_modifier_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				Keyboard::set_copy_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void insert_note_modifier_chosen ()
	{
		string const txt = _insert_note_modifier_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				Keyboard::set_insert_note_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void snap_modifier_chosen ()
	{
		string const txt = _snap_modifier_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				Keyboard::set_snap_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void snap_delta_modifier_chosen ()
	{
		string const txt = _snap_delta_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				Keyboard::set_snap_delta_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void constraint_modifier_chosen ()
	{
		string const txt = _constraint_modifier_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_constraint_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void slip_contents_modifier_chosen ()
	{
		string const txt = _slip_contents_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_slip_contents_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void trim_overlap_modifier_chosen ()
	{
		string const txt = _trim_overlap_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_trim_overlap_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void trim_anchored_modifier_chosen ()
	{
		string const txt = _trim_anchored_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_trim_anchored_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void fine_adjust_modifier_chosen ()
	{
		string const txt = _fine_adjust_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_fine_adjust_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void push_points_modifier_chosen ()
	{
		string const txt = _push_points_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_push_points_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void note_size_relative_modifier_chosen ()
	{
		string const txt = _note_size_relative_combo.get_active_text();

		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == S_(modifiers[i].name)) {
				ArdourKeyboard::set_note_size_relative_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void delete_button_changed ()
	{
		Keyboard::set_delete_button (_delete_button_spin.get_value_as_int());
	}

	void edit_button_changed ()
	{
		Keyboard::set_edit_button (_edit_button_spin.get_value_as_int());
	}

	void insert_note_button_changed ()
	{
		Keyboard::set_insert_note_button (_insert_note_button_spin.get_value_as_int());
	}

	void reset_to_defaults ()
	{
		/* when clicking*/
		Keyboard::set_edit_modifier (Keyboard::PrimaryModifier);
		Keyboard::set_edit_button (3);
		Keyboard::set_delete_modifier (Keyboard::TertiaryModifier);
		Keyboard::set_delete_button (3);
		Keyboard::set_insert_note_modifier (Keyboard::PrimaryModifier);
		Keyboard::set_insert_note_button (1);

		/* when beginning a drag */
#ifdef __APPLE__
		Keyboard::set_copy_modifier (Keyboard::SecondaryModifier);
#else
		Keyboard::set_copy_modifier (Keyboard::PrimaryModifier);
#endif

#ifdef __APPLE__
		ArdourKeyboard::set_constraint_modifier (Keyboard::PrimaryModifier);
#else
		ArdourKeyboard::set_constraint_modifier (Keyboard::TertiaryModifier);
#endif
		ArdourKeyboard::set_push_points_modifier (Keyboard::PrimaryModifier | Keyboard::Level4Modifier);

		/* when beginning a trim */
		ArdourKeyboard::set_slip_contents_modifier (Keyboard::PrimaryModifier | Keyboard::TertiaryModifier);
		ArdourKeyboard::set_trim_anchored_modifier (Keyboard::PrimaryModifier | Keyboard::TertiaryModifier);
		ArdourKeyboard::set_note_size_relative_modifier (Keyboard::TertiaryModifier); // XXX better: 2ndary

		/* while dragging */
#ifdef __APPLE__
		Keyboard::set_snap_modifier (Keyboard::TertiaryModifier);
#else
		Keyboard::set_snap_modifier (Keyboard::SecondaryModifier);
#endif
#ifdef __APPLE__
		Keyboard::set_snap_delta_modifier (Keyboard::Level4Modifier);
#else
		Keyboard::set_snap_delta_modifier (Keyboard::SecondaryModifier | Keyboard::Level4Modifier);
#endif

		/* while trimming */
		ArdourKeyboard::set_trim_overlap_modifier (Keyboard::TertiaryModifier);

		/* while dragging ctrl points */
		ArdourKeyboard::set_fine_adjust_modifier (/*Keyboard::PrimaryModifier | */Keyboard::SecondaryModifier); // XXX

		set_state_from_config ();
	}

	ComboBoxText _keyboard_layout_selector;
	ComboBoxText _edit_modifier_combo;
	ComboBoxText _delete_modifier_combo;
	ComboBoxText _copy_modifier_combo;
	ComboBoxText _insert_note_modifier_combo;
	ComboBoxText _snap_modifier_combo;
	ComboBoxText _snap_delta_combo;
	ComboBoxText _constraint_modifier_combo;
	ComboBoxText _slip_contents_combo;
	ComboBoxText _trim_overlap_combo;
	ComboBoxText _trim_anchored_combo;
	ComboBoxText _trim_jump_combo;
	ComboBoxText _fine_adjust_combo;
	ComboBoxText _push_points_combo;
	ComboBoxText _note_size_relative_combo;
	Adjustment _delete_button_adjustment;
	SpinButton _delete_button_spin;
	Adjustment _edit_button_adjustment;
	SpinButton _edit_button_spin;
	Adjustment _insert_note_button_adjustment;
	SpinButton _insert_note_button_spin;

};

class FontScalingOptions : public HSliderOption
{
	public:
		FontScalingOptions ()
			: HSliderOption ("font-scale", _("GUI and Font scaling"),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_font_scale),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_font_scale),
					50, 250, 1, 5,
					1024, false)
	{
		const std::string dflt = _("100%");
		const std::string empty = X_(""); // despite gtk-doc saying so, NULL does not work as reference

		_hscale.set_name("FontScaleSlider");
		_hscale.set_draw_value(false);
		_hscale.add_mark(50,  Gtk::POS_TOP, empty);
		_hscale.add_mark(60,  Gtk::POS_TOP, empty);
		_hscale.add_mark(70,  Gtk::POS_TOP, empty);
		_hscale.add_mark(80,  Gtk::POS_TOP, empty);
		_hscale.add_mark(90,  Gtk::POS_TOP, empty);
		_hscale.add_mark(100, Gtk::POS_TOP, dflt);
		_hscale.add_mark(125, Gtk::POS_TOP, empty);
		_hscale.add_mark(150, Gtk::POS_TOP, empty);
		_hscale.add_mark(175, Gtk::POS_TOP, empty);
		_hscale.add_mark(200, Gtk::POS_TOP, empty);
		_hscale.add_mark(250, Gtk::POS_TOP, empty);

		set_note (_("Adjusting the scale requires an application restart for fully accurate re-layout."));
	}

	void changed ()
	{
		HSliderOption::changed ();
		/* XXX: should be triggered from the parameter changed signal */
		UIConfiguration::instance().reset_dpi ();
	}
};

class PluginScanTimeOutSliderOption : public HSliderOption
{
public:
	PluginScanTimeOutSliderOption (RCConfiguration* c)
		: HSliderOption ("vst-scan-timeout", _("Scan Time Out"),
				sigc::mem_fun (*c, &RCConfiguration::get_plugin_scan_timeout),
				sigc::mem_fun (*c, &RCConfiguration::set_plugin_scan_timeout),
				1, 900, 50, 50)
	{
		_label.set_alignment (1.0, 0.5); // match buttons below
		_hscale.set_digits (0);
		_hscale.set_draw_value(false);
		_hscale.add_mark ( 10,  Gtk::POS_TOP, _("1 sec"));
		_hscale.add_mark (150,  Gtk::POS_TOP, _("15 sec"));
		_hscale.add_mark (300,  Gtk::POS_TOP, _("30 sec"));
		_hscale.add_mark (450,  Gtk::POS_TOP, _("45 sec"));
		_hscale.add_mark (600,  Gtk::POS_TOP, _("1 min"));
		_hscale.add_mark (900,  Gtk::POS_TOP, _("1'30\""));

		Gtkmm2ext::UI::instance()->set_tip(_hscale,
			 _("Specify the default timeout for plugin instantiation. Plugins that require more time to load will be ignored. A value of 0 disables the timeout."));
	}
};

class ClipLevelOptions : public HSliderOption
{
public:
	ClipLevelOptions ()
		: HSliderOption (X_("waveform-clip-level"),
		                 _("Waveform Clip Level (dBFS):"),
		                 sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_waveform_clip_level),
		                 sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_waveform_clip_level),
		                 -50.0, -0.5, 0.1, 1.0, /* units of dB */
		                 1.0,
		                 false)
	{
	}

	void parameter_changed (string const & p)
	{
		if (p == "waveform-clip-level") {
			ArdourWaveView::WaveView::set_clip_level (UIConfiguration::instance().get_waveform_clip_level());
		}
		if (p == "show-waveform-clipping") {
			_hscale.set_sensitive (UIConfiguration::instance().get_show_waveform_clipping ());
		}
	}

	void set_state_from_config ()
	{
		parameter_changed ("waveform-clip-level");
		parameter_changed ("show-waveform-clipping");
	}
};

class BufferingOptions : public OptionEditorComponent
{
	public:
		BufferingOptions (RCConfiguration* c)
			: _rc_config (c)
			, _label (_("Preset:"))
			, _playback ("playback-buffer-seconds", _("Playback (seconds of buffering)"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_audio_playback_buffer_seconds),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_audio_playback_buffer_seconds),
					1, 60, 1, 4)
			, _capture ("capture-buffer-seconds", _("Recording (seconds of buffering)"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_audio_capture_buffer_seconds),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_audio_capture_buffer_seconds),
					1, 60, 1, 4)
		{
			// TODO use  ComboOption
			vector<string> presets;

			/* these must match the order of the enums for BufferingPreset */
			presets.push_back (_("Small sessions (4-16 tracks)"));
			presets.push_back (_("Medium sessions (16-64 tracks)"));
			presets.push_back (_("Large sessions (64+ tracks)"));
			presets.push_back (_("Custom (set by sliders below)"));

			set_popdown_strings (_buffering_presets_combo, presets);
			_buffering_presets_combo.signal_changed().connect (sigc::mem_fun (*this, &BufferingOptions::preset_changed));

			_label.set_name ("OptionsLabel");
			_label.set_alignment (0, 0.5);
		}

		void
		add_to_page (OptionEditorPage* p)
		{
			add_widgets_to_page (p, &_label, &_buffering_presets_combo);
			_playback.add_to_page (p);
			_capture.add_to_page (p);
		}

		void parameter_changed (string const & p)
		{
			if (p == "buffering-preset") {
				switch (_rc_config->get_buffering_preset()) {
					case Small:
						_playback.set_sensitive (false);
						_capture.set_sensitive (false);
						_buffering_presets_combo.set_active (0);
						break;
					case Medium:
						_playback.set_sensitive (false);
						_capture.set_sensitive (false);
						_buffering_presets_combo.set_active (1);
						break;
					case Large:
						_playback.set_sensitive (false);
						_capture.set_sensitive (false);
						_buffering_presets_combo.set_active (2);
						break;
					case Custom:
						_playback.set_sensitive (true);
						_capture.set_sensitive (true);
						_buffering_presets_combo.set_active (3);
						break;
				}
			}
			_playback.parameter_changed (p);
			_capture.parameter_changed (p);
		}

		void set_state_from_config ()
		{
			parameter_changed ("buffering-preset");
			_playback.set_state_from_config();
			_capture.set_state_from_config();
		}

		Gtk::Widget& tip_widget() { return _buffering_presets_combo; }

	private:

		void preset_changed ()
		{
			int index = _buffering_presets_combo.get_active_row_number ();
			if (index < 0) {
				return;
			}
			switch (index) {
				case 0:
					_rc_config->set_buffering_preset (Small);
					break;
				case 1:
					_rc_config->set_buffering_preset (Medium);
					break;
				case 2:
					_rc_config->set_buffering_preset (Large);
					break;
				case 3:
					_rc_config->set_buffering_preset (Custom);
					break;
				default:
					error << string_compose (_("programming error: unknown buffering preset string, index = %1"), index) << endmsg;
					break;
			}
		}

		RCConfiguration* _rc_config;
		Label         _label;
		HSliderOption _playback;
		HSliderOption _capture;
		ComboBoxText  _buffering_presets_combo;
};

class PortSelectOption : public OptionEditorComponent, public sigc::trackable {
public:
	PortSelectOption (RCConfiguration* c, SessionHandlePtr* shp, std::string const & tooltip, std::string const & parameter_name, std::string const & label, DataType dt, PortFlags pf)
		: _rc_config (c)
                , _shp (shp)
                , _label (label)
                , _ignore_change (false)
                , data_type (dt)
                , port_flags (pf)
	{
		_store = ListStore::create (_port_columns);
		_combo.set_model (_store);
		_combo.pack_start (_port_columns.short_name);

		set_tooltip (_combo, tooltip);

		_combo.signal_map ().connect (sigc::mem_fun (*this, &PortSelectOption::on_map));
		_combo.signal_unmap ().connect (sigc::mem_fun (*this, &PortSelectOption::on_unmap));
		_combo.signal_changed ().connect (sigc::mem_fun (*this, &PortSelectOption::port_changed));
	}

	void add_to_page (OptionEditorPage* p)
	{
		add_widgets_to_page (p, &_label, &_combo);
	}

	Gtk::Widget& tip_widget()
	{
		return _combo;
	}

	void parameter_changed (string const & p)
	{
		if (p == parameter_name) {
			update_selection ();
		}
	}

	void set_state_from_config ()
	{
		parameter_changed (parameter_name);
	}

protected:
	struct PortColumns : public Gtk::TreeModel::ColumnRecord {
		PortColumns() {
			add (short_name);
			add (full_name);
		}
		Gtk::TreeModelColumn<std::string> short_name;
		Gtk::TreeModelColumn<std::string> full_name;
	};

	RCConfiguration*  _rc_config;
	SessionHandlePtr* _shp;
	Label             _label;
	Gtk::ComboBox     _combo;
	bool              _ignore_change;
	std::string        parameter_name;
	ARDOUR::DataType   data_type;
	ARDOUR::PortFlags  port_flags;
	PortColumns       _port_columns;

	Glib::RefPtr<Gtk::ListStore> _store;
	PBD::ScopedConnectionList    _engine_connection;

	void on_map ()
	{
		AudioEngine::instance()->PortRegisteredOrUnregistered.connect (
				_engine_connection,
				invalidator (*this),
				boost::bind (&PortSelectOption::update_port_combo, this),
				gui_context());

		AudioEngine::instance()->PortPrettyNameChanged.connect (
				_engine_connection,
				invalidator (*this),
				boost::bind (&PortSelectOption::update_port_combo, this),
				gui_context());
	}

	void on_unmap ()
	{
		_engine_connection.drop_connections ();
	}


	void update_port_combo ()
	{
		vector<string> ports;
		ARDOUR::AudioEngine::instance()->get_ports ("", data_type, port_flags, ports);

		PBD::Unwinder<bool> uw (_ignore_change, true);
		_store->clear ();

		TreeModel::Row row;
		row = *_store->append ();
		row[_port_columns.full_name] = string();
		row[_port_columns.short_name] = _("Disconnected");

		for (vector<string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {
			row = *_store->append ();
			row[_port_columns.full_name] = *p;
			std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
			if (pn.empty ()) {
				pn = (*p).substr ((*p).find (':') + 1);
			}
			row[_port_columns.short_name] = pn;
		}

		update_selection ();
	}

	virtual void update_selection() = 0;
	virtual void port_changed () = 0;
};

class LTCPortSelectOption : public PortSelectOption
{
public:
	LTCPortSelectOption (RCConfiguration* c, SessionHandlePtr* shp)
		: PortSelectOption (c, shp,
		                       _("The LTC generator output will be auto-connected to this port when a session is loaded."),
		                       X_("ltc-output-port"),
		                       _("LTC Output Port:"),
		                       ARDOUR::DataType::AUDIO,
		                       ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsTerminal)) {
		/* cannot call from parent due to the method being pure virtual */
		update_port_combo ();
	}

	void port_changed ()
	{
		if (_ignore_change) {
			return;
		}
		TreeModel::iterator active = _combo.get_active ();
		string new_port = (*active)[_port_columns.full_name];
		_rc_config->set_ltc_output_port (new_port);

		if (!_shp->session()) {
			return;
		}
		boost::shared_ptr<Port> ltc_port = _shp->session()->ltc_output_port ();
		if (!ltc_port) {
			return;
		}
		if (ltc_port->connected_to (new_port)) {
			return;
		}

		ltc_port->disconnect_all ();
		if (!new_port.empty()) {
			ltc_port->connect (new_port);
		}
	}

	void update_selection ()
	{
		int n;
		Gtk::TreeModel::Children children = _store->children();
		Gtk::TreeModel::Children::iterator i = children.begin();
		++i; /* skip "Disconnected" */

		std::string const& pn = _rc_config->get_ltc_output_port ();
		boost::shared_ptr<Port> ltc_port;
		if (_shp->session()) {
			ltc_port = _shp->session()->ltc_output_port ();
		}

		PBD::Unwinder<bool> uw (_ignore_change, true);

		/* try match preference with available port-names */
		for (n = 1;  i != children.end(); ++i, ++n) {
			string port_name = (*i)[_port_columns.full_name];
			if (port_name == pn) {
				_combo.set_active (n);
				return;
			}
		}

		/* Set preference to current port connection
		 * (LTC is auto-connected at session load).
		 */
		if (ltc_port) {
			i = children.begin();
			++i; /* skip "Disconnected" */
			for (n = 1;  i != children.end(); ++i, ++n) {
				string port_name = (*i)[_port_columns.full_name];
				if (ltc_port->connected_to (port_name)) {
					_combo.set_active (n);
					return;
				}
			}
		}

		if (pn.empty ()) {
			_combo.set_active (0); /* disconnected */
		} else {
			/* The port is currently not available, retain preference */
			TreeModel::Row row = *_store->append ();
			row[_port_columns.full_name] = pn;
			row[_port_columns.short_name] = (pn).substr ((pn).find (':') + 1);
			_combo.set_active (n);
		}
	}

};

class TriggerPortSelectOption : public PortSelectOption
{
public:
	TriggerPortSelectOption (RCConfiguration* c, SessionHandlePtr* shp)
		: PortSelectOption (c, shp,
		                    _("TriggerBoxes will be connected to this port when it is set."),
		                    X_("default-trigger-input-port"),
		                    _("Default trigger input:"),
		                    ARDOUR::DataType::MIDI,
		                    ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsTerminal)) {
		/* cannot call from parent due to the method being pure virtual */
		update_port_combo ();
	}

	void port_changed ()
	{
		if (_ignore_change) {
			return;
		}
		TreeModel::iterator active = _combo.get_active ();
		string new_port = (*active)[_port_columns.full_name];
		_rc_config->set_default_trigger_input_port (new_port);
		/* everything that needs it will pick up the new port via ParameterChanged */
	}

	void update_selection ()
	{
		int n;
		Gtk::TreeModel::Children children = _store->children();
		Gtk::TreeModel::Children::iterator i = children.begin();
		++i; /* skip "Disconnected" */

		std::string const& pn = _rc_config->get_default_trigger_input_port ();
		boost::shared_ptr<Port> port;

		PBD::Unwinder<bool> uw (_ignore_change, true);

		/* try match preference with available port-names */
		for (n = 1;  i != children.end(); ++i, ++n) {
			string port_name = (*i)[_port_columns.full_name];
			if (port_name == pn) {
				_combo.set_active (n);
				return;
			}
		}

		if (pn.empty ()) {
			_combo.set_active (0); /* disconnected */
		} else {
			/* The port is currently not available, retain preference */
			TreeModel::Row row = *_store->append ();
			row[_port_columns.full_name] = pn;
			row[_port_columns.short_name] = (pn).substr ((pn).find (':') + 1);
			_combo.set_active (n);
		}
	}

};

class ControlSurfacesOptions : public OptionEditorMiniPage
{
	public:
		ControlSurfacesOptions ()
			: _ignore_view_change (0)
		{
			_store = ListStore::create (_model);
			_view.set_model (_store);
			_view.append_column_editable (_("Enable"), _model.enabled);
			_view.append_column (_("Control Surface Protocol"), _model.name);
			_view.get_column(1)->set_resizable (true);
			_view.get_column(1)->set_expand (true);

			Gtk::HBox* edit_box = manage (new Gtk::HBox);
			edit_box->set_spacing(3);
			edit_box->show ();

			Label* label = manage (new Label);
			label->set_text (_("Edit the settings for selected protocol (it must be ENABLED first):"));
			edit_box->pack_start (*label, false, false);
			label->show ();

			edit_button = manage (new Button(_("Show Protocol Settings")));
			edit_button->signal_clicked().connect (sigc::mem_fun(*this, &ControlSurfacesOptions::edit_btn_clicked));
			edit_box->pack_start (*edit_button, true, true);
			edit_button->set_sensitive (false);
			edit_button->show ();

			int const n = table.property_n_rows();
			table.resize (n + 2, 3);
			table.attach (_view, 0, 3, n, n + 1);
			table.attach (*edit_box, 0, 3, n + 1, n + 2);

			ControlProtocolManager& m = ControlProtocolManager::instance ();
			m.ProtocolStatusChange.connect (protocol_status_connection, MISSING_INVALIDATOR,
					boost::bind (&ControlSurfacesOptions::protocol_status_changed, this, _1), gui_context());

			_store->signal_row_changed().connect (sigc::mem_fun (*this, &ControlSurfacesOptions::view_changed));
			_view.signal_button_press_event().connect_notify (sigc::mem_fun(*this, &ControlSurfacesOptions::edit_clicked));
			_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &ControlSurfacesOptions::selection_changed));
		}

		void parameter_changed (std::string const &)
		{

		}

		void set_state_from_config ()
		{
			_store->clear ();

			ControlProtocolManager& m = ControlProtocolManager::instance ();
			for (list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {

				if (!(*i)->mandatory) {
					TreeModel::Row r = *_store->append ();
					r[_model.name] = (*i)->name;
					r[_model.enabled] = 0 != (*i)->protocol;
					r[_model.protocol_info] = *i;
				}
			}
		}

	private:

		void protocol_status_changed (ControlProtocolInfo* cpi) {
			/* find the row */
			TreeModel::Children rows = _store->children();

			for (TreeModel::Children::iterator x = rows.begin(); x != rows.end(); ++x) {
				string n = ((*x)[_model.name]);

				if ((*x)[_model.protocol_info] == cpi) {
					_ignore_view_change++;
					(*x)[_model.enabled] = 0 != cpi->protocol;
					_ignore_view_change--;
					selection_changed (); // update sensitivity
					break;
				}
			}
		}

		void selection_changed ()
		{
			//enable the Edit button when a row is selected for editing
			TreeModel::Row row = *(_view.get_selection()->get_selected());
			if (row && row[_model.enabled]) {
				ControlProtocolInfo* cpi = row[_model.protocol_info];
				edit_button->set_sensitive (cpi && cpi->protocol && cpi->protocol->has_editor ());
			} else {
				edit_button->set_sensitive (false);
			}
		}

		void view_changed (TreeModel::Path const &, TreeModel::iterator const & i)
		{
			TreeModel::Row r = *i;

			if (_ignore_view_change) {
				return;
			}

			ControlProtocolInfo* cpi = r[_model.protocol_info];
			if (!cpi) {
				return;
			}

			bool const was_enabled = (cpi->protocol != 0);
			bool const is_enabled = r[_model.enabled];


			if (was_enabled != is_enabled) {

				if (!was_enabled) {
					ControlProtocolManager::instance().activate (*cpi);
				} else {
					ControlProtocolManager::instance().deactivate (*cpi);
				}
			}

			selection_changed ();
		}

		void edit_btn_clicked ()
		{
			std::string name;
			ControlProtocolInfo* cpi;
			TreeModel::Row row;

			row = *(_view.get_selection()->get_selected());
			if (!row[_model.enabled]) {
				return;
			}
			cpi = row[_model.protocol_info];
			if (!cpi || !cpi->protocol || !cpi->protocol->has_editor ()) {
				return;
			}
			Box* box = (Box*) cpi->protocol->get_gui ();
			if (!box) {
				return;
			}
			if (box->get_parent()) {
				static_cast<ArdourWindow*>(box->get_parent())->present();
				return;
			}
			WindowTitle title (Glib::get_application_name());
			title += row[_model.name];
			title += _("Configuration");
			/* once created, the window is managed by the surface itself (as ->get_parent())
			 * Surface's tear_down_gui() is called on session close, when de-activating
			 * or re-initializing a surface.
			 * tear_down_gui() hides an deletes the Window if it exists.
			 */
			ArdourWindow* win = new ArdourWindow (*((Gtk::Window*) _view.get_toplevel()), title.get_string());
			win->set_title (_("Control Protocol Settings"));
			win->add (*box);
			box->show ();
			win->present ();
		}

		void edit_clicked (GdkEventButton* ev)
		{
			if (ev->type != GDK_2BUTTON_PRESS) {
				return;
			}

			edit_btn_clicked();
		}

		class ControlSurfacesModelColumns : public TreeModelColumnRecord
	{
		public:

			ControlSurfacesModelColumns ()
			{
				add (name);
				add (enabled);
				add (protocol_info);
			}

			TreeModelColumn<string> name;
			TreeModelColumn<bool> enabled;
			TreeModelColumn<ControlProtocolInfo*> protocol_info;
	};

		Glib::RefPtr<ListStore> _store;
		ControlSurfacesModelColumns _model;
		TreeView _view;
		PBD::ScopedConnection protocol_status_connection;
		uint32_t _ignore_view_change;
		Gtk::Button* edit_button;
};

class VideoTimelineOptions : public OptionEditorMiniPage
{
	public:
		VideoTimelineOptions (RCConfiguration* c)
			: _rc_config (c)
			, _show_video_export_info_button (_("Show Video Export Info before export"))
			, _show_video_server_dialog_button (_("Show Video Server Startup Dialog"))
			, _video_advanced_setup_button (_("Advanced Setup (remote video server)"))
			, _xjadeo_browse_button (_("Browse..."))
		{
			Table* t = &table;
			int n = table.property_n_rows();

			t->attach (_show_video_export_info_button, 1, 4, n, n + 1);
			_show_video_export_info_button.signal_toggled().connect (sigc::mem_fun (*this, &VideoTimelineOptions::show_video_export_info_toggled));
			Gtkmm2ext::UI::instance()->set_tip (_show_video_export_info_button,
					_("<b>When enabled</b> an information window with details is displayed before the video-export dialog."));
			++n;

			t->attach (_show_video_server_dialog_button, 1, 4, n, n + 1);
			_show_video_server_dialog_button.signal_toggled().connect (sigc::mem_fun (*this, &VideoTimelineOptions::show_video_server_dialog_toggled));
			Gtkmm2ext::UI::instance()->set_tip (_show_video_server_dialog_button,
					_("<b>When enabled</b> the video server is never launched automatically without confirmation"));
			++n;

			t->attach (_video_advanced_setup_button, 1, 4, n, n + 1, FILL);
			_video_advanced_setup_button.signal_toggled().connect (sigc::mem_fun (*this, &VideoTimelineOptions::video_advanced_setup_toggled));
			Gtkmm2ext::UI::instance()->set_tip (_video_advanced_setup_button,
					_("<b>When enabled</b> you can specify a custom video-server URL and docroot. - Do not enable this option unless you know what you are doing."));
			++n;

			Label* l = manage (new Label (_("Video Server URL:")));
			l->set_alignment (0, 0.5);
			t->attach (*l, 1, 2, n, n + 1, FILL);
			t->attach (_video_server_url_entry, 2, 4, n, n + 1, FILL);
			Gtkmm2ext::UI::instance()->set_tip (_video_server_url_entry,
					_("Base URL of the video-server including http prefix. This is usually 'http://hostname.example.org:1554/' and defaults to 'http://localhost:1554/' when the video-server is running locally"));
			++n;

			l = manage (new Label (_("Video Folder:")));
			l->set_alignment (0, 0.5);
			t->attach (*l, 1, 2, n, n + 1, FILL);
			t->attach (_video_server_docroot_entry, 2, 4, n, n + 1);
			Gtkmm2ext::UI::instance()->set_tip (_video_server_docroot_entry,
					_("Local path to the video-server document-root. Only files below this directory will be accessible by the video-server. If the server run on a remote host, it should point to a network mounted folder of the server's docroot or be left empty if it is unavailable. It is used for the local video-monitor and file-browsing when opening/adding a video file."));
			++n;

			l = manage (new Label (""));
			t->attach (*l, 0, 4, n, n + 1, EXPAND | FILL);
			++n;

			l = manage (new Label (string_compose ("<b>%1</b>", _("Video Monitor"))));
			l->set_use_markup (true);
			l->set_alignment (0, 0.5);
			t->attach (*l, 0, 4, n, n + 1, EXPAND | FILL);
			++n;

			l = manage (new Label (string_compose (_("Custom Path to Video Monitor (%1) - leave empty for default:"),
#ifdef __APPLE__
							"Jadeo.app"
#elif defined PLATFORM_WINDOWS
							"xjadeo.exe"
#else
							"xjadeo"
#endif
							)));
			l->set_alignment (0, 0.5);
			t->attach (*l, 1, 4, n, n + 1, FILL);
			++n;

			t->attach (_custom_xjadeo_path, 2, 3, n, n + 1, EXPAND|FILL);
			Gtkmm2ext::UI::instance()->set_tip (_custom_xjadeo_path, _("Set a custom path to the Video Monitor Executable, changing this requires a restart."));
			t->attach (_xjadeo_browse_button, 3, 4, n, n + 1, FILL);

			_video_server_url_entry.signal_changed().connect (sigc::mem_fun(*this, &VideoTimelineOptions::server_url_changed));
			_video_server_url_entry.signal_activate().connect (sigc::mem_fun(*this, &VideoTimelineOptions::server_url_changed));
			_video_server_docroot_entry.signal_changed().connect (sigc::mem_fun(*this, &VideoTimelineOptions::server_docroot_changed));
			_video_server_docroot_entry.signal_activate().connect (sigc::mem_fun(*this, &VideoTimelineOptions::server_docroot_changed));
			_custom_xjadeo_path.signal_changed().connect (sigc::mem_fun (*this, &VideoTimelineOptions::custom_xjadeo_path_changed));
			_xjadeo_browse_button.signal_clicked ().connect (sigc::mem_fun (*this, &VideoTimelineOptions::xjadeo_browse_clicked));
		}

		void server_url_changed ()
		{
			_rc_config->set_video_server_url (_video_server_url_entry.get_text());
		}

		void server_docroot_changed ()
		{
			_rc_config->set_video_server_docroot (_video_server_docroot_entry.get_text());
		}

		void show_video_export_info_toggled ()
		{
			bool const x = _show_video_export_info_button.get_active ();
			_rc_config->set_show_video_export_info (x);
		}

		void show_video_server_dialog_toggled ()
		{
			bool const x = _show_video_server_dialog_button.get_active ();
			_rc_config->set_show_video_server_dialog (x);
		}

		void video_advanced_setup_toggled ()
		{
			bool const x = _video_advanced_setup_button.get_active ();
			_rc_config->set_video_advanced_setup(x);
		}

		void custom_xjadeo_path_changed ()
		{
			_rc_config->set_xjadeo_binary (_custom_xjadeo_path.get_text());
		}

		void xjadeo_browse_clicked ()
		{
			Gtk::FileChooserDialog dialog(_("Set Video Monitor Executable"), Gtk::FILE_CHOOSER_ACTION_OPEN);
			dialog.set_filename (_rc_config->get_xjadeo_binary());
			dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
			dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
			if (dialog.run () == Gtk::RESPONSE_OK) {
				const std::string& filename = dialog.get_filename();
				if (!filename.empty() && (
#ifdef __APPLE__
							Glib::file_test (filename + "/Contents/MacOS/xjadeo", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE) ||
#endif
							Glib::file_test (filename, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)
							)) {
					_rc_config->set_xjadeo_binary (filename);
				}
			}
		}

		void parameter_changed (string const & p)
		{
			if (p == "video-server-url") {
				_video_server_url_entry.set_text (_rc_config->get_video_server_url());
			} else if (p == "video-server-docroot") {
				_video_server_docroot_entry.set_text (_rc_config->get_video_server_docroot());
			} else if (p == "show-video-export-info") {
				bool const x = _rc_config->get_show_video_export_info();
				_show_video_export_info_button.set_active (x);
			} else if (p == "show-video-server-dialog") {
				bool const x = _rc_config->get_show_video_server_dialog();
				_show_video_server_dialog_button.set_active (x);
			} else if (p == "video-advanced-setup") {
				bool const x = _rc_config->get_video_advanced_setup();
				_video_advanced_setup_button.set_active(x);
				_video_server_docroot_entry.set_sensitive(x);
				_video_server_url_entry.set_sensitive(x);
			} else if (p == "xjadeo-binary") {
				_custom_xjadeo_path.set_text (_rc_config->get_xjadeo_binary());
			}
		}

		void set_state_from_config ()
		{
			parameter_changed ("video-server-url");
			parameter_changed ("video-server-docroot");
			parameter_changed ("video-monitor-setup-dialog");
			parameter_changed ("show-video-export-info");
			parameter_changed ("show-video-server-dialog");
			parameter_changed ("video-advanced-setup");
			parameter_changed ("xjadeo-binary");
		}

	private:
		RCConfiguration* _rc_config;
		Entry _video_server_url_entry;
		Entry _video_server_docroot_entry;
		Entry _custom_xjadeo_path;
		CheckButton _show_video_export_info_button;
		CheckButton _show_video_server_dialog_button;
		CheckButton _video_advanced_setup_button;
		Button _xjadeo_browse_button;
};

class ColumVisibilityOption : public Option
{
	public:
	ColumVisibilityOption (string id, string name, uint32_t n_col, sigc::slot<uint32_t> get, sigc::slot<bool, uint32_t> set)
		: Option (id, name)
		, _heading (name)
		, _n_col (n_col)
		, _get (get)
		, _set (set)
	{
		cb = (CheckButton**) malloc (sizeof (CheckButton*) * n_col);
		for (uint32_t i = 0; i < n_col; ++i) {
			CheckButton* col = manage (new CheckButton (string_compose (_("Column %1 (Actions %2 + %3)"), i + 1, i * 2 + 1, i * 2 + 2)));
			col->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ColumVisibilityOption::column_toggled), i));
			_vbox.pack_start (*col);
			cb[i] = col;
		}
		parameter_changed (id);
	}

	~ColumVisibilityOption () {
		free (cb);
	}

	Gtk::Widget& tip_widget() { return _vbox; }

	void set_state_from_config ()
	{
		uint32_t c = _get();
		for (uint32_t i = 0; i < _n_col; ++i) {
			bool en = (c & (1<<i)) ? true : false;
			if (cb[i]->get_active () != en) {
				cb[i]->set_active (en);
			}
		}
	}

	void add_to_page (OptionEditorPage* p)
	{
		_heading.add_to_page (p);
		add_widget_to_page (p, &_vbox);
	}
	private:

	void column_toggled (int b) {
		uint32_t c = _get();
		uint32_t cc = c;
		if (cb[b]->get_active ()) {
			c |= (1<<b);
		} else {
			c &= ~(1<<b);
		}
		if (cc != c) {
			_set (c);
		}
	}

	VBox _vbox;
	OptionEditorHeading _heading;

	CheckButton** cb;
	uint32_t _n_col;
	sigc::slot<uint32_t> _get;
	sigc::slot<bool, uint32_t> _set;
};


/** A class which allows control of visibility of some editor components using
 *  a VisibilityGroup.  The caller should pass in a `dummy' VisibilityGroup
 *  which has the correct members, but with null widget pointers.  This
 *  class allows the user to set visibility of the members, the details
 *  of which are stored in a configuration variable which can be watched
 *  by parts of the editor that actually contain the widgets whose visibility
 *  is being controlled.
 */

class VisibilityOption : public Option
{
public:
	/** @param name User-visible name for this group.
	 *  @param g `Dummy' VisibilityGroup (as described above).
	 *  @param get Method to get the value of the appropriate configuration variable.
	 *  @param set Method to set the value of the appropriate configuration variable.
	 */
	VisibilityOption (string name, VisibilityGroup* g, sigc::slot<string> get, sigc::slot<bool, string> set)
		: Option (g->get_state_name(), name)
		, _heading (name)
		, _visibility_group (g)
		, _get (get)
		, _set (set)
	{
		/* Watch for changes made by the user to our members */
		_visibility_group->VisibilityChanged.connect_same_thread (
			_visibility_group_connection, sigc::bind (&VisibilityOption::changed, this)
			);
	}

	void set_state_from_config ()
	{
		/* Set our state from the current configuration */
		_visibility_group->set_state (_get ());
	}

	void add_to_page (OptionEditorPage* p)
	{
		_heading.add_to_page (p);
		add_widget_to_page (p, _visibility_group->list_view ());
	}

	Gtk::Widget& tip_widget() { return *_visibility_group->list_view (); }

private:
	void changed ()
	{
		/* The user has changed something, so reflect this change
		   in the RCConfiguration.
		*/
		_set (_visibility_group->get_state_value ());
	}

	OptionEditorHeading _heading;
	VisibilityGroup* _visibility_group;
	sigc::slot<std::string> _get;
	sigc::slot<bool, std::string> _set;
	PBD::ScopedConnection _visibility_group_connection;
};


class MidiPortOptions : public OptionEditorMiniPage, public sigc::trackable
{
	public:
		MidiPortOptions()
			: input_heading (_("MIDI Inputs"))
			, output_heading (_("MIDI Outputs"))
		{
			setup_midi_port_view (midi_output_view, false);
			setup_midi_port_view (midi_input_view, true);

			input_heading.add_to_page (this);

			input_scroller.add (midi_input_view);
			input_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
			input_scroller.set_size_request (-1, 180);
			input_scroller.show ();

			int n = table.property_n_rows();
			table.attach (input_scroller, 0, 3, n, n + 1, FILL | EXPAND);

			output_heading.add_to_page (this);

			output_scroller.add (midi_output_view);
			output_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
			output_scroller.set_size_request (-1, 180);
			output_scroller.show ();

			n = table.property_n_rows();
			table.attach (output_scroller, 0, 3, n, n + 1, FILL | EXPAND);

			midi_output_view.show ();
			midi_input_view.show ();

			table.signal_map().connect (sigc::mem_fun (*this, &MidiPortOptions::on_map));
			table.signal_unmap().connect (sigc::mem_fun (*this, &MidiPortOptions::on_unmap));
		}

		void parameter_changed (string const&) {}
		void set_state_from_config() {}

		void on_map () {
			refill ();

			AudioEngine::instance()->PortRegisteredOrUnregistered.connect (connections,
					invalidator (*this),
					boost::bind (&MidiPortOptions::refill, this),
					gui_context());
			AudioEngine::instance()->MidiPortInfoChanged.connect (connections,
					invalidator (*this),
					boost::bind (&MidiPortOptions::refill, this),
					gui_context());
			AudioEngine::instance()->MidiSelectionPortsChanged.connect (connections,
					invalidator (*this),
					boost::bind (&MidiPortOptions::refill, this),
					gui_context());
		}

		void on_unmap () {
			connections.drop_connections ();
		}

		void refill () {
			if (refill_midi_ports (true, midi_input_view)) {
				input_heading.tip_widget ().show ();
				input_scroller.show ();
			} else {
				input_heading.tip_widget ().hide ();
				input_scroller.hide ();
			}
			if (refill_midi_ports (false, midi_output_view)) {
				output_heading.tip_widget ().show ();
				output_scroller.show ();
			} else {
				output_heading.tip_widget ().hide ();
				output_scroller.hide ();
			}
		}

	private:
		PBD::ScopedConnectionList connections;

		/* MIDI port management */
		struct MidiPortColumns : public Gtk::TreeModel::ColumnRecord {

			MidiPortColumns () {
				add (pretty_name);
				add (music_data);
				add (control_data);
				add (selection);
				add (fullname);
				add (shortname);
				add (filler);
			}

			Gtk::TreeModelColumn<std::string> pretty_name;
			Gtk::TreeModelColumn<bool> music_data;
			Gtk::TreeModelColumn<bool> control_data;
			Gtk::TreeModelColumn<bool> selection;
			Gtk::TreeModelColumn<std::string> fullname;
			Gtk::TreeModelColumn<std::string> shortname;
			Gtk::TreeModelColumn<std::string> filler;
		};

		MidiPortColumns midi_port_columns;
		Gtk::TreeView midi_input_view;
		Gtk::TreeView midi_output_view;

		Gtk::ScrolledWindow input_scroller;
		Gtk::ScrolledWindow output_scroller;
		OptionEditorHeading input_heading;
		OptionEditorHeading output_heading;

		void setup_midi_port_view (Gtk::TreeView&, bool with_selection);
		bool refill_midi_ports (bool for_input, Gtk::TreeView&);
		void pretty_name_edit (std::string const & path, std::string const & new_text, Gtk::TreeView*);
		void midi_music_column_toggled (std::string const & path, Gtk::TreeView*);
		void midi_control_column_toggled (std::string const & path, Gtk::TreeView*);
		void midi_selection_column_toggled (std::string const & path, Gtk::TreeView*);
};

void
MidiPortOptions::setup_midi_port_view (Gtk::TreeView& view, bool with_selection)
{
	int pretty_name_column;
	int music_column;
	int control_column;
	int selection_column;
	TreeViewColumn* col;
	Gtk::Label* l;

	pretty_name_column = view.append_column_editable (_("Name (click twice to edit)"), midi_port_columns.pretty_name) - 1;

	col = manage (new TreeViewColumn ("", midi_port_columns.music_data));
	col->set_alignment (ALIGN_CENTER);
	l = manage (new Label (_("Music Data")));
	set_tooltip (*l, string_compose (_("If ticked, %1 will consider this port to be a source of music performance data."), PROGRAM_NAME));
	col->set_widget (*l);
	l->show ();
	music_column = view.append_column (*col) - 1;

	col = manage (new TreeViewColumn ("", midi_port_columns.control_data));
	col->set_alignment (ALIGN_CENTER);
	l = manage (new Label (_("Control Data")));
	set_tooltip (*l, string_compose (_("If ticked, %1 will consider this port to be a source of control data."), PROGRAM_NAME));
	col->set_widget (*l);
	l->show ();
	control_column = view.append_column (*col) - 1;

	if (with_selection) {
		col = manage (new TreeViewColumn (_("Follow Selection"), midi_port_columns.selection));
		selection_column = view.append_column (*col) - 1;
		l = manage (new Label (_("Follow Selection")));
		set_tooltip (*l, string_compose (_("If ticked, and \"MIDI input follows selection\" is enabled,\n%1 will automatically connect the first selected MIDI track to this port.\n"), PROGRAM_NAME));
		col->set_widget (*l);
		l->show ();
	}

	/* filler column so that the last real column doesn't expand */
	view.append_column ("", midi_port_columns.filler);

	CellRendererText* pretty_name_cell = dynamic_cast<CellRendererText*> (view.get_column_cell_renderer (pretty_name_column));
	pretty_name_cell->property_editable() = true;
	pretty_name_cell->signal_edited().connect (sigc::bind (sigc::mem_fun (*this, &MidiPortOptions::pretty_name_edit), &view));

	CellRendererToggle* toggle_cell;

	toggle_cell = dynamic_cast<CellRendererToggle*> (view.get_column_cell_renderer (music_column));
	toggle_cell->property_activatable() = true;
	toggle_cell->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiPortOptions::midi_music_column_toggled), &view));

	toggle_cell = dynamic_cast<CellRendererToggle*> (view.get_column_cell_renderer (control_column));
	toggle_cell->property_activatable() = true;
	toggle_cell->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiPortOptions::midi_control_column_toggled), &view));

	if (with_selection) {
		toggle_cell = dynamic_cast<CellRendererToggle*> (view.get_column_cell_renderer (selection_column));
		toggle_cell->property_activatable() = true;
		toggle_cell->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiPortOptions::midi_selection_column_toggled), &view));
	}

	view.get_selection()->set_mode (SELECTION_SINGLE);
	view.set_tooltip_column (5); /* port short name */
	view.get_column(0)->set_resizable (true);
	view.get_column(0)->set_expand (true);
}

bool
MidiPortOptions::refill_midi_ports (bool for_input, Gtk::TreeView& view)
{
	using namespace ARDOUR;

	std::vector<string> ports;
	AudioEngine::instance()->get_configurable_midi_ports (ports, for_input);

	if (ports.empty()) {
		view.hide ();
		return false;
	}

	view.unset_model ();

	Glib::RefPtr<ListStore> model = Gtk::ListStore::create (midi_port_columns);

	for (vector<string>::const_iterator s = ports.begin(); s != ports.end(); ++s) {

		TreeModel::Row row  = *(model->append());
		MidiPortFlags flags = AudioEngine::instance()->midi_port_metadata (*s);
		std::string   pn    = AudioEngine::instance()->get_pretty_name_by_name (*s);

		row[midi_port_columns.music_data]   = flags & MidiPortMusic;
		row[midi_port_columns.control_data] = flags & MidiPortControl;
		row[midi_port_columns.selection]    = flags & MidiPortSelection;
		row[midi_port_columns.fullname]     = *s;
		row[midi_port_columns.shortname]    = AudioEngine::instance()->short_port_name_from_port_name (*s);
		row[midi_port_columns.pretty_name]  = pn.empty () ? row[midi_port_columns.shortname] : pn;
	}

	view.set_model (model);
	view.show ();

	return true;
}

void
MidiPortOptions::midi_music_column_toggled (string const & path, TreeView* view)
{
	TreeIter iter = view->get_model()->get_iter (path);

	if (!iter) {
		return;
	}

	bool new_value = ! bool ((*iter)[midi_port_columns.music_data]);

	/* don't reset model - wait for MidiPortInfoChanged signal */

	if (new_value) {
		ARDOUR::AudioEngine::instance()->add_midi_port_flags ((*iter)[midi_port_columns.fullname], MidiPortMusic);
	} else {
		ARDOUR::AudioEngine::instance()->remove_midi_port_flags ((*iter)[midi_port_columns.fullname], MidiPortMusic);
	}
}

void
MidiPortOptions::midi_control_column_toggled (string const & path, TreeView* view)
{
	TreeIter iter = view->get_model()->get_iter (path);

	if (!iter) {
		return;
	}

	bool new_value = ! bool ((*iter)[midi_port_columns.control_data]);

	/* don't reset model - wait for MidiPortInfoChanged signal */

	if (new_value) {
		ARDOUR::AudioEngine::instance()->add_midi_port_flags ((*iter)[midi_port_columns.fullname], MidiPortControl);
	} else {
		ARDOUR::AudioEngine::instance()->remove_midi_port_flags ((*iter)[midi_port_columns.fullname], MidiPortControl);
	}
}

void
MidiPortOptions::midi_selection_column_toggled (string const & path, TreeView* view)
{
	TreeIter iter = view->get_model()->get_iter (path);

	if (!iter) {
		return;
	}

	bool new_value = ! bool ((*iter)[midi_port_columns.selection]);

	/* don't reset model - wait for MidiSelectionPortsChanged signal */

	if (new_value) {
		ARDOUR::AudioEngine::instance()->add_midi_port_flags ((*iter)[midi_port_columns.fullname], MidiPortSelection);
	} else {
		ARDOUR::AudioEngine::instance()->remove_midi_port_flags ((*iter)[midi_port_columns.fullname], MidiPortSelection);
	}
}

void
MidiPortOptions::pretty_name_edit (std::string const & path, string const & new_text, Gtk::TreeView* view)
{
	TreeIter iter = view->get_model()->get_iter (path);

	if (!iter) {
		return;
	}

	AudioEngine::instance()->set_port_pretty_name ((*iter)[midi_port_columns.fullname], new_text);
}



RCOptionEditor::RCOptionEditor ()
	: OptionEditorContainer (Config)
	  /* pack self-as-vbox into tabbable */
	, Tabbable (*this, _("Preferences"), X_("preferences"), /* detached by default */ false)
	, _rc_config (Config)
	, _mixer_strip_visibility ("mixer-element-visibility")
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RCOptionEditor::parameter_changed));
	BoolOption* bo;

	/* GENERAL *****************************************************************/

	add_option (_("General"), new OptionEditorHeading (_("Audio/MIDI Setup")));

	add_option (_("General"),
			new RcActionButton (_("Show Audio/MIDI Setup Window"),
				sigc::mem_fun (*this, &RCOptionEditor::show_audio_setup)));

	bo = new BoolOption (
		     "try-autostart-engine",
		     _("Try to auto-launch audio/midi engine"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_try_autostart_engine),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_try_autostart_engine)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    _("When opening an existing session, if the most recent audio engine is available and can open the session's sample rate, the audio engine dialog may be skipped."));
	add_option (_("General"), bo );

	add_option (_("General"), new OptionEditorHeading (S_("Options|Editor Undo")));

	add_option (_("General"), new UndoOptions (_rc_config));

	add_option (_("General"),
	     new BoolOption (
		     "verify-remove-last-capture",
		     _("Verify removal of last capture"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_verify_remove_last_capture),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_verify_remove_last_capture)
		     ));

	add_option (_("General"), new OptionEditorHeading (_("Session Management")));

	add_option (_("General"),
	     new BoolOption (
		     "periodic-safety-backups",
		     _("Make periodic backups of the session file"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_periodic_safety_backups),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_periodic_safety_backups)
		     ));

	add_option (_("General"), new DirectoryOption (
			    X_("default-session-parent-dir"),
			    _("Default folder for new sessions:"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_default_session_parent_dir),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_default_session_parent_dir)
			    ));

	add_option (_("General"),
	     new SpinOption<uint32_t> (
		     "max-recent-sessions",
		     _("Maximum number of recent sessions"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_max_recent_sessions),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_max_recent_sessions),
		     0, 1000, 1, 20
		     ));

	add_option (_("General"), new OptionEditorHeading (_("Import")));

	add_option (_("General"),
	     new BoolOption (
		     "only-copy-imported-files",
		     _("Drag and drop import always copies files to session"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_only_copy_imported_files),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_only_copy_imported_files)
		     ));

	add_option (_("General"), new OptionEditorHeading (_("Export")));

	add_option (_("General"),
	     new BoolOption (
		     "save-export-analysis-image",
		     _("Save loudness analysis as image file after export"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_save_export_analysis_image),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_save_export_analysis_image)
		     ));

	add_option (_("General"),
	     new BoolOption (
		     "save-export-mixer-screenshot",
		     _("Save Mixer screenshot after export"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_save_export_mixer_screenshot),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_save_export_mixer_screenshot)
		     ));

#if defined PHONE_HOME && !defined MIXBUS
	add_option (_("General"), new OptionEditorHeading (_("New Version Check")));
	bo = new BoolOption (
		     "check-announcements",
		     _("Check for announcements at application start"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_check_announcements),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_check_announcements)
		     );
	bo ->set_note (string_compose (_("An anonymous request is performed to query announcements by contacting\n%1"),
#ifdef __APPLE__
				Config->get_osx_pingback_url ()
#elif defined PLATFORM_WINDOWS
				Config->get_windows_pingback_url ()
#else
				Config->get_linux_pingback_url ()
#endif
				));
	add_option (_("General"), bo);

#endif

	/* APPEARANCE ***************************************************************/

	if (!ARDOUR::Profile->get_mixbus()) {
		add_option (_("Appearance"), new OptionEditorHeading (_("GUI Lock")));
		/* Lock GUI timeout */

		HSliderOption *slts = new HSliderOption("lock-gui-after-seconds",
				_("Lock timeout (seconds)"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_lock_gui_after_seconds),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_lock_gui_after_seconds),
				0, 1000, 1, 10
				);
		slts->scale().set_digits (0);
		Gtkmm2ext::UI::instance()->set_tip (
				slts->tip_widget(),
				_("Lock GUI after this many idle seconds (zero to never lock)"));
		add_option (_("Appearance"), slts);

		ComboOption<ScreenSaverMode>* scsvr = new ComboOption<ScreenSaverMode> (
				"screen-saver-mode",
				_("System Screensaver Mode"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_screen_saver_mode),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_screen_saver_mode)
				);

		scsvr->add (InhibitNever, _("Never Inhibit"));
		scsvr->add (InhibitWhileRecording, _("Inhibit while Recording"));
		scsvr->add (InhibitAlways, string_compose (_("Inhibit while %1 is running"), PROGRAM_NAME));

		add_option (_("Appearance"), scsvr);


	} // !mixbus

	add_option (_("Appearance"), new OptionEditorHeading (_("Theme")));

	add_option (_("Appearance"), new BoolOption (
				"flat-buttons",
				_("Draw \"flat\" buttons"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_flat_buttons),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_flat_buttons)
				));

	add_option (_("Appearance"), new BoolOption (
				"boxy-buttons",
				_("Draw \"boxy\" buttons"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_boxy_buttons),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_boxy_buttons)
				));

	add_option (_("Appearance"), new BoolOption (
				"meter-style-led",
				_("LED meter style"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_meter_style_led),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_meter_style_led)
				));

	if (!Profile->get_mixbus()) {
		vector<string> icon_sets = ::get_icon_sets ();
		if (icon_sets.size() > 1) {
			ComboOption<std::string>* io = new ComboOption<std::string> (
					"icon-set", _("Icon Set"),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_icon_set),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_icon_set)
					);
			for (vector<string>::const_iterator i = icon_sets.begin (); i != icon_sets.end (); ++i) {
				io->add (*i, *i);
			}
			add_option (_("Appearance"), io);
		}
	}

	add_option (_("Appearance"), new OptionEditorHeading (_("Graphical User Interface")));

	add_option (_("Appearance"),
	     new BoolOption (
		     "widget-prelight",
		     _("Highlight widgets on mouseover"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_widget_prelight),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_widget_prelight)
		     ));

	add_option (_("Appearance"),
	     new BoolOption (
		     "use-tooltips",
		     _("Show tooltips if mouse hovers over a control"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_tooltips),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_tooltips)
		     ));

	bo = new BoolOption (
			"super-rapid-clock-update",
			_("Update clocks at TC Frame rate"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_super_rapid_clock_update),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_super_rapid_clock_update)
			);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> clock displays are updated every Timecode Frame (fps).\n\n"
				"<b>When disabled</b> clock displays are updated only every 100ms."
			 ));
	add_option (_("Appearance"), bo);

	add_option (_("Appearance"),
			new BoolOption (
				"blink-rec-arm",
				_("Blink Rec-Arm buttons"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_blink_rec_arm),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_blink_rec_arm)
				));

	add_option (_("Appearance"),
			new BoolOption (
				"blink-alert-indicators",
				_("Blink Alert Indicators"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_blink_alert_indicators),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_blink_alert_indicators)
				));

	add_option (_("Appearance/Recorder"), new OptionEditorHeading (_("Input Meter Layout")));

	ComboOption<InputMeterLayout>* iml = new ComboOption<InputMeterLayout> (
		"input-meter-layout",
		_("Input Meter Layout"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_input_meter_layout),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_input_meter_layout)
		);

	iml->add (LayoutAutomatic,  _("Automatic"));
	iml->add (LayoutHorizontal, _("Horizontal"));
	iml->add (LayoutVertical,   _("Vertical"));

	add_option (S_("Appearance/Recorder"), iml);


	add_option (_("Appearance/Editor"), new OptionEditorHeading (_("General")));
	add_option (_("Appearance/Editor"),
	     new BoolOption (
		     "show-name-highlight",
		     _("Use name highlight bars in region displays (requires a restart)"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_name_highlight),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_name_highlight)
		     ));

	add_option (_("Appearance/Editor"),
			new BoolOption (
			"color-regions-using-track-color",
			_("Region color follows track color"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_color_regions_using_track_color),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_color_regions_using_track_color)
			));

	add_option (_("Appearance/Editor"),
			new BoolOption (
			"show-region-names",
			_("Show Region Names"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_region_name),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_region_name)
			));


	HSliderOption *gui_hs;

	if (!Profile->get_mixbus()) {
		gui_hs = new HSliderOption(
				"waveform-gradient-depth",
				_("Waveforms color gradient depth"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_waveform_gradient_depth),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_waveform_gradient_depth),
				0, 1.0, 0.05
				);
		gui_hs->scale().set_update_policy (Gtk::UPDATE_DELAYED);
		add_option (_("Appearance/Editor"), gui_hs);
	}

	gui_hs = new HSliderOption(
			"timeline-item-gradient-depth",
			_("Timeline item gradient depth"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_timeline_item_gradient_depth),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_timeline_item_gradient_depth),
			0, 1.0, 0.05
			);
	gui_hs->scale().set_update_policy (Gtk::UPDATE_DELAYED);
	add_option (_("Appearance/Editor"), gui_hs);

	ComboOption<int>* emode = new ComboOption<int> (
			"time-axis-name-ellipsize-mode",
			_("Track name ellipsize mode"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_time_axis_name_ellipsize_mode),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_time_axis_name_ellipsize_mode)
		);
	emode->add (-1, _("Ellipsize start of name"));
	emode->add (0, _("Ellipsize middle of name"));
	emode->add (1, _("Ellipsize end of name"));

	Gtkmm2ext::UI::instance()->set_tip (emode->tip_widget(), _("Choose which part of long track names are hidden in the editor's track headers"));
	add_option (_("Appearance/Editor"), emode);

	ComboOption<uint32_t>* gap = new ComboOption<uint32_t> (
		     "vertical-region-gap",
		     _("Add a visual gap below Audio Regions"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_vertical_region_gap),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_vertical_region_gap)
		     );
	gap->add (0, _("None"));
	gap->add (2, _("Small"));
	gap->add (4, _("Large"));
	add_option (_("Appearance/Editor"), gap);

	add_option (_("Appearance/Editor"), new OptionEditorHeading (_("Waveforms")));

	if (!Profile->get_mixbus()) {
		add_option (_("Appearance/Editor"),
				new BoolOption (
					"show-waveforms",
					_("Show waveforms in regions"),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_waveforms),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_waveforms)
					));
	}  // !mixbus

	add_option (_("Appearance/Editor"),
	     new BoolOption (
		     "show-waveforms-while-recording",
		     _("Show waveforms while recording"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_waveforms_while_recording),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_waveforms_while_recording)
		     ));

	add_option (_("Appearance/Editor"),
			new BoolOption (
			"show-waveform-clipping",
			_("Show waveform clipping"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_waveform_clipping),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_waveform_clipping)
			));

	add_option (_("Appearance/Editor"), new ClipLevelOptions ());

	ComboOption<WaveformScale>* wfs = new ComboOption<WaveformScale> (
		"waveform-scale",
		_("Waveform scale"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_waveform_scale),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_waveform_scale)
		);

	wfs->add (Linear, _("linear"));
	wfs->add (Logarithmic, _("logarithmic"));

	add_option (_("Appearance/Editor"), wfs);

	ComboOption<WaveformShape>* wfsh = new ComboOption<WaveformShape> (
		"waveform-shape",
		_("Waveform shape"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_waveform_shape),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_waveform_shape)
		);

	wfsh->add (Traditional, _("traditional"));
	wfsh->add (Rectified, _("rectified"));

	add_option (_("Appearance/Editor"), wfsh);

	add_option (_("Appearance/Editor"), new OptionEditorHeading (_("Editor Meters")));

	add_option (_("Appearance/Editor"),
	     new BoolOption (
		     "show-track-meters",
		     _("Show meters in track headers"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_track_meters),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_track_meters)
		     ));

	add_option (_("Appearance/Editor"),
	     new BoolOption (
		     "editor-stereo-only-meters",
		     _("Limit track header meters to stereo"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_editor_stereo_only_meters),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_editor_stereo_only_meters)
		     ));

	add_option (_("Appearance/Editor"), new OptionEditorHeading (_("MIDI Regions")));

	add_option (_("Appearance/Editor"),
		    new BoolOption (
			    "display-first-midi-bank-as-zero",
			    _("Display first MIDI bank/program as 0"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_first_midi_bank_is_zero),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_first_midi_bank_is_zero)
			    ));

	add_option (_("Appearance/Editor"),
	     new BoolOption (
		     "never-display-periodic-midi",
		     _("Don't display periodic (MTC, MMC) SysEx messages in MIDI Regions"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_never_display_periodic_midi),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_never_display_periodic_midi)
		     ));


	add_option (_("Appearance/Editor"),
	            new BoolOption (
		            "use-note-bars-for-velocity",
		            _("Show velocity horizontally inside notes"),
		            sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_note_bars_for_velocity),
		            sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_note_bars_for_velocity)
		            ));

	add_option (_("Appearance/Editor"),
	            new BoolOption (
		            "use-note-color-for-velocity",
		            _("Use colors to show note velocity"),
		            sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_note_color_for_velocity),
		            sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_note_color_for_velocity)
		            ));

	add_option (_("Appearance/Editor"), new OptionEditorBlank ());

	/* The names of these controls must be the same as those given in MixerStrip
	   for the actual widgets being controlled.
	*/
	_mixer_strip_visibility.add (0, X_("Input"), _("Input"));
	_mixer_strip_visibility.add (0, X_("PhaseInvert"), _("Phase Invert"));
	_mixer_strip_visibility.add (0, X_("RecMon"), _("Record & Monitor"));
	_mixer_strip_visibility.add (0, X_("SoloIsoLock"), _("Solo Iso / Lock"));
	_mixer_strip_visibility.add (0, X_("Output"), _("Output"));
	_mixer_strip_visibility.add (0, X_("Comments"), _("Comments"));
	_mixer_strip_visibility.add (0, X_("VCA"), _("VCA Assigns"));
	_mixer_strip_visibility.add (0, X_("TriggerGrid"), _("Trigger Grid"));
	_mixer_strip_visibility.add (0, X_("TriggerMaster"), _("Trigger Masters"));

#ifndef MIXBUS
	add_option (_("Appearance/Mixer"),
		new VisibilityOption (
			_("Mixer Strip"),
			&_mixer_strip_visibility,
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_mixer_strip_visibility),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_mixer_strip_visibility)
			)
		);
#else
	add_option (_("Appearance/Mixer"), new OptionEditorHeading (_("Mixer Strip")));
#endif

#ifndef MIXBUS32C
	add_option (_("Appearance/Mixer"),
	     new BoolOption (
		     "default-narrow_ms",
		     _("Use narrow strips in the mixer for new strips by default"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_default_narrow_ms),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_default_narrow_ms)
		     ));
#endif

	ComboOption<uint32_t>* mic = new ComboOption<uint32_t> (
		     "max-inline-controls",
		     _("Limit inline-mixer-strip controls per plugin"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_max_inline_controls),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_max_inline_controls)
		     );
	mic->add (0, _("Unlimited"));
	mic->add (16,  _("16 parameters"));
	mic->add (32,  _("32 parameters"));
	mic->add (64,  _("64 parameters"));
	mic->add (128, _("128 parameters"));
	add_option (_("Appearance/Mixer"), mic);

	add_option (_("Appearance/Mixer"), new OptionEditorBlank ());

	add_option (_("Appearance/Toolbar"), new OptionEditorHeading (_("Main Transport Toolbar Items")));

	add_option (_("Appearance/Toolbar"),
	     new BoolOption (
		     "show-toolbar-recpunch",
		     _("Display Record/Punch Options"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_toolbar_recpunch),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_toolbar_recpunch)
		     ));

	add_option (_("Appearance/Toolbar"),
	     new BoolOption (
		     "show-toolbar-latency",
		     _("Display Latency Compensation Info"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_toolbar_latency),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_toolbar_latency)
		     ));

	if (!ARDOUR::Profile->get_small_screen()) {
		add_option (_("Appearance/Toolbar"),
				new BoolOption (
					"show-secondary-clock",
					_("Display Secondary Clock"),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_secondary_clock),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_secondary_clock)
					));
	}

	add_option (_("Appearance/Toolbar"),
	     new BoolOption (
		     "show-toolbar-selclock",
		     _("Display Selection Clock"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_toolbar_selclock),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_toolbar_selclock)
		     ));

	add_option (_("Appearance/Toolbar"),
	     new BoolOption (
		     "show-toolbar-monitor-info",
		     _("Display Monitor Section Info"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_toolbar_monitor_info),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_toolbar_monitor_info)
		     ));

	add_option (_("Appearance/Toolbar"),
	     new BoolOption (
		     "show-mini-timeline",
		     _("Display Navigation Timeline"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_mini_timeline),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_mini_timeline)
		     ));

	add_option (_("Appearance/Toolbar"),
	     new BoolOption (
		     "show-editor-meter",
		     _("Display Master Level Meter"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_editor_meter),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_editor_meter)
		     ));

	add_option (_("Appearance/Toolbar"),
			new ColumVisibilityOption (
				"action-table-columns", _("Display Action-Buttons"), MAX_LUA_ACTION_BUTTONS / 2,
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_action_table_columns),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_action_table_columns)
				)
			);
	add_option (_("Appearance/Toolbar"), new OptionEditorBlank ());

	/* size and scale */

#if (!defined __APPLE__ || defined MIXBUS)
	add_option (_("Appearance/Size and Scale"), new OptionEditorHeading (_("User Interface Size and Scale")));
#endif

#ifndef __APPLE__
	/* font scaling does nothing with GDK/Quartz */
	add_option (_("Appearance/Size and Scale"), new FontScalingOptions ());
#endif

	add_option (_("Appearance/Colors"), new OptionEditorHeading (_("Colors")));
	add_option (_("Appearance/Colors"), new ColorThemeManager);
	add_option (_("Appearance/Colors"), new OptionEditorBlank ());

	/* Quirks */

	OptionEditorHeading* quirks_head = new OptionEditorHeading (_("Various Workarounds for Windowing Systems"));

	quirks_head->set_note (string_compose (_("Rules for closing, minimizing, maximizing, and stay-on-top can vary \
with each version of your OS, and the preferences that you've set in your OS.\n\n\
You can adjust the options, below, to change how application windows and dialogs behave.\n\n\
These settings will only take effect after %1 is restarted.\n\
	"), PROGRAM_NAME));

	add_option (_("Appearance/Quirks"), quirks_head);

	bo = new BoolOption (
		     "use-wm-visibility",
		     _("Use visibility information provided by your Window Manager/Desktop"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_wm_visibility),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_wm_visibility)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget (),
				_("If you have trouble toggling between hidden Editor and Mixer windows, try changing this setting."));
	add_option (_("Appearance/Quirks"), bo);

#ifndef __APPLE__

#ifndef PLATFORM_WINDOWS
	bo = new BoolOption (
			"hide-splash-screen",
			_("Show/Hide splash screen instead of setting z-axis stack order"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_hide_splash_screen),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_hide_splash_screen)
			);
	add_option (_("Appearance/Quirks"), bo);
#endif

	bo = new BoolOption (
			"all-floating-windows-are-dialogs",
			_("All floating windows are dialogs"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_all_floating_windows_are_dialogs),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_all_floating_windows_are_dialogs)
			);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget (),
			_("Mark all floating windows to be type \"Dialog\" rather than using \"Utility\" for some.\nThis may help with some window managers."));
	add_option (_("Appearance/Quirks"), bo);

	bo = new BoolOption (
			"transients-follow-front",
			_("Transient windows follow front window."),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_transients_follow_front),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_transients_follow_front)
			);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget (),
				_("Make transient windows follow the front window when toggling between the editor and mixer."));
	add_option (_("Appearance/Quirks"), bo);
#endif

	if (!Profile->get_mixbus()) {
		bo = new BoolOption (
				"floating-monitor-section",
				_("Float detached monitor-section window"),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_floating_monitor_section),
				sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_floating_monitor_section)
				);
		Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget (),
					_("When detaching the monitoring section, mark it as \"Utility\" window to stay in front."));
		add_option (_("Appearance/Quirks"), bo);
	}

	add_option (_("Appearance/Quirks"), new OptionEditorBlank ());
#if (!defined USE_CAIRO_IMAGE_SURFACE || defined CAIRO_SUPPORTS_FORCE_BUGGY_GRADIENTS_ENVIRONMENT_VARIABLE)
	add_option (_("Appearance"), new OptionEditorHeading (_("Graphics Acceleration")));
#endif

#ifndef USE_CAIRO_IMAGE_SURFACE
	BoolOption* bgc = new BoolOption (
		"cairo-image-surface",
		_("Disable Graphics Hardware Acceleration (requires restart)"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_cairo_image_surface),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_cairo_image_surface)
		);

	Gtkmm2ext::UI::instance()->set_tip (bgc->tip_widget(), string_compose (
				_("Render large parts of the application user-interface in software, instead of using 2D-graphics acceleration.\nThis requires restarting %1 before having an effect"), PROGRAM_NAME));
	add_option (_("Appearance"), bgc);
#endif

#ifdef CAIRO_SUPPORTS_FORCE_BUGGY_GRADIENTS_ENVIRONMENT_VARIABLE
	BoolOption* bgo = new BoolOption (
		"buggy-gradients",
		_("Possibly improve slow graphical performance (requires restart)"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_buggy_gradients),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_buggy_gradients)
		);

	Gtkmm2ext::UI::instance()->set_tip (bgo->tip_widget(), string_compose (_("Disables hardware gradient rendering on buggy video drivers (\"buggy gradients patch\").\nThis requires restarting %1 before having an effect"), PROGRAM_NAME));
	add_option (_("Appearance"), bgo);
#endif

#if ENABLE_NLS

	add_option (_("Appearance/Translation"), new OptionEditorHeading (_("Internationalization")));

	bo = new BoolOption (
			"enable-translation",
			_("Use translations"),
			sigc::ptr_fun (ARDOUR::translations_are_enabled),
			sigc::ptr_fun (ARDOUR::set_translations_enabled)
			);

	bo->set_note (string_compose (_("These settings will only take effect after %1 is restarted (if available for your language preferences)."), PROGRAM_NAME));

	add_option (_("Appearance/Translation"), bo);

	parameter_changed ("enable-translation");
#endif // ENABLE_NLS


	/* EDITOR *******************************************************************/

	add_option (_("Editor"), new OptionEditorHeading (_("Region Information")));

	add_option (_("Editor"),
			new BoolOption (
		     "show-region-xrun-markers",
		     _("Show xrun markers in regions"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_region_xrun_markers),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_region_xrun_markers)
		     ));

	add_option (_("Editor"),
			new BoolOption (
		     "show-region-cue-markers",
		     _("Show cue markers in regions"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_region_cue_markers),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_region_cue_markers)
		     ));

	add_option (_("Editor"),
	     new BoolComboOption (
		     "show-region-gain-envelopes",
		     _("Show gain envelopes in audio regions"),
		     _("in all modes"),
		     _("only in Draw and Internal Edit modes"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_region_gain),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_region_gain)
		     ));

	add_option (_("Editor"), new OptionEditorHeading (_("Scroll and Zoom Behaviors")));

	if (!Profile->get_mixbus()) {

		add_option (_("Editor"),
				new BoolOption (
					"use-mouse-position-as-zoom-focus-on-scroll",
					_("Zoom to mouse position when zooming with scroll wheel"),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_mouse_position_as_zoom_focus_on_scroll),
					sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_mouse_position_as_zoom_focus_on_scroll)
					));
	}  // !mixbus

	add_option (_("Editor"),
		    new BoolOption (
			    "use-time-rulers-to-zoom-with-vertical-drag",
			    _("Zoom with vertical drag in rulers"),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_time_rulers_to_zoom_with_vertical_drag),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_time_rulers_to_zoom_with_vertical_drag)
			    ));

	add_option (_("Editor"),
		    new BoolOption (
			    "use-double-click-to-zoom-to-selection",
			    _("Double click zooms to selection"),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_use_double_click_to_zoom_to_selection),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_use_double_click_to_zoom_to_selection)
			    ));

	add_option (_("Editor"),
		    new BoolOption (
			    "update-editor-during-summary-drag",
			    _("Update editor window during drags of the summary"),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_update_editor_during_summary_drag),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_update_editor_during_summary_drag)
			    ));

	add_option (_("Editor"),
	    new BoolOption (
		    "autoscroll-editor",
		    _("Auto-scroll editor window when dragging near its edges"),
		    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_autoscroll_editor),
		    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_autoscroll_editor)
		    ));

	ComboOption<float>* dps = new ComboOption<float> (
		     "draggable-playhead-speed",
		     _("Auto-scroll speed when dragging playhead"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_draggable_playhead_speed),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_draggable_playhead_speed)
		     );
	dps->add (0.05, _("5%"));
	dps->add (0.1, _("10%"));
	dps->add (0.25, _("25%"));
	dps->add (0.5, _("50%"));
	dps->add (1.0, _("100%"));
	add_option (_("Editor"), dps);

	// XXX Long label, pushes other ComboBoxes to the right
	ComboOption<float>* eet = new ComboOption<float> (
		     "extra-ui-extents-time",
		     _("Limit zoom & summary view beyond session extents to"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_extra_ui_extents_time),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_extra_ui_extents_time)
		     );
	eet->add (1, _("1 minute"));
	eet->add (2, _("2 minutes"));
	eet->add (20, _("20 minutes"));
	eet->add (60, _("1 hour"));
	eet->add (60*2, _("2 hours"));
	eet->add (60*24, _("24 hours"));
	add_option (_("Editor"), eet);

	add_option (_("Editor"), new OptionEditorHeading (_("Editor Behavior")));

	add_option (_("Editor"),
	     new BoolOption (
		     "automation-follows-regions",
		     _("Move relevant automation when audio regions are moved"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_automation_follows_regions),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_automation_follows_regions)
		     ));

	bo = new BoolOption (
		     "new-automation-points-on-lane",
		     _("Ignore Y-axis when adding new automation-points"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_new_automation_points_on_lane),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_new_automation_points_on_lane)
		     );
	add_option (_("Editor"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> new points drawn in any automation lane will be placed on the existing line, regardless of mouse y-axis position."));

	bo = new BoolOption (
		     "automation-edit-cancels-auto-hide",
		     _("Automation edit cancels auto hide"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_automation_edit_cancels_auto_hide),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_automation_edit_cancels_auto_hide)
		     );
	add_option (_("Editor"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> automatically displayed automation lanes remain visible if events are added to the lane.\n"
			  "<b>When disabled</b>, spilled automation lanes are unconditionally hidden when a different control is touched.\n"
			  "This setting only has effect if 'Show Automation Lane on Touch' is used.")
			);

	ComboOption<FadeShape>* fadeshape = new ComboOption<FadeShape> (
			"default-fade-shape",
			_("Default fade shape"),
			sigc::mem_fun (*_rc_config,
				&RCConfiguration::get_default_fade_shape),
			sigc::mem_fun (*_rc_config,
				&RCConfiguration::set_default_fade_shape)
			);

	fadeshape->add (FadeLinear,
			_("Linear (for highly correlated material)"));
	fadeshape->add (FadeConstantPower, _("Constant power"));
	fadeshape->add (FadeSymmetric, _("Symmetric"));
	fadeshape->add (FadeSlow, _("Slow"));
	fadeshape->add (FadeFast, _("Fast"));

	add_option (_("Editor"), fadeshape);

#if 1 // XXX wide ComboBox
	ComboOption<RegionEquivalence> *eqv = new ComboOption<RegionEquivalence> (
		     "region-equivalency",
		     _("Regions in edit groups are edited together"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_region_equivalence),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_region_equivalence)
		     );

	eqv->add (Overlap,   _("whenever they overlap in time"));
	eqv->add (Enclosed,  _("if either encloses the other"));
	eqv->add (Exact,     _("if they have identical length, position and origin"));
	eqv->add (LayerTime, _("if they have identical length, position and layer"));

	add_option (_("Editor"), eqv);
#endif

	ComboOption<LayerModel>* lm = new ComboOption<LayerModel> (
		"layer-model",
		_("Layering model"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_layer_model),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_layer_model)
		);

	lm->add (LaterHigher, _("later is higher"));
	lm->add (Manual, _("manual layering"));
	add_option (_("Editor"), lm);

	bo = new BoolOption (
		"interview-editing",
		_("Improve editing behavior for editing multi-track voice interviews"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_interview_editing),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_interview_editing)
		);
	add_option (_("Editor"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
	                                    _("<b>When enabled</b>, range selection while in Ripple All will not propagate across all tracks.\n"
	                                      "<b>When disabled</b>, range selection while in Ripple All will propagate across all tracks.\n"
	                                      "This setting only has effect when in Ripple All mode.")
		);


	add_option (_("Editor"), new OptionEditorHeading (_("Split/Separate")));

	ComboOption<RangeSelectionAfterSplit> *rras = new ComboOption<RangeSelectionAfterSplit> (
		    "range-selection-after-separate",
		    _("After a Separate operation, in Range mode"),
		    sigc::mem_fun (*_rc_config, &RCConfiguration::get_range_selection_after_split),
		    sigc::mem_fun (*_rc_config, &RCConfiguration::set_range_selection_after_split));

	rras->add(ClearSel,    _("Clear the Range Selection"));
	rras->add(PreserveSel, _("Preserve the Range Selection"));
	rras->add(ForceSel,    _("Select the regions under the range."));
	add_option (_("Editor"), rras);

#if 1 // XXX very wide ComboBox
	ComboOption<RegionSelectionAfterSplit> *rsas = new ComboOption<RegionSelectionAfterSplit> (
		    "region-selection-after-split",
		    _("After a Split operation, in Object mode"),
		    sigc::mem_fun (*_rc_config, &RCConfiguration::get_region_selection_after_split),
		    sigc::mem_fun (*_rc_config, &RCConfiguration::set_region_selection_after_split));

	// TODO: decide which of these modes are really useful
	rsas->add (None,                     _("Clear the Region Selection"));
	rsas->add (NewlyCreatedLeft,         _("Select the newly-created regions BEFORE the split point"));
	rsas->add (NewlyCreatedRight,        _("Select only the newly-created regions AFTER the split point"));
	rsas->add (NewlyCreatedBoth,         _("Select the newly-created regions"));
#if 0
	rsas->add(Existing,                  _("unmodified regions in the existing selection"));
	rsas->add(ExistingNewlyCreatedLeft,  _("existing selection and newly-created regions before the split"));
	rsas->add(ExistingNewlyCreatedRight, _("existing selection and newly-created regions after the split"));
#endif
	rsas->add(ExistingNewlyCreatedBoth,  _("Preserve existing selection, and select newly-created regions"));

	add_option (_("Editor"), rsas);
#endif

	add_option (_("Editor/Snap"), new OptionEditorHeading (_("General Snap options:")));

	add_option (_("Editor/Snap"),
		    new SpinOption<uint32_t> (
			    "snap-threshold",
			    _("Snap Threshold (pixels)"),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_snap_threshold),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_snap_threshold),
			    10, 200,
			    1, 10
			    ));

	add_option (_("Editor/Snap"),
		    new SpinOption<uint32_t> (
			    "ruler-granularity",
			    _("Approximate Grid/Ruler granularity (pixels)"),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_ruler_granularity),
			    sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_ruler_granularity),
			    1, 100,
			    1, 10
			    ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "show-snapped-cursor",
		     _("Show \"snapped cursor\""),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_snapped_cursor),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_snapped_cursor)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "rubberbanding-snaps-to-grid",
		     _("Snap rubberband selection to grid"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_rubberbanding_snaps_to_grid),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_rubberbanding_snaps_to_grid)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "grid-follows-internal",
		     _("Grid switches to alternate selection for Internal Edit tools"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_grid_follows_internal),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_grid_follows_internal)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "rulers-follow-grid",
		     _("Rulers automatically change to follow the Grid mode selection"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_rulers_follow_grid),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_rulers_follow_grid)
		     ));

	add_option (_("Editor/Snap"), new OptionEditorHeading (_("When \"Snap\" is enabled, snap to:")));


	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "snap-to-marks",
		     _("Markers"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_snap_to_marks),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_snap_to_marks)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "snap-to-region-sync",
		     _("Region Sync Points"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_snap_to_region_sync),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_snap_to_region_sync)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "snap-to-region-start",
		     _("Region Starts"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_snap_to_region_start),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_snap_to_region_start)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "snap-to-region-end",
		     _("Region Ends"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_snap_to_region_end),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_snap_to_region_end)
		     ));

	add_option (_("Editor/Snap"),
	     new BoolOption (
		     "snap-to-grid",
		     _("Grid"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_snap_to_grid),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_snap_to_grid)
		     ));

	add_option (_("Editor/Modifiers"), new OptionEditorHeading (_("Keyboard Modifiers")));
	add_option (_("Editor/Modifiers"), new KeyboardOptions);
	add_option (_("Editor/Modifiers"), new OptionEditorBlank ());


	/* MIDI *********************************************************************/

	add_option (_("MIDI"), new OptionEditorHeading (_("Session")));

	bo = new BoolOption (
		"allow-non-quarter-pulse",
		_("Allow non quarter-note pulse"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_allow_non_quarter_pulse),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_allow_non_quarter_pulse)
		);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    string_compose (_("<b>When enabled</b> %1 will allow tempo to be expressed in divisions per minute\n"
							      "<b>When disabled</b> %1 will only allow tempo to be expressed in quarter notes per minute"),
							    PROGRAM_NAME));
	add_option (_("MIDI"), bo);

	add_option (_("MIDI"),
	     new SpinOption<int32_t> (
		     "initial-program-change",
		     _("Initial program change"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_initial_program_change),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_initial_program_change),
		     -1, 65536, 1, 10
		     ));

	add_option (_("MIDI"), new OptionEditorHeading (_("Audition")));

	add_option (_("MIDI"),
	     new BoolOption (
		     "sound-midi-notes",
		     _("Sound MIDI notes as they are selected in the editor"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_sound_midi_notes),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_sound_midi_notes)
		     ));

	add_option (_("MIDI"), new OptionEditorHeading (_("Virtual Keyboard")));

	ComboOption<std::string>* vkeybdlayout = new ComboOption<std::string> (
		"vkeybd-layout",
		_("Virtual Keyboard Layout"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_vkeybd_layout),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_vkeybd_layout)
		);

	vkeybdlayout->add ("None",   _("Mouse-only (no keyboard)"));
	vkeybdlayout->add ("QWERTY", _("QWERTY"));
	vkeybdlayout->add ("QWERTZ", _("QWERTZ"));
	vkeybdlayout->add ("AZERTY", _("AZERTY"));
	vkeybdlayout->add ("DVORAK", _("DVORAK"));
	vkeybdlayout->add ("QWERTY Single", _("QWERTY Single"));
	vkeybdlayout->add ("QWERTZ Single", _("QWERTZ Single"));

	add_option (_("MIDI"), vkeybdlayout);

	/* MIDI PORTs */
	add_option (_("MIDI"), new OptionEditorHeading (_("MIDI Port Options")));

	add_option (_("MIDI"),
		    new BoolOption (
			    "midi-input-follows-selection",
			    _("MIDI input follows MIDI track selection"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_midi_input_follows_selection),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_midi_input_follows_selection)
			    ));

	add_option (_("MIDI/MIDI Port Config"), new OptionEditorBlank ());
	add_option (_("MIDI/MIDI Port Config"), new MidiPortOptions ());

	add_option (_("MIDI"), new OptionEditorBlank ());

	/* TRANSPORT & SYNC *********************************************************/

	add_option (_("Transport"), new OptionEditorHeading (_("General")));

	bo = new BoolOption (
		     "name-new-markers",
		     _("Prompt for new marker names"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_name_new_markers),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_name_new_markers)
		);
	add_option (_("Transport"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(), _("If enabled, popup a dialog when a new marker is created to allow its name to be set as it is created."
								"\n\nYou can always rename markers by right-clicking on them."));

	bo = new BoolOption (
		     "stop-at-session-end",
		     _("Stop at the end of the session"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_stop_at_session_end),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_stop_at_session_end)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    string_compose (_("<b>When enabled</b> if %1 is <b>not recording</b>, it will stop the transport "
							      "when it reaches the current session end marker\n\n"
							      "<b>When disabled</b> %1 will continue to roll past the session end marker at all times"),
							    PROGRAM_NAME));
	add_option (_("Transport"), bo);


	bo = new BoolOption (
		     "latched-record-enable",
		     _("Keep record-enable engaged on stop"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_latched_record_enable),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_latched_record_enable)
		     );
	add_option (_("Transport"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> master record will remain engaged when the transport transitions to stop.\n<b>When disabled</b> master record will be disabled when the transport transitions to stop."));

	bo = new BoolOption (
		     "reset-default-speed-on-stop",
		     _("Reset default speed on stop"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_reset_default_speed_on_stop),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_reset_default_speed_on_stop)
		     );
	add_option (_("Transport"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b>, stopping the transport will reset the default speed to normal.\n<b>When disabled</b> any current default speed will remain in effect across transport stops."));

	bo = new BoolOption (
		     "disable-disarm-during-roll",
		     _("Disable per-track record disarm while rolling"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_disable_disarm_during_roll),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_disable_disarm_during_roll)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(), _("<b>When enabled</b> this will prevent you from accidentally stopping specific tracks recording during a take."));
	add_option (_("Transport"), bo);

	bo = new BoolOption (
		     "quieten_at_speed",
		     _("12dB gain reduction during fast-forward and fast-rewind"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_quieten_at_speed),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_quieten_at_speed)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> this will reduce the unpleasant increase in perceived volume "
				"that occurs when fast-forwarding or rewinding through some kinds of audio"));
	add_option (_("Transport"), bo);


	bo = new BoolOption (
		     "rewind-ffwd-like-tape-decks",
		     _("Rewind/Fast-forward buttons change direction immediately"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_rewind_ffwd_like_tape_decks),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_rewind_ffwd_like_tape_decks)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
	                                    _("<b>When enabled</b> rewind/ffwd controls will immediately change playback direction when appropriate.\n\n"
	                                      "<b>When disabled</b> rewind/ffwd controls will gradually speed up/slow down playback"));
	add_option (_("Transport"), bo);


	bo = new BoolOption (
		"auto-return-after-rewind-ffwd",
		_("Allow auto-return after rewind/ffwd operations"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_auto_return_after_rewind_ffwd),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_auto_return_after_rewind_ffwd)
		);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
	                                    _("<b>When enabled</b> if auto-return is enabled, the playhead will auto-return after rewind/ffwd operations\n\n"
	                                      "<b>When disabled</b> the playhead will never auto-return after rewind/ffwd operations")
		);
	add_option (_("Transport"), bo);


	ComboOption<float>* psc = new ComboOption<float> (
		     "preroll-seconds",
		     _("Preroll"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_preroll_seconds),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_preroll_seconds)
		     );
	Gtkmm2ext::UI::instance()->set_tip (psc->tip_widget(),
					    (_("The amount of preroll to apply when <b>Play with Preroll</b> or <b>Record with Preroll</b>is initiated.\n\n"
					       "If <b>Follow Edits</b> is enabled, the preroll is applied to the playhead position when a region is selected or trimmed.")));
	psc->add (-4.0, _("4 Bars"));
	psc->add (-2.0, _("2 Bars"));
	psc->add (-1.0, _("1 Bar"));
	psc->add (0.0, _("0 (no pre-roll)"));
	psc->add (0.1, _("0.1 second"));
	psc->add (0.25, _("0.25 second"));
	psc->add (0.5, _("0.5 second"));
	psc->add (1.0, _("1.0 second"));
	psc->add (2.0, _("2.0 seconds"));
	add_option (_("Transport"), psc);


	add_option (_("Transport"), new OptionEditorHeading (_("Looping")));

	bo = new BoolOption (
		     "loop-is-mode",
		     _("Play loop is a transport mode"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_loop_is_mode),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_loop_is_mode)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    (_("<b>When enabled</b> the loop button does not start playback but forces playback to always play the loop\n\n"
					       "<b>When disabled</b> the loop button starts playing the loop, but stop then cancels loop playback")));


	add_option (_("Transport"), bo);


	ComboOption<LoopFadeChoice>* lca = new ComboOption<LoopFadeChoice> (
		     "loop-fade-choice",
		     _("Loop Fades"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_loop_fade_choice),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_loop_fade_choice)
		     );
	lca->add (NoLoopFade, _("No fades at loop boundaries"));
	lca->add (EndLoopFade, _("Fade out at loop end"));
	lca->add (BothLoopFade, _("Fade in at loop start & Fade out at loop end"));
	lca->add (XFadeLoop, _("Cross-fade loop end and start"));
	add_option (_("Transport"), lca);
	Gtkmm2ext::UI::instance()->set_tip (lca->tip_widget(), _("Options for fades/crossfades at loop boundaries"));

	add_option (_("Transport"), new OptionEditorHeading (_("Dropout (xrun) Handling")));
	bo = new BoolOption (
		     "stop-recording-on-xrun",
		     _("Stop recording when an xrun occurs"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_stop_recording_on_xrun),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_stop_recording_on_xrun)
		     );
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    string_compose (_("<b>When enabled</b> %1 will stop recording if an over- or underrun is detected by the audio engine"),
							    PROGRAM_NAME));
	add_option (_("Transport"), bo);

	bo = new BoolOption (
		     "create-xrun-marker",
		     _("Create markers where xruns occur"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_create_xrun_marker),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_create_xrun_marker)
		     );
	add_option (_("Transport"), bo);

	bo = new BoolOption (
		     "recording-resets-xrun-count",
		     _("Reset xrun counter when starting to record"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_recording_resets_xrun_count),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_recording_resets_xrun_count)
		     );
	add_option (_("Transport"), bo);

	add_option (_("Transport/Chase"), new OptionEditorHeading (_("MIDI Machine Control (MMC)")));

	add_option (_("Transport/Chase"),
		    new BoolOption (
			    "mmc-control",
			    _("Respond to MMC commands"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_mmc_control),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_mmc_control)
			    ));

	add_option (_("Transport/Chase"),
	     new SpinOption<uint8_t> (
		     "mmc-receive-device-id",
		     _("Inbound MMC device ID"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_mmc_receive_device_id),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_mmc_receive_device_id),
		     0, 127, 1, 10
		     ));

	add_option (_("Transport/Chase"), new OptionEditorHeading (_("Transport Masters")));

	add_option (_("Transport/Chase"),
			new RcActionButton (_("Show Transport Masters Window"),
				sigc::mem_fun (*this, &RCOptionEditor::show_transport_masters)));

	_sync_framerate = new BoolOption (
		     "timecode-sync-frame-rate",
		     _("Match session video frame rate to external timecode"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_timecode_sync_frame_rate),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_timecode_sync_frame_rate)
		     );
	Gtkmm2ext::UI::instance()->set_tip
		(_sync_framerate->tip_widget(),
		 string_compose (_("This option controls the value of the video frame rate <i>while chasing</i> an external timecode source.\n\n"
				   "<b>When enabled</b> the session video frame rate will be changed to match that of the selected external timecode source.\n\n"
				   "<b>When disabled</b> the session video frame rate will not be changed to match that of the selected external timecode source."
				   "Instead the frame rate indication in the main clock will flash red and %1 will convert between the external "
				   "timecode standard and the session standard."), PROGRAM_NAME));
	add_option (_("Transport/Chase"), _sync_framerate);

	add_option (_("Transport/Generate"), new OptionEditorHeading (_("Linear Timecode (LTC) Generator")));

	add_option (_("Transport/Generate"),
		    new BoolOption (
			    "send-ltc",
			    _("Enable LTC generator"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_send_ltc),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_send_ltc)
			    ));

	_ltc_send_continuously = new BoolOption (
			    "ltc-send-continuously",
			    _("Send LTC while stopped"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_ltc_send_continuously),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_ltc_send_continuously)
			    );
	Gtkmm2ext::UI::instance()->set_tip
		(_ltc_send_continuously->tip_widget(),
		 string_compose (_("<b>When enabled</b> %1 will continue to send LTC information even when the transport (playhead) is not moving"), PROGRAM_NAME));
	add_option (_("Transport/Generate"), _ltc_send_continuously);

	_ltc_volume_slider = new HSliderOption("ltcvol", _("LTC generator level [dBFS]"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_ltc_output_volume),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_ltc_output_volume),
					-50, 0, .5, 5,
					.05, true);

	Gtkmm2ext::UI::instance()->set_tip
		(_ltc_volume_slider->tip_widget(),
		 _("Specify the Peak Volume of the generated LTC signal in dBFS. A good value is  0dBu ^= -18dBFS in an EBU calibrated system"));

	add_option (_("Transport/Generate"), _ltc_volume_slider);

	add_option (_("Transport/Generate"), new LTCPortSelectOption (_rc_config, this));

	add_option (_("Transport/Generate"), new OptionEditorHeading (_("MIDI Time Code (MTC) Generator")));

	add_option (_("Transport/Generate"),
		    new BoolOption (
			    "send-mtc",
			    _("Enable MTC Generator"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_send_mtc),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_send_mtc)
			    ));


	SpinOption<int>* soi = new SpinOption<int> (
			    "mtc-qf-speed-tolerance",
			    _("Max MTC varispeed (%)"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_mtc_qf_speed_tolerance),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_mtc_qf_speed_tolerance),
			    0, 20, 1, 5
			    );
	Gtkmm2ext::UI::instance()->set_tip (soi->tip_widget(), _("Percentage either side of normal transport speed to transmit MTC."));
	add_option (_("Transport/Generate"), soi);

	add_option (_("Transport/Generate"), new OptionEditorHeading (_("MIDI Machine Control (MMC)")));

	add_option (_("Transport/Generate"),
		    new BoolOption (
			    "send-mmc",
			    _("Send MMC commands"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_send_mmc),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_send_mmc)
			    ));

	add_option (_("Transport/Generate"),
	     new SpinOption<uint8_t> (
		     "mmc-send-device-id",
		     _("Outbound MMC device ID"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_mmc_send_device_id),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_mmc_send_device_id),
		     0, 127, 1, 10
		     ));

	add_option (_("Transport/Generate"), new OptionEditorHeading (_("MIDI Beat Clock (Mclk) Generator")));

	add_option (_("Transport/Generate"),
		    new BoolOption (
			    "send-midi-clock",
			    _("Enable Mclk generator"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_send_midi_clock),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_send_midi_clock)
			    ));

	add_option (_("Transport"), new OptionEditorHeading (_("Plugins")));
	bo = new BoolOption (
		"plugins-stop-with-transport",
		_("Silence plugins when the transport is stopped"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_plugins_stop_with_transport),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_plugins_stop_with_transport)
		);
	add_option (_("Transport"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    _("<b>When enabled</b> plugins will be reset at transport stop. When disabled plugins will be left unchanged at transport stop.\n\nThis mostly affects plugins with a \"tail\" like Reverbs."));

	/* PLUGINS ******************************************************************/

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT || defined AUDIOUNIT_SUPPORT || defined VST3_SUPPORT)
	add_option (_("Plugins"), new OptionEditorHeading (_("Scan/Discover")));
	add_option (_("Plugins"),
			new RcActionButton (_("Scan for Plugins"),
				sigc::mem_fun (*this, &RCOptionEditor::plugin_scan_refresh)));

	add_option (_("Plugins"), new PluginScanTimeOutSliderOption (_rc_config));
#endif

	add_option (_("Plugins"), new OptionEditorHeading (_("General")));

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT || defined AUDIOUNIT_SUPPORT || defined VST3_SUPPORT)

	bo = new BoolOption (
			"discover-plugins-on-start",
			_("Scan for [new] Plugins on Application Start"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_discover_plugins_on_start),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_discover_plugins_on_start)
			);
	add_option (_("Plugins"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    _("<b>When enabled</b> new plugins are searched, tested and added to the cache index on application start. When disabled new plugins will only be available after triggering a 'Scan' manually"));

	bo = new BoolOption (
			"show-plugin-scan-window",
			_("Always Display Plugin Scan Progress"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_plugin_scan_window),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_plugin_scan_window)
			);
	add_option (_("Plugins"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> a popup window showing plugin scan progress is displayed for indexing (cache load) and discovery (detect new plugins)"));

	bo = new BoolOption (
			"verbose-plugin-scan",
			_("Verbose Plugin Scan"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_verbose_plugin_scan),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_verbose_plugin_scan)
			);
	add_option (_("Plugins"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    _("<b>When enabled</b> additional information for every plugin is shown to the Plugin Manager Log."));
#endif

	bo = new BoolOption (
		"new-plugins-active",
			_("Make new plugins active"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_new_plugins_active),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_new_plugins_active)
			);
	add_option (_("Plugins"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    _("<b>When enabled</b> plugins will be activated when they are added to tracks/busses. When disabled plugins will be left inactive when they are added to tracks/busses"));

	bo = new BoolOption (
		"one-plugin-window-only",
		_("Show only one plugin window at a time"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_one_plugin_window_only),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_one_plugin_window_only)
		);
	add_option (_("Plugins"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
					    _("<b>When enabled</b> at most one plugin GUI window be on-screen at a time. When disabled, the number of visible plugin GUI windows is unlimited"));

#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT || defined VST3_SUPPORT)
	add_option (_("Plugins/VST"), new OptionEditorHeading (_("VST")));
#if 0
	add_option (_("Plugins/VST"),
			new RcActionButton (_("Scan for Plugins"),
				sigc::mem_fun (*this, &RCOptionEditor::plugin_scan_refresh)));
#endif

#if (defined AUDIOUNIT_SUPPORT && defined MACVST_SUPPORT)
	bo = new BoolOption (
			"use-macvst",
			_("Enable Mac VST2 support (requires restart or re-scan)"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_use_macvst),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_use_macvst)
			);
	add_option (_("Plugins/VST"), bo);
#endif

#ifndef MIXBUS

#ifdef WINDOWS_VST_SUPPORT
	bo = new BoolOption (
			"use-windows-vst",
			_("Enable Windows VST2 support (requires restart or re-scan)"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_use_windows_vst),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_use_windows_vst)
			);
	add_option (_("Plugins/VST"), bo);
#endif

#ifdef LXVST_SUPPORT
	bo = new BoolOption (
			"use-lxvst",
			_("Enable Linux VST2 support (requires restart or re-scan)"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_use_lxvst),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_use_lxvst)
			);
	add_option (_("Plugins/VST"), bo);
#endif

#ifdef VST3_SUPPORT
	bo = new BoolOption (
			"use-vst3",
			_("Enable VST3 support (requires restart or re-scan)"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_use_vst3),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_use_vst3)
			);
	add_option (_("Plugins/VST"), bo);
#endif

#endif // !Mixbus

#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
	add_option (_("Plugins/VST"), new OptionEditorHeading (_("VST 2.x")));

	add_option (_("Plugins/VST"),
			new RcActionButton (_("Clear"),
				sigc::mem_fun (*this, &RCOptionEditor::clear_vst2_cache),
				_("VST 2 Cache:")));

	add_option (_("Plugins/VST"),
			new RcActionButton (_("Clear"),
				sigc::mem_fun (*this, &RCOptionEditor::clear_vst2_blacklist),
				_("VST 2 Ignorelist:")));
#endif

#ifdef LXVST_SUPPORT
	add_option (_("Plugins/VST"),
			new RcActionButton (_("Edit"),
				sigc::bind (sigc::mem_fun (*this, &RCOptionEditor::edit_vst_path),
					_("Set Linux VST2 Search Path"),
					PluginManager::instance().get_default_lxvst_path (),
					sigc::mem_fun (*_rc_config, &RCConfiguration::get_plugin_path_lxvst),
					sigc::mem_fun (*_rc_config, &RCConfiguration::set_plugin_path_lxvst)
					),
				_("Linux VST2 Path:")));

	add_option (_("Plugins/VST"),
			new RcConfigDisplay (
				"plugin-path-lxvst",
				_("Path:"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_plugin_path_lxvst),
				0));
#endif

#ifdef WINDOWS_VST_SUPPORT
	add_option (_("Plugins/VST"),
			new RcActionButton (_("Edit"),
				sigc::bind (sigc::mem_fun (*this, &RCOptionEditor::edit_vst_path),
					_("Set Windows VST2 Search Path"),
					PluginManager::instance().get_default_windows_vst_path (),
					sigc::mem_fun (*_rc_config, &RCConfiguration::get_plugin_path_vst),
					sigc::mem_fun (*_rc_config, &RCConfiguration::set_plugin_path_vst)
					),
				_("Windows VST2 Path:")));

	add_option (_("Plugins/VST"),
			new RcConfigDisplay (
				"plugin-path-vst",
				_("Path:"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_plugin_path_vst),
				';'));
#endif

#ifdef VST3_SUPPORT
	add_option (_("Plugins/VST"), new OptionEditorHeading (_("VST 3")));
	add_option (_("Plugins/VST"),
			new RcActionButton (_("Clear"),
				sigc::mem_fun (*this, &RCOptionEditor::clear_vst3_cache),
				_("VST 3 Cache:")));

	add_option (_("Plugins/VST"),
			new RcActionButton (_("Clear"),
				sigc::mem_fun (*this, &RCOptionEditor::clear_vst3_blacklist),
				_("VST 3 Ignorelist:")));

	RcActionButton* vst3_path =
			new RcActionButton (_("Edit"),
				sigc::bind (sigc::mem_fun (*this, &RCOptionEditor::edit_vst_path),
					_("Set Additional VST3 Search Path"),
					"", /* default is blank */
					sigc::mem_fun (*_rc_config, &RCConfiguration::get_plugin_path_vst3),
					sigc::mem_fun (*_rc_config, &RCConfiguration::set_plugin_path_vst3)
					),
				_("Additional VST3 Path:"));

	vst3_path->set_note (_("Customizing VST3 paths is discouraged. Note that default VST3 paths as per "
	                       "<a href=\"https://developer.steinberg.help/display/VST/Plug-in+Locations\">specification</a>"
	                       "are always searched, and need not be explicitly set."));
	add_option (_("Plugins/VST"), vst3_path);

	// -> Appearance/Mixer ?
	add_option (_("Plugins/VST"),
	     new BoolOption (
		     "show-vst3-micro-edit-inline",
		     _("Automatically show 'Micro Edit' tagged controls on the mixer-strip"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_show_vst3_micro_edit_inline),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_show_vst3_micro_edit_inline)
		     ));

#if (defined WINDOWS_VST_SUPPORT || defined MACVST_SUPPORT || defined LXVST_SUPPORT)
	add_option (_("Plugins/VST"), new OptionEditorHeading (_("VST2/VST3")));
	add_option (_("Plugins/VST"),
	     new BoolOption (
		     "conceal-vst2-if-vst3-exists",
		     _("Conceal VST2 Plugin if matching VST3 exists"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_conceal_vst2_if_vst3_exists),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_conceal_vst2_if_vst3_exists)
		     ));
#endif
#endif // VST3
#endif // Any VST (2 or 3)

#ifdef AUDIOUNIT_SUPPORT

	add_option (_("Plugins/Audio Unit"), new OptionEditorHeading (_("Audio Unit")));
#if 0
	add_option (_("Plugins/Audio Unit"),
			new RcActionButton (_("Scan for Plugins"),
				sigc::mem_fun (*this, &RCOptionEditor::plugin_scan_refresh)));
#endif

	bo = new BoolOption (
			"use-audio-units",
			_("Enable Autio Unit support (requires restart or re-scan)"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_use_audio_units),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_use_audio_units)
			);
	add_option (_("Plugins/Audio Unit"), bo);

	add_option (_("Plugins/Audio Unit"),
			new RcActionButton (_("Clear"),
				sigc::mem_fun (*this, &RCOptionEditor::clear_au_cache),
				_("AU Cache:")));

	add_option (_("Plugins/Audio Unit"),
			new RcActionButton (_("Clear"),
				sigc::mem_fun (*this, &RCOptionEditor::clear_au_blacklist),
				_("AU Ignorelist:")));
#endif

	add_option (_("Plugins"), new OptionEditorHeading (_("LV1/LV2")));
	add_option (_("Plugins"),
	     new BoolOption (
		     "conceal-lv1-if-lv2-exists",
		     _("Conceal LADSPA (LV1) Plugins if matching LV2 exists"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_conceal_lv1_if_lv2_exists),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_conceal_lv1_if_lv2_exists)
		     ));

	add_option (_("Plugins"), new OptionEditorHeading (_("Plugin GUI")));
	add_option (_("Plugins"),
	     new BoolOption (
		     "open-gui-after-adding-plugin",
		     _("Automatically open the plugin GUI when adding a new plugin"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_open_gui_after_adding_plugin),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_open_gui_after_adding_plugin)
		     ));

#ifdef LV2_EXTENDED
	add_option (_("Plugins"),
	     new BoolOption (
		     "show-inline-display-by-default",
		     _("Show Plugin Inline Display on Mixerstrip by default"),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_show_inline_display_by_default),
		     sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_show_inline_display_by_default)
		     ));

	_plugin_prefer_inline = new BoolOption (
			"prefer-inline-over-gui",
			_("Don't automatically open the plugin GUI when the plugin has an inline display mode"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_prefer_inline_over_gui),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_prefer_inline_over_gui)
			);
	add_option (_("Plugins"), _plugin_prefer_inline);
#endif

	add_option (_("Plugins"), new OptionEditorHeading (_("Instrument")));

	bo = new BoolOption (
			"ask-replace-instrument",
			_("Ask to replace existing instrument plugin"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_ask_replace_instrument),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_ask_replace_instrument)
			);
	add_option (_("Plugins"), bo);

	bo = new BoolOption (
			"ask-setup_instrument",
			_("Interactively configure instrument plugins on insert"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_ask_setup_instrument),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_ask_setup_instrument)
			);
	add_option (_("Plugins"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> show a dialog to select instrument channel configuration before adding a multichannel plugin."));

	add_option (_("Plugins"), new OptionEditorHeading (_("Statistics")));

	add_option (_("Plugins"),
			new RcActionButton (_("Reset Statistics"),
				sigc::mem_fun (*this, &RCOptionEditor::plugin_reset_stats)));

	add_option (_("Plugins"),
	     new SpinOption<int32_t> (
		     "max-plugin-chart",
		     _("Plugin chart (use-count) length"),
				 sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_max_plugin_chart),
				 sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_max_plugin_chart),
		     10, 25, 1, 5
		     ));

	add_option (_("Plugins"),
	     new SpinOption<int32_t> (
		     "max-plugin-recent",
		     _("Plugin recent list length"),
				 sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_max_plugin_recent),
				 sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_max_plugin_recent),
		     10, 50, 1, 5
		     ));

	add_option (_("Plugins"), new OptionEditorBlank ());

	/* MONITORING, SOLO) ********************************************************/

	add_option (_("Monitoring"), new OptionEditorHeading (_("Monitoring")));

	ComboOption<MonitorModel>* mm = new ComboOption<MonitorModel> (
		"monitoring-model",
		_("Record monitoring handled by"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_monitoring_model),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_monitoring_model)
		);

	if (AudioEngine::instance()->port_engine().can_monitor_input()) {
		mm->add (HardwareMonitoring, _("via Audio Driver"));
	}

	string prog (PROGRAM_NAME);
	boost::algorithm::to_lower (prog);
	mm->add (SoftwareMonitoring, string_compose (_("%1"), prog));
	mm->add (ExternalMonitoring, _("audio hardware"));

	add_option (_("Monitoring"), mm);

	bo = new BoolOption (
		     "auto-input-does-talkback",
		     _("Auto Input does 'talkback'"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_auto_input_does_talkback),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_auto_input_does_talkback)
		     );
	add_option (_("Monitoring"), bo);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			string_compose (_("<b>When enabled</b>, and Transport->Auto-Input is enabled, %1 will always monitor audio inputs when transport is stopped, even if tracks aren't armed."),
					PROGRAM_NAME));

	add_option (_("Monitoring"), new OptionEditorHeading (_("Solo")));

	_solo_control_is_listen_control = new BoolOption (
		"solo-control-is-listen-control",
		_("Solo controls are Listen controls"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_solo_control_is_listen_control),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_solo_control_is_listen_control)
		);

	add_option (_("Monitoring"), _solo_control_is_listen_control);

	add_option (_("Monitoring"),
	     new BoolOption (
		     "exclusive-solo",
		     _("Exclusive solo"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_exclusive_solo),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_exclusive_solo)
		     ));

	add_option (_("Monitoring"),
	     new BoolOption (
		     "show-solo-mutes",
		     _("Show solo muting"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_show_solo_mutes),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_show_solo_mutes)
		     ));

	add_option (_("Monitoring"),
	     new BoolOption (
		     "solo-mute-override",
		     _("Soloing overrides muting"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_solo_mute_override),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_solo_mute_override)
		     ));

	add_option (_("Monitoring"),
	     new FaderOption (
		     "solo-mute-gain",
		     _("Solo-in-place mute cut (dB)"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_solo_mute_gain),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_solo_mute_gain)
		     ));

	_listen_position = new ComboOption<ListenPosition> (
		"listen-position",
		_("Listen Position"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_listen_position),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_listen_position)
		);

	_listen_position->add (AfterFaderListen, _("after-fader (AFL)"));
	_listen_position->add (PreFaderListen, _("pre-fader (PFL)"));

	add_option (_("Monitoring"), _listen_position);

	ComboOption<PFLPosition>* pp = new ComboOption<PFLPosition> (
		"pfl-position",
		_("PFL signals come from"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_pfl_position),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_pfl_position)
		);

	pp->add (PFLFromBeforeProcessors, _("before pre-fader processors"));
	pp->add (PFLFromAfterProcessors, _("pre-fader but after pre-fader processors"));

	add_option (_("Monitoring"), pp);

	ComboOption<AFLPosition>* pa = new ComboOption<AFLPosition> (
		"afl-position",
		_("AFL signals come from"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_afl_position),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_afl_position)
		);

	pa->add (AFLFromBeforeProcessors, _("immediately post-fader"));
	pa->add (AFLFromAfterProcessors, _("after post-fader processors (before pan)"));

	add_option (_("Monitoring"), pa);

	/* SIGNAL FLOW **************************************************************/

	add_option (_("Signal Flow"), new OptionEditorHeading (_("Master")));
	add_option (_("Signal Flow"),
			new BoolOption (
				"use-master-volume",
				_("Enable master-bus output gain control"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_use_master_volume),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_use_master_volume)
				));

	add_option (_("Signal Flow"), new OptionEditorHeading (_("Default Track / Bus Muting Options")));

	add_option (_("Signal Flow"),
	     new BoolOption (
		     "mute-affects-pre-fader",
		     _("Mute affects pre-fader sends"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_mute_affects_pre_fader),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_mute_affects_pre_fader)
		     ));

	add_option (_("Signal Flow"),
	     new BoolOption (
		     "mute-affects-post-fader",
		     _("Mute affects post-fader sends"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_mute_affects_post_fader),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_mute_affects_post_fader)
		     ));

	add_option (_("Signal Flow"),
	     new BoolOption (
		     "mute-affects-control-outs",
		     _("Mute affects control outputs"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_mute_affects_control_outs),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_mute_affects_control_outs)
		     ));

	add_option (_("Signal Flow"),
	     new BoolOption (
		     "mute-affects-main-outs",
		     _("Mute affects main outputs"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_mute_affects_main_outs),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_mute_affects_main_outs)
		     ));


	add_option (_("Signal Flow"), new OptionEditorHeading (_("Send Routing")));
	add_option (_("Signal Flow"),
			new BoolOption (
				"link-send-and-route-panner",
				_("Link panners of Aux and External Sends with main panner by default"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_link_send_and_route_panner),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_link_send_and_route_panner)
				));

	add_option (_("Signal Flow"), new OptionEditorHeading (_("Audio Regions")));

	add_option (_("Signal Flow"),
	     new BoolOption (
		     "replicate-missing-region-channels",
		     _("Replicate missing region channels"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_replicate_missing_region_channels),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_replicate_missing_region_channels)
		     ));

	if (!Profile->get_mixbus()) {

		add_option (_("Signal Flow"), new OptionEditorHeading (_("Track and Bus Connections")));

		bo = new BoolOption (
					"auto-connect-standard-busses",
					_("Auto-connect main output (master or monitor) bus to physical ports"),
					sigc::mem_fun (*_rc_config, &RCConfiguration::get_auto_connect_standard_busses),
					sigc::mem_fun (*_rc_config, &RCConfiguration::set_auto_connect_standard_busses)
					);
		add_option (_("Signal Flow"), bo);
		Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("<b>When enabled</b> the main output bus is auto-connected to the first N physical ports. "
				"If the session has a monitor-section, the monitor-bus output is connected to the hardware playback ports, "
				"otherwise the master-bus output is directly used for playback."));

		ComboOption<AutoConnectOption>* iac = new ComboOption<AutoConnectOption> (
				"input-auto-connect",
				_("Connect track inputs"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_input_auto_connect),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_input_auto_connect)
				);

		iac->add (AutoConnectPhysical, _("automatically to physical inputs"));
		iac->add (ManualConnect, _("manually"));

		add_option (_("Signal Flow"), iac);

		ComboOption<AutoConnectOption>* oac = new ComboOption<AutoConnectOption> (
				"output-auto-connect",
				_("Connect track and bus outputs"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_output_auto_connect),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_output_auto_connect)
				);

		oac->add (AutoConnectPhysical, _("automatically to physical outputs"));
		oac->add (AutoConnectMaster, _("automatically to master bus"));
		oac->add (ManualConnect, _("manually"));

		add_option (_("Signal Flow"), oac);

		bo = new BoolOption (
				"strict-io",
				_("Use 'Strict-I/O' for new tracks or busses"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_strict_io),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_strict_io)
				);

		add_option (_("Signal Flow"), bo);
		Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
				_("With strict-i/o enabled, Effect Processors will not modify the number of channels on a track. The number of output channels will always match the number of input channels."));

	}  // !mixbus

	/* Click */

	add_option (_("Metronome"), new OptionEditorHeading (_("Metronome")));
	add_option (_("Metronome"), new ClickOptions (_rc_config));

	add_option (_("Metronome"), new OptionEditorHeading (_("Options")));

	bo = new BoolOption (
			"click-record-only",
			_("Enable metronome only while recording"),
			sigc::mem_fun (*_rc_config, &RCConfiguration::get_click_record_only),
			sigc::mem_fun (*_rc_config, &RCConfiguration::set_click_record_only)
			);

	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			string_compose (_("<b>When enabled</b> the metronome will remain silent if %1 is <b>not recording</b>."), PROGRAM_NAME));
	add_option (_("Metronome"), bo);
	add_option (_("Metronome"), new OptionEditorBlank ());


	/* CONTROL SURFACES *********************************************************/

	add_option (_("Control Surfaces"), new OptionEditorHeading (_("Control Surfaces")));
	add_option (_("Control Surfaces"), new ControlSurfacesOptions ());


	/* METERS *******************************************************************/

	if (Profile->get_mixbus()) {
		add_option (S_("Preferences|Metering"), new OptionEditorHeading (_("Meterbridge meters")));
	} else {
		add_option (S_("Preferences|Metering"), new OptionEditorHeading (_("Metering")));
	}

	ComboOption<float>* mht = new ComboOption<float> (
		"meter-hold",
		_("Peak hold time"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_meter_hold),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_meter_hold)
		);

	mht->add (MeterHoldOff, _("off"));
	mht->add (MeterHoldShort, _("short"));
	mht->add (MeterHoldMedium, _("medium"));
	mht->add (MeterHoldLong, _("long"));

	add_option (S_("Preferences|Metering"), mht);

	ComboOption<float>* mfo = new ComboOption<float> (
		"meter-falloff",
		_("DPM fall-off"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_meter_falloff),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_meter_falloff)
		);

	mfo->add (METER_FALLOFF_OFF,      _("off"));
	mfo->add (METER_FALLOFF_SLOWEST,  _("slowest [6.6dB/sec]"));
	mfo->add (METER_FALLOFF_SLOW,     _("slow [8.6dB/sec] (BBC PPM, EBU PPM)"));
	mfo->add (METER_FALLOFF_SLOWISH,  _("moderate [12.0dB/sec] (DIN)"));
	mfo->add (METER_FALLOFF_MODERATE, _("medium [13.3dB/sec] (EBU Digi PPM, IRT Digi PPM)"));
	mfo->add (METER_FALLOFF_MEDIUM,   _("fast [20dB/sec]"));
	mfo->add (METER_FALLOFF_FAST,     _("very fast [32dB/sec]"));

	add_option (S_("Preferences|Metering"), mfo);

	ComboOption<MeterLineUp>* mlu = new ComboOption<MeterLineUp> (
		"meter-line-up-level",
		_("Meter line-up level; 0dBu"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_meter_line_up_level),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_meter_line_up_level)
		);

	mlu->add (MeteringLineUp24, _("-24dBFS (SMPTE US: 4dBu = -20dBFS)"));
	mlu->add (MeteringLineUp20, _("-20dBFS (SMPTE RP.0155)"));
	mlu->add (MeteringLineUp18, _("-18dBFS (EBU, BBC)"));
	mlu->add (MeteringLineUp15, _("-15dBFS (DIN)"));

	Gtkmm2ext::UI::instance()->set_tip (mlu->tip_widget(), _("Configure meter-marks and color-knee point for dBFS scale DPM, set reference level for IEC1/Nordic, IEC2 PPM and VU meter."));

	add_option (S_("Preferences|Metering"), mlu);

	ComboOption<MeterLineUp>* mld = new ComboOption<MeterLineUp> (
		"meter-line-up-din",
		_("IEC1/DIN Meter line-up level; 0dBu"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_meter_line_up_din),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_meter_line_up_din)
		);

	mld->add (MeteringLineUp24, _("-24dBFS (SMPTE US: 4dBu = -20dBFS)"));
	mld->add (MeteringLineUp20, _("-20dBFS (SMPTE RP.0155)"));
	mld->add (MeteringLineUp18, _("-18dBFS (EBU, BBC)"));
	mld->add (MeteringLineUp15, _("-15dBFS (DIN)"));

	Gtkmm2ext::UI::instance()->set_tip (mld->tip_widget(), _("Reference level for IEC1/DIN meter."));

	add_option (S_("Preferences|Metering"), mld);

	ComboOption<VUMeterStandard>* mvu = new ComboOption<VUMeterStandard> (
		"meter-vu-standard",
		_("VU Meter standard"),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_meter_vu_standard),
		sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_meter_vu_standard)
		);

	mvu->add (MeteringVUfrench,   _("0VU = -2dBu (France)"));
	mvu->add (MeteringVUamerican, _("0VU = 0dBu (North America, Australia)"));
	mvu->add (MeteringVUstandard, _("0VU = +4dBu (standard)"));
	mvu->add (MeteringVUeight,    _("0VU = +8dBu"));

	add_option (S_("Preferences|Metering"), mvu);

	HSliderOption *mpks = new HSliderOption("meter-peak",
			_("Peak indicator threshold [dBFS]"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_meter_peak),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_meter_peak),
			-10, 0, .1, .1
			);

	Gtkmm2ext::UI::instance()->set_tip (
			mpks->tip_widget(),
			_("Specify the audio signal level in dBFS at and above which the meter-peak indicator will flash red."));

	add_option (S_("Preferences|Metering"), mpks);

	OptionEditorHeading* default_meter_head = new OptionEditorHeading (_("Default Meter Types"));
	default_meter_head->set_note (_("These settings apply to newly created tracks and busses. For the Master bus, this will be when a new session is created."));

	add_option (S_("Preferences|Metering"), default_meter_head);

	ComboOption<MeterType>* mtm = new ComboOption<MeterType> (
		"meter-type-master",
		_("Default Meter Type for Master Bus"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_meter_type_master),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_meter_type_master)
		);
	mtm->add (MeterPeak,    ArdourMeter::meter_type_string(MeterPeak));
	mtm->add (MeterK20,     ArdourMeter::meter_type_string(MeterK20));
	mtm->add (MeterK14,     ArdourMeter::meter_type_string(MeterK14));
	mtm->add (MeterK12,     ArdourMeter::meter_type_string(MeterK12));
	mtm->add (MeterIEC1DIN, ArdourMeter::meter_type_string(MeterIEC1DIN));
	mtm->add (MeterIEC1NOR, ArdourMeter::meter_type_string(MeterIEC1NOR));
	mtm->add (MeterIEC2BBC, ArdourMeter::meter_type_string(MeterIEC2BBC));
	mtm->add (MeterIEC2EBU, ArdourMeter::meter_type_string(MeterIEC2EBU));

	add_option (S_("Preferences|Metering"), mtm);

	ComboOption<MeterType>* mtb = new ComboOption<MeterType> (
		"meter-type-bus",
		_("Default meter type for busses"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_meter_type_bus),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_meter_type_bus)
		);
	mtb->add (MeterPeak,    ArdourMeter::meter_type_string(MeterPeak));
	mtb->add (MeterK20,     ArdourMeter::meter_type_string(MeterK20));
	mtb->add (MeterK14,     ArdourMeter::meter_type_string(MeterK14));
	mtb->add (MeterK12,     ArdourMeter::meter_type_string(MeterK12));
	mtb->add (MeterIEC1DIN, ArdourMeter::meter_type_string(MeterIEC1DIN));
	mtb->add (MeterIEC1NOR, ArdourMeter::meter_type_string(MeterIEC1NOR));
	mtb->add (MeterIEC2BBC, ArdourMeter::meter_type_string(MeterIEC2BBC));
	mtb->add (MeterIEC2EBU, ArdourMeter::meter_type_string(MeterIEC2EBU));

	add_option (S_("Preferences|Metering"), mtb);

	ComboOption<MeterType>* mtt = new ComboOption<MeterType> (
		"meter-type-track",
		_("Default meter type for tracks"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_meter_type_track),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_meter_type_track)
		);
	mtt->add (MeterPeak,    ArdourMeter::meter_type_string(MeterPeak));
	mtt->add (MeterPeak0dB, ArdourMeter::meter_type_string(MeterPeak0dB));

	add_option (S_("Preferences|Metering"), mtt);

	add_option (S_("Preferences|Metering"), new OptionEditorHeading (_("Region Analysis")));  //needs translation

	add_option (S_("Preferences|Metering"),
	     new BoolOption (
		     "auto-analyse-audio",
		     _("Enable automatic analysis of audio"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_auto_analyse_audio),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_auto_analyse_audio)
		     ));


	/* PERFORMANCE **************************************************************/

	uint32_t hwcpus = hardware_concurrency ();

	if (hwcpus > 1) {
		add_option (_("Performance"), new OptionEditorHeading (_("DSP CPU Utilization")));

		ComboOption<int32_t>* procs = new ComboOption<int32_t> (
				"processor-usage",
				_("Signal processing uses"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_processor_usage),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_processor_usage)
				);

		procs->add (-1, _("all but one processor"));
		procs->add (0, _("all available processors"));

		for (uint32_t i = 1; i <= hwcpus; ++i) {
			procs->add (i, string_compose (P_("%1 processor", "%1 processors", i), i));
		}

		procs->set_note (string_compose (_("This setting will only take effect when %1 is restarted."), PROGRAM_NAME));

		add_option (_("Performance"), procs);
	}

#if !(defined PLATFORM_WINDOWS || defined __APPLE__)
	if (Glib::file_test ("/dev/cpu_dma_latency", Glib::FILE_TEST_EXISTS)) {

		ComboOption<int32_t>* cpudma = new ComboOption<int32_t> (
				"cpu-dma-latency",
				_("Power Management, CPU DMA latency:"),
				sigc::mem_fun (*_rc_config, &RCConfiguration::get_cpu_dma_latency),
				sigc::mem_fun (*_rc_config, &RCConfiguration::set_cpu_dma_latency)
				);

		set<int> lvalues;
		vector<string> latency_files;
		Searchpath sp ("/sys/devices/system/cpu/cpu0/cpuidle/");
		find_files_matching_regex (latency_files, sp, "latency$", true);
		for (vector<string>::const_iterator i = latency_files.begin(); i != latency_files.end (); ++i) {
			try {
				std::string l = Glib::file_get_contents (*i);
				int lv = atoi (l.c_str());
				if (lv > 0) {
					lvalues.insert (lv);
				}
			} catch (...) { }
		}

		if (lvalues.empty ()) {
			lvalues.insert (5);
			lvalues.insert (55);
			lvalues.insert (100);
		}

		int32_t cpudma_val = _rc_config->get_cpu_dma_latency ();
		if (cpudma_val > 0) {
			lvalues.insert (cpudma_val);
		}

		cpudma->add (-1, _("Unset"));
		cpudma->add (0, _("Lowest (prevent CPU sleep states)"));

		for (set<int>::const_iterator i = lvalues.begin(); i != lvalues.end (); ++i) {
			cpudma->add (*i, string_compose (_("%1 usec"), *i));
		}

		set_tooltip (cpudma->tip_widget(), _("This setting sets the maximum tolerable CPU DMA latency. This prevents the CPU from entering power-save states which can be beneficial for reliable low latency."));

		if (access ("/dev/cpu_dma_latency", W_OK)) {
			cpudma->set_note (_("This setting requires write access to `/dev/cpu_dma_latency'."));
		}

		add_option (_("Performance"), cpudma);
	}

#endif


	add_option (_("Performance"), new OptionEditorHeading (_("CPU/FPU Denormals")));

	add_option (_("Performance"),
	     new BoolOption (
		     "denormal-protection",
		     _("Use DC bias to protect against denormals"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_denormal_protection),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_denormal_protection)
		     ));

	ComboOption<DenormalModel>* dm = new ComboOption<DenormalModel> (
		"denormal-model",
		_("Processor handling"),
		sigc::mem_fun (*_rc_config, &RCConfiguration::get_denormal_model),
		sigc::mem_fun (*_rc_config, &RCConfiguration::set_denormal_model)
		);

	int dmsize = 1;
	dm->add (DenormalNone, _("no processor handling"));

	FPU* fpu = FPU::instance();

	if (fpu->has_flush_to_zero()) {
		++dmsize;
		dm->add (DenormalFTZ, _("use FlushToZero"));
	} else if (_rc_config->get_denormal_model() == DenormalFTZ) {
		_rc_config->set_denormal_model(DenormalNone);
	}

	if (fpu->has_denormals_are_zero()) {
		++dmsize;
		dm->add (DenormalDAZ, _("use DenormalsAreZero"));
	} else if (_rc_config->get_denormal_model() == DenormalDAZ) {
		_rc_config->set_denormal_model(DenormalNone);
	}

	if (fpu->has_flush_to_zero() && fpu->has_denormals_are_zero()) {
		++dmsize;
		dm->add (DenormalFTZDAZ, _("use FlushToZero and DenormalsAreZero"));
	} else if (_rc_config->get_denormal_model() == DenormalFTZDAZ) {
		_rc_config->set_denormal_model(DenormalNone);
	}

	if (dmsize == 1) {
		dm->set_sensitive(false);
	}

	dm->set_note (_("Changes may not be effective until audio-engine restart."));

	add_option (_("Performance"), dm);


	add_option (_("Performance"), new OptionEditorHeading (_("Disk I/O Buffering"))); //ToDo: this changed, needs translation.  disambiguated from soundcard i/o buffering

	add_option (_("Performance"), new BufferingOptions (_rc_config));

	/* Image cache size */
	add_option (_("Performance"), new OptionEditorHeading (_("Memory Usage")));

	HSliderOption *sics = new HSliderOption ("waveform-cache-size",
			_("Waveform image cache size (megabytes)"),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::get_waveform_cache_size),
			sigc::mem_fun (UIConfiguration::instance(), &UIConfiguration::set_waveform_cache_size),
			1, 1024, 10 /* 1 MB to 1GB in steps of 10MB */
			);
	sics->scale().set_digits (0);
	Gtkmm2ext::UI::instance()->set_tip (
			sics->tip_widget(),
		 _("Increasing the cache size uses more memory to store waveform images, which can improve graphical performance."));
	add_option (_("Performance"), sics);

	add_option (_("Performance"), new OptionEditorHeading (_("Automation")));

	add_option (_("Performance"),
	     new SpinOption<double> (
		     "automation-thinning-factor",
		     _("Thinning factor (larger value => less data)"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_automation_thinning_factor),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_automation_thinning_factor),
		     0, 1000, 1, 20
		     ));

	add_option (_("Performance"),
	     new SpinOption<double> (
		     "automation-interval-msecs",
		     _("Automation sampling interval (milliseconds)"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_automation_interval_msecs),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_automation_interval_msecs),
		     1, 1000, 1, 20
		     ));

	add_option (_("Performance"), new OptionEditorHeading (_("Automatables")));

	ComboOption<uint32_t>* lna = new ComboOption<uint32_t> (
		     "limit-n-automatables",
		     _("Limit automatable parameters per plugin"),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::get_limit_n_automatables),
		     sigc::mem_fun (*_rc_config, &RCConfiguration::set_limit_n_automatables)
		     );
	lna->add (0, _("Unlimited"));
	lna->add (64,  _("64 parameters"));
	lna->add (128, _("128 parameters"));
	lna->add (256, _("256 parameters"));
	lna->add (512, _("512 parameters"));
	lna->add (999, _("999 parameters"));
	add_option (_("Performance"), lna);
	Gtkmm2ext::UI::instance()->set_tip (lna->tip_widget(),
					    _("Some Plugins expose an unreasonable amount of control-inputs. This option limits the number of parameters that can are listed as automatable without restricting the number of total controls.\n\nThis reduces lag in the GUI and shortens excessively long drop-down lists for plugins with a large number of control ports.\n\nNote: This only affects newly added plugins and is applied to plugin on session-reload. Already automated parameters are retained."));

	/* VIDEO Timeline */
	add_option (_("Video"), new OptionEditorHeading (_("Video Server")));
	add_option (_("Video"), new VideoTimelineOptions (_rc_config));

	/* trigger launcing */

	add_option (_("Triggering"), new OptionEditorHeading (_("Triggering")));

	TriggerPortSelectOption* dtip  = new TriggerPortSelectOption (_rc_config, this);

	set_tooltip (dtip->tip_widget(), _("If set, the identifies the input MIDI port that will be automatically connected to trigger boxes.\n\n"
	                                   "It is intended to be either an NxN pad device such as the Ableton Push 2 or Novation Launchpad\n"
	                                   "or a regular MIDI device capable of sending sequential note numbers (like a typical keyboard)"));
	add_option (_("Triggering"), dtip);

	add_option (_("Triggering"), new OptionEditorHeading (_("Clip Library")));

	add_option (_("Triggering"), new DirectoryOption (
			    X_("clip-library-dir"),
			    _("User writable Clip Library:"),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::get_clip_library_dir),
			    sigc::mem_fun (*_rc_config, &RCConfiguration::set_clip_library_dir)
			    ));

	add_option (_("Triggering"),
			new RcActionButton (_("Reset Clip Library Dir"),
				sigc::mem_fun (*this, &RCOptionEditor::reset_clip_library_dir)));

	/* END OF SECTIONS/OPTIONS etc */

	Widget::show_all ();

	//trigger some parameter-changed messages which affect widget-visibility or -sensitivity
	parameter_changed ("send-ltc");
	parameter_changed ("sync-source");
	parameter_changed ("open-gui-after-adding-plugin");

	XMLNode* node = ARDOUR_UI::instance()->preferences_settings();
	if (node) {
		/* gcc4 complains about ambiguity with Gtk::Widget::set_state
		   (Gtk::StateType) here !!!
		*/
		Tabbable::set_state (*node, Stateful::loading_state_version);
	}

	set_current_page (_("General"));
}

bool
RCOptionEditor::on_key_release_event (GdkEventKey* event)
{
	if (Keyboard::modifier_state_equals (event->state, Keyboard::close_window_modifier)) {
		if (event->keyval == (guint) Keyboard::close_window_key) {
			WindowProxy::hide ();
			return true;
		}
	}

	return false;
}

void
RCOptionEditor::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);
	_transport_masters_widget.set_session (s);
}

void
RCOptionEditor::parameter_changed (string const & p)
{
	OptionEditor::parameter_changed (p);

	if (p == "use-monitor-bus") {
		bool const s = Config->get_use_monitor_bus ();
		if (!s) {
			/* we can't use this if we don't have a monitor bus */
			Config->set_solo_control_is_listen_control (false); // XXX
		}
		_solo_control_is_listen_control->set_sensitive (s);
		_listen_position->set_sensitive (s);
	} else if (p == "sync-source") {
		boost::shared_ptr<TransportMaster> tm (TransportMasterManager::instance().current());
		if (boost::dynamic_pointer_cast<TimecodeTransportMaster> (tm)) {
			_sync_framerate->set_sensitive (true);
		} else {
			_sync_framerate->set_sensitive (false);
		}
	} else if (p == "send-ltc") {
		bool const s = Config->get_send_ltc ();
		_ltc_send_continuously->set_sensitive (s);
		_ltc_volume_slider->set_sensitive (s);
	} else if (p == "open-gui-after-adding-plugin" || p == "show-inline-display-by-default") {
#ifdef LV2_EXTENDED
		_plugin_prefer_inline->set_sensitive (UIConfiguration::instance().get_open_gui_after_adding_plugin() && UIConfiguration::instance().get_show_inline_display_by_default());
#endif
	} else if (p == "conceal-lv1-if-lv2-exists") {
		plugin_scan_refresh ();
	} else if (p == "conceal-vst2-if-vst3-exists") {
		plugin_scan_refresh ();
	}
}

void RCOptionEditor::reset_clip_library_dir () {
	_rc_config->set_clip_library_dir ("@default@");
	clip_library_dir (false);
}

void RCOptionEditor::show_audio_setup () {
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Window", "toggle-audio-midi-setup");
	tact->set_active();
}

void RCOptionEditor::show_transport_masters () {
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Window", "toggle-transport-masters");
	tact->set_active();
}

void RCOptionEditor::plugin_scan_refresh () {
	/* first argument says discover new plugins, second means be verbose */
	PluginScanDialog psd (false, true, current_toplevel ());
	psd.start ();
	ARDOUR_UI::instance()->show_plugin_manager ();
}

void RCOptionEditor::plugin_reset_stats () {
	PluginManager::instance().reset_stats();
}

void RCOptionEditor::clear_vst2_cache () {
	PluginManager::instance().clear_vst_cache();
}

void RCOptionEditor::clear_vst2_blacklist () {
	PluginManager::instance().clear_vst_blacklist();
}

void RCOptionEditor::clear_vst3_cache () {
	PluginManager::instance().clear_vst3_cache();
}

void RCOptionEditor::clear_vst3_blacklist () {
	PluginManager::instance().clear_vst3_blacklist();
}

void RCOptionEditor::clear_au_cache () {
	PluginManager::instance().clear_au_cache();
}

void RCOptionEditor::clear_au_blacklist () {
	PluginManager::instance().clear_au_blacklist();
}

void
RCOptionEditor::edit_vst_path (std::string const& title, std::string const& dflt, sigc::slot<string> get, sigc::slot<bool, string> set)
{
	/* see also PluginManagerUI::edit_vst_path */
	PathsDialog pd (*current_toplevel(), title, get (), dflt);
	if (pd.run () != Gtk::RESPONSE_ACCEPT) {
		return;
	}
	pd.hide();
	set (pd.get_serialized_paths());
	MessageDialog msg (_("Re-scan Plugins now?"), false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
	msg.set_default_response (Gtk::RESPONSE_YES);
	if (msg.run() != Gtk::RESPONSE_YES) {
		return;
	}
	msg.hide ();
	plugin_scan_refresh ();
}


Gtk::Window*
RCOptionEditor::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window ();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("PreferencesWindow");
		ARDOUR_UI::instance()->setup_toplevel_window (*win, _("Preferences"), this);
		win->resize (1, 1);
		win->set_resizable (false);
	}

	return win;
}

XMLNode&
RCOptionEditor::get_state ()
{
	XMLNode* node = new XMLNode (X_("Preferences"));
	node->add_child_nocopy (Tabbable::get_state());
	return *node;
}
