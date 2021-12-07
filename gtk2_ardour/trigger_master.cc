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
#include "trigger_master.h"
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

TriggerMaster::TriggerMaster (Item* parent, boost::shared_ptr<TriggerBox> t)
	: ArdourCanvas::Rectangle (parent)
	, _triggerbox (t)
{
	set_layout_sensitive(true);  //why???

	name = X_("trigger stopper");

	Event.connect (sigc::mem_fun (*this, &TriggerMaster::event_handler));

	active_bar = new ArdourCanvas::Rectangle (this);
	active_bar->set_outline (false);

	stop_shape = new ArdourCanvas::Polygon (this);
	stop_shape->set_outline (false);
	stop_shape->name = X_("stopbutton");
	stop_shape->set_ignore_events (true);
	stop_shape->show ();

	name_text = new Text (this);
	name_text->set("Now Playing");
	name_text->set_ignore_events (false);

	/* trigger changes */
	_triggerbox->PropertyChanged.connect (trigger_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerMaster::prop_change, this, _1), gui_context());

	/* route changes */
//	dynamic_cast<Stripable*> (_triggerbox->owner())->presentation_info().Change.connect (owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerMaster::owner_prop_change, this, _1), gui_context());

	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);
	prop_change (changed);

	/* prefs (theme colors) */
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &TriggerMaster::ui_parameter_changed));
	set_default_colors();
}

TriggerMaster::~TriggerMaster ()
{
}

void
TriggerMaster::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
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

	//fade-over at top
	uint32_t bg_color = UIConfiguration::instance().color ("theme:bg");
	double bg_r,bg_g,bg_b, unused;
	Gtkmm2ext::color_to_rgba( bg_color, bg_r, bg_g, bg_b, unused);
	Cairo::RefPtr<Cairo::LinearGradient> left_pattern = Cairo::LinearGradient::create (0, 0, 0, 6.*scale);
	left_pattern->add_color_stop_rgba (0, 0,	0,	    0, 1);
	left_pattern->add_color_stop_rgba (1, 0,	0,	    0, 0);
	context->set_source (left_pattern);
	context->rectangle(0, 0, width, 6.*scale);
	context->fill ();

	render_children (area, context);
}

void
TriggerMaster::owner_prop_change (PropertyChange const & pc)
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
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 1) {
			_triggerbox->request_stop_all ();
			return true;
		}
		break;
	case GDK_ENTER_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			stop_shape->set_fill_color (UIConfiguration::instance().color("neutral:foregroundest"));
		}
		redraw ();
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			stop_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
		}
		redraw ();
		break;
	default:
		break;
	}

	return false;
}

void
TriggerMaster::maybe_update ()
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
TriggerMaster::_size_allocate (ArdourCanvas::Rect const & alloc)
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
	stop_shape->set (p);

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
TriggerMaster::prop_change (PropertyChange const & change)
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
TriggerMaster::set_default_colors ()
{
	set_fill_color (HSV (UIConfiguration::instance().color("theme:bg")).darker(0.25).color ());
	name_text->set_color (UIConfiguration::instance().color("neutral:foreground"));
	stop_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
}


void
TriggerMaster::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors();
	}
}



//====================================

CueMaster::CueMaster (Item* parent)
	: ArdourCanvas::Rectangle (parent)
{
	set_layout_sensitive(true);  //why???

	name = X_("trigger stopper");

	Event.connect (sigc::mem_fun (*this, &CueMaster::event_handler));

	stop_shape = new ArdourCanvas::Polygon (this);
	stop_shape->set_outline (false);
	stop_shape->set_fill (true);
	stop_shape->name = X_("stopbutton");
	stop_shape->set_ignore_events (true);
	stop_shape->show ();

	name_text = new Text (this);
	name_text->set("");
	name_text->set_ignore_events (false);

	/* prefs (theme colors) */
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &CueMaster::ui_parameter_changed));
	set_default_colors();
}

CueMaster::~CueMaster ()
{
}

void
CueMaster::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
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

	//fade-over at top
	uint32_t bg_color = UIConfiguration::instance().color ("theme:bg");
	double bg_r,bg_g,bg_b, unused;
	Gtkmm2ext::color_to_rgba( bg_color, bg_r, bg_g, bg_b, unused);
	Cairo::RefPtr<Cairo::LinearGradient> left_pattern = Cairo::LinearGradient::create (0, 0, 0, 6.*scale);
	left_pattern->add_color_stop_rgba (0, 0,	0,	    0, 1);
	left_pattern->add_color_stop_rgba (1, 0,	0,	    0, 0);
	context->set_source (left_pattern);
	context->rectangle(0, 0, width, 6.*scale);
	context->fill ();

	render_children (area, context);
}

bool
CueMaster::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 1) {
			//TriggerBox::StopAllTriggers ();
			return true;
		}
		break;
	case GDK_ENTER_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			stop_shape->set_fill_color (UIConfiguration::instance().color("neutral:foregroundest"));
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			stop_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
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
CueMaster::_size_allocate (ArdourCanvas::Rect const & alloc)
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
	stop_shape->set (p);

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
CueMaster::set_default_colors ()
{
	set_fill_color (HSV (UIConfiguration::instance().color("theme:bg")).darker(0.25).color ());
	name_text->set_color (UIConfiguration::instance().color("neutral:foreground"));
	stop_shape->set_fill_color (UIConfiguration::instance().color("neutral:midground"));
}

void
CueMaster::ui_parameter_changed (std::string const& p)
{
	if (p == "color-file") {
		set_default_colors();
	}
}

