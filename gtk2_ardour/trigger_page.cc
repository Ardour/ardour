/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <list>

#include <ytkmm/label.h>

#include "pbd/properties.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/ardour_spacer.h"

#include "ardour/audio_track.h"
#include "ardour/audioregion.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/region_factory.h"
#include "ardour/profile.h"
#include "ardour/smf_source.h"
#include "ardour/stripable.h"

#include "actions.h"
#include "application_bar.h"
#include "ardour_ui.h"
#include "editor.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "pianoroll.h"
#include "timers.h"
#include "trigger_page.h"
#include "trigger_strip.h"
#include "triggerbox_ui.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

TriggerPage::TriggerPage ()
	: Tabbable (_("Cues"), X_("trigger"), NULL, true, Tabbable::PaneLayout (Tabbable::PaneRight | Tabbable::PaneBottom))
	, _cue_area_frame (0.5, 0, 1.0, 0)
	, _cue_box (16, 16 * TriggerBox::default_triggers_per_box)
	, _master_widget (16, 16)
	, _master (_master_widget.root ())
	, _show_bottom_pane (false)
	, _selection (*this, *this)
	, clip_editor_column (-1)
{
	load_bindings ();
	register_actions ();

	/* Match TriggerStrip::input_button height */
	ArdourButton* spacer = manage (new ArdourButton (ArdourButton::Text));
	spacer->set_name ("mixer strip button");
	spacer->set_sensitive (false);
	spacer->set_text (" ");

	/* Match TriggerStrip::_name_button height */
	ArdourButton* spacer2 = manage (new ArdourButton (ArdourButton::Text));
	spacer2->set_name ("mixer strip button");
	spacer2->set_sensitive (false);
	spacer2->set_text (" ");

	/* left-side, fixed-size cue-box */
	_cue_area_box.set_spacing (2);
	_cue_area_box.pack_start (*spacer, Gtk::PACK_SHRINK);
	_cue_area_box.pack_start (*spacer2, Gtk::PACK_SHRINK);
	_cue_area_box.pack_start (_cue_box, Gtk::PACK_SHRINK);
	_cue_area_box.pack_start (_master_widget, Gtk::PACK_SHRINK);

	/* left-side frame, same layout as TriggerStrip.
	 * use Alignment instead of Frame with SHADOW_IN (2px)
	 * +1px padding for _strip_scroller frame -> 3px top padding
	 */
	_cue_area_frame.set_padding (4, 1, 1, 1);
	_cue_area_frame.add (_cue_area_box);

	_strip_scroller.add (_strip_packer);
	_strip_scroller.set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_AUTOMATIC);

	/* Last item of strip packer, "+" background */
	_strip_packer.pack_end (_no_strips, true, true);
	_no_strips.set_can_focus ();
	_no_strips.add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	_no_strips.set_size_request (PX_SCALE (20), -1);
	_no_strips.signal_expose_event ().connect (sigc::bind (sigc::ptr_fun (&ArdourWidgets::ArdourIcon::expose_with_text), &_no_strips, ArdourWidgets::ArdourIcon::ShadedPlusSign,
			_("Drop a clip here\nto create a new Track")));
	_no_strips.signal_button_press_event ().connect (sigc::mem_fun (*this, &TriggerPage::no_strip_button_event));
	_no_strips.signal_button_release_event ().connect (sigc::mem_fun (*this, &TriggerPage::no_strip_button_event));
	_no_strips.signal_drag_motion ().connect (sigc::mem_fun (*this, &TriggerPage::no_strip_drag_motion));
	_no_strips.signal_drag_data_received ().connect (sigc::mem_fun (*this, &TriggerPage::no_strip_drag_data_received));

	std::vector<Gtk::TargetEntry> target_table;
	target_table.push_back (Gtk::TargetEntry ("x-ardour/region.pbdid", Gtk::TARGET_SAME_APP));
	target_table.push_back (Gtk::TargetEntry ("text/uri-list"));
	target_table.push_back (Gtk::TargetEntry ("text/plain"));
	target_table.push_back (Gtk::TargetEntry ("application/x-rootwin-drop"));
	_no_strips.drag_dest_set (target_table, DEST_DEFAULT_ALL, Gdk::ACTION_COPY);

	_strip_group_box.pack_start (_cue_area_frame, false, false);
	_strip_group_box.pack_start (_strip_scroller, true, true);

	/* sidebar */
	_sidebar_notebook.set_show_tabs (false);
	_sidebar_notebook.set_scrollable (true);
	_sidebar_notebook.popup_disable ();

	add_sidebar_page (_("Clips"), _("Clips"), _trigger_clip_picker);
	add_sidebar_page (_("Tracks"), _("Tracks & Busses"), _trigger_route_list.widget ());
	add_sidebar_page (_("Sources"), _("Sources"), _trigger_source_list.widget ());
	add_sidebar_page (_("Regions"), _("Regions"), _trigger_region_list.widget ());

	_sidebar_pager2.set_index (3);

	_midi_editor = new Pianoroll (X_("MIDICueEditor"));

	/* Bottom -- Properties of selected Slot/Region */

	table.set_homogeneous (false);
	table.set_spacings (8);  //match to slot_properties_box::set_spacings
	table.set_border_width (8);

	int col = 0;
	table.attach (_slot_prop_box, col, col + 1, 0, 1, Gtk::FILL, Gtk::SHRINK | Gtk::FILL);
	++col;
	table.attach (_audio_trig_box, col, col + 1, 0, 1, Gtk::FILL, Gtk::SHRINK | Gtk::FILL);
	clip_editor_column = ++col;

	table.set_no_show_all ();

	_parameter_box.pack_start (table);
	_parameter_box.show ();

	_sidebar_notebook.signal_switch_page().connect ([this](GtkNotebookPage*, guint page) {
			std::string label (_sidebar_notebook.get_tab_label_text (*_sidebar_notebook.get_nth_page (page)));
			_sidebar_pager1.set_active (label);
			_sidebar_pager2.set_active (label);
			//instant_save ();
			});

	_sidebar_pager1.set_name ("tab button");
	_sidebar_pager2.set_name ("tab button");

	HBox* tabbox = manage (new HBox (true));
	tabbox->set_spacing (3);
	tabbox->pack_start (_sidebar_pager1);
	tabbox->pack_start (_sidebar_pager2);

	_sidebar_vbox.pack_start (*tabbox, false, false, 2);
	_sidebar_vbox.pack_start (_sidebar_notebook);

	/* Top-level Layout */
	content_app_bar.add (_application_bar);
	content_main.add (_strip_group_box);
	content_att_bottom.add (_parameter_box);
	content_att_right.add (_sidebar_vbox);

	/* Show all */
	_strip_group_box.show ();
	_strip_scroller.show ();
	_strip_packer.show ();
	_cue_area_frame.show_all ();
	_trigger_clip_picker.show ();
	_no_strips.show ();
	_sidebar_vbox.show_all ();

	/* setup keybidings */
	contents().set_data ("ardour-bindings", bindings);

	/* subscribe to signals */
	Config->ParameterChanged.connect (*this, invalidator (*this), std::bind (&TriggerPage::parameter_changed, this, _1), gui_context ());
	PresentationInfo::Change.connect (*this, invalidator (*this), std::bind (&TriggerPage::pi_property_changed, this, _1), gui_context ());

	/* init */
	update_title ();
}

