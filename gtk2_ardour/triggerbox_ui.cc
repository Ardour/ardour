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

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>
#include "gtkmm/sizegroup.h"

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
#include "timers.h"
#include "trigger_ui.h"
#include "triggerbox_ui.h"
#include "slot_properties_box.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

TriggerEntry::TriggerEntry (Item* item, TriggerReference tr)
	: ArdourCanvas::Rectangle (item)
	, _file_chooser (0)
	, _launch_context_menu (0)
	, _follow_context_menu (0)
	, _context_menu (0)
	, _ignore_menu_action (false)
{
	set_layout_sensitive (true); // why???

	name = string_compose ("trigger %1", tr.slot);

	set_outline (false);

	play_button = new ArdourCanvas::Rectangle (this);
	play_button->set_outline (false);
	play_button->set_fill (true);
	play_button->name = string_compose ("playbutton %1", tr.slot);
	play_button->show ();

	follow_button = new ArdourCanvas::Rectangle (this);
	follow_button->set_outline (false);
	follow_button->set_fill (true);
	follow_button->name = ("slot_selector_button");
	follow_button->show ();

	name_button = new ArdourCanvas::Rectangle (this);
	name_button->set_outline (true);
	name_button->set_fill (true);
	name_button->name = ("slot_selector_button");
	name_button->show ();

	name_text = new Text (name_button);
	name_text->set_ignore_events (false);
	name_text->show ();

	/* this will trigger a call to on_trigger_changed() */
	set_trigger(tr);

	/* event handling */
	play_button->Event.connect (sigc::mem_fun (*this, &TriggerEntry::play_button_event));
	name_button->Event.connect (sigc::mem_fun (*this, &TriggerEntry::name_button_event));
	follow_button->Event.connect (sigc::mem_fun (*this, &TriggerEntry::follow_button_event));

	/* watch for change in theme */
	UIConfiguration::instance ().ParameterChanged.connect (sigc::mem_fun (*this, &TriggerEntry::ui_parameter_changed));
	set_default_colors ();

	/* owner color changes (?) */
	dynamic_cast<Stripable*> (tref.box->owner ())->presentation_info ().Change.connect (owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerEntry::owner_prop_change, this, _1), gui_context ());

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
	if (PublicEditor::instance ().get_selection ().selected (this)) {
		name_button->set_outline_color (UIConfiguration::instance ().color ("alert:red"));
	} else {
		set_default_colors ();
	}
}

void
TriggerEntry::maybe_update ()
{
	if (trigger()->active()) {
		redraw();
	}
}

void
TriggerEntry::_size_allocate (ArdourCanvas::Rect const& alloc)
{
	Rectangle::_size_allocate (alloc);

	const Distance width  = _rect.width ();
	const Distance height = _rect.height ();

	play_button->set (ArdourCanvas::Rect (0, 0, height, height));
	name_button->set (ArdourCanvas::Rect (height, 0, width-height, height));
	follow_button->set (ArdourCanvas::Rect (width-height, 0, width, height));

	const double scale = UIConfiguration::instance ().get_ui_scale ();
	_poly_margin       = 2. * scale;
	_poly_size         = height - 2 * _poly_margin;

	name_text->size_allocate (ArdourCanvas::Rect (0, 0, width, height -_poly_margin*2));
	float tleft = height; // make room for the play button
	name_text->set_position (Duple (tleft + _poly_margin, _poly_margin));  //@paul why do we need tleft here?  isn't name_text a child of name_button?
	name_text->clamp_width (width - height*2 -_poly_margin*3 );

	/* font scale may have changed. uiconfig 'embeds' the ui-scale in the font */
	name_text->set_font_description (UIConfiguration::instance ().get_NormalFont ());
}

