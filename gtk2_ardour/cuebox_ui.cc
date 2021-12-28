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

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/region.h"
#include "ardour/session.h"
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/colors.h"
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "cuebox_ui.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "region_view.h"
#include "selection.h"
#include "timers.h"
#include "trigger_ui.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

CueEntry::CueEntry (Item* item, uint64_t cue_index)
	: ArdourCanvas::Rectangle (item)
	, _cue_idx (cue_index)
{
	set_layout_sensitive (true); // why???

	name = string_compose ("cue %1", _cue_idx);

	Event.connect (sigc::mem_fun (*this, &CueEntry::event_handler));

	set_outline (false);
	set_fill_color (UIConfiguration::instance ().color ("theme:bg"));

	play_button = new ArdourCanvas::Rectangle (this);
	play_button->set_outline (false);
	play_button->set_fill (true);
	play_button->name = string_compose ("playbutton %1", _cue_idx);
	play_button->show ();

	play_shape       = new ArdourCanvas::Polygon (play_button);
	play_shape->name = string_compose ("playshape %1", _cue_idx);
	play_shape->show ();

	name_button = new ArdourCanvas::Rectangle (this);
	name_button->set_outline (false);
	name_button->set_fill (true);
	name_button->name = ("slot_selector_button");
	name_button->show ();

	name_text = new Text (name_button);
	name_text->set (string_compose ("%1", (char)('A' + _cue_idx))); // XXX not translatable

	name_text->set_ignore_events (false);
	name_text->show ();

	/* watch for change in theme */
	UIConfiguration::instance ().ParameterChanged.connect (sigc::mem_fun (*this, &CueEntry::ui_parameter_changed));
	set_default_colors ();
}

CueEntry::~CueEntry ()
{
}

bool
CueEntry::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			break;
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				name_text->set_color (UIConfiguration::instance ().color ("neutral:foregroundest"));
				play_shape->set_outline_color (UIConfiguration::instance ().color ("neutral:foregroundest"));
				play_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
				name_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
				set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
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
CueEntry::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	ArdourCanvas::Rectangle::_size_allocate (alloc);

	const Distance width  = _rect.width ();
	const Distance height = _rect.height ();

	play_button->set (ArdourCanvas::Rect (0, 0, height, height));
	name_button->set (ArdourCanvas::Rect (height, 0, width, height));

	const double scale = UIConfiguration::instance ().get_ui_scale ();
	_poly_margin        = 2. * scale;
	_poly_size          = height - 2 * _poly_margin;
	shape_play_button ();

	float tleft  = height; // make room for the play button
	float twidth = name_button->width () - _poly_margin * 2;

	name_text->size_allocate (ArdourCanvas::Rect (0, 0, width, height));
	name_text->set_position (Duple (tleft + _poly_margin, _poly_margin - 0.5));
	name_text->clamp_width (width - height);

	/* font scale may have changed. uiconfig 'embeds' the ui-scale in the font */
	name_text->set_font_description (UIConfiguration::instance ().get_NormalFont ());
}

void
CueEntry::render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	ArdourCanvas::Rectangle::render (area, context);

	/* Note that item_to_window() already takes _position into account (as
	 * part of item_to_canvas()
	 */
	ArdourCanvas::Rect       self (item_to_window (_rect));
	const ArdourCanvas::Rect draw = self.intersection (area);

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

	if (_cue_idx == 0) {
		Cairo::RefPtr<Cairo::LinearGradient> drop_shadow_pattern = Cairo::LinearGradient::create (0.0, 0.0, 0.0, 6 * scale);
		drop_shadow_pattern->add_color_stop_rgba (0, 0, 0, 0, 0.7);
		drop_shadow_pattern->add_color_stop_rgba (1, 0, 0, 0, 0.0);
		context->set_source (drop_shadow_pattern);
		context->rectangle (0, 0, width, 6 * scale);
		context->fill ();
	}
}

void
CueEntry::shape_play_button ()
{
	Points p;

	p.push_back (Duple (_poly_margin, _poly_margin));
	p.push_back (Duple (_poly_margin, _poly_size));
	p.push_back (Duple (_poly_size, 0.5 + _poly_size / 2.));

	play_shape->set (p);

	if (false /*TODO*/) {
		play_shape->set_outline (false);
		play_shape->set_fill (true);
	} else {
		play_shape->set_outline (true);
		play_shape->set_fill (false);
	}
}

