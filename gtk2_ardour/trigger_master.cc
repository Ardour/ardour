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

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/region.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "region_view.h"
#include "selection.h"
#include "timers.h"
#include "trigger_master.h"
#include "trigger_ui.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

static const int nslices = 8; // in 8 pie slices .. TODO .. maybe make this meter-senstive ... triplets and such... ?

Loopster::Loopster (Item* parent)
	: ArdourCanvas::Rectangle (parent)
	, _fraction (0)
{
}

void
Loopster::set_fraction (float f)
{
	f = std::max (0.f, f);
	f = std::min (1.f, f);

	float prior_slice = floor (_fraction * nslices);
	float new_slice   = floor (f * nslices);

	if (new_slice != prior_slice) {
		_fraction = f;
		redraw ();
	}
}

void
Loopster::render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* Note that item_to_window() already takes _position into account (as
	 * part of item_to_canvas()
	 */
	ArdourCanvas::Rect       self (item_to_window (_rect));
	ArdourCanvas::Rect const draw = self.intersection (area);

	if (!draw) {
		return;
	}

	context->set_identity_matrix ();
	context->translate (self.x0, self.y0 - 0.5);

	float size = _rect.height ();

	const double scale = UIConfiguration::instance ().get_ui_scale ();

	/* white area */
	set_source_rgba (context, rgba_to_color (1, 1, 1, 1));
	context->arc (size / 2, size / 2, size / 2 - 4 * scale, 0, 2 * M_PI);
	context->fill ();

	/* arc fill */
	context->set_line_width (5 * scale);
	float slices        = floor (_fraction * nslices);
	float deg_per_slice = 360 / nslices;
	float degrees       = slices * deg_per_slice;
	float radians       = (degrees / 180) * M_PI;
	set_source_rgba (context, rgba_to_color (0, 0, 0, 1));
	context->arc (size / 2, size / 2, size / 2 - 5 * scale, 1.5 * M_PI + radians, 1.5 * M_PI + 2 * M_PI);
	context->stroke ();

	context->set_line_width (1);
	context->set_identity_matrix ();
}

TriggerMaster::TriggerMaster (Item* parent)
	: ArdourCanvas::Rectangle (parent)
	, _context_menu (0)
	, _ignore_menu_action (false)
{
	set_layout_sensitive (true); // why???

	name = X_("trigger stopper");

	Event.connect (sigc::mem_fun (*this, &TriggerMaster::event_handler));

	_loopster = new Loopster (this);

#if 0 /* XXX trigger changes */
	_triggerbox->PropertyChanged.connect (_trigger_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerMaster::prop_change, this, _1), gui_context());
	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);
	prop_change (changed);
#endif

#if 0 /* XXX route changes */
	dynamic_cast<Stripable*> (_triggerbox->owner())->presentation_info().Change.connect (_owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerMaster::owner_prop_change, this, _1), gui_context());
#endif

	_update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &TriggerMaster::maybe_update));

	/* prefs (theme colors) */
	UIConfiguration::instance ().ParameterChanged.connect (sigc::mem_fun (*this, &TriggerMaster::ui_parameter_changed));
	set_default_colors ();
}

TriggerMaster::~TriggerMaster ()
{
	_update_connection.disconnect ();
}

void
TriggerMaster::set_triggerbox (boost::shared_ptr<ARDOUR::TriggerBox> t)
{
	_triggerbox = t;
}