void
TriggerEntry::draw_follow_icon (Cairo::RefPtr<Cairo::Context> context, Trigger::FollowAction icon, float size, float scale) const
{
	//in the case where there is a random follow-action, just put a "?"
	if (trigger()->follow_action_probability()>0) {
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

	context->set_line_width (1 * scale);

	switch (icon) {
		case Trigger::Stop:
			context->rectangle (6 * scale, 6 * scale, size - 12 * scale, size - 12 * scale);
			context->stroke ();
			break;
		case Trigger::Again:
			context->arc (size / 2, size / 2, size * 0.20, 60. * (M_PI / 180.0), 2 * M_PI);
			context->stroke ();
			context->arc (size / 2 + size * 0.2, size / 2, 1.5 * scale, 0, 2 * M_PI); // arrow head
			context->fill ();
			break;
		case Trigger::NextTrigger:
			context->move_to (size / 2, 3 * scale);
			context->line_to (size / 2, size - 5 * scale);
			context->stroke ();
			context->arc (size / 2, size - 5 * scale, 1.5 * scale, 0, 2 * M_PI); // arrow head
			context->fill ();
			break;
		case Trigger::PrevTrigger:
			context->move_to (size / 2, 5 * scale);
			context->line_to (size / 2, size - 3 * scale);
			context->stroke ();
			context->arc (size / 2, 5 * scale, 1.5 * scale, 0, 2 * M_PI); // arrow head
			context->fill ();
			break;
		case Trigger::QueuedTrigger: {
			Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			layout->set_text (icon == Trigger::AnyTrigger ? "&" : "@");
			int tw, th;
			layout->get_pixel_size (tw, th);
			context->move_to (size / 2, size / 2);
			context->rel_move_to (-tw / 2, -th / 2);
			layout->show_in_cairo_context (context);
		} break;
		case Trigger::AnyTrigger: {
			for (int i = 0; i<6; i++) {
				Cairo::Matrix m = context->get_matrix();
				context->translate (size / 2, size /2);
				context->rotate (i*M_PI/3);
				context->move_to (0, 0);
				context->line_to (0, (size/2)-4*scale);
				context->stroke ();
				context->set_matrix(m);
			}
			context->set_identity_matrix ();
		} break;
		case Trigger::OtherTrigger: {
			for (int i = 0; i<6; i++) {
				Cairo::Matrix m = context->get_matrix();
				context->translate (size / 2, size /2);
				context->rotate (i*M_PI/3);
				context->move_to (0, 2*scale);
				context->line_to (0, (size/2)-4*scale);
				context->stroke ();
				context->set_matrix(m);
			}
			context->set_identity_matrix ();
		} break;
		case Trigger::None:
		default:
			break;
	}
}


void
TriggerEntry::draw_launch_icon (Cairo::RefPtr<Cairo::Context> context, float sz, float scale) const
{
	context->set_line_width (1 * scale);

	float margin = 4*scale;
	float size = sz - 2*margin;

	if (trigger()->active()) {
		if (trigger()->launch_style()==Trigger::Toggle) {
			//clicking again will Stop this clip
			set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
			context->move_to (margin, margin);
			context->rel_line_to (size,  0);
			context->rel_line_to (0,     size);
			context->rel_line_to (-size, 0);
			context->rel_line_to (0,     -size);
			context->fill ();
			return;  //done
		} else {
			//actively playing; draw a filled play triangle
			set_source_rgba (context, UIConfiguration::instance ().color ("neutral:foreground"));
			context->move_to (margin, margin);
			context->rel_line_to (0, size);
			context->rel_line_to (size, -size/2);
			context->fill ();
			return;  //done
		}
	}

	set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));

	if (!trigger()->region ()) {
		//no content in this slot, it is only a Stop button
		context->move_to (margin, margin);
		context->rel_line_to (size,  0);
		context->rel_line_to (0,     size);
		context->rel_line_to (-size, 0);
		context->rel_line_to (0,     -size);
		context->stroke ();
		return;  //done
	}

	switch (trigger()->launch_style()) {
		case Trigger::Toggle:
		case Trigger::OneShot:
			context->move_to (margin, margin);
			context->rel_line_to (0, size);
			context->rel_line_to (size, -size/2);
			context->line_to (margin, margin);
			context->stroke ();
			break;
		case Trigger::Gate:  //diamond shape
			context->move_to ( margin+size/2, margin );
			context->rel_line_to ( size/2,  size/2);
			context->rel_line_to ( -size/2, size/2);
			context->rel_line_to ( -size/2, -size/2);
			context->rel_line_to ( size/2,  -size/2);
			context->stroke ();
			break;
		case Trigger::Repeat:  //'stutter' shape
			set_source_rgba (context, HSV (UIConfiguration::instance ().color ("neutral:midground")).lighter (0.25).color ()); //stutter shape needs to be brighter to maintain balance
			context->set_line_width (1 * scale);
			context->move_to ( margin, margin );
			context->rel_line_to ( 0, size);

			context->move_to ( margin + scale*3, margin + scale*2 );
			context->rel_line_to ( 0, size - scale*4);

			context->move_to ( margin + scale*6, margin + scale*3 );
			context->rel_line_to ( 0, size - scale*6);

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
	   part of item_to_canvas()
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

	if (trigger()->scene_isolated ()) {
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

	if (tref.slot == 1) {
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
	if (trigger()->region () && trigger()->use_follow()) {
		context->set_identity_matrix ();
		context->translate (self.x0, self.y0 - 0.5);
		context->translate (width - height, 0); // right side of the widget
		set_source_rgba (context, UIConfiguration::instance ().color ("neutral:midground"));
		draw_follow_icon (context, trigger()->follow_action (0), height, scale);
		context->set_identity_matrix ();
	}
}

void
TriggerEntry::on_trigger_changed (PropertyChange const& change)
{
	if (change.contains (ARDOUR::Properties::name)) {
		if (trigger()->region ()) {
			name_text->set (short_version (trigger()->name (), 16));
		} else {
			name_text->set ("");
		}
	}

	name_text->set_color (trigger()->color());

	PropertyChange interesting_stuff;
	interesting_stuff.add (ARDOUR::Properties::name);
	interesting_stuff.add (ARDOUR::Properties::color);
	interesting_stuff.add (ARDOUR::Properties::launch_style);
	interesting_stuff.add (ARDOUR::Properties::follow_action0);
	interesting_stuff.add (ARDOUR::Properties::follow_action1);
	interesting_stuff.add (ARDOUR::Properties::use_follow);
	interesting_stuff.add (ARDOUR::Properties::follow_action_probability);
	interesting_stuff.add (ARDOUR::Properties::follow_count);
	interesting_stuff.add (ARDOUR::Properties::isolated);
	interesting_stuff.add (ARDOUR::Properties::running);

	if (change.contains (interesting_stuff)) {
		redraw ();
	}
}

void
TriggerEntry::set_default_colors ()
{
	set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	play_button->set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	name_button->set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	name_button->set_outline_color (UIConfiguration::instance ().color ("theme:bg"));
	follow_button->set_fill_color (UIConfiguration::instance ().color ("theme:bg"));
	if ((tref.slot / 2) % 2 == 0) {
		set_fill_color (HSV (fill_color ()).darker (0.15).color ());
		play_button->set_fill_color (HSV (fill_color ()).darker (0.15).color ());
		name_button->set_fill_color (HSV (fill_color ()).darker (0.15).color ());
		name_button->set_outline_color (HSV (fill_color ()).darker (0.15).color ());
		follow_button->set_fill_color (HSV (fill_color ()).darker (0.15).color ());
	}

	name_text->set_fill_color (UIConfiguration::instance ().color ("neutral:midground"));

	/*preserve selection border*/
	if (PublicEditor::instance ().get_selection ().selected (this)) {
		name_button->set_outline_color (UIConfiguration::instance ().color ("alert:red"));
	}
}

void
TriggerEntry::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors ();
	}
}

