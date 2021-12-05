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
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "trigger_stopper.h"
#include "trigger_ui.h"
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

TriggerStopper::TriggerStopper (Item* parent, boost::shared_ptr<TriggerBox> t)
	: ArdourCanvas::Rectangle (parent)
	, _triggerbox (t)
{
	set_layout_sensitive(true);  //why???

	name = X_("trigger stopper");

	Event.connect (sigc::mem_fun (*this, &TriggerStopper::event_handler));

	play_shape = new ArdourCanvas::Polygon (this);
	play_shape->set_outline (false);
	play_shape->name = X_("stopbutton");
	play_shape->set_ignore_events (true);
	play_shape->show ();

	name_text = new Text (this);
	name_text->set("Now Playing");
	name_text->set_ignore_events (false);

	/* prefs (theme colors) */
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &TriggerStopper::ui_parameter_changed));

	/* trigger changes */
	_triggerbox->PropertyChanged.connect (trigger_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerStopper::prop_change, this, _1), gui_context());

	/* route changes */
//	dynamic_cast<Stripable*> (_triggerbox->owner())->presentation_info().Change.connect (owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerStopper::owner_prop_change, this, _1), gui_context());

	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);
	prop_change (changed);

	ui_parameter_changed("color-file");
}

TriggerStopper::~TriggerStopper ()
{
}

void
TriggerStopper::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* Note that item_to_window() already takes _position into account (as
	   part of item_to_canvas()
	*/
	Rect self (item_to_window (_rect));
	const Rect draw = self.intersection (area);

	if (!draw) {
		return;
	}

	float width = _rect.width();
	float height = _rect.height();

	const double scale = UIConfiguration::instance().get_ui_scale();

	if (_fill && !_transparent) {
		setup_fill_context (context);
		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->fill ();
	}

	//black area around text
	set_source_rgba (context, UIConfiguration::instance().color ("theme:bg2"));
	context->rectangle (16*scale, 1*scale, _rect.width()-2*scale, _rect.height()-2*scale);
	context->fill ();

#if 0
//text
Glib::RefPtr<Pango::Layout> layout (Pango::Layout::create (context));
layout->set_font_description (UIConfiguration::instance().get_NormalFont());
layout->set_text (name_text->text());
//text clipping rect
context->save();
context->rectangle (2, 1, width-4, height-2);
context->clip();
//calculate the text size
int tw, th;
layout->get_pixel_size (tw, th);
//render the text (centered vertically)
context->translate( 18*scale, (height/2)-(th/2) );
set_source_rgba (context, UIConfiguration::instance().color ("neutral:foreground"));
layout->show_in_cairo_context (context);
context->restore ();
#endif

	render_children (area, context);

	//fade-over at right
	uint32_t bg_color = UIConfiguration::instance().color ("theme:bg");
	double bg_r,bg_g,bg_b, unused;
	Gtkmm2ext::color_to_rgba( bg_color, bg_r, bg_g, bg_b, unused);
	Cairo::RefPtr<Cairo::LinearGradient> left_pattern = Cairo::LinearGradient::create (_rect.width()-12*scale, 0, _rect.width(), 0);
	left_pattern->add_color_stop_rgba (0, 0,	0,	    0, 0);
	left_pattern->add_color_stop_rgba (1, 0,	0,	    0, 1);
	context->set_source (left_pattern);
	context->rectangle( _rect.width()-12*scale, 2*scale, 10*scale, _rect.height()-4*scale );
	context->fill ();
}

void
TriggerStopper::owner_prop_change (PropertyChange const & pc)
{
	if (pc.contains (Properties::color)) {
	}
}

void
TriggerStopper::selection_change ()
{
}

bool
TriggerStopper::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 1) {
			_triggerbox->request_stop_all ();
			return true;
		}
		break;
	case GDK_ENTER_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_shape->set_fill_color (UIConfiguration::instance().color("neutral:foregroundest"));
		}
		redraw ();
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
		}
		redraw ();
		break;
	default:
		break;
	}

	return false;
}