void
CueEntry::set_default_colors ()
{
	set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	play_button->set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	play_button->set_outline_color (UIConfiguration::instance ().color ("theme:bg"));
	name_button->set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	name_text->set_fill_color (UIConfiguration::instance ().color ("theme:bg"));

	if ((_cue_idx / 2) % 2 == 0) {
		set_fill_color (HSV (fill_color ()).darker (0.15).color ());
		play_button->set_fill_color (HSV (fill_color ()).darker (0.15).color ());
		play_button->set_outline_color (HSV (fill_color ()).darker (0.15).color ());
		name_button->set_fill_color (HSV (fill_color ()).darker (0.15).color ());
		name_text->set_fill_color (HSV (fill_color ()).darker (0.15).color ());
	}

	name_text->set_color (UIConfiguration::instance ().color ("neutral:foreground"));

	play_shape->set_outline_color (UIConfiguration::instance ().color ("neutral:foreground"));
	play_shape->set_fill_color (UIConfiguration::instance ().color ("neutral:foreground"));
}

void
CueEntry::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors ();
	}
}

Gtkmm2ext::Bindings*           CueBoxUI::bindings = 0;
Glib::RefPtr<Gtk::ActionGroup> CueBoxUI::trigger_actions;

CueBoxUI::CueBoxUI (ArdourCanvas::Item* parent)
	: ArdourCanvas::Rectangle (parent)
	, _context_menu (0)
{
	set_layout_sensitive (true); // why???

	set_fill_color (UIConfiguration::instance ().color (X_("theme:bg")));
	set_fill (true);

	build ();
}

CueBoxUI::~CueBoxUI ()
{
}

