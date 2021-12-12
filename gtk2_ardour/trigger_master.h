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

#ifndef __ardour_trigger_master_h__
#define __ardour_trigger_master_h__

#include <map>

#include <gtkmm/window.h>

#include "pbd/properties.h"

#include "ardour/triggerbox.h"

#include "canvas/table.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

namespace Gtk {
class FileChooserDialog;
class Menu;
}

namespace Temporal {
	struct BBT_Offset;
}

namespace ArdourCanvas {
	class Text;
	class Polygon;
};

class TriggerMaster : public ArdourCanvas::Rectangle
{
  public:
	TriggerMaster (ArdourCanvas::Item* canvas, boost::shared_ptr<ARDOUR::TriggerBox>);
	~TriggerMaster ();

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const & alloc);

	ArdourCanvas::Rectangle* active_bar;
	ArdourCanvas::Polygon* stop_shape;
	ArdourCanvas::Text*    name_text;

	void maybe_update ();
	bool event_handler (GdkEvent*);
	void selection_change ();

  private:
	boost::shared_ptr<ARDOUR::TriggerBox> _triggerbox;
	double poly_size;
	double poly_margin;

	PBD::ScopedConnection trigger_prop_connection;
	void prop_change (PBD::PropertyChange const & change);
	void shape_stop_button ();

	PBD::ScopedConnection owner_prop_connection;
	void owner_prop_change (PBD::PropertyChange const &);

	void ui_parameter_changed (std::string const& p);
	void set_default_colors();
};


class CueMaster : public ArdourCanvas::Rectangle
{
  public:
	CueMaster (ArdourCanvas::Item* canvas);
	~CueMaster ();

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const & alloc);

	ArdourCanvas::Polygon* stop_shape;
	ArdourCanvas::Text*    name_text;

	void maybe_update ();
	bool event_handler (GdkEvent*);

  private:
	double poly_size;
	double poly_margin;

	void shape_stop_button ();

	void ui_parameter_changed (std::string const& p);
	void set_default_colors();
};
#endif /* __ardour_trigger_master_h__ */
