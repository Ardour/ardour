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

#include <vector>

#include "gtkmm/sizegroup.h"
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/unwind.h"

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/pathexpand.h"
#include "pbd/search_path.h"

#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "region_view.h"
#include "selection.h"
#include "slot_properties_box.h"
#include "timers.h"
#include "trigger_ui.h"
#include "triggerbox_ui.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

TriggerEntry::TriggerEntry (Item* item, TriggerStrip& s, TriggerReference tr)
	: ArdourCanvas::Rectangle (item)
	, _strip (s)
	, _grabbed (false)
	, _drag_active (false)
{
	set_layout_sensitive (true); // why???

	name = string_compose ("trigger %1", tr.slot);

	set_outline (false);

	play_button = new ArdourCanvas::Rectangle (this);
	play_button->set_outline (true);
	play_button->set_fill (true);
	play_button->name = string_compose ("playbutton %1", tr.slot);
	play_button->show ();

	follow_button = new ArdourCanvas::Rectangle (this);
	follow_button->set_outline (false);
	follow_button->set_fill (true);
	follow_button->name = ("slot_selector_button");
	follow_button->set_tooltip (_("Click to select Follow-Actions for this clip"));
	follow_button->show ();

	name_button = new ArdourCanvas::Rectangle (this);
	name_button->set_outline (true);
	name_button->set_fill (true);
	name_button->name = ("slot_selector_button");
	name_button->show ();

	name_text = new Text (name_button);
	name_text->set_ignore_events (false);
	name_text->set_tooltip (_("Click to select this clip and edit its properties\nRight-Click for context menu"));
	name_text->show ();

	/* this will trigger a call to on_trigger_changed() */
	set_trigger (tr);

	/* DnD Source */
	GtkCanvas* gtkcanvas = static_cast<GtkCanvas*> (canvas ());
	assert (gtkcanvas);

	gtkcanvas->signal_drag_begin ().connect (sigc::mem_fun (*this, &TriggerEntry::drag_begin));
	gtkcanvas->signal_drag_end ().connect (sigc::mem_fun (*this, &TriggerEntry::drag_end));
	gtkcanvas->signal_drag_data_get ().connect (sigc::mem_fun (*this, &TriggerEntry::drag_data_get));

	/* event handling */
	play_button->Event.connect (sigc::mem_fun (*this, &TriggerEntry::play_button_event));
	name_button->Event.connect (sigc::mem_fun (*this, &TriggerEntry::name_button_event));
	follow_button->Event.connect (sigc::mem_fun (*this, &TriggerEntry::follow_button_event));

	Event.connect (sigc::mem_fun (*this, &TriggerEntry::event));

	/* watch for change in theme */
	UIConfiguration::instance ().ParameterChanged.connect (sigc::mem_fun (*this, &TriggerEntry::ui_parameter_changed));
	set_widget_colors ();

	/* owner color changes (?) */
	dynamic_cast<Stripable*> (tref.box->owner ())->presentation_info ().Change.connect (_owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerEntry::owner_prop_change, this, _1), gui_context ());

	selection_change ();
}

TriggerEntry::~TriggerEntry ()
{
}

void
TriggerEntry::owner_prop_change (PropertyChange const& pc)
{
	if (pc.contains (Properties::color)) {
		owner_color_changed ();
	}
}

void
TriggerEntry::owner_color_changed ()
{
	// TODO
}

void
TriggerEntry::selection_change ()
{
	set_widget_colors ();
}