TriggerPage::~TriggerPage ()
{
}

Gtk::Window*
TriggerPage::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window ();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("TriggerWindow");
		ARDOUR_UI::instance ()->setup_toplevel_window (*win, _("Cues"), this);
		win->signal_event ().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), win));
		win->set_data ("ardour-bindings", bindings);
		update_title ();
#if 0 // TODO
		if (!win->get_focus()) {
			win->set_focus (scroller);
		}
#endif
	}

	contents ().show ();
	return win;
}

void
TriggerPage::showhide_att_bottom (bool yn)
{
	if (_show_bottom_pane == yn) {
		return;
	}

	_show_bottom_pane = yn;

	if (!_show_bottom_pane) {
		Tabbable::showhide_att_bottom (false);
	} else if (!Editor::instance ().get_selection ().triggers.empty ()) {
		Tabbable::showhide_att_bottom (true);
	}
}

XMLNode&
TriggerPage::get_state () const
{
	XMLNode* node = new XMLNode (X_("TriggerPage"));
	node->add_child_nocopy (Tabbable::get_state ());

	node->set_property (X_("triggerpage-sidebar-page"), _sidebar_notebook.get_current_page ());
	node->set_property (X_("triggerpage-sidebar-btn1"), _sidebar_pager1.index ());
	node->set_property (X_("triggerpage-sidebar-btn2"), _sidebar_pager2.index ());

	node->add_child_nocopy (_midi_editor->get_state());

	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Cues", "ToggleTriggerList");
	node->set_property ("show-trigger-list", act->get_active ());

	act = ActionManager::get_toggle_action ("Cues", "ToggleTriggerProps");
	node->set_property ("show-trigger-properties", act->get_active ());

	return *node;
}

