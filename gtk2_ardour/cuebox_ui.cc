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
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "cuebox_ui.h"
#include "public_editor.h"
#include "region_view.h"
#include "selection.h"
#include "timers.h"
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
	set_layout_sensitive(true);  //why???

	name = string_compose ("cue %1", _cue_idx);

	Event.connect (sigc::mem_fun (*this, &CueEntry::event_handler));

	set_outline (false);
	set_fill_color (UIConfiguration::instance().color ("theme:bg"));

	play_button = new ArdourCanvas::Rectangle (this);
	play_button->set_outline (false);
	play_button->set_fill(true);
	play_button->name = string_compose ("playbutton %1", _cue_idx);
	play_button->show ();

	play_shape = new ArdourCanvas::Polygon (play_button);
	play_shape->name = string_compose ("playshape %1", _cue_idx);
	play_shape->show ();

	name_button = new ArdourCanvas::Rectangle (this);
	name_button->set_outline (true);
	name_button->set_fill(false);
	name_button->name = ("slot_selector_button");
	name_button->show ();

	name_text = new Text (name_button);
	name_text->set (string_compose ("%1", (char) ('A'+ _cue_idx) ));
	name_text->set_ignore_events (false);
	name_text->show();
	
	/* watch for change in theme */
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &CueEntry::ui_parameter_changed));
	set_default_colors();
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
			play_shape->set_outline_color (UIConfiguration::instance().color ("neutral:foreground"));
			name_text->set_color (UIConfiguration::instance().color("neutral:foreground"));
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_shape->set_outline_color (UIConfiguration::instance().color ("neutral:midground"));
			name_text->set_color (UIConfiguration::instance().color("neutral:midground"));
		}
		break;
	default:
		break;
	}

	return false;
}

void
CueEntry::_size_allocate (ArdourCanvas::Rect const & alloc)
{
	Rectangle::_size_allocate (alloc);

	const Distance width = _rect.width();
	const Distance height = _rect.height();

	play_button->set (ArdourCanvas::Rect (0, 0, height, height));
	name_button->set (ArdourCanvas::Rect (height, 0, width, height));

	const double scale = UIConfiguration::instance().get_ui_scale();
	poly_margin = 2. * scale;
	poly_size = height - 2*poly_margin;
	shape_play_button ();

	float tleft = height;  //make room for the play button
	float twidth = name_button->width() - poly_margin*2;

	name_text->size_allocate (Rect(0, 0, width, height));
	name_text->set_position (Duple (tleft + poly_margin, poly_margin -0.5));
	name_text->clamp_width ( width - height );

	//font scale may have changed. uiconfig 'embeds' the ui-scale in the font
	name_text->set_font_description (UIConfiguration::instance().get_NormalFont());
}

void
CueEntry::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render(area, context);
}

void
CueEntry::shape_play_button ()
{
	Points p;

	{
		/* region exists; draw triangle to show that we can trigger */
		p.push_back (Duple (poly_margin, poly_margin));
		p.push_back (Duple (poly_margin, poly_size));
		p.push_back (Duple (poly_size, 0.5+poly_size / 2.));
	}

	play_shape->set (p);

	if (false /*ToDo*/) {
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
	set_fill_color (UIConfiguration::instance().color ("theme:bg"));
	play_button->set_fill_color (UIConfiguration::instance().color("theme:bg"));
	play_button->set_outline_color (UIConfiguration::instance().color("theme:bg"));
	name_button->set_fill_color (UIConfiguration::instance().color("theme:bg"));
	name_text->set_fill_color (UIConfiguration::instance().color("theme:bg"));
	name_button->set_outline_color (UIConfiguration::instance().color("theme:bg"));
	if ((_cue_idx/2)%2==0) {
		set_fill_color (HSV (fill_color()).darker(0.15).color ());
		play_button->set_fill_color (HSV (fill_color()).darker(0.15).color ());
		play_button->set_outline_color (HSV (fill_color()).darker(0.15).color ());
		name_button->set_fill_color (HSV (fill_color()).darker(0.15).color ());
		name_text->set_fill_color (HSV (fill_color()).darker(0.15).color ());
		name_button->set_outline_color (HSV (fill_color()).darker(0.15).color ());
	}

	name_text->set_color (UIConfiguration::instance().color("neutral:midground"));

	play_shape->set_outline_color (UIConfiguration::instance().color("neutral:midground"));
	play_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
}

void
CueEntry::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors ();
	}
}



/* ---------------------------- */

Gtkmm2ext::Bindings* CueBoxUI::bindings = 0;
Glib::RefPtr<Gtk::ActionGroup> CueBoxUI::trigger_actions;

CueBoxUI::CueBoxUI (ArdourCanvas::Item* parent)
	: Rectangle (parent)
{
	set_layout_sensitive(true);  //why???

	set_fill_color (UIConfiguration::instance().color(X_("theme:bg")));
	set_fill (true);

	build ();
}

CueBoxUI::~CueBoxUI ()
{
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

	for (int32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {  //ToDo
		CueEntry* te = new CueEntry (this, n);

		_slots.push_back (te);

//		te->play_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &CueBoxUI::trigger_scene), n));
//		te->name_text->Event.connect (sigc::bind (sigc::mem_fun (*this, &CueBoxUI::text_event), n));
		te->Event.connect (sigc::bind (sigc::mem_fun (*this, &CueBoxUI::event), n));
	}
}

void
CueBoxUI::_size_allocate (ArdourCanvas::Rect const & alloc)
{
	Rectangle::_size_allocate (alloc);

	const float width = alloc.width();
	const float height = alloc.height();

	const float slot_h = height / TriggerBox::default_triggers_per_box;  //ToDo

	float ypos = 0;
	for (auto & slot : _slots) {
		slot->size_allocate (Rect(0, 0, width, slot_h));
		slot->set_position (Duple (0, ypos));
		ypos += slot_h;
		slot->show();
	}

}

bool
CueBoxUI::text_event (GdkEvent *ev, uint64_t n)
{
	return false;
}

bool
CueBoxUI::event (GdkEvent* ev, uint64_t n)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		trigger_scene(n);
		break;
	case GDK_2BUTTON_PRESS:
		break;
	case GDK_BUTTON_RELEASE:
		break;
	default:
		break;
	}

	return false;
}


/* ------------ */

CueBoxWidget::CueBoxWidget (float w, float h) : FittedCanvasWidget(w,h)
{
	ui = new CueBoxUI (root());
	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));
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


/* ------------ */

CueBoxWindow::CueBoxWindow ()
{
	CueBoxWidget* tbw = manage (new CueBoxWidget (-1., TriggerBox::default_triggers_per_box*16.));
	set_title (_("CueBox for XXXX"));

	set_default_size (-1., TriggerBox::default_triggers_per_box*16.);
	add (*tbw);
	tbw->show ();
}

bool
CueBoxWindow::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

bool
CueBoxWindow::on_key_release_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