void
TriggerEntry::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	Rectangle::_size_allocate (alloc);

	const Distance width  = _rect.width ();
	const Distance height = _rect.height ();

	play_button->set (ArdourCanvas::Rect (0, 0, height, height));
	name_button->set (ArdourCanvas::Rect (height, 0, width - height, height));
	follow_button->set (ArdourCanvas::Rect (width - height, 0, width, height));

	const double scale = UIConfiguration::instance ().get_ui_scale ();
	_poly_margin       = 2. * scale;
	_poly_size         = height - 2 * _poly_margin;

	float font_margin = 2. * scale;

	name_text->size_allocate (ArdourCanvas::Rect (0, 0, width, height - font_margin * 2));
	float tleft = height;                                                 // make room for the play button
	name_text->set_position (Duple (tleft + _poly_margin, font_margin)); // @paul why do we need tleft here? isn't name_text a child of name_button?
	name_text->clamp_width (width - height * 2 - _poly_margin * 3);

	/* font scale may have changed. uiconfig 'embeds' the ui-scale in the font */
	name_text->set_font_description (UIConfiguration::instance ().get_NormalFont ());
}

void
TriggerEntry::draw_follow_icon (Cairo::RefPtr<Cairo::Context> context, FollowAction const & icon, float size, float scale) const
{
	uint32_t fg_color = UIConfiguration::instance ().color ("neutral:midground");

	/* in the case where there is a random follow-action, just put a "?" */
	if (trigger ()->follow_action_probability () > 0) {
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
		layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
		layout->set_text ("?");
		int tw, th;
		layout->get_pixel_size (tw, th);
		context->move_to (size / 2, size / 2);
		context->rel_move_to (-tw / 2, -th / 2);
		layout->show_in_cairo_context (context);
		return;
	}

	set_source_rgba (context, fg_color);
	context->set_line_width (1 * scale);

	switch (icon.type) {
		case FollowAction::Stop:
			context->rectangle (6 * scale, 6 * scale, size - 12 * scale, size - 12 * scale);
			context->stroke ();
			break;
		case FollowAction::Again:
			context->arc (size / 2, size / 2, size * 0.20, 60. * (M_PI / 180.0), 2 * M_PI);
			context->stroke ();
			context->arc (size / 2 + size * 0.2, size / 2, 1.5 * scale, 0, 2 * M_PI); // arrow head
			context->fill ();
			break;
		case FollowAction::ForwardTrigger:
			context->move_to (size / 2, 3 * scale);
			context->line_to (size / 2, size - 5 * scale);
			context->stroke ();
			context->arc (size / 2, size - 5 * scale, 2 * scale, 0, 2 * M_PI); // arrow head
			context->fill ();
			break;
		case FollowAction::ReverseTrigger:
			context->move_to (size / 2, 5 * scale);
			context->line_to (size / 2, size - 3 * scale);
			context->stroke ();
			context->arc (size / 2, 5 * scale, 2 * scale, 0, 2 * M_PI); // arrow head
			context->fill ();
			break;
		case FollowAction::JumpTrigger: {
			if ( icon.targets.count() == 1 ) {  //Jump to a specific row; just draw the letter of the row we are jumping to
				int cue_idx = -1;
				for (int i = 0; i < TriggerBox::default_triggers_per_box; i++) {
					if (icon.targets.test(i)) {
						cue_idx = i;
						break;
					}
				}
				Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
				layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
				layout->set_text (cue_marker_name (cue_idx));
				int tw, th;
				layout->get_pixel_size (tw, th);
				context->move_to (size / 2, size / 2);
				context->rel_move_to (-tw / 2, -th / 2);
				layout->show_in_cairo_context (context);
			} else { // Multi-Jump: draw a star-like icon
				context->set_line_width (1.5 * scale);
				set_source_rgba (context, HSV (UIConfiguration::instance ().color ("neutral:midground")).lighter (0.25).color ()); // needs to be brighter to maintain balance
				for (int i = 0; i < 6; i++) {
					Cairo::Matrix m = context->get_matrix ();
					context->translate (size / 2, size / 2);
					context->rotate (i * M_PI / 3);
					context->move_to (0, 2 * scale);
					context->line_to (0, (size / 2) - 4 * scale);
					context->stroke ();
					context->set_matrix (m);
				}
			}
		} break;
		case FollowAction::None:
		default:
			break;
	}
}

