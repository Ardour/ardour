/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cmath>

#include <gtkmm/listviewtext.h>
#include <gtkmm/stock.h>

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/unwind.h"

#include "gtkmm2ext/dndtreeview.h"

#include "widgets/tooltips.h"

#include "ardour/plugin_manager.h"
#include "ardour/region.h"
#include "ardour/region_fx_plugin.h"
#include "ardour/session.h"
#include "ardour/source.h"

#include "ardour_message.h"
#include "ardour_ui.h"
#include "clock_group.h"
#include "context_menu_helper.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "main_clock.h"
#include "mixer_ui.h"
#include "new_plugin_preset_dialog.h"
#include "region_editor.h"
#include "region_view.h"
#include "plugin_selector.h"
#include "plugin_window_proxy.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtkmm2ext;

RegionEditor::RegionEditor (Session* s, RegionView* rv)
	: ArdourDialog (_("Region"))
	, _table (9, 3)
	, _table_row (0)
	, _region (rv->region ())
	, name_label (_("Name:"))
	, audition_button (_("Audition"))
	, _clock_group (new ClockGroup)
	, position_clock (X_("regionposition"), true, "", true, false)
	, end_clock (X_("regionend"), true, "", true, false)
	, length_clock (X_("regionlength"), true, "", true, false, true)
	, sync_offset_relative_clock (X_("regionsyncoffsetrelative"), true, "", true, false)
	, sync_offset_absolute_clock (X_("regionsyncoffsetabsolute"), true, "", true, false)
	  /* XXX cannot file start yet */
	, start_clock (X_("regionstart"), true, "", false, false)
	, _region_fx_box (_region)
	, _sources (1)
{
	set_session (s);

	switch (_region->time_domain()) {
	case Temporal::AudioTime:
		/* XXX check length of region and choose samples or minsec */
		_clock_group->set_clock_mode (AudioClock::MinSec);
		break;
	default:
		_clock_group->set_clock_mode (AudioClock::BBT);
	}
	// ARDOUR_UI::instance()->primary_clock->mode_changed.connect (sigc::mem_fun (*this, &RegionEditor::set_clock_mode_from_primary));

	_clock_group->add (position_clock);
	_clock_group->add (end_clock);
	_clock_group->add (length_clock);
	_clock_group->add (sync_offset_relative_clock);
	_clock_group->add (sync_offset_absolute_clock);
	_clock_group->add (start_clock);

	position_clock.set_session (_session);
	end_clock.set_session (_session);
	length_clock.set_session (_session);
	sync_offset_relative_clock.set_session (_session);
	sync_offset_absolute_clock.set_session (_session);
	start_clock.set_session (_session);

	ArdourWidgets::set_tooltip (audition_button, _("audition this region"));

	audition_button.set_can_focus (false);

	audition_button.set_events (audition_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	name_entry.set_name ("RegionEditorEntry");
	name_label.set_name ("RegionEditorLabel");
	position_label.set_name ("RegionEditorLabel");
	position_label.set_text (_("Position:"));
	end_label.set_name ("RegionEditorLabel");
	end_label.set_text (_("End:"));
	length_label.set_name ("RegionEditorLabel");
	length_label.set_text (_("Length:"));
	sync_relative_label.set_name ("RegionEditorLabel");
	sync_relative_label.set_text (_("Sync point (relative to region):"));
	sync_absolute_label.set_name ("RegionEditorLabel");
	sync_absolute_label.set_text (_("Sync point (absolute):"));
	start_label.set_name ("RegionEditorLabel");
	start_label.set_text (_("File start:"));
	_sources_label.set_name ("RegionEditorLabel");
	region_fx_label.set_text (_("Region Effects"));
	region_fx_label.set_name ("RegionEditorLabel");

	if (_region->sources().size() > 1) {
		_sources_label.set_text (_("Sources:"));
	} else {
		_sources_label.set_text (_("Source:"));
	}

	_table.set_col_spacings (12);
	_table.set_row_spacings (6);
	_table.set_border_width (12);

	name_label.set_alignment (1, 0.5);
	position_label.set_alignment (1, 0.5);
	end_label.set_alignment (1, 0.5);
	length_label.set_alignment (1, 0.5);
	sync_relative_label.set_alignment (1, 0.5);
	sync_absolute_label.set_alignment (1, 0.5);
	start_label.set_alignment (1, 0.5);
	_sources_label.set_alignment (1, 0.5);

	Gtk::HBox* nb = Gtk::manage (new Gtk::HBox);
	nb->set_spacing (6);
	nb->pack_start (name_entry);
	nb->pack_start (audition_button, false, false);

	_table.attach (name_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (*nb, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (position_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (position_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (end_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (end_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (length_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (length_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (sync_relative_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (sync_offset_relative_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (sync_absolute_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (sync_offset_absolute_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (start_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (start_clock, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (_sources_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (_sources, 1, 2, _table_row, _table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++_table_row;

	_table.attach (region_fx_label, 2, 3, 0, 1, Gtk::FILL, Gtk::FILL);
	_table.attach (_region_fx_box,  2, 3, 1, _table_row + 2, Gtk::FILL, Gtk::FILL);

	get_vbox()->pack_start (_table, true, true);

	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_ACCEPT);

	set_name ("RegionEditorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_response().connect (sigc::mem_fun (*this, &RegionEditor::handle_response));

	set_title (string_compose (_("Region '%1'"), _region->name()));

	for (uint32_t i = 0; i < _region->sources().size(); ++i) {
		_sources.append (_region->source(i)->name());
	}

	_sources.set_headers_visible (false);
	Gtk::CellRendererText* t = dynamic_cast<Gtk::CellRendererText*> (_sources.get_column_cell_renderer(0));
	assert (t);
	t->property_ellipsize() = Pango::ELLIPSIZE_END;

	region_fx_label.set_no_show_all ();
	_region_fx_box.set_no_show_all ();

	show_all();

	name_changed ();

	PropertyChange change;

	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::sync_position);

	bounds_changed (change);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&RegionEditor::region_changed, this, _1), gui_context());
	_region->RegionFxChanged.connect (region_connection, invalidator (*this), boost::bind (&RegionEditor::region_fx_changed, this), gui_context ());

	spin_arrow_grab = false;

	/* for now only audio region effects are supported */
	if (std::dynamic_pointer_cast<AudioRegion> (_region)) {
		region_fx_label.show ();
		_region_fx_box.show ();
	}

	connect_editor_events ();
}

RegionEditor::~RegionEditor ()
{
	delete _clock_group;
}

void
RegionEditor::set_clock_mode_from_primary ()
{
	_clock_group->set_clock_mode (ARDOUR_UI::instance()->primary_clock->mode());
}

void
RegionEditor::region_changed (const PBD::PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}

	PropertyChange interesting_stuff;

	interesting_stuff.add (ARDOUR::Properties::length);
	interesting_stuff.add (ARDOUR::Properties::start);
	interesting_stuff.add (ARDOUR::Properties::sync_position);

	if (what_changed.contains (interesting_stuff)) {
		bounds_changed (what_changed);
	}
}

void
RegionEditor::region_fx_changed ()
{
	_region_fx_box.redisplay_plugins ();
}

gint
RegionEditor::bpressed (GdkEventButton* ev, Gtk::SpinButton* /*but*/, void (RegionEditor::*/*pmf*/)())
{
	switch (ev->button) {
	case 1:
	case 2:
	case 3:
		if (ev->type == GDK_BUTTON_PRESS) { /* no double clicks here */
			if (!spin_arrow_grab) {
				// GTK2FIX probably nuke the region editor
				// if ((ev->window == but->gobj()->panel)) {
				// spin_arrow_grab = true;
				// (this->*pmf)();
				// }
			}
		}
		break;
	default:
		break;
	}
	return FALSE;
}

gint
RegionEditor::breleased (GdkEventButton* /*ev*/, Gtk::SpinButton* /*but*/, void (RegionEditor::*pmf)())
{
	if (spin_arrow_grab) {
		(this->*pmf)();
		spin_arrow_grab = false;
	}
	return FALSE;
}

void
RegionEditor::connect_editor_events ()
{
	name_entry.signal_changed().connect (sigc::mem_fun(*this, &RegionEditor::name_entry_changed));

	position_clock.ValueChanged.connect (sigc::mem_fun(*this, &RegionEditor::position_clock_changed));
	end_clock.ValueChanged.connect (sigc::mem_fun(*this, &RegionEditor::end_clock_changed));
	length_clock.ValueChanged.connect (sigc::mem_fun(*this, &RegionEditor::length_clock_changed));
	sync_offset_absolute_clock.ValueChanged.connect (sigc::mem_fun (*this, &RegionEditor::sync_offset_absolute_clock_changed));
	sync_offset_relative_clock.ValueChanged.connect (sigc::mem_fun (*this, &RegionEditor::sync_offset_relative_clock_changed));

	audition_button.signal_toggled().connect (sigc::mem_fun(*this, &RegionEditor::audition_button_toggled));

	_session->AuditionActive.connect (audition_connection, invalidator (*this), boost::bind (&RegionEditor::audition_state_changed, this, _1), gui_context());
}

void
RegionEditor::position_clock_changed ()
{
	bool in_command = false;
	std::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		PublicEditor::instance().begin_reversible_command (_("change region start position"));
		in_command = true;

		_region->clear_changes ();
		_region->set_position (position_clock.last_when());
		_session->add_command(new StatefulDiffCommand (_region));
	}

	if (in_command) {
		PublicEditor::instance().commit_reversible_command ();
	}
}

void
RegionEditor::end_clock_changed ()
{
	bool in_command = false;
	std::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		PublicEditor::instance().begin_reversible_command (_("change region end position"));
		in_command = true;

		_region->clear_changes ();
		_region->trim_end (end_clock.last_when());
		_session->add_command(new StatefulDiffCommand (_region));
	}

	if (in_command) {
		PublicEditor::instance().commit_reversible_command ();
	}

	end_clock.set (_region->nt_last(), true);
}

void
RegionEditor::length_clock_changed ()
{
	timecnt_t len = length_clock.current_duration();
	bool in_command = false;
	std::shared_ptr<Playlist> pl = _region->playlist();

	if (pl) {
		PublicEditor::instance().begin_reversible_command (_("change region length"));
		in_command = true;

		_region->clear_changes ();
		/* new end is actually 1 domain unit before the clock duration
		 * would otherwise indicate
		 */
		const timepos_t new_end = (_region->position() + len).decrement ();
		_region->trim_end (new_end);
		_session->add_command(new StatefulDiffCommand (_region));
	}

	if (in_command) {
		PublicEditor::instance().commit_reversible_command ();
	}

	length_clock.set_duration (_region->length());
}

void
RegionEditor::audition_button_toggled ()
{
	if (audition_button.get_active()) {
		_session->audition_region (_region);
	} else {
		_session->cancel_audition ();
	}
}

void
RegionEditor::name_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		name_entry.set_text (_region->name());
	}
}

void
RegionEditor::bounds_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::length)) {
		position_clock.set (_region->position(), true);
		end_clock.set (_region->nt_last(), true);
		length_clock.set_duration (_region->length(), true);
	}

	if (what_changed.contains (ARDOUR::Properties::sync_position) || what_changed.contains (ARDOUR::Properties::length)) {
		int dir;
		timecnt_t off = _region->sync_offset (dir);
		if (dir == -1) {
			off = -off;
		}

		if (what_changed.contains (ARDOUR::Properties::sync_position)) {
			sync_offset_relative_clock.set_duration (off, true);
		}

		sync_offset_absolute_clock.set (_region->position () + off, true);
	}

	if (what_changed.contains (ARDOUR::Properties::start)) {
		start_clock.set (timepos_t (_region->start()), true);
	}
}

void
RegionEditor::activation ()
{

}

void
RegionEditor::name_entry_changed ()
{
	if (name_entry.get_text() != _region->name()) {
		_region->set_name (name_entry.get_text());
	}
}

void
RegionEditor::audition_state_changed (bool yn)
{
	ENSURE_GUI_THREAD (*this, &RegionEditor::audition_state_changed, yn)

	if (!yn) {
		audition_button.set_active (false);
	}
}

void
RegionEditor::sync_offset_absolute_clock_changed ()
{
	PublicEditor::instance().begin_reversible_command (_("change region sync point"));

	_region->clear_changes ();
	_region->set_sync_position (sync_offset_absolute_clock.last_when());
	_session->add_command (new StatefulDiffCommand (_region));

	PublicEditor::instance().commit_reversible_command ();
}

void
RegionEditor::sync_offset_relative_clock_changed ()
{
	PublicEditor::instance().begin_reversible_command (_("change region sync point"));

	_region->clear_changes ();
	_region->set_sync_position (sync_offset_relative_clock.last_when() + _region->position ());
	_session->add_command (new StatefulDiffCommand (_region));

	PublicEditor::instance().commit_reversible_command ();
}

bool
RegionEditor::on_delete_event (GdkEventAny*)
{
	PropertyChange change;

	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::sync_position);

	bounds_changed (change);

	return true;
}

void
RegionEditor::handle_response (int)
{
	hide ();
}

/* ****************************************************************************/

static std::list<Gtk::TargetEntry>
drop_targets ()
{
	std::list<Gtk::TargetEntry> tmp;
	tmp.push_back (Gtk::TargetEntry ("x-ardour/region-fx", Gtk::TARGET_SAME_APP));       // re-order
	tmp.push_back (Gtk::TargetEntry ("x-ardour/plugin.info", Gtk::TARGET_SAME_APP));     // from plugin-manager
	tmp.push_back (Gtk::TargetEntry ("x-ardour/plugin.favorite", Gtk::TARGET_SAME_APP)); // from sidebar
	return tmp;
}

static std::list<Gtk::TargetEntry>
drag_targets ()
{
	std::list<Gtk::TargetEntry> tmp;
	tmp.push_back (Gtk::TargetEntry ("x-ardour/region-fx", Gtk::TARGET_SAME_APP));     // re-order
	tmp.push_back (Gtk::TargetEntry ("x-ardour/plugin.preset", Gtk::TARGET_SAME_APP)); // to sidebar (optional preset)
	return tmp;
}

RegionEditor::RegionFxBox::RegionFxBox (std::shared_ptr<ARDOUR::Region> r)
	: _region (r)
	, _display (drop_targets ())
	, _no_redisplay (false)
	, _placement (-1)
{
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	_scroller.set_name ("ProcessorScroller");
	_scroller.add (_display);
	pack_start (_scroller, true, true);

	_display.set_can_focus ();
	_display.set_name ("ProcessorList");
	_display.set_data ("regionfxbox", this);
	_display.set_size_request (104, -1); // TODO UI scale
	_display.set_spacing (0);

	_display.ButtonPress.connect (sigc::mem_fun (*this, &RegionFxBox::fxe_button_press_event));
	_display.ButtonRelease.connect (sigc::mem_fun (*this, &RegionFxBox::fxe_button_release_event));

	_display.Reordered.connect (sigc::mem_fun (*this, &RegionFxBox::reordered));
	_display.DropFromAnotherBox.connect (sigc::mem_fun (*this, &RegionFxBox::object_drop));
	_display.DropFromExternal.connect (sigc::mem_fun (*this, &RegionFxBox::plugin_drop));

	_display.signal_key_press_event ().connect (sigc::mem_fun (*this, &RegionFxBox::on_key_press), false);

	_scroller.show ();
	_display.show ();

	redisplay_plugins ();
}

bool
RegionEditor::RegionFxBox::use_plugins (SelectedPlugins const& plugins)
{
	int errors = 0;
	{
		PBD::Unwinder<bool> uw (_no_redisplay, true);
		for (auto const& p : plugins) {
			std::shared_ptr<RegionFxPlugin> pos;
			if (_placement >= 0) {
				pos = _region->nth_plugin (_placement++);
			}
			if (!_region->add_plugin (std::shared_ptr<RegionFxPlugin> (new RegionFxPlugin (_region->session (), _region->time_domain (), p)), pos)) {
				++errors;
			}
		}
	}
	redisplay_plugins ();
	if (errors) {
		notify_plugin_load_fail (errors);
	}
	return false;
}

void
RegionEditor::RegionFxBox::redisplay_plugins ()
{
	if (_no_redisplay) {
		return;
	}
	_display.clear ();
	_region->foreach_plugin (sigc::mem_fun (*this, &RegionFxBox::add_fx_to_display));
}

void
RegionEditor::RegionFxBox::add_fx_to_display (std::weak_ptr<RegionFxPlugin> wfx)
{
	std::shared_ptr<RegionFxPlugin> fx (wfx.lock ());
	if (!fx) {
		return;
	}
	std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (_region);
	RegionFxEntry* e = new RegionFxEntry (fx, ar && ar->fade_before_fx ());
	_display.add_child (e, drag_targets ());
}

bool
RegionEditor::RegionFxBox::fxe_button_press_event (GdkEventButton* ev, RegionFxEntry* child)
{
	if (child) {
		std::weak_ptr<RegionFxPlugin> wfx (std::weak_ptr<RegionFxPlugin> (child->region_fx_plugin ()));

		if (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)) {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
				show_plugin_gui (wfx, false);
			} else {
				show_plugin_gui (wfx, true);
			}
			return true;
		}

		if (Keyboard::is_context_menu_event (ev)) {
			using namespace Gtk::Menu_Helpers;

			PluginSelector* ps = Mixer_UI::instance ()->plugin_selector ();
			ps->set_interested_object (*this);

			Gtk::Menu* m     = ARDOUR_UI_UTILS::shared_popup_menu ();
			MenuList&  items = m->items ();

			items.push_back (MenuElem (_("New Plugin")));
			Gtk::MenuItem& npm = items.back ();
			npm.set_submenu (*ps->plugin_menu ());

			std::shared_ptr<Plugin> plugin = child->region_fx_plugin ()->plugin ();

			items.push_back (SeparatorElem ());
			items.push_back (MenuElem (_("Edit..."), sigc::bind (sigc::mem_fun (*this, &RegionFxBox::show_plugin_gui), wfx, true)));
			items.back ().set_sensitive (plugin->has_editor ());
			items.push_back (MenuElem (_("Edit with generic controls..."), sigc::bind (sigc::mem_fun (*this, &RegionFxBox::show_plugin_gui), wfx, false)));

			Gtk::Menu* automation_menu = manage (new Gtk::Menu);
			MenuList&  ac_items (automation_menu->items ());

			for (size_t i = 0; i < plugin->parameter_count (); ++i) {
				if (!plugin->parameter_is_control (i) || !plugin->parameter_is_input (i)) {
					continue;
				}
				const Evoral::Parameter param (PluginAutomation, 0, i);
				std::string             label = plugin->describe_parameter (param);
				if (label == X_("latency") || label == X_("hidden")) {
					continue;
				}
				std::shared_ptr<ARDOUR::AutomationControl> c (std::dynamic_pointer_cast<ARDOUR::AutomationControl> (child->region_fx_plugin ()->control (param)));
				if (c && c->flags () & (Controllable::HiddenControl | Controllable::NotAutomatable)) {
					continue;
				}

				std::weak_ptr<ARDOUR::AutomationControl> wac (c);
				bool                                     play = c->automation_state () == Play;

				ac_items.push_back (CheckMenuElem (label));
				Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*> (&ac_items.back ());
				cmi->set_active (play);
				cmi->signal_activate ().connect ([wac, play] () {
					std::shared_ptr<ARDOUR::AutomationControl> ac = wac.lock ();
					if (ac) {
						ac->set_automation_state (play ? ARDOUR::Off : Play);
					}
				});
			}

			if (!ac_items.empty ()) {
				items.push_back (SeparatorElem ());
				items.push_back (MenuElem (_("Automation Enable"), *automation_menu));
				items.push_back (MenuElem (_("Clear All Automation"), sigc::bind (sigc::mem_fun (*this, &RegionFxBox::clear_automation), wfx)));
			} else {
				delete automation_menu;
			}

			items.push_back (SeparatorElem ());
			items.push_back (MenuElem (_("Delete"), sigc::bind (sigc::mem_fun (*this, &RegionFxBox::queue_delete_region_fx), wfx)));

			m->signal_unmap ().connect ([this, &npm] () { npm.remove_submenu (); _display.remove_placeholder (); });
			m->popup (ev->button, ev->time);

			int x, y;
			_display.get_pointer (x, y);
			_placement = _display.add_placeholder (y);
			return true;
		}
		return false;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		_placement = -1;
		using namespace Gtk::Menu_Helpers;

		PluginSelector* ps = Mixer_UI::instance ()->plugin_selector ();
		ps->set_interested_object (*this);

		Gtk::Menu* m     = ARDOUR_UI_UTILS::shared_popup_menu ();
		MenuList&  items = m->items ();

		items.push_back (MenuElem (_("New Plugin")));
		Gtk::MenuItem& npm = items.back ();
		npm.set_submenu (*ps->plugin_menu ());

		m->signal_unmap ().connect ([&npm] () { npm.remove_submenu (); });
		m->popup (ev->button, ev->time);
		return true;
	} else if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
		_placement         = -1;
		PluginSelector* ps = Mixer_UI::instance ()->plugin_selector ();
		ps->set_interested_object (*this);
		ps->show_manager ();
		return true;
	}

	return false;
}

bool
RegionEditor::RegionFxBox::fxe_button_release_event (GdkEventButton* ev, RegionFxEntry* child)
{
	if (child && Keyboard::is_delete_event (ev)) {
		queue_delete_region_fx (std::weak_ptr<RegionFxPlugin> (child->region_fx_plugin ()));
	}
	return false;
}

bool
RegionEditor::RegionFxBox::on_key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_KEY_Delete:
			break;
		case GDK_KEY_BackSpace:
			break;
		default:
			return false;
	}
	for (auto const& i : _display.selection (true)) {
		queue_delete_region_fx (std::weak_ptr<RegionFxPlugin> (i->region_fx_plugin ()));
	}
	return true;
}

