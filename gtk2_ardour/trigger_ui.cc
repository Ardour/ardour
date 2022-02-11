/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/alignment.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/pathexpand.h"
#include "pbd/search_path.h"
#include "pbd/unwind.h"

#include "ardour/audioengine.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "slot_properties_box.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "region_view.h"
#include "trigger_jump_dialog.h"
#include "ui_config.h"

#include "trigger_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Temporal;

std::vector<std::string> TriggerUI::follow_strings;
std::string              TriggerUI::longest_follow;
std::vector<std::string> TriggerUI::quantize_strings;
std::string              TriggerUI::longest_quantize;
std::vector<std::string> TriggerUI::launch_strings;
std::string              TriggerUI::longest_launch;
std::vector<std::string> TriggerUI::stretch_mode_strings;
std::string              TriggerUI::longest_stretch_mode;

Gtkmm2ext::Bindings*           TriggerUI::bindings = 0;
Glib::RefPtr<Gtk::ActionGroup> TriggerUI::trigger_actions;

TriggerUI::TriggerUI ()
	: _renaming (false)
	, _file_chooser (0)
	, _launch_context_menu (0)
	, _follow_context_menu (0)
	, _context_menu (0)
	, _ignore_menu_action (false)
{
	if (follow_strings.empty()) {
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::None)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::Stop)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::Again)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::ForwardTrigger)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::ReverseTrigger)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::FirstTrigger)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::LastTrigger)));
		follow_strings.push_back (follow_action_to_string (FollowAction (FollowAction::JumpTrigger)));

		for (std::vector<std::string>::const_iterator i = follow_strings.begin(); i != follow_strings.end(); ++i) {
			if (i->length() > longest_follow.length()) {
				longest_follow = *i;
			}
		}

		launch_strings.push_back (launch_style_to_string (Trigger::OneShot));
		launch_strings.push_back (launch_style_to_string (Trigger::ReTrigger));
		launch_strings.push_back (launch_style_to_string (Trigger::Gate));
		launch_strings.push_back (launch_style_to_string (Trigger::Toggle));
		launch_strings.push_back (launch_style_to_string (Trigger::Repeat));

		for (std::vector<std::string>::const_iterator i = launch_strings.begin(); i != launch_strings.end(); ++i) {
			if (i->length() > longest_launch.length()) {
				longest_launch = *i;
			}
		}

		stretch_mode_strings.push_back (stretch_mode_to_string (Trigger::Crisp));
		stretch_mode_strings.push_back (stretch_mode_to_string (Trigger::Mixed));
		stretch_mode_strings.push_back (stretch_mode_to_string (Trigger::Smooth));

		for (std::vector<std::string>::const_iterator i = stretch_mode_strings.begin(); i != stretch_mode_strings.end(); ++i) {
			if (i->length() > longest_stretch_mode.length()) {
				longest_stretch_mode = *i;
			}
		}
	}
}

TriggerUI::~TriggerUI()
{
	trigger_swap_connection.disconnect ();
	trigger_connections.drop_connections ();
}

void
TriggerUI::trigger_swap (uint32_t n)
{
	if (n != tref.slot) {
		/* some other slot in the same box got swapped. we don't care */
		return;
	}
	trigger_connections.drop_connections ();

	trigger()->PropertyChanged.connect (trigger_connections, MISSING_INVALIDATOR, boost::bind (&TriggerUI::trigger_changed, this, _1), gui_context ());
	tref.box->PropertyChanged.connect (trigger_connections, MISSING_INVALIDATOR, boost::bind (&TriggerUI::trigger_changed, this, _1), gui_context ());

	trigger_changed (Properties::name);
}


void
TriggerUI::setup_actions_and_bindings ()
{
	load_bindings ();
	register_actions ();
}

void
TriggerUI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Cues"));
}

void
TriggerUI::register_actions ()
{
	trigger_actions = ActionManager::create_action_group (bindings, X_("Cues"));

	for (int32_t n = 0; n < default_triggers_per_box; ++n) {
		const std::string action_name  = string_compose ("trigger-cue-%1", n);
		const std::string display_name = string_compose (_("Trigger Cue %1"), cue_marker_name (n));

		ActionManager::register_action (trigger_actions, action_name.c_str (), display_name.c_str (), sigc::bind (sigc::ptr_fun (TriggerUI::trigger_cue), n));
	}
}

void
TriggerUI::trigger_cue (int32_t n)
{
	Session* s = AudioEngine::instance()->session();

	if (s) {
		s->cue_bang (n);
	}
}

