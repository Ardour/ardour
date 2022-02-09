/*
 * Author Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef _gtk_ardour_trigger_master_h_
#define _gtk_ardour_trigger_master_h_

#include <gtkmm/colorselection.h>

#include "pbd/properties.h"

#include "ardour/triggerbox.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/table.h"

namespace Gtk
{
	class FileChooserDialog;
	class Menu;
}

namespace Temporal
{
	struct BBT_Offset;
}

namespace ArdourCanvas
{
	class Text;
	class Polygon;
}

class Loopster : public ArdourCanvas::Rectangle
{
public:
	Loopster (ArdourCanvas::Item* canvas);

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;
	void set_fraction (float);

private:
	float _fraction;
};

class TriggerMaster : public ArdourCanvas::Rectangle
{
public:
	TriggerMaster (ArdourCanvas::Item* canvas);
	~TriggerMaster ();

	void set_triggerbox (boost::shared_ptr<ARDOUR::TriggerBox>);

	void render (ArdourCanvas::Rect const&, Cairo::RefPtr<Cairo::Context>) const;

	void _size_allocate (ArdourCanvas::Rect const&);

	std::string   play_text;
	std::string   loop_text;

	void maybe_update ();
	bool event_handler (GdkEvent*);
	void selection_change ();

private:
	void context_menu ();

	void clear_all_triggers();
	void set_all_colors();
	void set_all_follow_action (ARDOUR::FollowAction const &);
	void set_all_launch_style (ARDOUR::Trigger::LaunchStyle);
	void set_all_quantization (Temporal::BBT_Offset const&);

	void prop_change (PBD::PropertyChange const& change);
	void owner_prop_change (PBD::PropertyChange const&);
	void ui_parameter_changed (std::string const& p);
	void set_default_colors ();
	void shape_stop_button ();

	boost::shared_ptr<ARDOUR::TriggerBox> _triggerbox;

	Loopster* _loopster;

	Gtk::ColorSelectionDialog _color_dialog;
	Gtk::Menu* _context_menu;
	bool       _ignore_menu_action;

	double _poly_size;
	double _poly_margin;

	PBD::ScopedConnection _trigger_prop_connection;
	PBD::ScopedConnection _owner_prop_connection;
	sigc::connection      _update_connection;
};

typedef std::list<boost::shared_ptr<ARDOUR::TriggerBox> > TriggerBoxList;

class CueMaster : public ArdourCanvas::Rectangle, public ARDOUR::SessionHandlePtr
{
public:
	CueMaster (ArdourCanvas::Item* canvas);
	~CueMaster ();

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const& alloc);

	ArdourCanvas::Polygon* stop_shape;

	void maybe_update ();
	bool event_handler (GdkEvent*);

private:
	void context_menu ();

	void get_boxen (TriggerBoxList &boxlist);
	void clear_all_triggers();
	void set_all_follow_action (ARDOUR::FollowAction const &);
	void set_all_launch_style (ARDOUR::Trigger::LaunchStyle);
	void set_all_quantization (Temporal::BBT_Offset const&);

	void ui_parameter_changed (std::string const& p);
	void set_default_colors ();
	void shape_stop_button ();

	Gtk::Menu* _context_menu;

	double _poly_size;
	double _poly_margin;
};

#endif