void
RegionEditor::RegionFxBox::clear_automation (std::weak_ptr<ARDOUR::RegionFxPlugin> wfx)
{
	std::shared_ptr<RegionFxPlugin> fx (wfx.lock ());
	if (!fx) {
		return;
	}
	bool in_command = false;

	timepos_t tas ((samplepos_t)_region->length().samples());

	for (auto const& c : fx->controls ()) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (c.second);
		if (!ac) {
			continue;
		}
		std::shared_ptr<ARDOUR::AutomationList> alist = ac->alist ();
		if (!alist) {
			continue;
		}

		XMLNode& before (alist->get_state());

		alist->freeze ();
		alist->clear ();
		fx->set_default_automation (tas);
		alist->thaw ();
		alist->set_automation_state (ARDOUR::Off);

		if (!in_command) {
			_region->session ().begin_reversible_command (_("Clear region fx automation"));
			in_command = true;
		}
		_region->session ().add_command (new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
	}

	if (in_command) {
		_region->session ().commit_reversible_command ();
	}
}

void
RegionEditor::RegionFxBox::reordered ()
{
	Region::RegionFxList fxl;
	for (auto const& i : _display.children ()) {
		fxl.push_back (i->region_fx_plugin ());
	}
	_region->reorder_plugins (fxl);
}