void
TriggerUI::choose_color ()
{
	_color_dialog.get_colorsel()->set_has_opacity_control (false);
	_color_dialog.get_colorsel()->set_has_palette (true);
	_color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (_color_dialog, &Gtk::Dialog::response), Gtk::RESPONSE_ACCEPT));
	_color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (_color_dialog, &Gtk::Dialog::response), Gtk::RESPONSE_CANCEL));

	Gdk::Color c = ARDOUR_UI_UTILS::gdk_color_from_rgba(trigger()->color());

	_color_dialog.get_colorsel()->set_previous_color (c);
	_color_dialog.get_colorsel()->set_current_color (c);

	switch (_color_dialog.run()) {
		case Gtk::RESPONSE_ACCEPT: {
			c = _color_dialog.get_colorsel()->get_current_color();
			color_t ct = ARDOUR_UI_UTILS::gdk_color_to_rgba(c);
			trigger()->set_color(ct);
		} break;
		default:
			break;
	}

	_color_dialog.hide ();
}

void
TriggerUI::choose_sample (bool allow_multiple_select)
{
	if (!_file_chooser) {
		_file_chooser = new Gtk::FileChooserDialog (_("Select sample"), Gtk::FILE_CHOOSER_ACTION_OPEN);
		_file_chooser->add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		_file_chooser->add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

		/* for newbies, start in the bundled media folder */
		Searchpath spath (ardour_data_search_path ());
		spath.add_subdirectory_to_paths (media_dir_name);
		for (auto const& f : spath) {
			if (Glib::file_test (f, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
				_file_chooser->set_current_folder (f);
			}
		}

		/* TODO: add various shortcut paths to user's media folders

		_file_chooser->add_shortcut_folder_uri(Glib::build_filename (user_config_directory (), media_dir_name);

		Searchpath cpath (Config->get_sample_lib_path ());
		for (auto const& f : cpath) {
			_file_chooser->add_shortcut_folder_uri (f);
		}
		*/

#ifdef __APPLE__
		try {
			/* add_shortcut_folder throws an exception if the folder being added already has a shortcut */
			_file_chooser->add_shortcut_folder_uri("file:///Library/GarageBand/Apple Loops");
			_file_chooser->add_shortcut_folder_uri("file:///Library/Audio/Apple Loops");
			_file_chooser->add_shortcut_folder_uri("file:///Library/Application Support/GarageBand/Instrument Library/Sampler/Sampler Files");
		}
		catch (Glib::Error & e) {
			std::cerr << "sfdb.add_shortcut_folder() threw Glib::Error " << e.what() << std::endl;
		}
#endif

	}

	_file_chooser->set_select_multiple (allow_multiple_select);

	_file_chooser_connection.disconnect ();
	_file_chooser_connection = _file_chooser->signal_response ().connect (sigc::mem_fun (*this, &TriggerUI::sample_chosen));

	_file_chooser->present ();
}

void
TriggerUI::sample_chosen (int response)
{
	_file_chooser->hide ();

	switch (response) {
		case Gtk::RESPONSE_OK:
			break;
		default:
			return;
	}

	std::list<std::string> paths = _file_chooser->get_filenames ();

	for (std::list<std::string>::iterator s = paths.begin (); s != paths.end (); ++s) {
		trigger()->box().set_from_path (trigger()->index(), *s);
	}
}

/* ****************************************************************************/

bool
TriggerUI::namebox_button_press (GdkEventButton* ev)
{
	if (_renaming) {
		return false;
	}
	if ((ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) || Keyboard::is_edit_event (ev)) {
		start_rename ();
		return true;
	}
	return false;
}

bool
TriggerUI::start_rename ()
{
	if (_renaming) {
		return false;
	}
	assert (_entry_connections.empty ());

	GtkRequisition r (_name_label.size_request ());
	_nameentry.set_size_request (r.width, -1);
	_nameentry.set_text (trigger()->name ());
	_namebox.remove ();
	_namebox.add (_nameentry);
	_nameentry.show ();
	_nameentry.grab_focus ();
	_nameentry.add_modal_grab ();
	_renaming = true;

	_entry_connections.push_back (_nameentry.signal_changed().connect (sigc::mem_fun (*this, &TriggerUI::entry_changed)));
	_entry_connections.push_back (_nameentry.signal_activate().connect (sigc::mem_fun (*this, &TriggerUI::entry_activated)));
	_entry_connections.push_back (_nameentry.signal_key_press_event().connect (sigc::mem_fun (*this, &TriggerUI::entry_key_press), false));
	_entry_connections.push_back (_nameentry.signal_key_release_event().connect (sigc::mem_fun (*this, &TriggerUI::entry_key_release), false));
	_entry_connections.push_back (_nameentry.signal_button_press_event ().connect (sigc::mem_fun (*this, &TriggerUI::entry_button_press), false));
	_entry_connections.push_back (_nameentry.signal_focus_in_event ().connect (sigc::mem_fun (*this, &TriggerUI::entry_focus_in)));
	_entry_connections.push_back (_nameentry.signal_focus_out_event ().connect (sigc::mem_fun (*this, &TriggerUI::entry_focus_out)));
	return true;
}

void
TriggerUI::end_rename (bool ignore_change)
{
	if (!_renaming) {
		return;
	}
	std::string result = _nameentry.get_text ();
	disconnect_entry_signals ();
	_nameentry.remove_modal_grab ();
	_namebox.remove ();
	_namebox.add (_name_label);
	_name_label.show ();
	_renaming = false;

	if (ignore_change) {
		return;
	}

	trigger()->set_name (result);
}

void
TriggerUI::entry_changed ()
{
}

void
TriggerUI::entry_activated ()
{
	end_rename (false);
}

bool
TriggerUI::entry_focus_in (GdkEventFocus*)
{
	return false;
}

bool
TriggerUI::entry_focus_out (GdkEventFocus*)
{
	end_rename (false);
	return false;
}

bool
TriggerUI::entry_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		return false;
	} else if (Gtkmm2ext::event_inside_widget_window (_namebox, (GdkEvent*) ev)) {
		return false;
	} else {
		end_rename (false);
		return false;
	}
}