void
TriggerPage::focus_on_clock()
{
	_application_bar.focus_on_clock();
}

int
TriggerPage::set_state (const XMLNode& node, int version)
{
	guint index;
	if (node.get_property (X_("triggerpage-sidebar-btn1"), index)) {
		_sidebar_pager1.set_index (index);
	}

	if (node.get_property (X_("triggerpage-sidebar-btn2"), index)) {
		_sidebar_pager2.set_index (index);
	}

	int32_t sidebar_page;
	if (node.get_property (X_("triggerpage-sidebar-page"), sidebar_page)) {
		_sidebar_notebook.set_current_page (sidebar_page);
	} else {
		sidebar_page = _sidebar_notebook.get_current_page ();
	}
	std::string label (_sidebar_notebook.get_tab_label_text (*_sidebar_notebook.get_nth_page (sidebar_page)));
	_sidebar_pager1.set_active (label);
	_sidebar_pager2.set_active (label);

	XMLNode* mn = node.child (_midi_editor->editor_name().c_str());
	if (mn) {
		_midi_editor->set_state (*mn, version);
	}

	bool yn = true;
	node.get_property ("show-trigger-list", yn);
	{
		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Cues"), X_("ToggleTriggerList"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}

	yn = true; // show properties by default
	node.get_property ("show-trigger-props", yn);
	{
		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Cues"), X_("ToggleTriggerProps"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}

	return Tabbable::set_state (node, version);
}

void
TriggerPage::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Cues"));
}

void
TriggerPage::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		_selection.clear ();
		return;
	}

	_application_bar.set_session (_session);
	_cue_box.set_session (s);
	_trigger_clip_picker.set_session (s);
	_master.set_session (s);
	_trigger_source_list.set_session (s);
	_trigger_region_list.set_session (s);
	_trigger_route_list.set_session (s);

	XMLNode* node = ARDOUR_UI::instance ()->trigger_page_settings ();
	set_state (*node, Stateful::loading_state_version);

	_session->DirtyChanged.connect (_session_connections, invalidator (*this), std::bind (&TriggerPage::update_title, this), gui_context ());
	_session->StateSaved.connect (_session_connections, invalidator (*this), std::bind (&TriggerPage::update_title, this), gui_context ());

	_session->RouteAdded.connect (_session_connections, invalidator (*this), std::bind (&TriggerPage::add_routes, this, _1), gui_context ());
	TriggerStrip::CatchDeletion.connect (*this, invalidator (*this), std::bind (&TriggerPage::remove_route, this, _1), gui_context ());

	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), std::bind (&TriggerPage::parameter_changed, this, _1), gui_context ());

	Editor::instance ().get_selection ().TriggersChanged.connect (sigc::mem_fun (*this, &TriggerPage::selection_changed));
	Trigger::TriggerArmChanged.connect (*this, invalidator (*this), std::bind (&TriggerPage::rec_enable_changed, this, _1), gui_context());

	initial_track_display ();

	_slot_prop_box.set_session (s);

	_audio_trig_box.set_session (s);

	_midi_editor->set_session (s);

	update_title ();
	start_updating ();
	selection_changed ();

	PBD::PropertyChange sc;
	sc.add (Properties::selected);
	_selection.presentation_info_changed (sc);
}