bool
TriggerEntry::name_button_event (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_default_colors ();
				name_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
				name_button->set_outline_color (HSV (fill_color ()).lighter (0.15).color ());
				follow_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
				play_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
				/*preserve selection border*/
				if (PublicEditor::instance ().get_selection ().selected (this)) {
					name_button->set_outline_color (UIConfiguration::instance ().color ("alert:red"));
				}
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_default_colors ();
			}
			break;
		case GDK_BUTTON_PRESS:
			PublicEditor::instance ().get_selection ().set (this);
			/* a side-effect of selection-change is that the slot's color is reset. retain the "entered-color" here: */
			name_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
			name_button->set_outline_color (UIConfiguration::instance ().color ("alert:red"));
			follow_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
			break;
		case GDK_2BUTTON_PRESS:
			edit_trigger ();
			return true;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 3:
					context_menu ();
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
						trigger()->box().stop_all_immediately ();
					} else {
						trigger()->box().stop_all_quantized ();
					}
					return true;
				}
				break;
			default:
				break;
		}
	}

	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			switch (ev->button.button) {
				case 1:
					trigger()->bang ();
					return true;
				default:
					break;
			}
			break;
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 1:
					if (trigger()->launch_style () == Trigger::Gate ||
					    trigger()->launch_style () == Trigger::Repeat) {
						trigger()->unbang ();
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
				set_default_colors ();
				play_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
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

bool
TriggerEntry::follow_button_event (GdkEvent* ev)
{
	switch (ev->type) {
		case GDK_BUTTON_RELEASE:
			switch (ev->button.button) {
				case 3:
					follow_context_menu ();
					return true;
				default:
					break;
			}
			break;
		case GDK_ENTER_NOTIFY:
			if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
				set_default_colors ();
				follow_button->set_fill_color (HSV (fill_color ()).lighter (0.15).color ());
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
TriggerEntry::context_menu ()
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

	loitems.push_back (MenuElem (_("from file"), sigc::mem_fun (*this, &TriggerUI::choose_sample)));
	loitems.push_back (MenuElem (_("from selection"), sigc::mem_fun (*this, &TriggerEntry::set_from_selection)));

	items.push_back (MenuElem (_("Load..."), *load_menu));
#if DOUBLE_CLICK_IS_NOT_OBVIOUS_ENOUGH
	items.push_back (MenuElem (_("Edit..."), sigc::mem_fun (*this, &TriggerEntry::edit_trigger)));
#endif
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &TriggerUI::choose_color)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear"), sigc::mem_fun (*this, &TriggerEntry::clear_trigger)));

	_context_menu->popup (1, gtk_get_current_event_time ());
}

void
TriggerEntry::launch_context_menu ()
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

	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::OneShot), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_launch_style), Trigger::OneShot)));
	if (trigger ()->launch_style () == Trigger::OneShot) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::Gate), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_launch_style), Trigger::Gate)));
	if (trigger ()->launch_style () == Trigger::Gate) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::Toggle), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_launch_style), Trigger::Toggle)));
	if (trigger ()->launch_style () == Trigger::Toggle) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, TriggerUI::launch_style_to_string(Trigger::Repeat), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_launch_style), Trigger::Repeat)));
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

	b = BBT_Offset (1, 0, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 4, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 2);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 4);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 8);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat / 16);
	qitems.push_back (RadioMenuElem (qgroup, TriggerUI::quantize_length_to_string (b), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_quantization), b)));
	if (trigger ()->quantization () == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}

	items.push_back (MenuElem (_("Launch Style..."), *launch_menu));
	items.push_back (MenuElem (_("Quantization..."), *quant_menu));

	items.push_back (CheckMenuElem (_("Cue Isolate"), sigc::mem_fun (*this, &TriggerEntry::toggle_trigger_isolated)));
	if (trigger ()->scene_isolated ()) {
		PBD::Unwinder<bool> uw (_ignore_menu_action, true);
		dynamic_cast<Gtk::CheckMenuItem*> (&items.back ())->set_active (true);
	}

	_launch_context_menu->popup (1, gtk_get_current_event_time ());
}