bool
TriggerUI::entry_key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_Escape:
			/* fallthrough */
		case GDK_ISO_Left_Tab:
			/* fallthrough */
		case GDK_Tab:
			/* fallthrough */
			return true;
		default:
			break;
	}
	return false;
}

bool
TriggerUI::entry_key_release (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_Escape:
			end_rename (true);
			return true;
		case GDK_ISO_Left_Tab:
			end_rename (false);
//			EditNextName (this, false); /* TODO */
			return true;
		case GDK_Tab:
			end_rename (false);
//			EditNextName (this, true); /* TODO */
			return true;
		default:
			break;
	}
	return false;
}

void
TriggerUI::disconnect_entry_signals ()
{
	for (std::list<sigc::connection>::iterator i = _entry_connections.begin(); i != _entry_connections.end(); ++i) {
		i->disconnect ();
	}
	_entry_connections.clear ();
}

/* ****************************************************************************/

void
TriggerUI::context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _context_menu;

	_context_menu   = new Menu;
	MenuList& items = _context_menu->items ();
	_context_menu->set_name ("ArdourContextMenu");

	Menu*     load_menu = manage (new Menu);
	MenuList& loitems (load_menu->items ());

	loitems.push_back (MenuElem (_("from file"), sigc::bind(sigc::mem_fun (*this, (&TriggerUI::choose_sample)), true)));
	loitems.push_back (MenuElem (_("from selection"), sigc::mem_fun (*this, &TriggerUI::set_from_selection)));

	items.push_back (MenuElem (_("Load..."), *load_menu));
#if DOUBLE_CLICK_IS_NOT_OBVIOUS_ENOUGH
	items.push_back (MenuElem (_("Edit..."), sigc::mem_fun (*this, &TriggerUI::edit_trigger)));
#endif
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &TriggerUI::choose_color)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear"), sigc::mem_fun (*this, &TriggerUI::clear_trigger)));


	_context_menu->popup (1, gtk_get_current_event_time ());
}

void
TriggerUI::launch_context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _launch_context_menu;

	_launch_context_menu   = new Menu;
	MenuList& items = _launch_context_menu->items ();
	_launch_context_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group lagroup;
	RadioMenuItem::Group qgroup;

	Menu*     launch_menu = manage (new Menu);
	MenuList& litems      = launch_menu->items ();

	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::OneShot), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::OneShot)));
	if (trigger ()->launch_style () == Trigger::OneShot) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::ReTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::ReTrigger)));
	if (trigger ()->launch_style () == Trigger::ReTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::Gate), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::Gate)));
	if (trigger ()->launch_style () == Trigger::Gate) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::Toggle), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::Toggle)));
	if (trigger ()->launch_style () == Trigger::Toggle) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::Repeat), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::Repeat)));
	if (trigger ()->launch_style () == Trigger::Repeat) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}

	Menu*     quant_menu = manage (new Menu);
	MenuList& qitems     = quant_menu->items ();

	BBT_Offset b;