void
TriggerEntry::draw_launch_icon (Cairo::RefPtr<Cairo::Context> context, float sz, float scale) const
{
	context->set_line_width (1 * scale);

	float margin = 4 * scale;
	float size   = sz - 2 * margin;

	bool active = trigger ()->active ();

	if (!trigger ()->region ()) {
		/* no content in this slot, it is only a Stop button */
		context->move_to (margin, margin);
		context->rel_line_to (size, 0);
		context->rel_line_to (0, size);
		context->rel_line_to (-size, 0);
		context->rel_line_to (0, -size);
		set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));
		context->stroke ();
		return;
	}

	set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));

	switch (trigger ()->launch_style ()) {
		case Trigger::Toggle:
			if (active) {
				/* special case: now it's a square Stop button */
				context->move_to (margin, margin);
				context->rel_line_to (size, 0);
				context->rel_line_to (0, size);
				context->rel_line_to (-size, 0);
				context->line_to (margin, margin);
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
				context->fill ();
				context->stroke ();
			} else {
				/* boxy arrow */
				context->move_to (margin, margin);
				context->rel_line_to (0, size);
				context->rel_line_to (size * 1 / 3, 0);
				context->rel_line_to (size * 2 / 3, -size / 2);
				context->rel_line_to (-size * 2 / 3, -size / 2);
				context->line_to (margin, margin);
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));
				context->stroke ();
			}
			break;
		case Trigger::OneShot:
			context->move_to (margin, margin);
			context->rel_line_to (0, size);
			context->rel_line_to (size, -size / 2);
			context->line_to (margin, margin);
			if (active) {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
				context->fill ();
				context->stroke ();
			} else {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));
				context->stroke ();
			}
			break;
		case Trigger::ReTrigger:
			/* line + boxy arrow + line */
			if (active) {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
			} else {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));
			}

			/* vertical line at left */
			context->set_line_width (2 * scale);
			context->move_to (margin + 1 * scale, margin);
			context->line_to (margin + 1 * scale, margin + size);
			context->stroke ();

			/* small triangle */
			context->set_line_width (1 * scale);
			context->move_to (margin + scale * 4, margin + 2 * scale);
			context->line_to (margin + size, margin + size / 2);
			context->line_to (margin + scale * 4, margin + size - 2 * scale);
			context->line_to (margin + scale * 4, margin + 2 * scale);
			if (active) {
				context->fill ();
			} else {
				context->stroke ();
			}
			break;
		case Trigger::Gate:
			/* diamond shape */
			context->move_to (margin + size / 2, margin);
			context->rel_line_to (size / 2, size / 2);
			context->rel_line_to (-size / 2, size / 2);
			context->rel_line_to (-size / 2, -size / 2);
			context->rel_line_to (size / 2, -size / 2);
			if (active) {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
				context->fill ();
				context->stroke ();
			} else {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));
				context->stroke ();
			}
			break;
		case Trigger::Repeat:
			/* 'stutter' shape */
			context->set_line_width (1 * scale);
			context->move_to (margin, margin);
			context->rel_line_to (0, size);

			context->move_to (margin + scale * 3, margin + scale * 2);
			context->rel_line_to (0, size - scale * 4);

			context->move_to (margin + scale * 6, margin + scale * 3);
			context->rel_line_to (0, size - scale * 6);

			if (active) {
				set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foregroundest"));
			} else {
				/* stutter shape needs to be brighter to maintain balance */
				set_source_rgba (context, HSV (UIConfiguration::instance ().color ("neutral:midground")).lighter (0.25).color ());
			}
			context->stroke ();
			break;
		default:
			break;
	}

	context->set_line_width (1);
}

