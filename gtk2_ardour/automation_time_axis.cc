/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <utility>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/utils.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "pbd/error.h"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

#include "ardour/automation_control.h"
#include "ardour/event_type_map.h"
#include "ardour/parameter_types.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "canvas/debug.h"

#include "automation_time_axis.h"
#include "automation_streamview.h"
#include "gui_thread.h"
#include "route_time_axis.h"
#include "automation_line.h"
#include "paste_context.h"
#include "public_editor.h"
#include "selection.h"
#include "tooltips.h"
#include "rgb_macros.h"
#include "point_selection.h"
#include "control_point.h"
#include "utils.h"
#include "item_counts.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

Pango::FontDescription AutomationTimeAxisView::name_font;
bool AutomationTimeAxisView::have_name_font = false;


/** \a a the automatable object this time axis is to display data for.
 * For route/track automation (e.g. gain) pass the route for both \r and \a.
 * For route child (e.g. plugin) automation, pass the child for \a.
 * For region automation (e.g. MIDI CC), pass null for \a.
 */
AutomationTimeAxisView::AutomationTimeAxisView (
	Session* s,
	boost::shared_ptr<Route> r,
	boost::shared_ptr<Automatable> a,
	boost::shared_ptr<AutomationControl> c,
	Evoral::Parameter p,
	PublicEditor& e,
	TimeAxisView& parent,
	bool show_regions,
	ArdourCanvas::Canvas& canvas,
	const string & nom,
	const string & nomparent
	)
	: SessionHandlePtr (s)
	, TimeAxisView (s, e, &parent, canvas)
	, _route (r)
	, _control (c)
	, _automatable (a)
	, _parameter (p)
	, _base_rect (new ArdourCanvas::Rectangle (_canvas_display))
	, _view (show_regions ? new AutomationStreamView (*this) : 0)
	, auto_dropdown ()
	, _show_regions (show_regions)
{
	//concatenate plugin name and param name into the tooltip
	string tipname = nomparent;
	if (!tipname.empty()) {
		tipname += ": ";
	}
	tipname += nom;
	set_tooltip(controls_ebox, tipname);

	//plugin name and param name appear on 2 separate lines in the track header
	tipname = nomparent;
	if (!tipname.empty()) {
		tipname += "\n";
	}
	tipname += nom;
	_name = tipname;

	CANVAS_DEBUG_NAME (_canvas_display, string_compose ("main for auto %2/%1", _name, r->name()));
	CANVAS_DEBUG_NAME (selection_group, string_compose ("selections for auto %2/%1", _name, r->name()));
	CANVAS_DEBUG_NAME (_ghost_group, string_compose ("ghosts for auto %2/%1", _name, r->name()));

	if (!have_name_font) {
		name_font = get_font_for_style (X_("AutomationTrackName"));
		have_name_font = true;
	}

	if (_automatable && _control) {
		_controller = AutomationController::create (_automatable, _control->parameter(), _control->desc(), _control);
	}

	const std::string fill_color_name = (dynamic_cast<MidiTimeAxisView*>(&parent)
	                                     ? "midi automation track fill"
	                                     : "audio automation track fill");

	auto_off_item = 0;
	auto_touch_item = 0;
	auto_write_item = 0;
	auto_play_item = 0;
	mode_discrete_item = 0;
	mode_line_item = 0;

	ignore_state_request = false;
	first_call_to_set_height = true;

	CANVAS_DEBUG_NAME (_base_rect, string_compose ("base rect for %1", _name));
	_base_rect->set_x1 (ArdourCanvas::COORD_MAX);
	_base_rect->set_outline (false);
	_base_rect->set_fill_color (UIConfiguration::instance().color_mod (fill_color_name, "automation track fill"));
	_base_rect->set_data ("trackview", this);
	_base_rect->Event.connect (sigc::bind (sigc::mem_fun (_editor, &PublicEditor::canvas_automation_track_event), _base_rect, this));
	if (!a) {
		_base_rect->lower_to_bottom();
	}

	using namespace Menu_Helpers;

	auto_dropdown.AddMenuElem (MenuElem (S_("Automation|Manual"), sigc::bind (sigc::mem_fun(*this,
						&AutomationTimeAxisView::set_automation_state), (AutoState) ARDOUR::Off)));
	auto_dropdown.AddMenuElem (MenuElem (_("Play"), sigc::bind (sigc::mem_fun(*this,
						&AutomationTimeAxisView::set_automation_state), (AutoState) Play)));
	auto_dropdown.AddMenuElem (MenuElem (_("Write"), sigc::bind (sigc::mem_fun(*this,
						&AutomationTimeAxisView::set_automation_state), (AutoState) Write)));
	auto_dropdown.AddMenuElem (MenuElem (_("Touch"), sigc::bind (sigc::mem_fun(*this,
						&AutomationTimeAxisView::set_automation_state), (AutoState) Touch)));

	/* XXX translators: use a string here that will be at least as long
	   as the longest automation label (see ::automation_state_changed()
	   below). be sure to include a descender.
	*/
	auto_dropdown.set_sizing_text(_("Mgnual"));

	hide_button.set_icon (ArdourIcon::CloseCross);
	hide_button.set_tweaks(ArdourButton::TrackHeader);

	auto_dropdown.set_name ("route button");
	hide_button.set_name ("route button");

	auto_dropdown.unset_flags (Gtk::CAN_FOCUS);
	hide_button.unset_flags (Gtk::CAN_FOCUS);

	controls_table.set_no_show_all();

	set_tooltip(auto_dropdown, _("automation state"));
	set_tooltip(hide_button, _("hide track"));

	const string str = gui_property ("height");
	if (!str.empty()) {
		set_height (atoi (str));
	} else {
		set_height (preset_height (HeightNormal));
	}

	//name label isn't editable on an automation track; remove the tooltip
	set_tooltip (name_label, X_(""));

	/* repack the name label, which TimeAxisView has already attached to
	 * the controls_table
	 */

	if (name_label.get_parent()) {
		name_label.get_parent()->remove (name_label);
	}

	name_label.set_text (_name);
	name_label.set_alignment (Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	name_label.set_name (X_("TrackParameterName"));
	name_label.set_ellipsize (Pango::ELLIPSIZE_END);

	/* add the buttons */
	controls_table.set_border_width (1);
	controls_table.attach (hide_button, 1, 2, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
	controls_table.attach (name_label,  2, 3, 1, 3, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 2, 0);
	controls_table.attach (auto_dropdown, 3, 4, 2, 3, Gtk::SHRINK, Gtk::SHRINK, 0, 0);

	Gtk::DrawingArea *blank0 = manage (new Gtk::DrawingArea());
	Gtk::DrawingArea *blank1 = manage (new Gtk::DrawingArea());

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&parent);
	// TODO use rtv->controls_base_unselected_name
	// subscribe to route_active_changed, ...
	if (rtv && rtv->is_audio_track()) {
		blank0->set_name ("AudioTrackControlsBaseUnselected");
	} else if (rtv && rtv->is_midi_track()) {
		blank0->set_name ("MidiTrackControlsBaseUnselected");
	} else if (rtv) {
		blank0->set_name ("AudioBusControlsBaseUnselected");
	} else {
		blank0->set_name ("UnknownControlsBaseUnselected");
	}
	blank0->set_size_request (-1, -1);
	blank1->set_size_request (1, 0);
	VSeparator* separator = manage (new VSeparator());
	separator->set_name("TrackSeparator");
	separator->set_size_request (1, -1);

	controls_button_size_group->add_widget(hide_button);
	controls_button_size_group->add_widget(*blank0);

	time_axis_hbox.pack_start (*blank0, false, false);
	time_axis_hbox.pack_start (*separator, false, false);
	time_axis_hbox.reorder_child (*blank0, 0);
	time_axis_hbox.reorder_child (*separator, 1);
	time_axis_hbox.reorder_child (time_axis_vbox, 2);

	if (!ARDOUR::Profile->get_mixbus() ) {
		time_axis_hbox.pack_start (*blank1, false, false);
	}

	blank0->show();
	separator->show();
	name_label.show ();
	hide_button.show ();

	if (_controller) {
		_controller->disable_vertical_scroll ();
		controls_table.attach (*_controller.get(), 2, 4, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	}

	controls_table.show_all ();

	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &AutomationTimeAxisView::hide_clicked));

	controls_base_selected_name = X_("AutomationTrackControlsBaseSelected");
	controls_base_unselected_name = X_("AutomationTrackControlsBase");

	controls_ebox.set_name (controls_base_unselected_name);
	time_axis_frame.set_name (controls_base_unselected_name);

	/* ask for notifications of any new RegionViews */
	if (show_regions) {

		if (_view) {
			_view->attach ();
		}

	} else {
		/* no regions, just a single line for the entire track (e.g. bus gain) */

		assert (_control);

		boost::shared_ptr<AutomationLine> line (
			new AutomationLine (
				ARDOUR::EventTypeMap::instance().to_symbol(_parameter),
				*this,
				*_canvas_display,
				_control->alist(),
				_control->desc()
				)
			);

		line->set_line_color (UIConfiguration::instance().color ("processor automation line"));
		line->set_fill (true);
		line->queue_reset ();
		add_line (line);
	}

	/* make sure labels etc. are correct */

	automation_state_changed ();
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &AutomationTimeAxisView::color_handler));

	_route->DropReferences.connect (
		_route_connections, invalidator (*this), boost::bind (&AutomationTimeAxisView::route_going_away, this), gui_context ()
		);
}