void
TriggerStopper::maybe_update ()
{
/*	double nbw;

	if (!_trigger->active()) {
		nbw = 0;
	} else {
		nbw = _trigger->position_as_fraction () * (_allocation.width() - _allocation.height());
	}

	if (nbw) {
		const double scale = UIConfiguration::instance().get_ui_scale();
		ArdourCanvas::Rect r (get());

		active_bar->set (ArdourCanvas::Rect (r.height() * scale,
		                                     (r.y0 + 1) * scale,
		                                     (r.height() + nbw - 1) * scale,
		                                     (r.y1 - 1) * scale));
		active_bar->show ();
	} else {
		active_bar->hide ();
	}
	* */
}

void
TriggerStopper::_size_allocate (ArdourCanvas::Rect const & alloc)
{
	Rectangle::_size_allocate (alloc);

	const double scale = UIConfiguration::instance().get_ui_scale();
	poly_margin = 2. * scale;

	const Distance width = _rect.width();
	const Distance height = _rect.height();

	poly_size = height - (poly_margin*2);

	Points p;
	p.push_back (Duple (poly_margin, poly_margin));
	p.push_back (Duple (poly_margin, poly_size));
	p.push_back (Duple (poly_size, poly_size));
	p.push_back (Duple (poly_size, poly_margin));
	play_shape->set (p);

	float tleft = poly_size + (poly_margin*3);
	float twidth = width-poly_size-(poly_margin*3);

	ArdourCanvas::Rect text_alloc (tleft, 0, twidth, height); //testing
	name_text->size_allocate (text_alloc);
	name_text->set_position (Duple (tleft, 1.*scale));
	name_text->clamp_width (twidth);

	//font scale may have changed. uiconfig 'embeds' the ui-scale in the font
	name_text->set_font_description (UIConfiguration::instance().get_NormalFont());
}

void
TriggerStopper::prop_change (PropertyChange const & change)
{
	if (change.contains (ARDOUR::Properties::name)
		|| change.contains (ARDOUR::Properties::running)
	) {
		ARDOUR::Trigger *trigger = _triggerbox->currently_playing();

		if (trigger) {
			name_text->set (trigger->region()->name());
		} else {
//			name_text->set ("");
		}

		redraw();
	}
}

void
TriggerStopper::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_fill_color (UIConfiguration::instance().color("gtk_background"));
		name_text->set_color (UIConfiguration::instance().color("neutral:foreground"));
		play_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
	}
	redraw();
}


//====================================

CueStopper::CueStopper (Item* parent, boost::shared_ptr<TriggerBox> t)
	: ArdourCanvas::Rectangle (parent)
	, _triggerbox (t)
{
	set_layout_sensitive(true);  //why???

	name = X_("trigger stopper");

	Event.connect (sigc::mem_fun (*this, &CueStopper::event_handler));

	play_shape = new ArdourCanvas::Polygon (this);
	play_shape->set_outline (false);
	play_shape->name = X_("stopbutton");
	play_shape->set_ignore_events (true);
	play_shape->show ();

	name_text = new Text (this);
	name_text->set("Now Playing");
	name_text->set_ignore_events (false);

	/* prefs (theme colors) */
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &CueStopper::ui_parameter_changed));

	/* trigger changes */
	_triggerbox->PropertyChanged.connect (trigger_prop_connection, MISSING_INVALIDATOR, boost::bind (&CueStopper::prop_change, this, _1), gui_context());

	/* route changes */
//	dynamic_cast<Stripable*> (_triggerbox->owner())->presentation_info().Change.connect (owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&CueStopper::owner_prop_change, this, _1), gui_context());

	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);
	prop_change (changed);

	ui_parameter_changed("color-file");
}

CueStopper::~CueStopper ()
{
}