#if TRIGGER_PAGE_GLOBAL_QUANTIZATION_IS_IMPLEMENTED
	bool      success;
	Beats      grid_beats (PublicEditor::instance ().get_grid_type_as_beats (success, timepos_t (0)));
	if (success) {
		b = BBT_Offset (0, grid_beats.get_beats (), grid_beats.get_ticks ());
		qitems.push_back (RadioMenuElem (qgroup, _("Main Grid"), sigc::bind(sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), b)));
		/* can't mark this active because the current trigger quant setting may just a specific setting below */
		/* XXX HOW TO GET THIS TO FOLLOW GRID CHANGES (which are GUI only) */
	}
#endif

	b = BBT_Offset (4, 0, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (2, 0, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (1, 0, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 2);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 4);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 8);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 16);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (-1, 0, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}

	items.push_back (MenuElem (_("Launch Style..."), *launch_menu));
	items.push_back (MenuElem (_("Quantization..."), *quant_menu));

	items.push_back (CheckMenuElem (_("Cue Isolate"), sigc::mem_fun (*this, &TriggerUI::toggle_trigger_isolated)));
	if (trigger ()->cue_isolated ()) {
		PBD::Unwinder<bool> uw (_ignore_menu_action, true);
		dynamic_cast<Gtk::CheckMenuItem*> (&items.back ())->set_active (true);
	}

	_launch_context_menu->popup (1, gtk_get_current_event_time ());
}

void
TriggerUI::follow_context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _follow_context_menu;

	_follow_context_menu   = new Menu;
	MenuList& items = _follow_context_menu->items ();
	_follow_context_menu->set_name ("ArdourContextMenu");

	_ignore_menu_action = true;

	items.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::None)), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_follow_action), FollowAction (FollowAction::None))));
	items.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::Stop)), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_follow_action), FollowAction (FollowAction::Stop))));
	items.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::Again)), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_follow_action), FollowAction (FollowAction::Again))));
	items.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::ForwardTrigger)), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_follow_action), FollowAction (FollowAction::ForwardTrigger))));
	items.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::ReverseTrigger)), sigc::bind(sigc::mem_fun (*this, &TriggerUI::set_follow_action), FollowAction (FollowAction::ReverseTrigger))));

	Menu*     jump_menu = manage (new Menu);
	MenuList& jitems      = jump_menu->items ();
	jitems.push_back (MenuElem (_("Multi..."), sigc::bind (sigc::mem_fun (*this, &TriggerUI::edit_jump), false)));
	for (int i = 0; i < default_triggers_per_box; i++) {
		FollowAction jump_fa = (FollowAction::JumpTrigger);
		jump_fa.targets.set(i);
		jitems.push_back (MenuElem (cue_marker_name (i), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action), jump_fa)));
	}

	items.push_back (MenuElem (_("Jump"), *jump_menu));

	_ignore_menu_action = false;

	_follow_context_menu->popup (1, gtk_get_current_event_time ());
}

void
TriggerUI::edit_jump (bool right_fa)
{
	if (_ignore_menu_action) {
		return;
	}

	TriggerJumpDialog* d = new TriggerJumpDialog (right_fa);
	d->set_trigger(tref);
	d->show_all ();

	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &TriggerUI::edit_jump_done), d));
}

void
TriggerUI::edit_jump_done (int r, TriggerJumpDialog* d)
{
	d->done (r);
	delete d;
}


void
TriggerUI::toggle_trigger_isolated ()
{
	if (_ignore_menu_action) {
		return;
	}

	trigger()->set_cue_isolated (!trigger()->cue_isolated ());
}

void
TriggerUI::clear_trigger ()
{
	trigger()->set_region (boost::shared_ptr<Region>());
}

void
TriggerUI::edit_trigger ()
{
	SlotPropertyWindow* tw      = static_cast<SlotPropertyWindow*> (trigger()->ui ());

	if (!tw) {
		tw = new SlotPropertyWindow (TriggerReference (trigger()->box(), trigger()->index()));
		trigger()->set_ui (tw);
	}

	tw->present ();
}

void
TriggerUI::set_follow_action (FollowAction const & fa)
{
	if (_ignore_menu_action) {
		return;
	}

	trigger()->set_follow_action0 (fa);
	trigger()->set_follow_action_probability (0);
}

void
TriggerUI::set_launch_style (Trigger::LaunchStyle ls)
{
	trigger()->set_launch_style (ls);
}

void
TriggerUI::set_quantization (Temporal::BBT_Offset const& q)
{
	trigger()->set_quantization (q);
}

void
TriggerUI::set_from_selection ()
{
	Selection&      selection (PublicEditor::instance ().get_selection ());
	RegionSelection rselection (selection.regions);

	if (rselection.empty ()) {
		/* XXX possible message about no selection ? */
		return;
	}

	int n = trigger()->index();

	for (RegionSelection::iterator r = rselection.begin (); r != rselection.end (); ++r) {
		trigger()->box().set_from_selection (n, (*r)->region ());
		++n;
	}
}