void
TriggerPage::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &TriggerPage::session_going_away);

	stop_updating ();

#if 0
	/* DropReferneces calls RouteUI::self_delete -> CatchDeletion .. */
	for (list<TriggerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		delete (*i);
	}
#endif
	_selection.clear ();
	_strips.clear ();

	SessionHandlePtr::session_going_away ();
	update_title ();
}

void
TriggerPage::update_title ()
{
	if (!own_window ()) {
		return;
	}

	if (_session) {
		string n;

		if (_session->snap_name () != _session->name ()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Cues");
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());

	} else {
		WindowTitle title (S_("Window|Cues"));
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());
	}
}

void
TriggerPage::add_sidebar_page (string const& label, string const& name, Gtk::Widget& widget)
{
	_sidebar_notebook.append_page (widget, name);
	using namespace Menu_Helpers;
	_sidebar_pager1.add_item (label, name, [this, &widget]() {_sidebar_notebook.set_current_page (_sidebar_notebook.page_num (widget)); });
	_sidebar_pager2.add_item (label, name, [this, &widget]() {_sidebar_notebook.set_current_page (_sidebar_notebook.page_num (widget)); });
}

void
TriggerPage::initial_track_display ()
{
	std::shared_ptr<RouteList> r = _session->get_tracks ();
	RouteList                    rl (*r);
	_strips.clear ();
	add_routes (rl);
}

void
TriggerPage::clear_selected_slot ()
{
	Selection& selection (Editor::instance ().get_selection ());
	TriggerSelection ts = selection.triggers;
	if (ts.empty ()) {
		return;
	}
	TriggerPtr trigger = ts.front ()->trigger ();
	trigger->set_region (std::shared_ptr<Region>());
}

void
TriggerPage::rec_enable_changed (Trigger const * trigger)
{
	assert (trigger);

	if (!trigger->armed()) {
		_midi_editor->trigger_rec_enable_change (*trigger);
		return;
	}

	/* hide everything */

	_slot_prop_box.hide ();
	_audio_trig_box.hide ();
	_midi_trig_box.hide ();
	_midi_editor->viewport().hide ();

	Tabbable::showhide_att_bottom (false);

	TriggerBox& box = trigger->box();
	TriggerReference ref (trigger->boxptr(), trigger->index());

	_slot_prop_box.set_slot (ref);
	_slot_prop_box.show ();

	if (box.data_type () == DataType::AUDIO) {
		if (trigger->the_region()) {
			_audio_trig_box.set_trigger (ref);
			_audio_trig_box.show ();
		}

	} else {

		_midi_trig_box.set_trigger (ref);
		_midi_trig_box.show ();

		_midi_editor->set (ref);
		_midi_editor->viewport().show ();
	}

	if (_show_bottom_pane) {
		Tabbable::showhide_att_bottom (true);
	}
}