void
TriggerMaster::render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* Note that item_to_window() already takes _position into account (as
	 * part of item_to_canvas()
	 */
	ArdourCanvas::Rect       self (item_to_window (_rect));
	ArdourCanvas::Rect const draw = self.intersection (area);

	if (!draw) {
		return;
	}

	float width  = _rect.width ();
	float height = _rect.height ();

	const double scale = UIConfiguration::instance ().get_ui_scale ();

	if (_fill && !_transparent) {
		setup_fill_context (context);
		context->rectangle (draw.x0, draw.y0, draw.width (), draw.height ());
		context->fill ();
	}

	render_children (area, context);

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	/* MIDI triggers get a 'note' symbol */
	if (_triggerbox && _triggerbox->data_type () == ARDOUR::DataType::MIDI) {
		layout->set_font_description (UIConfiguration::instance ().get_BigBoldMonospaceFont ());
		layout->set_text ("\u266b");
		int tw, th;
		layout->get_pixel_size (tw, th);
		context->move_to (width / 2, height / 2);
		context->rel_move_to (-tw / 2, -th / 2);
		Gtkmm2ext::set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
		layout->show_in_cairo_context (context);
	}

	if (play_text != "") {
		layout->set_font_description (UIConfiguration::instance ().get_NormalFont ());
		layout->set_text (play_text);
		int tw, th;
		layout->get_pixel_size (tw, th);
		context->move_to ( height + 4*scale, height / 2);  //left side, but make room for loopster
		context->rel_move_to ( 0, -th / 2);  //vertically centered text
		Gtkmm2ext::set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
		layout->show_in_cairo_context (context);
	}

	if (loop_text != "") {
		layout->set_font_description (UIConfiguration::instance ().get_NormalFont ());
		layout->set_text (loop_text);
		int tw, th;
		layout->get_pixel_size (tw, th);
		context->move_to ( width-4*scale, height / 2);  //right side
		context->rel_move_to ( -tw, -th / 2);  //right justified, vertically centered text
		Gtkmm2ext::set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
		layout->show_in_cairo_context (context);
	}

	if (true) {
		/* drop-shadow at top */
		Cairo::RefPtr<Cairo::LinearGradient> drop_shadow_pattern = Cairo::LinearGradient::create (0.0, 0.0, 0.0, 6 * scale);
		drop_shadow_pattern->add_color_stop_rgba (0, 0, 0, 0, 0.7);
		drop_shadow_pattern->add_color_stop_rgba (1, 0, 0, 0, 0.0);
		context->set_source (drop_shadow_pattern);
		context->rectangle (0, 0, width, 6 * scale);
		context->fill ();
	}
}

void
TriggerMaster::owner_prop_change (PropertyChange const& pc)
{
	if (pc.contains (Properties::color)) {
	}
}

void
TriggerMaster::selection_change ()
{
}

bool
TriggerMaster::event_handler (GdkEvent* ev)
{
	if (!_triggerbox) {
		return false;
	}

	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (ev->button.button == 1) {
				if (Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier)) {
					_triggerbox->stop_all_immediately ();
				} else {
					_triggerbox->stop_all_quantized ();
				}
			}
			break;
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
			}
			redraw ();
			break;
		case GDK_LEAVE_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_default_colors ();
			}
			redraw ();
			break;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 3:
					context_menu ();
			}
		default:
			break;
	}

	return true;
}

void
TriggerMaster::context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _context_menu;

	_context_menu   = new Menu;
	MenuList& items = _context_menu->items ();
	_context_menu->set_name ("ArdourContextMenu");

	Menu*     follow_menu = manage (new Menu);
	MenuList& fitems      = follow_menu->items ();

	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::None)), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_follow_action), FollowAction (FollowAction::None))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::Stop)), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_follow_action), FollowAction (FollowAction::Stop))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::Again)), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_follow_action), FollowAction (FollowAction::Again))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::ForwardTrigger)), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_follow_action), FollowAction (FollowAction::ForwardTrigger))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::ReverseTrigger)), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_follow_action), FollowAction (FollowAction::ReverseTrigger))));

	Menu*     launch_menu = manage (new Menu);
	MenuList& litems      = launch_menu->items ();

	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::OneShot), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_launch_style), Trigger::OneShot)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::ReTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_launch_style), Trigger::ReTrigger)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Gate), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_launch_style), Trigger::Gate)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Toggle), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_launch_style), Trigger::Toggle)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Repeat), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_launch_style), Trigger::Repeat)));

	Menu*     quant_menu = manage (new Menu);
	MenuList& qitems     = quant_menu->items ();

	BBT_Offset b;

	b = BBT_Offset (4, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (2, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (1, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 2);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 4);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 8);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 16);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));
	b = BBT_Offset (-1, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerMaster::set_all_quantization), b)));

	items.push_back (MenuElem (_("Set All Follow Actions..."), *follow_menu));
	items.push_back (MenuElem (_("Set All Launch Styles..."), *launch_menu));
	items.push_back (MenuElem (_("Set All Quantizations..."), *quant_menu));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Set All Colors..."), sigc::mem_fun (*this, &TriggerMaster::set_all_colors)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear All..."), sigc::mem_fun (*this, &TriggerMaster::clear_all_triggers)));

	_context_menu->popup (1, gtk_get_current_event_time ());
}