void
RegionEditor::RegionFxBox::queue_delete_region_fx (std::weak_ptr<ARDOUR::RegionFxPlugin> wfx)
{
	Glib::signal_idle ().connect (sigc::bind (sigc::mem_fun (*this, &RegionFxBox::idle_delete_region_fx), wfx));
}

bool
RegionEditor::RegionFxBox::idle_delete_region_fx (std::weak_ptr<RegionFxPlugin> wfx)
{
	std::shared_ptr<RegionFxPlugin> fx (wfx.lock ());
	if (!fx) {
		return false;
	}
	_region->remove_plugin (fx);
	return false;
}

void
RegionEditor::RegionFxBox::notify_plugin_load_fail (uint32_t cnt)
{
	assert (cnt > 0);
	ArdourMessageDialog (_("Failed to load Region Effect Plugin"), false, Gtk::MESSAGE_ERROR).run ();
}

std::shared_ptr<RegionFxPlugin>
RegionEditor::RegionFxBox::find_drop_position (RegionFxEntry* pos)
{
	std::shared_ptr<RegionFxPlugin> rv;
	if (pos) {
		rv = pos->region_fx_plugin ();
		if (!rv) {
			rv = _display.children ().front ()->region_fx_plugin ();
		}
	}
	return rv;
}