void
TriggerEntry::render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);

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

	if (trigger ()->cue_isolated ()) {
		/* left shadow */
		context->set_identity_matrix ();
		context->translate (self.x0, self.y0 - 0.5);
		Cairo::RefPtr<Cairo::LinearGradient> l_shadow = Cairo::LinearGradient::create (0, 0, scale * 12, 0);
		l_shadow->add_color_stop_rgba (0.0, 0.0, 0.0, 0.0, 0.8);
		l_shadow->add_color_stop_rgba (1.0, 0.0, 0.0, 0.0, 0.0);
		context->set_source (l_shadow);
		context->rectangle (0, 0, scale * 12, height);
		context->fill ();
		context->set_identity_matrix ();
	}

	if (false /*tref.slot == 1*/) {
		/* drop-shadow at top */
		Cairo::RefPtr<Cairo::LinearGradient> drop_shadow_pattern = Cairo::LinearGradient::create (0.0, 0.0, 0.0, 6 * scale);
		drop_shadow_pattern->add_color_stop_rgba (0, 0, 0, 0, 0.7);
		drop_shadow_pattern->add_color_stop_rgba (1, 0, 0, 0, 0.0);
		context->set_source (drop_shadow_pattern);
		context->rectangle (0, 0, width, 6 * scale);
		context->fill ();
	}

	/* launch icon */
	{
		context->set_identity_matrix ();
		context->translate (self.x0, self.y0 - 0.5);
		context->translate (0, 0); // left side of the widget
		draw_launch_icon (context, height, scale);
		context->set_identity_matrix ();
	}

	/* follow-action icon */
	if (trigger ()->region () && trigger ()->will_follow ()) {
		context->set_identity_matrix ();
		context->translate (self.x0, self.y0 - 0.5);
		context->translate (width - height, 0); // right side of the widget
		draw_follow_icon (context, trigger ()->follow_action0 (), height, scale);
		context->set_identity_matrix ();
	}
}

void
TriggerEntry::on_trigger_changed (PropertyChange const& change)
{
	if (change.contains (ARDOUR::Properties::name)) {
		if (trigger ()->region ()) {
			name_text->set (short_version (trigger ()->name (), 16));
			play_button->set_tooltip (_("Launch this clip\nRight-click to select Launch Options for this clip"));
		} else {
			name_text->set ("");
			play_button->set_tooltip (_("Stop other clips on this track.\nRight-click to select Launch Options for this clip"));
		}
	}

	set_widget_colors (); //depending on the state, this might change a color and queue a redraw

	PropertyChange interesting_stuff;
	interesting_stuff.add (ARDOUR::Properties::name);
	interesting_stuff.add (ARDOUR::Properties::color);
	interesting_stuff.add (ARDOUR::Properties::launch_style);
	interesting_stuff.add (ARDOUR::Properties::follow_action0);
	interesting_stuff.add (ARDOUR::Properties::follow_action1);
	interesting_stuff.add (ARDOUR::Properties::follow_action_probability);
	interesting_stuff.add (ARDOUR::Properties::follow_count);
	interesting_stuff.add (ARDOUR::Properties::cue_isolated);
	interesting_stuff.add (ARDOUR::Properties::running);

	if (change.contains (interesting_stuff)) {
		redraw ();
	}
}

void
TriggerEntry::set_widget_colors (TriggerEntry::EnteredState es)
{
	color_t bg_col = UIConfiguration::instance ().color ("theme:bg");

	//alternating darker bands
	if ((tref.slot / 2) % 2 == 0) {
		bg_col = HSV (bg_col).darker (0.25).color ();
	}

	set_fill_color (bg_col);

	//child widgets highlight when entered
	color_t hilite = HSV (bg_col).lighter (0.15).color ();

	play_button->set_fill_color ((es == PlayEntered) ? hilite : bg_col);
	play_button->set_outline_color ((es == PlayEntered) ? hilite : bg_col);

	name_button->set_fill_color ((es == NameEntered) ? hilite : bg_col);
	name_button->set_outline_color ((es == NameEntered) ? hilite : bg_col);

	follow_button->set_fill_color ((es == FollowEntered) ? hilite : bg_col);

	name_text->set_color (trigger ()->color ());
	name_text->set_fill_color (UIConfiguration::instance ().color ("neutral:midground"));

	/*preserve selection border*/
	if (PublicEditor::instance ().get_selection ().selected (this)) {
		name_button->set_outline_color (UIConfiguration::instance ().color ("alert:red"));
	}

	/*draw a box around 'queued' trigger*/
	if (!trigger ()->active () && trigger ()->box ().currently_playing () == trigger ()) {
		play_button->set_outline_color (UIConfiguration::instance ().color ("neutral:foreground"));
	}
}