void
TriggerMaster::clear_all_triggers ()
{
	_triggerbox->clear_all_triggers();
}

void
TriggerMaster::set_all_colors ()
{
	_color_dialog.get_colorsel()->set_has_opacity_control (false);
	_color_dialog.get_colorsel()->set_has_palette (true);
	_color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (_color_dialog, &Gtk::Dialog::response), Gtk::RESPONSE_ACCEPT));
	_color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (_color_dialog, &Gtk::Dialog::response), Gtk::RESPONSE_CANCEL));

	Gdk::Color c = ARDOUR_UI_UTILS::gdk_color_from_rgba(0xBEBEBEFF);

	_color_dialog.get_colorsel()->set_previous_color (c);
	_color_dialog.get_colorsel()->set_current_color (c);

	switch (_color_dialog.run()) {
		case Gtk::RESPONSE_ACCEPT: {
			c = _color_dialog.get_colorsel()->get_current_color();
			color_t ct = ARDOUR_UI_UTILS::gdk_color_to_rgba(c);
			for (int n = 0; n < default_triggers_per_box; n++) {
				_triggerbox->trigger (n)->set_color(ct);
			}
		} break;
		default:
			break;
	}

	_color_dialog.hide ();
}

void
TriggerMaster::set_all_follow_action (FollowAction const & fa)
{
	_triggerbox->set_all_follow_action (fa);
	_triggerbox->set_all_probability (0);
}

void
TriggerMaster::set_all_launch_style (Trigger::LaunchStyle ls)
{
	_triggerbox->set_all_launch_style(ls);
}

void
TriggerMaster::set_all_quantization (Temporal::BBT_Offset const& q)
{
	_triggerbox->set_all_quantization(q);
}

void
TriggerMaster::maybe_update ()
{
	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);
	prop_change (changed);
}

void
TriggerMaster::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	Rectangle::_size_allocate (alloc);

	const double scale = UIConfiguration::instance ().get_ui_scale ();
	_poly_margin       = 3. * scale;

	const Distance height = _rect.height ();

	_loopster->set (ArdourCanvas::Rect (0, 0, height, height));
}

void
TriggerMaster::prop_change (PropertyChange const& what_changed)
{
	if (!_triggerbox) {
		return;
	}

	/* currently, TriggerBox generates a continuous stream of ::name and ::running messages
	 TODO: I'd prefer a discrete message when currently_playing, follow_count, or loop_count has changed
	 Until then, we will cache our prior settings and only redraw when something actually changes */
	std::string old_play = play_text;
	std::string old_loop = loop_text;
	bool old_vis = _loopster->visible();

	/* for debugging */
	/* for (auto & c : what_changed) { std::cout << g_quark_to_string (c) << std::endl; } */

	if (what_changed.contains (ARDOUR::Properties::running)) {

		ARDOUR::TriggerPtr trigger = _triggerbox->currently_playing ();
		if (!trigger) {
			play_text = X_("");
			loop_text = X_("");
			_loopster->hide ();
		} else {

			play_text = cue_marker_name (trigger->index ());

			int fc = trigger->follow_count ();
			int lc = trigger->loop_count ();
			std::string text;
			if (fc > 1) {
				text = string_compose (X_("%1/%2"), lc+1, fc);
			} else if (lc > 1) {
				text = string_compose (X_("%1"), lc+1);  /* TODO: currently loop-count never updates unless follow_count is in use. */
			}
			loop_text = text;

			if (trigger->active ()) {
				double f = trigger->position_as_fraction ();
				_loopster->set_fraction (f); /*this sometimes triggers a redraw of the loopster widget (only). */
				_loopster->show ();
			} else {
				_loopster->hide ();
			}
		}
	}

	/* only trigger a redraw if a display value actually changes */
	if((_loopster->visible() != old_vis)
		|| (play_text != old_play)
		|| (loop_text != old_loop))
	{
		redraw();
	}

}