void
RegionEditor::RegionFxBox::plugin_drop (Gtk::SelectionData const& data, RegionFxEntry* pos, Glib::RefPtr<Gdk::DragContext> const& context)
{
	uint32_t                        errors = 0;
	std::shared_ptr<RegionFxPlugin> at     = find_drop_position (pos);
	if (data.get_target () == "x-ardour/plugin.info") {
		const void*                                          d  = data.get_data ();
		const Gtkmm2ext::DnDTreeView<ARDOUR::PluginInfoPtr>* tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginInfoPtr>*> (d);
		PluginInfoList                                       nfos;
		Gtk::TreeView*                                       source;
		tv->get_object_drag_data (nfos, &source);
		for (auto const& i : nfos) {
			PluginPtr p = (i)->load (_region->session ());
			if (!_region->add_plugin (std::shared_ptr<RegionFxPlugin> (new RegionFxPlugin (_region->session (), _region->time_domain (), p)), at)) {
				++errors;
			}
		}
	} else if (data.get_target () == "x-ardour/plugin.favorite") {
		const void*                                            d  = data.get_data ();
		const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>* tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>*> (d);

		PluginPresetList nfos;
		Gtk::TreeView*   source;
		tv->get_object_drag_data (nfos, &source);
		for (auto const& i : nfos) {
			PluginPresetPtr ppp (i);
			PluginInfoPtr   pip = ppp->_pip;
			PluginPtr       p   = pip->load (_region->session ());
			if (!p) {
				continue;
			}
			if (ppp->_preset.valid) {
				p->load_preset (ppp->_preset);
			}
			if (!_region->add_plugin (std::shared_ptr<RegionFxPlugin> (new RegionFxPlugin (_region->session (), _region->time_domain (), p)), at)) {
				++errors;
			}
		}
	}
	if (errors) {
		notify_plugin_load_fail (errors);
	}
}