void
TriggerEntry::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_widget_colors ();
	}
}

bool
TriggerEntry::name_button_event (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_widget_colors (NameEntered);
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_widget_colors (NoneEntered);
			}
			break;
		case GDK_BUTTON_PRESS:
			break;
		case GDK_2BUTTON_PRESS:
#if SELECTION_PROPERTIES_BOX_TODO
			edit_trigger ();
#endif
			return true;
		case GDK_BUTTON_RELEASE:
			if (Gtkmm2ext::Keyboard::is_delete_event (&ev->button)) {
				clear_trigger ();
				return true;
			}
			switch (ev->button.button) {
				case 3:
					PublicEditor::instance ().get_selection ().set (this);
					context_menu ();
					return true;
				case 1:
					PublicEditor::instance ().get_selection ().set (this);
					return true;
				default:
					break;
			}
			break;
		default:
			break;
	}

	return false;
}

bool
TriggerEntry::play_button_event (GdkEvent* ev)
{
	if (!trigger ()->region ()) {
		/* empty slot; this is just a stop button */
		switch (ev->type) {
			case GDK_BUTTON_PRESS:
				if (ev->button.button == 1) {
					if (Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier)) {
						trigger ()->box ().stop_all_immediately ();
					} else {
						trigger ()->box ().stop_all_quantized ();
					}
				}
				break;
			default:
				break;
		}
	}

	switch (ev->type) {
		case GDK_2BUTTON_PRESS:  //need to un-grab in a double-click action
		case GDK_BUTTON_PRESS:
			switch (ev->button.button) {
				case 1:
					if (trigger ()->launch_style () == Trigger::Gate ||
					    trigger ()->launch_style () == Trigger::Repeat) {
						trigger ()->bang ();
						_grabbed = true;
						play_button->grab ();
					} else {
						trigger ()->bang ();
					}
				default:
					break;
			}
			break;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 1:
					if (_grabbed) {
						trigger ()->unbang ();
						play_button->ungrab ();
						_grabbed = false;
					}
					break;
				case 3:
					launch_context_menu ();
					return true;
				default:
					break;
			}
			break;
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_widget_colors (PlayEntered);
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_widget_colors (NoneEntered);
			}
			break;
		default:
			break;
	}
	return true;
}

bool
TriggerEntry::follow_button_event (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			return true;  //wait for release to show the menu
			break;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 1:
				case 3:
					follow_context_menu (&ev->button);
				default:
					break;
			}
			break;
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_widget_colors (FollowEntered);
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_widget_colors (NoneEntered);
			}
			break;
		default:
			break;
	}
	return true;
}