void
TriggerEntry::follow_context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _follow_context_menu;

	_follow_context_menu   = new Menu;
	MenuList& items = _follow_context_menu->items ();
	_follow_context_menu->set_name ("ArdourContextMenu");

	Menu*     follow_menu = manage (new Menu);
	MenuList& fitems      = follow_menu->items ();

	RadioMenuItem::Group fagroup;

	_ignore_menu_action = true;

	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::None), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::None)));
	if (trigger ()->follow_action (0) == Trigger::None) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::Stop), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::Stop)));
	if (trigger ()->follow_action (0) == Trigger::Stop) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::Again), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::Again)));
	if (trigger ()->follow_action (0) == Trigger::Again) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
#if QUEUED_SLOTS_IMPLEMENTED
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::QueuedTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::QueuedTrigger)));
	if (trigger ()->follow_action (0) == Trigger::QueuedTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
#endif
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::PrevTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::PrevTrigger)));
	if (trigger ()->follow_action (0) == Trigger::PrevTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::NextTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::NextTrigger)));
	if (trigger ()->follow_action (0) == Trigger::NextTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
#if 0
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::FirstTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::FirstTrigger)));
	if (trigger ()->follow_action (0) == Trigger::FirstTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::LastTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::LastTrigger)));
	if (trigger ()->follow_action (0) == Trigger::LastTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
#endif
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::AnyTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::AnyTrigger)));
	if (trigger ()->follow_action (0) == Trigger::AnyTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, TriggerUI::follow_action_to_string(Trigger::OtherTrigger), sigc::bind(sigc::mem_fun (*this, &TriggerEntry::set_follow_action), Trigger::OtherTrigger)));
	if (trigger ()->follow_action (0) == Trigger::OtherTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}

	_ignore_menu_action = false;

	items.push_back (MenuElem (_("Follow Action..."), *follow_menu));

	_follow_context_menu->popup (1, gtk_get_current_event_time ());
}