void
CueStopper::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* Note that item_to_window() already takes _position into account (as
	   part of item_to_canvas()
	*/
	Rect self (item_to_window (_rect));
	const Rect draw = self.intersection (area);

	if (!draw) {
		return;
	}

	float width = _rect.width();
	float height = _rect.height();

	const double scale = UIConfiguration::instance().get_ui_scale();

	if (_fill && !_transparent) {
		setup_fill_context (context);
		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->fill ();
	}

	//black area around text
	set_source_rgba (context, UIConfiguration::instance().color ("theme:bg2"));
	context->rectangle (16*scale, 1*scale, _rect.width()-2*scale, _rect.height()-2*scale);
	context->fill ();

#if 0
//text
Glib::RefPtr<Pango::Layout> layout (Pango::Layout::create (context));
layout->set_font_description (UIConfiguration::instance().get_NormalFont());
layout->set_text (name_text->text());
//text clipping rect
context->save();
context->rectangle (2, 1, width-4, height-2);
context->clip();
//calculate the text size
int tw, th;
layout->get_pixel_size (tw, th);
//render the text (centered vertically)
context->translate( 18*scale, (height/2)-(th/2) );
set_source_rgba (context, UIConfiguration::instance().color ("neutral:foreground"));
layout->show_in_cairo_context (context);
context->restore ();
#endif

	render_children (area, context);

	//fade-over at right
	uint32_t bg_color = UIConfiguration::instance().color ("theme:bg");
	double bg_r,bg_g,bg_b, unused;
	Gtkmm2ext::color_to_rgba( bg_color, bg_r, bg_g, bg_b, unused);
	Cairo::RefPtr<Cairo::LinearGradient> left_pattern = Cairo::LinearGradient::create (_rect.width()-12*scale, 0, _rect.width(), 0);
	left_pattern->add_color_stop_rgba (0, 0,	0,	    0, 0);
	left_pattern->add_color_stop_rgba (1, 0,	0,	    0, 1);
	context->set_source (left_pattern);
	context->rectangle( _rect.width()-12*scale, 2*scale, 10*scale, _rect.height()-4*scale );
	context->fill ();
}

void
CueStopper::owner_prop_change (PropertyChange const & pc)
{
	if (pc.contains (Properties::color)) {
	}
}

void
CueStopper::selection_change ()
{
}

bool
CueStopper::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 1) {
			_triggerbox->request_stop_all ();
			return true;
		}
		break;
	case GDK_ENTER_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_shape->set_fill_color (UIConfiguration::instance().color("neutral:foregroundest"));
		}
		redraw ();
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
		}
		redraw ();
		break;
	default:
		break;
	}

	return false;
}

void
CueStopper::maybe_update ()
{
/*	double nbw;

	if (!_trigger->active()) {
		nbw = 0;
	} else {
		nbw = _trigger->position_as_fraction () * (_allocation.width() - _allocation.height());
	}

	if (nbw) {
		const double scale = UIConfiguration::instance().get_ui_scale();
		ArdourCanvas::Rect r (get());

		active_bar->set (ArdourCanvas::Rect (r.height() * scale,
		                                     (r.y0 + 1) * scale,
		                                     (r.height() + nbw - 1) * scale,
		                                     (r.y1 - 1) * scale));
		active_bar->show ();
	} else {
		active_bar->hide ();
	}
	* */
}

void
CueStopper::_size_allocate (ArdourCanvas::Rect const & alloc)
{
	Rectangle::_size_allocate (alloc);

	const double scale = UIConfiguration::instance().get_ui_scale();
	poly_margin = 2. * scale;

	const Distance width = _rect.width();
	const Distance height = _rect.height();

	poly_size = height - (poly_margin*2);

	Points p;
	p.push_back (Duple (poly_margin, poly_margin));
	p.push_back (Duple (poly_margin, poly_size));
	p.push_back (Duple (poly_size, poly_size));
	p.push_back (Duple (poly_size, poly_margin));
	play_shape->set (p);

	float tleft = poly_size + (poly_margin*3);
	float twidth = width-poly_size-(poly_margin*3);

	ArdourCanvas::Rect text_alloc (tleft, 0, twidth, height); //testing
	name_text->size_allocate (text_alloc);
	name_text->set_position (Duple (tleft, 1.*scale));
	name_text->clamp_width (twidth);

	//font scale may have changed. uiconfig 'embeds' the ui-scale in the font
	name_text->set_font_description (UIConfiguration::instance().get_NormalFont());
}

void
CueStopper::prop_change (PropertyChange const & change)
{
	if (change.contains (ARDOUR::Properties::name)
		|| change.contains (ARDOUR::Properties::running)
	) {
		ARDOUR::Trigger *trigger = _triggerbox->currently_playing();

		if (trigger) {
			name_text->set (trigger->region()->name());
		} else {
//			name_text->set ("");
		}

		redraw();
	}
}

void
CueStopper::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_fill_color (UIConfiguration::instance().color("gtk_background"));
		name_text->set_color (UIConfiguration::instance().color("neutral:foreground"));
		play_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
	}
	redraw();
}