void
RegionEditor::RegionFxBox::delete_dragged_plugins (Region::RegionFxList const& fxl)
{
	{
		PBD::Unwinder<bool> uw (_no_redisplay, true);
		for (auto const& fx : fxl) {
			_region->remove_plugin (fx);
		}
	}
	redisplay_plugins ();
}

void
RegionEditor::RegionFxBox::object_drop (Gtkmm2ext::DnDVBox<RegionFxEntry>* source, RegionFxEntry* pos, Glib::RefPtr<Gdk::DragContext> const& context)
{
	if (Gdk::ACTION_LINK == context->get_selected_action ()) {
		std::list<RegionFxEntry*> children = source->selection ();
		assert (children.size () == 1);
		RegionFxEntry* other = *children.begin ();
		assert (other->can_copy_state (pos));
		std::shared_ptr<ARDOUR::RegionFxPlugin> othr = other->region_fx_plugin ();
		std::shared_ptr<ARDOUR::RegionFxPlugin> self = pos->region_fx_plugin ();

		PBD::ID  id    = self->id ();
		XMLNode& state = othr->get_state ();
		state.remove_property ("count");

		/* Controllable and automation IDs should not be copied */
		PBD::Stateful::ForceIDRegeneration force_ids;
		self->set_state (state, Stateful::current_state_version);
		self->update_id (id);
		return;
	}

	std::shared_ptr<RegionFxPlugin> at     = find_drop_position (pos);
	uint32_t                        errors = 0;

	Region::RegionFxList fxl;
	for (auto const& i : source->selection (true)) {
		fxl.push_back (i->region_fx_plugin ());
	}

	for (auto const& i : fxl) {
		XMLNode& state = i->get_state ();
		state.remove_property ("count");
		PBD::Stateful::ForceIDRegeneration force_ids;
		std::shared_ptr<RegionFxPlugin>    rfx (new RegionFxPlugin (_region->session (), _region->time_domain ()));
		rfx->set_state (state, Stateful::current_state_version);
		if (!_region->add_plugin (rfx, at)) {
			++errors;
		}
		delete &state;
	}

	if ((context->get_suggested_action () == Gdk::ACTION_MOVE) && source) {
		RegionFxBox* other = reinterpret_cast<RegionFxBox*> (source->get_data ("regionfxbox"));
		if (other) {
			other->delete_dragged_plugins (fxl);
		}
	}
	if (errors) {
		notify_plugin_load_fail (errors);
	}
}