AutomationTimeAxisView::~AutomationTimeAxisView ()
{
	cleanup_gui_properties ();
	delete _view;
}

void
AutomationTimeAxisView::route_going_away ()
{
	_route.reset ();
}

void
AutomationTimeAxisView::set_automation_state (AutoState state)
{
	if (ignore_state_request) {
		return;
	}

	if (_automatable) {
		_automatable->set_parameter_automation_state (_parameter, state);
	}

	if (_view) {
		_view->set_automation_state (state);

		/* AutomationStreamViews don't signal when their automation state changes, so handle
		   our updates `manually'.
		*/
		automation_state_changed ();
	}
}

void
AutomationTimeAxisView::automation_state_changed ()
{
	AutoState state;

	/* update button label */

	if (_view) {
		state = _view->automation_state ();
	} else if (_line) {
		assert (_control);
		state = _control->alist()->automation_state ();
	} else {
		state = ARDOUR::Off;
	}

	switch (state & (ARDOUR::Off|Play|Touch|Write)) {
	case ARDOUR::Off:
		auto_dropdown.set_text (S_("Automation|Manual"));
		if (auto_off_item) {
			ignore_state_request = true;
			auto_off_item->set_active (true);
			auto_play_item->set_active (false);
			auto_touch_item->set_active (false);
			auto_write_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	case Play:
		auto_dropdown.set_text (_("Play"));
		if (auto_play_item) {
			ignore_state_request = true;
			auto_play_item->set_active (true);
			auto_off_item->set_active (false);
			auto_touch_item->set_active (false);
			auto_write_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	case Write:
		auto_dropdown.set_text (_("Write"));
		if (auto_write_item) {
			ignore_state_request = true;
			auto_write_item->set_active (true);
			auto_off_item->set_active (false);
			auto_play_item->set_active (false);
			auto_touch_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	case Touch:
		auto_dropdown.set_text (_("Touch"));
		if (auto_touch_item) {
			ignore_state_request = true;
			auto_touch_item->set_active (true);
			auto_off_item->set_active (false);
			auto_play_item->set_active (false);
			auto_write_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	default:
		auto_dropdown.set_text (_("???"));
		break;
	}
}

/** The interpolation style of our AutomationList has changed, so update */
void
AutomationTimeAxisView::interpolation_changed (AutomationList::InterpolationStyle s)
{
	if (mode_line_item && mode_discrete_item) {
		if (s == AutomationList::Discrete) {
			mode_discrete_item->set_active(true);
			mode_line_item->set_active(false);
		} else {
			mode_line_item->set_active(true);
			mode_discrete_item->set_active(false);
		}
	}
}

/** A menu item has been selected to change our interpolation mode */
void
AutomationTimeAxisView::set_interpolation (AutomationList::InterpolationStyle style)
{
	/* Tell our view's list, if we have one, otherwise tell our own.
	 * Everything else will be signalled back from that.
	 */

	if (_view) {
		_view->set_interpolation (style);
	} else {
		assert (_control);
		_control->list()->set_interpolation (style);
	}
}

void
AutomationTimeAxisView::clear_clicked ()
{
	assert (_line || _view);

	_editor.begin_reversible_command (_("clear automation"));

	if (_line) {
		_line->clear ();
	} else if (_view) {
		_view->clear ();
	}
	set_automation_state ((AutoState) ARDOUR::Off);
	_editor.commit_reversible_command ();
	_session->set_dirty ();
}

void
AutomationTimeAxisView::set_height (uint32_t h, TrackHeightMode m)
{
	bool const changed = (height != (uint32_t) h) || first_call_to_set_height;
	uint32_t const normal = preset_height (HeightNormal);
	bool const changed_between_small_and_normal = ( (height < normal && h >= normal) || (height >= normal || h < normal) );

	TimeAxisView::set_height (h, m);

	_base_rect->set_y1 (h);

	if (_line) {
		_line->set_height(h - 2.5);
	}

	if (_view) {
		_view->set_height(h);
		_view->update_contents_height();
	}

	if (changed_between_small_and_normal || first_call_to_set_height) {

		first_call_to_set_height = false;

		if (h >= preset_height (HeightNormal)) {
			if (!(_parameter.type() >= MidiCCAutomation &&
			      _parameter.type() <= MidiChannelPressureAutomation)) {
				auto_dropdown.show();
			}
			name_label.show();
			hide_button.show();

		} else if (h >= preset_height (HeightSmall)) {
			controls_table.hide_all ();
			auto_dropdown.hide();
			name_label.hide();
		}
	}

	if (changed) {
		if (_canvas_display->visible() && _route) {
			/* only emit the signal if the height really changed and we were visible */
			_route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AutomationTimeAxisView::set_samples_per_pixel (double fpp)
{
	TimeAxisView::set_samples_per_pixel (fpp);

	if (_line) {
		_line->reset ();
	}

	if (_view) {
		_view->set_samples_per_pixel (fpp);
	}
}

void
AutomationTimeAxisView::hide_clicked ()
{
	hide_button.set_sensitive(false);
	set_marked_for_display (false);
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(parent);
	if (rtv) {
		rtv->request_redraw ();
	}
	hide_button.set_sensitive(true);
}

void
AutomationTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	/* prepare it */

	TimeAxisView::build_display_menu ();

	/* now fill it with our stuff */

	MenuList& items = display_menu->items();

	items.push_back (MenuElem (_("Hide"), sigc::mem_fun(*this, &AutomationTimeAxisView::hide_clicked)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear"), sigc::mem_fun(*this, &AutomationTimeAxisView::clear_clicked)));
	items.push_back (SeparatorElem());

	/* state menu */

	Menu* auto_state_menu = manage (new Menu);
	auto_state_menu->set_name ("ArdourContextMenu");
	MenuList& as_items = auto_state_menu->items();

	as_items.push_back (CheckMenuElem (S_("Automation|Manual"), sigc::bind (
			sigc::mem_fun(*this, &AutomationTimeAxisView::set_automation_state),
			(AutoState) ARDOUR::Off)));
	auto_off_item = dynamic_cast<Gtk::CheckMenuItem*>(&as_items.back());

	as_items.push_back (CheckMenuElem (_("Play"), sigc::bind (
			sigc::mem_fun(*this, &AutomationTimeAxisView::set_automation_state),
			(AutoState) Play)));
	auto_play_item = dynamic_cast<Gtk::CheckMenuItem*>(&as_items.back());

	as_items.push_back (CheckMenuElem (_("Write"), sigc::bind (
			sigc::mem_fun(*this, &AutomationTimeAxisView::set_automation_state),
			(AutoState) Write)));
	auto_write_item = dynamic_cast<Gtk::CheckMenuItem*>(&as_items.back());

	as_items.push_back (CheckMenuElem (_("Touch"), sigc::bind (
			sigc::mem_fun(*this, &AutomationTimeAxisView::set_automation_state),
			(AutoState) Touch)));
	auto_touch_item = dynamic_cast<Gtk::CheckMenuItem*>(&as_items.back());

	if (!(_parameter.type() >= MidiCCAutomation &&
	      _parameter.type() <= MidiChannelPressureAutomation)) {
		items.push_back (MenuElem (_("State"), *auto_state_menu));
	}

	/* mode menu */

	/* current interpolation state */
	AutomationList::InterpolationStyle const s = _view ? _view->interpolation() : _control->list()->interpolation ();

	if (ARDOUR::parameter_is_midi((AutomationType)_parameter.type())) {

		Menu* auto_mode_menu = manage (new Menu);
		auto_mode_menu->set_name ("ArdourContextMenu");
		MenuList& am_items = auto_mode_menu->items();

		RadioMenuItem::Group group;

		am_items.push_back (RadioMenuElem (group, _("Discrete"), sigc::bind (
				sigc::mem_fun(*this, &AutomationTimeAxisView::set_interpolation),
				AutomationList::Discrete)));
		mode_discrete_item = dynamic_cast<Gtk::CheckMenuItem*>(&am_items.back());
		mode_discrete_item->set_active (s == AutomationList::Discrete);

		am_items.push_back (RadioMenuElem (group, _("Linear"), sigc::bind (
				sigc::mem_fun(*this, &AutomationTimeAxisView::set_interpolation),
				AutomationList::Linear)));
		mode_line_item = dynamic_cast<Gtk::CheckMenuItem*>(&am_items.back());
		mode_line_item->set_active (s == AutomationList::Linear);

		items.push_back (MenuElem (_("Mode"), *auto_mode_menu));
	}

	/* make sure the automation menu state is correct */

	automation_state_changed ();
	interpolation_changed (s);
}

void
AutomationTimeAxisView::add_automation_event (GdkEvent* event, framepos_t frame, double y, bool with_guard_points)
{
	if (!_line) {
		return;
	}

	boost::shared_ptr<AutomationList> list = _line->the_list ();

	if (list->in_write_pass()) {
		/* do not allow the GUI to add automation events during an
		   automation write pass.
		*/
		return;
	}

	double x = 0;

	_line->grab_item().canvas_to_item (x, y);

	/* compute vertical fractional position */

	y = 1.0 - (y / _line->height());

	/* map using line */

	_line->view_to_model_coord (x, y);

	MusicFrame when (frame, 0);
	_editor.snap_to_with_modifier (when, event);

	XMLNode& before = list->get_state();
	std::list<Selectable*> results;

	if (list->editor_add (when.frame, y, with_guard_points)) {
		XMLNode& after = list->get_state();
		_editor.begin_reversible_command (_("add automation event"));
		_session->add_command (new MementoCommand<ARDOUR::AutomationList> (*list.get (), &before, &after));

		_line->get_selectables (when.frame, when.frame, 0.0, 1.0, results);
		_editor.get_selection ().set (results);

		_editor.commit_reversible_command ();
		_session->set_dirty ();
	}
}

bool
AutomationTimeAxisView::paste (framepos_t pos, const Selection& selection, PasteContext& ctx, const int32_t divisions)
{
	if (_line) {
		return paste_one (pos, ctx.count, ctx.times, selection, ctx.counts, ctx.greedy);
	} else if (_view) {
		AutomationSelection::const_iterator l = selection.lines.get_nth(_parameter, ctx.counts.n_lines(_parameter));
		if (l == selection.lines.end()) {
			if (ctx.greedy && selection.lines.size() == 1) {
				l = selection.lines.begin();
			}
		}
		if (l != selection.lines.end() && _view->paste (pos, ctx.count, ctx.times, *l)) {
			ctx.counts.increase_n_lines(_parameter);
			return true;
		}
	}

	return false;
}

bool
AutomationTimeAxisView::paste_one (framepos_t pos, unsigned paste_count, float times, const Selection& selection, ItemCounts& counts, bool greedy)
{
	boost::shared_ptr<AutomationList> alist(_line->the_list());

	if (_session->transport_rolling() && alist->automation_write()) {
		/* do not paste if this control is in write mode and we're rolling */
		return false;
	}

	/* Get appropriate list from selection. */
	AutomationSelection::const_iterator p = selection.lines.get_nth(_parameter, counts.n_lines(_parameter));
	if (p == selection.lines.end()) {
		if (greedy && selection.lines.size() == 1) {
			p = selection.lines.begin();
		} else {
			return false;
		}
	}
	counts.increase_n_lines(_parameter);

	/* add multi-paste offset if applicable */
	pos += _editor.get_paste_offset(pos, paste_count, (*p)->length());

	double const model_pos = _line->time_converter().from (pos - _line->time_converter().origin_b ());

	XMLNode &before = alist->get_state();
	alist->paste (**p, model_pos, times);
	_session->add_command (new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));

	return true;
}

void
AutomationTimeAxisView::get_selectables (framepos_t start, framepos_t end, double top, double bot, list<Selectable*>& results, bool /*within*/)
{
	if (!_line && !_view) {
		return;
	}

	if (touched (top, bot)) {

		/* remember: this is X Window - coordinate space starts in upper left and moves down.
		   _y_position is the "origin" or "top" of the track.
		*/

		/* bottom of our track */
		double const mybot = _y_position + height;

		double topfrac;
		double botfrac;

		if (_y_position >= top && mybot <= bot) {

			/* _y_position is below top, mybot is above bot, so we're fully
			   covered vertically.
			*/

			topfrac = 1.0;
			botfrac = 0.0;

		} else {

			/* top and bot are within _y_position .. mybot */

			topfrac = 1.0 - ((top - _y_position) / height);
			botfrac = 1.0 - ((bot - _y_position) / height);

		}

		if (_line) {
			_line->get_selectables (start, end, botfrac, topfrac, results);
		} else if (_view) {
			_view->get_selectables (start, end, botfrac, topfrac, results);
		}
	}
}

void
AutomationTimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& result)
{
	if (_line) {
		_line->get_inverted_selectables (sel, result);
	}
}

void
AutomationTimeAxisView::set_selected_points (PointSelection& points)
{
	if (_line) {
		_line->set_selected_points (points);
	} else if (_view) {
		_view->set_selected_points (points);
	}
}

void
AutomationTimeAxisView::clear_lines ()
{
	_line.reset();
	_list_connections.drop_connections ();
}

void
AutomationTimeAxisView::add_line (boost::shared_ptr<AutomationLine> line)
{
	if (_control && line) {
		assert(line->the_list() == _control->list());

		_control->alist()->automation_state_changed.connect (
			_list_connections, invalidator (*this), boost::bind (&AutomationTimeAxisView::automation_state_changed, this), gui_context()
			);

		_control->alist()->InterpolationChanged.connect (
			_list_connections, invalidator (*this), boost::bind (&AutomationTimeAxisView::interpolation_changed, this, _1), gui_context()
			);
	}

	_line = line;

	line->set_height (height - 2.5);

	/* pick up the current state */
	automation_state_changed ();

	line->add_visibility (AutomationLine::Line);
}

void
AutomationTimeAxisView::entered()
{
	if (_line) {
		_line->track_entered();
	}
}

void
AutomationTimeAxisView::exited ()
{
	if (_line) {
		_line->track_exited();
	}
}

void
AutomationTimeAxisView::color_handler ()
{
	if (_line) {
		_line->set_colors();
	}
}

int
AutomationTimeAxisView::set_state_2X (const XMLNode& node, int /*version*/)
{
	if (node.name() == X_("gain") && _parameter == Evoral::Parameter (GainAutomation)) {
		XMLProperty const * shown = node.property (X_("shown"));
		if (shown) {
			bool yn = string_is_affirmative (shown->value ());
			if (yn) {
				_canvas_display->show (); /* FIXME: necessary? show_at? */
			}
			set_gui_property ("visible", yn);
		} else {
			set_gui_property ("visible", false);
		}
	}

	return 0;
}

int
AutomationTimeAxisView::set_state (const XMLNode&, int /*version*/)
{
	return 0;
}

void
AutomationTimeAxisView::what_has_visible_automation (const boost::shared_ptr<Automatable>& automatable, set<Evoral::Parameter>& visible)
{
	/* this keeps "knowledge" of how we store visibility information
	   in XML private to this class.
	*/

	assert (automatable);

	Automatable::Controls& controls (automatable->controls());

	for (Automatable::Controls::iterator i = controls.begin(); i != controls.end(); ++i) {

		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (i->second);

		if (ac && ac->alist()) {

			const XMLNode* gui_node = ac->extra_xml ("GUI");

			if (gui_node) {
				XMLProperty const * prop = gui_node->property ("shown");
				if (prop) {
					if (string_is_affirmative (prop->value())) {
						visible.insert (i->first);
					}
				}
			}
		}
	}
}


/** @return true if this view has any automation data to display */
bool
AutomationTimeAxisView::has_automation () const
{
	return ( (_line && _line->npoints() > 0) || (_view && _view->has_automation()) );
}

list<boost::shared_ptr<AutomationLine> >
AutomationTimeAxisView::lines () const
{
	list<boost::shared_ptr<AutomationLine> > lines;

	if (_line) {
		lines.push_back (_line);
	} else if (_view) {
		lines = _view->get_lines ();
	}

	return lines;
}

string
AutomationTimeAxisView::state_id() const
{
	if (_automatable != _route && _control) {
		return string_compose ("automation %1", _control->id().to_s());
	} else if (_parameter) {
		return string_compose ("automation %1 %2/%3/%4",
				       _route->id(),
				       _parameter.type(),
				       _parameter.id(),
				       (int) _parameter.channel());
	} else {
		error << "Automation time axis has no state ID" << endmsg;
		return "";
	}
}

/** Given a state id string, see if it is one generated by
 *  this class.  If so, parse it into its components.
 *  @param state_id State ID string to parse.
 *  @param route_id Filled in with the route's ID if the state ID string is parsed.
 *  @param has_parameter Filled in with true if the state ID has a parameter, otherwise false.
 *  @param parameter Filled in with the state ID's parameter, if it has one.
 *  @return true if this is a state ID generated by this class, otherwise false.
 */

bool
AutomationTimeAxisView::parse_state_id (
	string const & state_id,
	PBD::ID & route_id,
	bool & has_parameter,
	Evoral::Parameter & parameter)
{
	stringstream s;
	s << state_id;

	string a, b, c;
	s >> a >> b >> c;

	if (a != X_("automation")) {
		return false;
	}

	route_id = PBD::ID (b);

	if (c.empty ()) {
		has_parameter = false;
		return true;
	}

	has_parameter = true;

	vector<string> p;
	boost::split (p, c, boost::is_any_of ("/"));

	assert (p.size() == 3);

	parameter = Evoral::Parameter (
		boost::lexical_cast<int> (p[0]),
		boost::lexical_cast<int> (p[2]),
		boost::lexical_cast<int> (p[1])
		);

	return true;
}

void
AutomationTimeAxisView::cut_copy_clear (Selection& selection, CutCopyOp op)
{
	list<boost::shared_ptr<AutomationLine> > lines;
	if (_line) {
		lines.push_back (_line);
	} else if (_view) {
		lines = _view->get_lines ();
	}

	for (list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		cut_copy_clear_one (**i, selection, op);
	}
}

void
AutomationTimeAxisView::cut_copy_clear_one (AutomationLine& line, Selection& selection, CutCopyOp op)
{
	boost::shared_ptr<Evoral::ControlList> what_we_got;
	boost::shared_ptr<AutomationList> alist (line.the_list());

	XMLNode &before = alist->get_state();

	/* convert time selection to automation list model coordinates */
	const Evoral::TimeConverter<double, ARDOUR::framepos_t>& tc = line.time_converter ();
	double const start = tc.from (selection.time.front().start - tc.origin_b ());
	double const end = tc.from (selection.time.front().end - tc.origin_b ());

	switch (op) {
	case Delete:
		if (alist->cut (start, end) != 0) {
			_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
		}
		break;

	case Cut:

		if ((what_we_got = alist->cut (start, end)) != 0) {
			_editor.get_cut_buffer().add (what_we_got);
			_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
		}
		break;
	case Copy:
		if ((what_we_got = alist->copy (start, end)) != 0) {
			_editor.get_cut_buffer().add (what_we_got);
		}
		break;

	case Clear:
		if ((what_we_got = alist->cut (start, end)) != 0) {
			_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
		}
		break;
	}

	if (what_we_got) {
		for (AutomationList::iterator x = what_we_got->begin(); x != what_we_got->end(); ++x) {
			double when = (*x)->when;
			double val  = (*x)->value;
			line.model_to_view_coord (when, val);
			(*x)->when = when;
			(*x)->value = val;
		}
	}
}

PresentationInfo const &
AutomationTimeAxisView::presentation_info () const
{
	return _route->presentation_info();
}

boost::shared_ptr<Stripable>
AutomationTimeAxisView::stripable () const
{
	return _route;
}

Gdk::Color
AutomationTimeAxisView::color () const
{
	return gdk_color_from_rgb (_route->presentation_info().color());
}