void
TriggerPage::selection_changed ()
{
	Selection& selection (Editor::instance ().get_selection ());

	/* hide everything */

	_slot_prop_box.hide ();
	_audio_trig_box.hide ();
	_midi_trig_box.hide ();

	if (_midi_editor->contents().get_parent()) {
		_midi_editor->contents().get_parent()->remove (_midi_editor->contents());
	}

	Tabbable::showhide_att_bottom (false);

	if (selection.triggers.empty ()) {
		return;
	}

	TriggerSelection ts      = selection.triggers;
	TriggerEntry*    entry   = *ts.begin ();
	TriggerReference ref     = entry->trigger_reference ();
	TriggerPtr       trigger = entry->trigger ();
	std::shared_ptr<TriggerBox> box = ref.box();

	_slot_prop_box.set_slot (ref);
	_slot_prop_box.show ();

	if (box->data_type () == DataType::AUDIO) {

		if (trigger->the_region()) {
			_audio_trig_box.set_trigger (ref);
			_audio_trig_box.show ();
		}

	} else {

		_midi_trig_box.set_trigger (ref);
		_midi_trig_box.show ();

		_midi_editor->set (ref);

		table.attach (_midi_editor->contents(), clip_editor_column, clip_editor_column + 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL);
		_midi_editor->contents().show_all ();
	}

	table.show ();

	if (_show_bottom_pane) {
		Tabbable::showhide_att_bottom (true);
	}
}

void
TriggerPage::add_routes (RouteList& rl)
{
	rl.sort (Stripable::Sorter ());
	for (RouteList::iterator r = rl.begin (); r != rl.end (); ++r) {
		/* we're only interested in Tracks */
		if (!std::dynamic_pointer_cast<Track> (*r)) {
			continue;
		}
#if 0
		/* TODO, only subscribe to PropertyChanged, create (and destroy) TriggerStrip as needed.
		 * For now we just hide non trigger strips.
		 */
		if (!(*r)->presentation_info ().trigger_track ()) {
			continue;
		}
#endif

		if (!(*r)->triggerbox ()) {
			/* This Route has no TriggerBox -- and can never have one */
			continue;
		}

		TriggerStrip* ts = new TriggerStrip (_session, *r);
		_strips.push_back (ts);

		(*r)->presentation_info ().PropertyChanged.connect (*this, invalidator (*this), std::bind (&TriggerPage::stripable_property_changed, this, _1, std::weak_ptr<Stripable> (*r)), gui_context ());
		(*r)->PropertyChanged.connect (*this, invalidator (*this), std::bind (&TriggerPage::stripable_property_changed, this, _1, std::weak_ptr<Stripable> (*r)), gui_context ());
		ts->signal_button_release_event().connect (sigc::bind (sigc::mem_fun(*this, &TriggerPage::strip_button_release_event), ts));
	}
	redisplay_track_list ();
}

void
TriggerPage::remove_route (TriggerStrip* ra)
{
	if (!_session || _session->deletion_in_progress ()) {
		_strips.clear ();
		return;
	}
	list<TriggerStrip*>::iterator i = find (_strips.begin (), _strips.end (), ra);
	if (i != _strips.end ()) {
		_strip_packer.remove (**i);
		_strips.erase (i);
	}
	redisplay_track_list ();
}

struct TriggerStripSorter {
	bool operator() (const TriggerStrip* ts_a, const TriggerStrip* ts_b)
	{
		std::shared_ptr<ARDOUR::Stripable> const& a = ts_a->stripable ();
		std::shared_ptr<ARDOUR::Stripable> const& b = ts_b->stripable ();
		return ARDOUR::Stripable::Sorter () (a, b);
	}
};

void
TriggerPage::redisplay_track_list ()
{
	_strips.sort (TriggerStripSorter ());
	PresentationInfo::ChangeSuspender cs;

	for (list<TriggerStrip*>::iterator i = _strips.begin (); i != _strips.end (); ++i) {
		TriggerStrip*                strip = *i;
		std::shared_ptr<Stripable> s     = strip->stripable ();
		std::shared_ptr<Route>     route = std::dynamic_pointer_cast<Route> (s);

		bool hidden = s->presentation_info ().hidden ();

		if (s->is_selected ()) {
			_selection.add (*i);
		} else {
			_selection.remove (*i);
		}

		if (!(s)->presentation_info ().trigger_track ()) {
			hidden = true;
		}
		assert (route && route->triggerbox ());
		if (!route || !route->triggerbox ()) {
			hidden = true;
		}

		if (hidden && strip->get_parent ()) {
			/* if packed, remove it */
			_strip_packer.remove (*strip);
		} else if (!hidden && strip->get_parent ()) {
			/* already packed, put it at the end */
			_strip_packer.reorder_child (*strip, -1); /* put at end */
		} else if (!hidden) {
			_strip_packer.pack_start (*strip, false, false);
		}
	}
}