void
RegionEditor::RegionFxBox::show_plugin_gui (std::weak_ptr<RegionFxPlugin> wfx, bool custom_ui)
{
	std::shared_ptr<RegionFxPlugin> rfx (wfx.lock ());
	if (!rfx) {
		return;
	}

	PluginWindowProxy* pwp;

	if (rfx->window_proxy ()) {
		pwp = dynamic_cast<PluginWindowProxy*> (rfx->window_proxy ());
	} else {
		pwp = new PluginWindowProxy (string_compose ("RFX-%1", rfx->id ()), _region->name (), rfx);

		const XMLNode* ui_xml = rfx->session ().extra_xml (X_("UI"));
		if (ui_xml) {
			pwp->set_state (*ui_xml, 0);
		}

		rfx->set_window_proxy (pwp);
		WM::Manager::instance ().register_window (pwp);
		RegionView* rv = PublicEditor::instance ().regionview_from_region (_region);
		rv->RegionViewGoingAway.connect_same_thread (*pwp, [pwp] (RegionView*) { pwp->hide (); });
	}

	pwp->set_custom_ui_mode (custom_ui);
	pwp->show_the_right_window ();

	Gtk::Window* tlw = PublicEditor::instance ().current_toplevel ();
	if (tlw) {
		pwp->set_transient_for (*tlw);
	}
}