void
TriggerMaster::set_default_colors ()
{
	set_fill_color (HSV (UIConfiguration::instance ().color ("theme:bg")).darker (0.5).color ());
}

void
TriggerMaster::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors ();
	}
}

CueMaster::CueMaster (Item* parent)
	: ArdourCanvas::Rectangle (parent)
	, _context_menu (0)
{
	set_layout_sensitive (true); // why???

	name = X_("trigger stopper");

	Event.connect (sigc::mem_fun (*this, &CueMaster::event_handler));

	stop_shape = new ArdourCanvas::Polygon (this);
	stop_shape->set_outline (false);
	stop_shape->set_fill (true);
	stop_shape->name = X_("stopbutton");
	stop_shape->set_ignore_events (true);
	stop_shape->show ();

	/* prefs (theme colors) */
	UIConfiguration::instance ().ParameterChanged.connect (sigc::mem_fun (*this, &CueMaster::ui_parameter_changed));
	set_default_colors ();
}

CueMaster::~CueMaster ()
{
}

void
CueMaster::render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* Note that item_to_window() already takes _position into account (as
	   part of item_to_canvas()
	*/
	ArdourCanvas::Rect       self (item_to_window (_rect));
	ArdourCanvas::Rect const draw = self.intersection (area);

	if (!draw) {
		return;
	}

	float width  = _rect.width ();

	const double scale = UIConfiguration::instance ().get_ui_scale ();

	if (_fill && !_transparent) {
		setup_fill_context (context);
		context->rectangle (draw.x0, draw.y0, draw.width (), draw.height ());
		context->fill ();
	}

	render_children (area, context);

	if (true) {
		/* drop-shadow at top */
		Cairo::RefPtr<Cairo::LinearGradient> drop_shadow_pattern = Cairo::LinearGradient::create (0.0, 0.0, 0.0, 6 * scale);
		drop_shadow_pattern->add_color_stop_rgba (0, 0, 0, 0, 0.7);
		drop_shadow_pattern->add_color_stop_rgba (1, 0, 0, 0, 0.0);
		context->set_source (drop_shadow_pattern);
		context->rectangle (0, 0, width, 6 * scale);
		context->fill ();
	}
}

bool
CueMaster::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (ev->button.button == 1) {
				if (Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier)) {
					_session->stop_all_triggers (true);  //stop 'now'
				} else {
					_session->stop_all_triggers (false);  //stop quantized (bar end)
				}
				return true;
			}
			break;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 3:
					context_menu ();
					return true;
			}
			break;
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				stop_shape->set_fill_color (UIConfiguration::instance ().color ("neutral:foreground"));
				set_fill_color (HSV (fill_color ()).lighter (0.25).color ());
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_default_colors ();
			}
			break;
		default:
			break;
	}

	return false;
}

void
CueMaster::maybe_update ()
{
}

void
CueMaster::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	Rectangle::_size_allocate (alloc);

	const double scale = UIConfiguration::instance ().get_ui_scale ();
	_poly_margin       = 2 * scale;

	const Distance height = _rect.height ();

	_poly_size = height - (_poly_margin * 2);

	Points p;
	p.push_back (Duple (_poly_margin, _poly_margin));
	p.push_back (Duple (_poly_margin, _poly_size));
	p.push_back (Duple (_poly_size, _poly_size));
	p.push_back (Duple (_poly_size, _poly_margin));
	stop_shape->set (p);
}

void
CueMaster::set_default_colors ()
{
	set_fill_color (HSV (UIConfiguration::instance ().color ("theme:bg")).darker (0.5).color ());
	stop_shape->set_fill_color (UIConfiguration::instance ().color ("location marker"));
}