AxisView*
TriggerPage::axis_view_by_stripable (std::shared_ptr<Stripable> s) const
{
	for (list<TriggerStrip*>::const_iterator i = _strips.begin (); i != _strips.end (); ++i) {
		TriggerStrip* strip = *i;
		if (s == strip->stripable ()) {
			return strip;
		}
	}
	return 0;
}

AxisView*
TriggerPage::axis_view_by_control (std::shared_ptr<AutomationControl> c) const
{
	return 0;
}

void
TriggerPage::parameter_changed (string const& p)
{
}

void
TriggerPage::pi_property_changed (PBD::PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::selected)) {
		_selection.presentation_info_changed (what_changed);
	}
	if (what_changed.contains (ARDOUR::Properties::order)) {
		redisplay_track_list ();
	}
}

void
TriggerPage::stripable_property_changed (PBD::PropertyChange const& what_changed, std::weak_ptr<Stripable> ws)
{
	if (what_changed.contains (ARDOUR::Properties::trigger_track)) {
#if 0
		std::shared_ptr<Stripable> s = ws.lock ();
		/* TODO: find trigger-strip for given stripable, delete *it; */
#else
		/* For now we just hide it */
		redisplay_track_list ();
		return;
#endif
	}
	if (what_changed.contains (ARDOUR::Properties::hidden)) {
		redisplay_track_list ();
	}
}

bool
TriggerPage::strip_button_release_event (GdkEventButton *ev, TriggerStrip *strip)
{
	if (ev->button != 1) {
		return false;
	}

	if (_selection.selected (strip)) {
		/* primary-click: toggle selection state of strip */
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			_selection.remove (strip, true);
		} else if (_selection.axes.size() > 1) {
			/* de-select others */
			_selection.set (strip);
		}
		PublicEditor& pe = PublicEditor::instance();
		TimeAxisView* tav = pe.time_axis_view_from_stripable (strip->stripable());
		if (tav) {
			pe.set_selected_mixer_strip (*tav);
		}
	} else {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			_selection.add (strip, true);
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier)) {

			/* extend selection */

			vector<TriggerStrip*> tmp;
			bool accumulate = false;
			bool found_another = false;

			_strips.sort (TriggerStripSorter ());

			for (list<TriggerStrip*>::iterator i = _strips.begin (); i != _strips.end (); ++i) {
				TriggerStrip* ts = *i;

				if (ts == strip) {
					/* hit clicked strip, start accumulating till we hit the first
						 selected strip
						 */
					if (accumulate) {
						/* done */
						break;
					} else {
						accumulate = true;
					}
				} else if (_selection.selected (ts)) {
					/* hit selected strip. if currently accumulating others,
						 we're done. if not accumulating others, start doing so.
						 */
					found_another = true;
					if (accumulate) {
						/* done */
						break;
					} else {
						accumulate = true;
					}
				} else {
					if (accumulate) {
						tmp.push_back (ts);
					}
				}
			}

			tmp.push_back (strip);

			if (found_another) {
				PresentationInfo::ChangeSuspender cs;
				for (vector<TriggerStrip*>::iterator i = tmp.begin(); i != tmp.end(); ++i) {
					_selection.add (*i, true);
				}
			} else {
				_selection.set (strip);  //user wants to start a range selection, but there aren't any others selected yet
			}
		} else {
			_selection.set (strip);
		}
	}
	return true;
}