bool
TriggerEntry::event (GdkEvent* ev)
{
	if (!trigger ()->region ()) {
		return false;
	}

	switch (ev->type) {
		case GDK_2BUTTON_PRESS:  //need to un-grab in a double-click action
		case GDK_BUTTON_RELEASE:
			if(_grabbed) {
				ungrab();
				_grabbed = false;
				if (ev->type == GDK_BUTTON_RELEASE) {
					/* Pass event down to child item, as if this item was not grabbed.
					 * This is needed to select item on release.
					 */
					name_button->Event (ev);
					return true;
				}
			}
			break;

		case GDK_BUTTON_PRESS:
			if (!_drag_active) {
				GdkEventButton* bev = (GdkEventButton*)ev;
				if (bev->button == 1) {
					_drag_start_x = bev->x;
					_drag_start_y = bev->y;
					_grabbed = true;
					grab();
					return true;
				} else {
					_drag_start_x = -1;
					_drag_start_y = -1;
				}
			}
			break;

		case GDK_MOTION_NOTIFY:
			if (!_drag_active) {
				int               x, y;
				Gdk::ModifierType mask;

				GtkCanvas* gtkcanvas = static_cast<GtkCanvas*> (canvas ());
				gtkcanvas->get_window ()->get_pointer (x, y, mask);

				if (mask & GDK_BUTTON1_MASK) {
					if (gtkcanvas->drag_check_threshold (_drag_start_x, _drag_start_y, x, y)) {
						_drag_active = true;
						gtkcanvas->drag_begin (TriggerBoxUI::dnd_src (), Gdk::ACTION_COPY, 1, ev);
						// -> save a reference to the dragged slot, for use in ::drag_begin ::drag_data_get()
						return true;
					}
				}
			}
			break;
		default:
			break;
	}
	return false;
}

void
TriggerEntry::drag_begin (Glib::RefPtr<Gdk::DragContext> const& context)
{
	if (!_drag_active) {
		/* Since the canvas is shared, all TriggerEntry inside
		 * the TriggerBox canvas receive this signal
		 */
		return;
	}
	//const ArdourCanvas::Rect rect = allocation ();
	int                       width     = _rect.width ();
	int                       height    = _rect.height ();
	GtkCanvas*                gtkcanvas = static_cast<GtkCanvas*> (canvas ());
	Glib::RefPtr<Gdk::Pixmap> pixmap    = Gdk::Pixmap::create (gtkcanvas->get_root_window (), width, height);

	{
		cairo_t*                      cr  = gdk_cairo_create (Glib::unwrap (pixmap));
		Cairo::RefPtr<Cairo::Context> ctx = Cairo::RefPtr<Cairo::Context> (new Cairo::Context (cr, true /* has_reference */));

		/* inverse offset, because ::render() translates coordinates itself */
		ArdourCanvas::Rect self (item_to_window (_rect));
		ctx->translate (-self.x0, -self.y0);
		/* save context because ::render() calls set_identity_matrix () */
		ctx->save ();
		render (self, ctx);
		ctx->restore ();

		/* draw an outline around the drag object, replace red selection border */
		ctx->set_identity_matrix ();
		ctx->rectangle (0, 0, width, height);
		set_source_rgba (ctx, UIConfiguration::instance ().color ("neutral:foreground"));
		ctx->set_line_width (1.5);
		ctx->stroke ();
		/* ctx leaves scope, cr is destroyed, and pixmap surface is flush()ed */
	}

	std::shared_ptr<Region> region = trigger ()->region ();
	if (region) {
		PublicEditor::instance ().pbdid_dragged_dt = region->data_type ();
	} else {
		PublicEditor::instance ().pbdid_dragged_dt = DataType::NIL;
	}
	context->set_icon (pixmap->get_colormap (), pixmap, Glib::RefPtr<Gdk::Bitmap> (NULL), width / 2, height / 2);
}

void
TriggerEntry::drag_end (Glib::RefPtr<Gdk::DragContext> const&)
{
	if (_drag_active) {
		PublicEditor::instance ().pbdid_dragged_dt = DataType::NIL;
	}
	_drag_active = false;
}

void
TriggerEntry::drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData& data, guint, guint)
{
	if (!_drag_active) {
		/* Since the canvas is shared, all TriggerEntry instances
		 * inside a TriggerBox canvas receive this signal.
		 */
		return;
	}
	if (data.get_target () == "x-ardour/region.pbdid") {
		std::shared_ptr<Region> region = trigger ()->region ();
		if (region) {
			data.set (data.get_target (), region->id ().to_s ());
		}
	}
	if (data.get_target () == "x-ardour/trigger.pbdid") {
		data.set (data.get_target (), trigger()->id ().to_s ());
	}
}

