/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_vca_time_axis_h__
#define __ardour_vca_time_axis_h__

#include "ardour_button.h"
#include "time_axis_view.h"
#include "gain_meter.h"

namespace ArdourCanvas {
	class Canvas;
}

namespace ARDOUR {
	class Session;
	class VCA;
}

class VCATimeAxisView : public TimeAxisView
{
  public:
	VCATimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
	virtual ~VCATimeAxisView ();

	void set_vca (boost::shared_ptr<ARDOUR::VCA>);
	boost::shared_ptr<ARDOUR::VCA> vca() const { return _vca; }

	std::string name() const;
	std::string state_id() const;

	bool selectable() const { return false; }

 protected:
	boost::shared_ptr<ARDOUR::VCA> _vca;
	ArdourButton  solo_button;
	ArdourButton  mute_button;
	ArdourButton  spill_button;
	ArdourButton  drop_button;
	ArdourButton  number_label;
	GainMeterBase gain_meter;
	PBD::ScopedConnectionList vca_connections;

	void parameter_changed (std::string const& p);
	void vca_property_changed (PBD::PropertyChange const&);
	void update_vca_name ();
	void set_button_names ();
	void update_solo_display ();
	void update_mute_display ();
	void update_track_number_visibility ();
	bool solo_release (GdkEventButton*);
	bool mute_release (GdkEventButton*);
	bool spill_release (GdkEventButton*);
	bool drop_release (GdkEventButton*);
	void self_delete ();
};

#endif /* __ardour_route_time_axis_h__ */