bool
TriggerPage::no_strip_button_event (GdkEventButton* ev)
{
	if ((ev->type == GDK_2BUTTON_PRESS && ev->button == 1) || (ev->type == GDK_BUTTON_RELEASE && Keyboard::is_context_menu_event (ev))) {
		ARDOUR_UI::instance ()->add_route ();
		return true;
	}
	return false;
}

bool
TriggerPage::no_strip_drag_motion (Glib::RefPtr<Gdk::DragContext> const& context, int, int y, guint time)
{
	context->drag_status (Gdk::ACTION_COPY, time);
	return true;
}

void
TriggerPage::no_strip_drag_data_received (Glib::RefPtr<Gdk::DragContext> const& context, int /*x*/, int y, Gtk::SelectionData const& data, guint /*info*/, guint time)
{
	if (data.get_target () == "x-ardour/region.pbdid") {
		PBD::ID rid (data.get_data_as_string ());
		std::shared_ptr<Region> region = RegionFactory::region_by_id (rid);
		std::shared_ptr<TriggerBox> triggerbox;

		if (std::dynamic_pointer_cast<AudioRegion> (region)) {
			uint32_t output_chan = region->sources().size();
			if ((Config->get_output_auto_connect() & AutoConnectMaster) && session()->master_out()) {
				output_chan =  session()->master_out()->n_inputs().n_audio();
			}
			std::list<std::shared_ptr<AudioTrack> > audio_tracks;
			audio_tracks = session()->new_audio_track (region->sources().size(), output_chan, 0, 1, region->name(), PresentationInfo::max_order, Normal, true, true);
			if (!audio_tracks.empty()) {
				triggerbox = audio_tracks.front()->triggerbox ();
			}
		} else if (std::dynamic_pointer_cast<MidiRegion> (region)) {
			ChanCount one_midi_port (DataType::MIDI, 1);
			list<std::shared_ptr<MidiTrack> > midi_tracks;
			midi_tracks = session()->new_midi_track (one_midi_port, one_midi_port,
			                                         Config->get_strict_io () || Profile->get_mixbus (),
			                                         std::shared_ptr<ARDOUR::PluginInfo>(),
			                                         (ARDOUR::Plugin::PresetRecord*) 0,
			                                         (ARDOUR::RouteGroup*) 0, 1, region->name(), PresentationInfo::max_order, Normal, true, true);
			if (!midi_tracks.empty()) {
				triggerbox = midi_tracks.front()->triggerbox ();
			}
		}

		if (!triggerbox) {
			context->drag_finish (false, false, time);
			return;
		}

		// XXX: check does the region need to be copied?
		std::shared_ptr<Region> region_copy = RegionFactory::create (region, true);
		triggerbox->set_from_selection (0, region_copy);

		context->drag_finish (true, false, time);
		return;
	}

	std::vector<std::string> paths;
	if (ARDOUR_UI_UTILS::convert_drop_to_paths (paths, data)) {
#ifdef __APPLE__
		/* We are not allowed to call recursive main event loops from within
		 * the main event loop with GTK/Quartz. Since import/embed wants
		 * to push up a progress dialog, defer all this till we go idle.
		 */
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &TriggerPage::idle_drop_paths), paths));
#else
		drop_paths_part_two (paths);
#endif
	}
	context->drag_finish (true, false, time);
}

void
TriggerPage::drop_paths_part_two (std::vector<std::string> paths)
{
	/* compare to Editor::drop_paths_part_two */
	std::vector<string> midi_paths;
	std::vector<string> audio_paths;
	for (std::vector<std::string>::iterator s = paths.begin (); s != paths.end (); ++s) {
		if (SMFSource::safe_midi_file_extension (*s)) {
			midi_paths.push_back (*s);
		} else {
			audio_paths.push_back (*s);
		}
	}
	timepos_t pos (0);
	Editing::ImportDisposition disposition = Editing::ImportSerializeFiles; // or Editing::ImportDistinctFiles // TODO use drop modifier? config?
	PublicEditor::instance().do_import (midi_paths, disposition, Editing::ImportAsTrigger, SrcBest, SMFFileAndTrackName, SMFTempoIgnore, pos, _trigger_clip_picker.instrument_plugin ());
	PublicEditor::instance().do_import (audio_paths, disposition, Editing::ImportAsTrigger, SrcBest, SMFFileAndTrackName, SMFTempoIgnore, pos);
}