/* ***************************************************** */

Glib::RefPtr<Gtk::TargetList> TriggerBoxUI::_dnd_src;

TriggerBoxUI::TriggerBoxUI (ArdourCanvas::Item* parent, TriggerStrip& s, TriggerBox& tb)
	: Rectangle (parent)
	, _triggerbox (tb)
	, _strip (s)
{
	set_layout_sensitive (true); // why???

	set_fill_color (UIConfiguration::instance ().color (X_("theme:bg")));
	set_fill (true);

	build ();

	_selection_connection = PublicEditor::instance ().get_selection ().TriggersChanged.connect (sigc::mem_fun (*this, &TriggerBoxUI::selection_changed));

	/* DnD */

	if (!_dnd_src) {
		std::vector<Gtk::TargetEntry> source_table;
		source_table.push_back (Gtk::TargetEntry ("x-ardour/trigger.pbdid", Gtk::TARGET_SAME_APP));
		source_table.push_back (Gtk::TargetEntry ("x-ardour/region.pbdid", Gtk::TARGET_SAME_APP));
		_dnd_src = Gtk::TargetList::create (source_table);
	}

	std::vector<Gtk::TargetEntry> target_table;
	target_table.push_back (Gtk::TargetEntry ("x-ardour/trigger.pbdid", Gtk::TARGET_SAME_APP));
	target_table.push_back (Gtk::TargetEntry ("x-ardour/region.pbdid", Gtk::TARGET_SAME_APP));
	target_table.push_back (Gtk::TargetEntry ("text/uri-list"));
	target_table.push_back (Gtk::TargetEntry ("text/plain"));
	target_table.push_back (Gtk::TargetEntry ("application/x-rootwin-drop"));

	GtkCanvas* gtkcanvas = static_cast<GtkCanvas*> (canvas ());
	assert (gtkcanvas);
	gtkcanvas->drag_dest_set (target_table);
	gtkcanvas->signal_drag_motion ().connect (sigc::mem_fun (*this, &TriggerBoxUI::drag_motion));
	gtkcanvas->signal_drag_leave ().connect (sigc::mem_fun (*this, &TriggerBoxUI::drag_leave));
	gtkcanvas->signal_drag_data_received ().connect (sigc::mem_fun (*this, &TriggerBoxUI::drag_data_received));
}

TriggerBoxUI::~TriggerBoxUI ()
{
	/* sigc connection's are not scoped (i.e. they do not disconnect the
	   functor from the signal when they are destroyed).
	*/
	_selection_connection.disconnect ();
}

void
TriggerBoxUI::selection_changed ()
{
	for (auto& slot : _slots) {
		slot->selection_change ();
	}
}

void
TriggerBoxUI::build ()
{
	TriggerPtr t;
	uint64_t   n = 0;

	// clear_items (true);

	_slots.clear ();

	while (true) {
		t = _triggerbox.trigger (n);
		if (!t) {
			break;
		}
		TriggerEntry* te = new TriggerEntry (this, _strip, TriggerReference (_triggerbox, n));

		_slots.push_back (te);

		++n;
	}
}

void
TriggerBoxUI::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	Rectangle::_size_allocate (alloc);

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

uint64_t
TriggerBoxUI::slot_at_y (int y) const
{
	uint64_t n = 0;
	for (auto& slot : _slots) {
		if (slot->height () < y) {
			++n;
			y -= slot->height ();
		}
	}
	return n;
}

