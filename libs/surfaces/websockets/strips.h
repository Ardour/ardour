/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

#ifndef _ardour_surface_websockets_strips_h_
#define _ardour_surface_websockets_strips_h_

#include "component.h"
#include "typed_value.h"

class ArdourStrips : public SurfaceComponent
{
public:
	ArdourStrips (ArdourSurface::ArdourWebsockets& surface)
	    : SurfaceComponent (surface){};
	virtual ~ArdourStrips (){};

	int start ();
	int stop ();

	static double to_db (double);
	static double from_db (double);

	double strip_gain (uint32_t) const;
	void   set_strip_gain (uint32_t, double);

	double strip_pan (uint32_t) const;
	void   set_strip_pan (uint32_t, double);

	bool strip_mute (uint32_t) const;
	void set_strip_mute (uint32_t, bool);

	bool strip_plugin_enabled (uint32_t, uint32_t) const;
	void set_strip_plugin_enabled (uint32_t, uint32_t, bool);

	TypedValue strip_plugin_param_value (uint32_t, uint32_t, uint32_t) const;
	void       set_strip_plugin_param_value (uint32_t, uint32_t, uint32_t, TypedValue);

	uint32_t                             strip_count () const;
	boost::shared_ptr<ARDOUR::Stripable> nth_strip (uint32_t) const;

	boost::shared_ptr<ARDOUR::PluginInsert> strip_plugin_insert (uint32_t, uint32_t) const;

	boost::shared_ptr<ARDOUR::AutomationControl> strip_plugin_param_control (
	    uint32_t, uint32_t, uint32_t) const;

	static TypedValue plugin_param_value (boost::shared_ptr<ARDOUR::AutomationControl>);

private:
	typedef std::vector<boost::shared_ptr<ARDOUR::Stripable> > StripableVector;
	StripableVector                                           _strips;
};

#endif // _ardour_surface_websockets_strips_h_