bool
TriggerPage::idle_drop_paths (std::vector<std::string> paths)
{
	drop_paths_part_two (paths);
	return false;
}

gint
TriggerPage::start_updating ()
{
	_fast_screen_update_connection = Timers::super_rapid_connect (sigc::mem_fun (*this, &TriggerPage::fast_update_strips));
	return 0;
}

gint
TriggerPage::stop_updating ()
{
	_fast_screen_update_connection.disconnect ();
	return 0;
}

void
TriggerPage::fast_update_strips ()
{
	if (contents().get_mapped () && _session) {
		for (list<TriggerStrip*>::iterator i = _strips.begin (); i != _strips.end (); ++i) {
			(*i)->fast_update ();
		}
	}
}

void
TriggerPage::register_actions ()
{
	Glib::RefPtr<ActionGroup> trigger_actions = ActionManager::create_action_group (bindings, X_("Cues"));

	/* attachments visibility (editor-mixer-strip, bottom properties, sidebar list) */

	Glib::RefPtr<Action> act = ActionManager::register_toggle_action (trigger_actions, "ToggleTriggerList", _("(Cue Page) Show Sidebar List"), sigc::mem_fun (*this, &Tabbable::att_right_button_toggled));
	right_attachment_button.set_related_action (act);

	act = ActionManager::register_toggle_action (trigger_actions, "ToggleTriggerProps", _("(Cue Page) Show Properties Bottom"), sigc::mem_fun (*this, &Tabbable::att_bottom_button_toggled));
	bottom_attachment_button.set_related_action (act);

	ActionManager::register_action (trigger_actions, "clear-trigger-slot", _("Clear Selected Slot"), sigc::mem_fun (*this, &TriggerPage::clear_selected_slot));
	ActionManager::register_action (trigger_actions, "clear-trigger-slot-alt", _("Clear Selected Slot"), sigc::mem_fun (*this, &TriggerPage::clear_selected_slot));

	for (int32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {
		const std::string action_name  = string_compose ("trigger-cue-%1", n);
		const std::string display_name = string_compose (_("Trigger Cue %1"), cue_marker_name (n));

		ActionManager::register_action (trigger_actions, action_name.c_str (), display_name.c_str (), sigc::bind (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::trigger_cue_row), n));
	}

	for (int32_t c = 0; c < 16; ++c) {
		for (int32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {
			const std::string action_name  = string_compose ("trigger-slot-%1-%2", c, n);
			const std::string display_name = string_compose (_("Trigger Slot %1/%2"), c, cue_marker_name (n));

			ActionManager::register_action (trigger_actions, action_name.c_str (), display_name.c_str (), sigc::bind (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::trigger_slot), c, n));
		}

		ActionManager::register_action (trigger_actions, string_compose ("stop-cues-%1-now", c).c_str(), string_compose (_("Stop Cues %1"), c).c_str(), sigc::bind (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::stop_cues), c, true));
		ActionManager::register_action (trigger_actions, string_compose ("stop-cues-%1-soon", c).c_str(), string_compose (_("Stop Cues %1"), c).c_str(), sigc::bind (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::stop_cues), c, false));
	}

	ActionManager::register_action (trigger_actions, X_("stop-all-cues-now"), _("Stop all cues now"), sigc::bind (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::stop_all_cues), true));
	ActionManager::register_action (trigger_actions, X_("stop-all-cues-soon"), _("Stop all cues soon"), sigc::bind (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::stop_all_cues), false));
}