/* ****************************************************************************/

RegionEditor::RegionFxEntry::RegionFxEntry (std::shared_ptr<RegionFxPlugin> rfx, bool pre)
	: _fx_btn (ArdourWidgets::ArdourButton::default_elements)
	, _rfx (rfx)
{
	_box.pack_start (_fx_btn, true, true);

	_plugin_preset_pointer = PluginPresetPtr (new PluginPreset (rfx->plugin ()->get_info ()));

	_fx_btn.set_fallthrough_to_parent (true);
	_fx_btn.set_text (name ());
	_fx_btn.set_active (true);
	if (pre) {
		_fx_btn.set_name ("processor prefader");
	} else {
		_fx_btn.set_name ("processor postfader");
	}

	if (rfx->plugin ()->has_editor ()) {
		set_tooltip (_fx_btn, string_compose (_("<b>%1</b>\nDouble-click to show GUI.\n%2+double-click to show generic GUI."), name (), Keyboard::secondary_modifier_name ()));
	} else {
		set_tooltip (_fx_btn, string_compose (_("<b>%1</b>\nDouble-click to show generic GUI."), name ()));
	}

	_box.show ();
	_fx_btn.show ();
}

std::string
RegionEditor::RegionFxEntry::name () const
{
	return _rfx->name ();
}