bool
TriggerBoxUI::drag_motion (Glib::RefPtr<Gdk::DragContext> const& context, int, int y, guint time)
{
	bool can_drop = true;
	GtkCanvas* gtkcanvas = static_cast<GtkCanvas*> (canvas ());
	std::string target = gtkcanvas->drag_dest_find_target (context, gtkcanvas->drag_dest_get_target_list ());

	if ((target == "x-ardour/region.pbdid") || (target == "x-ardour/trigger.pbdid")) {
		can_drop = PublicEditor::instance ().pbdid_dragged_dt == _triggerbox.data_type ();
	}

	uint64_t n = slot_at_y (y);
	if (n >= _slots.size ()) {
		assert (0);
		can_drop = false;
	}

	if (can_drop) {
		context->drag_status (Gdk::ACTION_COPY, time);
		/* prelight */
		GdkEventCrossing ev;
		ev.detail = GDK_NOTIFY_ANCESTOR;
		for (size_t i = 0; i < _slots.size (); ++i) {
			ev.type = (i == n) ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY;
			_slots[i]->name_button_event ((GdkEvent*)&ev);
		}
		return true;
	} else {
		context->drag_status (Gdk::DragAction (0), time);
		return false;
	}
}

void
TriggerBoxUI::drag_leave (Glib::RefPtr<Gdk::DragContext> const&, guint)
{
	GdkEventCrossing ev;
	ev.type   = GDK_LEAVE_NOTIFY;
	ev.detail = GDK_NOTIFY_ANCESTOR;
	for (size_t i = 0; i < _slots.size (); ++i) {
		_slots[i]->name_button_event ((GdkEvent*)&ev);
	}
}

void
TriggerBoxUI::drag_data_received (Glib::RefPtr<Gdk::DragContext> const& context, int /*x*/, int y, Gtk::SelectionData const& data, guint /*info*/, guint time)
{
	uint64_t n = slot_at_y (y);
	if (n >= _slots.size ()) {
		context->drag_finish (false, false, time);
		return;
	}

	if (data.get_target () == "x-ardour/region.pbdid") {
		PBD::ID                   rid (data.get_data_as_string ());
		std::shared_ptr<Region> region = RegionFactory::region_by_id (rid);
		if (region) {
			_triggerbox.set_from_selection (n, region);
			context->drag_finish (true, false, time);
		} else {
			context->drag_finish (false, false, time);
		}
		return;
	}

	if (data.get_target () == "x-ardour/trigger.pbdid") {
		PBD::ID tid (data.get_data_as_string ());
		std::shared_ptr<Trigger> source = _triggerbox.session().trigger_by_id (tid);
		if (source) {
			Trigger::UIState *state = new Trigger::UIState();
			source->get_ui_state(*state);
			std::shared_ptr<Trigger::UIState> state_p (state);
			_triggerbox.enqueue_trigger_state_for_region(source->region(), state_p);
			_triggerbox.set_from_selection (n, source->region());
			context->drag_finish (true, false, time);
		} else {
			context->drag_finish (false, false, time);
		}
		return;
	}

	std::vector<std::string> paths;
	if (ARDOUR_UI_UTILS::convert_drop_to_paths (paths, data)) {
		for (std::vector<std::string>::iterator s = paths.begin (); s != paths.end (); ++s) {
			/* this will do nothing if n is too large */
			_triggerbox.set_from_path (n, *s);
#if 1 /* assume drop from sidebar -- TODO use a special data.get_target() ? */
			ARDOUR_UI_UTILS::copy_patch_changes (_triggerbox.session().the_auditioner (), _triggerbox.trigger (n));
#endif
			++n;
		}
	}
	context->drag_finish (true, false, time);
}

/* ********************************************** */

TriggerBoxWidget::TriggerBoxWidget (TriggerStrip& s, float w, float h)
	: FittedCanvasWidget (w, h)
	, ui (nullptr)
	, _strip (s)
{
	set_background_color (UIConfiguration::instance ().color (X_("theme:bg")));
}

void
TriggerBoxWidget::set_triggerbox (TriggerBox* tb)
{
	if (ui) {
		root ()->remove (ui);
		delete ui;
		ui = nullptr;
	}

	if (!tb) {
		return;
	}

	ui = new TriggerBoxUI (root (), _strip, *tb);
	repeat_size_allocation ();
}