void
TriggerEntry::toggle_trigger_isolated ()
{
	if (_ignore_menu_action) {
		return;
	}

	trigger()->set_scene_isolated (!trigger()->scene_isolated ());
}

void
TriggerEntry::clear_trigger ()
{
	trigger()->set_region (boost::shared_ptr<Region>());
}

void
TriggerEntry::edit_trigger ()
{
	SlotPropertyWindow* tw      = static_cast<SlotPropertyWindow*> (trigger()->ui ());

	if (!tw) {
		tw = new SlotPropertyWindow (TriggerReference (trigger()->box(), trigger()->index()));
		trigger()->set_ui (tw);
	}

	tw->present ();
}

void
TriggerEntry::set_follow_action (Trigger::FollowAction fa)
{
	if (_ignore_menu_action) {
		return;
	}

	trigger()->set_follow_action (fa, 0);
	trigger()->set_follow_action_probability (0);
	trigger()->set_use_follow (true);
}

void
TriggerEntry::set_launch_style (Trigger::LaunchStyle ls)
{
	trigger()->set_launch_style (ls);
}

void
TriggerEntry::set_quantization (Temporal::BBT_Offset const& q)
{
	trigger()->set_quantization (q);
}

void
TriggerEntry::set_from_selection ()
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

/* ***************************************************** */

TriggerBoxUI::TriggerBoxUI (ArdourCanvas::Item* parent, TriggerBox& tb)
	: Rectangle (parent)
	, _triggerbox (tb)
{
	set_layout_sensitive (true); // why???

	set_fill_color (UIConfiguration::instance ().color (X_("theme:bg")));
	set_fill (true);

	build ();

	_selection_connection = PublicEditor::instance ().get_selection ().TriggersChanged.connect (sigc::mem_fun (*this, &TriggerBoxUI::selection_changed));

	std::vector<Gtk::TargetEntry> target_table;
	target_table.push_back (Gtk::TargetEntry ("regions"));
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
	_update_connection.disconnect ();
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
	uint64_t n = 0;

	// clear_items (true);

	_slots.clear ();

	while (true) {
		t = _triggerbox.trigger (n);
		if (!t) {
			break;
		}
		TriggerEntry* te = new TriggerEntry (this, TriggerReference (_triggerbox, n));

		_slots.push_back (te);

#if 0
		te->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::event), n));
#endif

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
	bool     can_drop = true;
	uint64_t n        = slot_at_y (y);
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
	if (data.get_target () == X_("regions")) {
		boost::shared_ptr<Region> region = PublicEditor::instance ().get_dragged_region_from_sidebar ();
		if (region) {
			_triggerbox.set_from_selection (n, region);
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
			++n;
		}
	}
	context->drag_finish (true, false, time);
}

void
TriggerBoxUI::start_updating ()
{
	_update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &TriggerBoxUI::rapid_update));
}

void
TriggerBoxUI::stop_updating ()
{
	_update_connection.disconnect ();
}

void
TriggerBoxUI::rapid_update ()
{
	for (auto& slot : _slots) {
		slot->maybe_update ();
	}
}

/* ********************************************** */

TriggerBoxWidget::TriggerBoxWidget (float w, float h)
	: FittedCanvasWidget (w, h)
	, ui (0)
{
	set_background_color (UIConfiguration::instance ().color (X_("theme:bg")));
}

void
TriggerBoxWidget::set_triggerbox (TriggerBox* tb)
{
	if (ui) {
		root ()->remove (ui);
		delete ui;
		ui = 0;
	}

	if (!tb) {
		return;
	}

	ui = new TriggerBoxUI (root (), *tb);
	repeat_size_allocation ();

	if (is_mapped ()) {
		ui->start_updating ();
	}
}

void
TriggerBoxWidget::on_map ()
{
	FittedCanvasWidget::on_map ();

	if (ui) {
		ui->start_updating ();
	}
}

void
TriggerBoxWidget::on_unmap ()
{
	FittedCanvasWidget::on_unmap ();
	if (ui) {
		ui->stop_updating ();
	}
}