bool
RegionEditor::RegionFxEntry::can_copy_state (Gtkmm2ext::DnDVBoxChild* o) const
{
	RegionFxEntry* other = dynamic_cast<RegionFxEntry*> (o);
	if (!other) {
		return false;
	}
	std::shared_ptr<ARDOUR::RegionFxPlugin> othr = other->region_fx_plugin ();
	std::shared_ptr<ARDOUR::RegionFxPlugin> self = region_fx_plugin ();

	if (self->type () != othr->type ()) {
		return false;
	}
	std::shared_ptr<Plugin> my_p = self->plugin ();
	std::shared_ptr<Plugin> ot_p = othr->plugin ();
	return my_p->unique_id () == ot_p->unique_id ();
}

void
RegionEditor::RegionFxEntry::set_visual_state (Gtkmm2ext::VisualState s, bool yn)
{
	if (yn) {
		_fx_btn.set_visual_state (Gtkmm2ext::VisualState (_fx_btn.visual_state () | s));
	} else {
		_fx_btn.set_visual_state (Gtkmm2ext::VisualState (_fx_btn.visual_state () & ~s));
	}
}

bool
RegionEditor::RegionFxEntry::drag_data_get (Glib::RefPtr<Gdk::DragContext> const, Gtk::SelectionData& data)
{
	/* compare to ProcessorEntry::drag_data_get */
	if (data.get_target () != "x-ardour/plugin.preset") {
		return false;
	}

	std::shared_ptr<Plugin> plugin = _rfx->plugin ();
	assert (plugin);

	PluginManager& manager (PluginManager::instance ());
	bool           fav = manager.get_status (_plugin_preset_pointer->_pip) == PluginManager::Favorite;

	NewPluginPresetDialog d (plugin, string_compose (_("New Favorite Preset for \"%1\""), _plugin_preset_pointer->_pip->name), !fav);

	_plugin_preset_pointer->_preset.valid = false;

	switch (d.run ()) {
		default:
		case Gtk::RESPONSE_CANCEL:
			data.set (data.get_target (), 8, NULL, 0);
			return true;
			break;

		case Gtk::RESPONSE_NO:
			break;

		case Gtk::RESPONSE_ACCEPT:
			if (d.name ().empty ()) {
				break;
			}

			if (d.replace ()) {
				plugin->remove_preset (d.name ());
			}

			Plugin::PresetRecord const r = plugin->save_preset (d.name ());

			if (!r.uri.empty ()) {
				_plugin_preset_pointer->_preset.uri   = r.uri;
				_plugin_preset_pointer->_preset.label = r.label;
				_plugin_preset_pointer->_preset.user  = r.user;
				_plugin_preset_pointer->_preset.valid = r.valid;
			}
	}
	data.set (data.get_target (), 8, (const guchar*)&_plugin_preset_pointer, sizeof (PluginPresetPtr));
	return true;
}