void
CueBoxUI::context_menu (uint64_t idx)
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

	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::None), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::None, idx)));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::Stop), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::Stop, idx)));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::Again), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::Again, idx)));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::PrevTrigger), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::PrevTrigger, idx)));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::NextTrigger), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::NextTrigger, idx)));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::AnyTrigger), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::AnyTrigger, idx)));
	fitems.push_back (MenuElem (TriggerUI::follow_action_to_string(Trigger::OtherTrigger), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_follow_action), Trigger::OtherTrigger, idx)));

	Menu*     launch_menu = manage (new Menu);
	MenuList& litems      = launch_menu->items ();

	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::OneShot), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_launch_style), Trigger::OneShot, idx)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Gate), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_launch_style), Trigger::Gate, idx)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Toggle), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_launch_style), Trigger::Toggle, idx)));
	litems.push_back (MenuElem (TriggerUI::launch_style_to_string(Trigger::Repeat), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_launch_style), Trigger::Repeat, idx)));

	Menu*     quant_menu = manage (new Menu);
	MenuList& qitems     = quant_menu->items ();

	BBT_Offset b;

	b = BBT_Offset (1, 0, 0);
	qitems.push_back (MenuElem (_("1 Bar"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 4, 0);
	qitems.push_back (MenuElem (_("Whole"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (MenuElem (_("1/2"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (MenuElem (_("1/4"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 0, ticks_per_beat / 2);
	qitems.push_back (MenuElem (_("1/8"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 0, ticks_per_beat / 4);
	qitems.push_back (MenuElem (_("1/16"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 0, ticks_per_beat / 8);
	qitems.push_back (MenuElem (_("1/32"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));
	b = BBT_Offset (0, 0, ticks_per_beat / 16);
	qitems.push_back (MenuElem (_("1/64"), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_quantization), b, idx)));

	Menu*     load_menu = manage (new Menu);
	MenuList& loitems (load_menu->items ());

	items.push_back (MenuElem (_("Set All Follow Actions..."), *follow_menu));
	items.push_back (MenuElem (_("Set All Launch Styles..."), *launch_menu));
	items.push_back (MenuElem (_("Set All Quantizations..."), *quant_menu));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Set All Colors..."), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::set_all_colors), idx)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear All..."), sigc::bind (sigc::mem_fun (*this, &CueBoxUI::clear_all_triggers), idx)));

	_context_menu->popup (1, gtk_get_current_event_time ());
}

void
CueBoxUI::get_slots (TriggerList &triggerlist, uint64_t idx)
{
	boost::shared_ptr<RouteList> rl = _session->get_routes();
	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		boost::shared_ptr<Route> route = *r;
		boost::shared_ptr<TriggerBox> box = route->triggerbox();
#warning @Ben disambiguate processor *active* vs *visibility*
		if (box /*&& box.active*/) {
			TriggerPtr trigger = box->trigger(idx);
			triggerlist.push_back(trigger);
		}
	}
}

void
CueBoxUI::clear_all_triggers (uint64_t idx)
{
	TriggerList tl;
	get_slots(tl, idx);
	for (TriggerList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_region(boost::shared_ptr<Region>());
	}
}

void
CueBoxUI::set_all_colors (uint64_t idx)
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
			TriggerList tl;
			get_slots(tl, idx);
			for (TriggerList::iterator t = tl.begin(); t != tl.end(); ++t) {
				(*t)->set_color(ct);
			}
		} break;
		default:
			break;
	}
	_color_dialog.hide ();
}

void
CueBoxUI::set_all_follow_action (Trigger::FollowAction fa, uint64_t idx)
{
	TriggerList tl;
	get_slots(tl, idx);
	for (TriggerList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_follow_action(fa, 0);
		(*t)->set_follow_action_probability(0);
	}
}

void
CueBoxUI::set_all_launch_style (Trigger::LaunchStyle ls, uint64_t idx)
{
	TriggerList tl;
	get_slots(tl, idx);
	for (TriggerList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_launch_style(ls);
	}
}

void
CueBoxUI::set_all_quantization (Temporal::BBT_Offset const& q, uint64_t idx)
{
	TriggerList tl;
	get_slots(tl, idx);
	for (TriggerList::iterator t = tl.begin(); t != tl.end(); ++t) {
		(*t)->set_quantization(q);
	}
}

void
CueBoxUI::setup_actions_and_bindings ()
{
	load_bindings ();
	register_actions ();
}

void
CueBoxUI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Triggers"));
}

void
CueBoxUI::register_actions ()
{
#if 0
	trigger_actions = ActionManager::create_action_group (bindings, X_("Triggers"));

	for (int32_t n = 0; n < CueBox::default_triggers_per_box; ++n) {
		const std::string action_name = string_compose ("trigger-scene-%1", n);
		const std::string display_name = string_compose (_("Scene %1"), n);

		ActionManager::register_toggle_action (trigger_actions, action_name.c_str(), display_name.c_str(), sigc::bind (sigc::ptr_fun (CueBoxUI::trigger_scene), n));
	}
#endif
}

void
CueBoxUI::trigger_scene (uint64_t n)
{
	TriggerBox::scene_bang (n);
}

void
CueBoxUI::build ()
{
	// clear_items (true);

	_slots.clear ();

	for (int32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) { // TODO
		CueEntry* te = new CueEntry (this, n);

		_slots.push_back (te);

#if 0
		te->play_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &CueBoxUI::trigger_scene), n));
		te->name_text->Event.connect (sigc::bind (sigc::mem_fun (*this, &CueBoxUI::text_event), n));
#endif
		te->Event.connect (sigc::bind (sigc::mem_fun (*this, &CueBoxUI::event), n));
	}
}

void
CueBoxUI::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	ArdourCanvas::Rectangle::_size_allocate (alloc);

	const float width  = alloc.width ();
	const float height = alloc.height ();

	const float slot_h = height / TriggerBox::default_triggers_per_box; // TODO

	float ypos = 0;
	for (auto& slot : _slots) {
		slot->size_allocate (ArdourCanvas::Rect (0, 0, width, slot_h));
		slot->set_position (Duple (0, ypos));
		ypos += slot_h;
		slot->show ();
	}
}

bool
CueBoxUI::text_event (GdkEvent* ev, uint64_t n)
{
	return false;
}

bool
CueBoxUI::event (GdkEvent* ev, uint64_t n)
{
	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (ev->button.button==1) {
				trigger_scene (n);
			}
			break;
		case GDK_2BUTTON_PRESS:
			break;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 3:
					context_menu (n);
					return true;
			}
		default:
			break;
	}

	return false;
}

CueBoxWidget::CueBoxWidget (float w, float h)
	: FittedCanvasWidget (w, h)
{
	ui = new CueBoxUI (root ());
	set_background_color (UIConfiguration::instance ().color (X_("theme:bg")));
}

void
CueBoxWidget::on_map ()
{
	FittedCanvasWidget::on_map ();
}

void
CueBoxWidget::on_unmap ()
{
	FittedCanvasWidget::on_unmap ();
}

CueBoxWindow::CueBoxWindow ()
{
	CueBoxWidget* tbw = manage (new CueBoxWidget (-1., TriggerBox::default_triggers_per_box * 16.));
	set_title (_("CueBox for XXXX"));

	set_default_size (-1., TriggerBox::default_triggers_per_box * 16.);
	add (*tbw);
	tbw->show ();
}

bool
CueBoxWindow::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance ()->main_window ());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

bool
CueBoxWindow::on_key_release_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance ()->main_window ());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