std::string
TriggerUI::launch_style_to_string (Trigger::LaunchStyle ls)
{
	switch (ls) {
	case Trigger::OneShot:
		return _("Trigger");
	case Trigger::ReTrigger:
		return _("Retrigger");
	case Trigger::Gate:
		return _("Gate");
	case Trigger::Toggle:
		return _("Toggle");
	case Trigger::Repeat:
		return _("Repeat");
	}
	/*NOTREACHED*/
	return std::string();
}

std::string
TriggerUI::quantize_length_to_string (BBT_Offset const & ql)
{
	if (ql < Temporal::BBT_Offset (0, 0, 0)) {
		/* negative quantization == do not quantize */
		return _("None");
	}

	if (ql == BBT_Offset (4, 0, 0)) {
		return _("4 Bars");
	} else if (ql == BBT_Offset (2, 0, 0)) {
		return _("2 Bars");
	} else if (ql == BBT_Offset (1, 0, 0)) {
		return _("1 Bar");
	} else if (ql == BBT_Offset (0, 1, 0)) {
		return _("1/4");
	} else if (ql == BBT_Offset (0, 2, 0)) {
		return _("1/2");
	} else if (ql == BBT_Offset (0, 4, 0)) {
		return _("Whole");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/2)) {
		return _("1/8");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/4)) {
		return _("1/16");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/8)) {
		return _("1/32");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/16)) {
		return _("1/64");
	} else {
		return "???";
	}
}

std::string
TriggerUI::follow_action_to_string (FollowAction const & fa, bool with_targets)
{
	switch (fa.type) {
	case FollowAction::None:
		return _("None");
	case FollowAction::Stop:
		return _("Stop");
	case FollowAction::Again:
		return _("Again");
	case FollowAction::ForwardTrigger:
		return _("Forward");
	case FollowAction::ReverseTrigger:
		return _("Reverse");
	case FollowAction::FirstTrigger:
		return _("First");
	case FollowAction::LastTrigger:
		return _("Last");
	case FollowAction::JumpTrigger:
		if (!with_targets) {
			return _("Jump");
		}
	}

	/* Jump case, and target(s) are desired */
	if ( fa.targets.count() == 1 ) {  //jump to a specific row
		for (int i = 0; i < default_triggers_per_box; i++) {
			if (fa.targets.test(i)) {
				return string_compose (_("Jump to: %1"), cue_marker_name (i));
			}
		}
	} else {
		return _("Jump: Multi");
	}

	/* should never be reached */
	return "";
}

std::string
TriggerUI::stretch_mode_to_string (Trigger::StretchMode sm)
{
	switch (sm) {
	case Trigger::Crisp:
		return _("Crisp");
	case Trigger::Mixed:
		return _("Mixed");
	case Trigger::Smooth:
		return _("Smooth");
	}
	/*NOTREACHED*/
	return std::string();
}

TriggerPtr
TriggerUI::trigger() const
{
	return tref.trigger();
}

void
TriggerUI::trigger_changed (PropertyChange const& what)
{
	on_trigger_changed(what);
}


void
TriggerUI::set_trigger (ARDOUR::TriggerReference tr)
{
	trigger_connections.drop_connections();
	trigger_swap_connection.disconnect();

	tref = tr;

	PropertyChange pc;

	pc.add (Properties::name);
	pc.add (Properties::color);
	pc.add (Properties::gain);
	pc.add (Properties::stretch_mode);
	pc.add (Properties::legato);
	pc.add (Properties::quantization);
	pc.add (Properties::launch_style);
	pc.add (Properties::use_follow_length);
	pc.add (Properties::follow_length);
	pc.add (Properties::follow_count);
	pc.add (Properties::follow_action0);
	pc.add (Properties::follow_action1);
	pc.add (Properties::velocity_effect);
	pc.add (Properties::follow_action_probability);

	trigger_changed (pc);

	trigger()->PropertyChanged.connect (trigger_connections, MISSING_INVALIDATOR, boost::bind (&TriggerUI::trigger_changed, this, _1), gui_context());
	tref.box->PropertyChanged.connect (trigger_connections, MISSING_INVALIDATOR, boost::bind (&TriggerUI::trigger_changed, this, _1), gui_context ());

	tref.box->TriggerSwapped.connect (trigger_swap_connection, MISSING_INVALIDATOR, boost::bind (&TriggerUI::trigger_swap, this, _1), gui_context ());

	on_trigger_set();  //derived classes can do initialization here
}