void
CueMaster::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors ();
	}
}

void
CueMaster::context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _context_menu;

	_context_menu   = new Menu;
	MenuList& items = _context_menu->items ();
	_context_menu->set_name ("ArdourContextMenu");

	Menu*     follow_menu = manage (new Menu);
	MenuList& fitems      = follow_menu->items ();

	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::None)), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_follow_action), FollowAction (FollowAction::None))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::Stop)), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_follow_action), FollowAction (FollowAction::Stop))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::Again)), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_follow_action), FollowAction (FollowAction::Again))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::ForwardTrigger)), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_follow_action), FollowAction (FollowAction::ForwardTrigger))));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(FollowAction (FollowAction::ReverseTrigger)), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_follow_action), FollowAction (FollowAction::ReverseTrigger))));

	Menu*     launch_menu = manage (new Menu);
	MenuList& litems      = launch_menu->items ();

	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::OneShot), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_launch_style), Trigger::OneShot)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::ReTrigger), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_launch_style), Trigger::ReTrigger)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Gate), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_launch_style), Trigger::Gate)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Toggle), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_launch_style), Trigger::Toggle)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Repeat), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_launch_style), Trigger::Repeat)));

	Menu*     quant_menu = manage (new Menu);
	MenuList& qitems     = quant_menu->items ();

	BBT_Offset b;

	b = BBT_Offset (4, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (2, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (1, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 2);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 4);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 8);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (0, 0, ticks_per_beat / 16);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));
	b = BBT_Offset (-1, 0, 0);
	qitems.push_back (MenuElem (TriggerUI::quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &CueMaster::set_all_quantization), b)));

//	items.push_back (CheckMenuElem (_("Toggle Monitor Thru"), sigc::mem_fun (*this, &CueMaster::toggle_thru)));
//	if (_triggerbox->pass_thru ()) {
//		_ignore_menu_action = true;
//		dynamic_cast<Gtk::CheckMenuItem*> (&items.back ())->set_active (true);
//		_ignore_menu_action = false;
//	}

	items.push_back (MenuElem (_("Set All Follow Actions..."), *follow_menu));
	items.push_back (MenuElem (_("Set All Launch Styles..."), *launch_menu));
	items.push_back (MenuElem (_("Set All Quantizations..."), *quant_menu));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear All..."), sigc::mem_fun (*this, &CueMaster::clear_all_triggers)));

	_context_menu->popup (1, gtk_get_current_event_time ());
}

void
CueMaster::get_boxen (TriggerBoxList &boxlist)
{
	boost::shared_ptr<RouteList> rl = _session->get_routes();
	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		boost::shared_ptr<Route> route = *r;
		boost::shared_ptr<TriggerBox> box = route->triggerbox();
#warning @Ben disambiguate processor *active* vs *visibility*
		if (box /*&& box.active*/) {
			boxlist.push_back(box);
		}
	}
}

//void
//CueMaster::toggle_thru ()
//{
//	TriggerBoxList tl;
//	get_boxen(tl);
//	for (TriggerBoxList::iterator t = tl.begin(); t != tl.end(); ++t) {
//		_triggerbox->set_pass_thru (!_triggerbox->pass_thru ());
//	}
//}


void
CueMaster::clear_all_triggers ()
{
	TriggerBoxList tl;
	get_boxen(tl);
	for (TriggerBoxList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->clear_all_triggers();
	}
}


void
CueMaster::set_all_follow_action (FollowAction const & fa)
{
	TriggerBoxList tl;
	get_boxen(tl);
	for (TriggerBoxList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_all_follow_action(fa, 0);
	}
}

void
CueMaster::set_all_launch_style (Trigger::LaunchStyle ls)
{
	TriggerBoxList tl;
	get_boxen(tl);
	for (TriggerBoxList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_all_launch_style(ls);
	}
}

void
CueMaster::set_all_quantization (Temporal::BBT_Offset const& q)
{
	TriggerBoxList tl;
	get_boxen(tl);
	for (TriggerBoxList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_all_quantization(q);
	}
}
