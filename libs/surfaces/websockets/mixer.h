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

#ifndef _ardour_surface_websockets_mixer_h_
#define _ardour_surface_websockets_mixer_h_

#include "component.h"
#include "typed_value.h"

struct ArdourMixerNotFoundException : public virtual std::runtime_error
{
	using std::runtime_error::runtime_error;
};

class ArdourMixerPlugin
{
public:
	ArdourMixerPlugin (boost::shared_ptr<ARDOUR::PluginInsert>);

	boost::shared_ptr<ARDOUR::PluginInsert> insert () const;

	bool enabled () const;
	void set_enabled (bool);

	TypedValue param_value (uint32_t);
	void       set_param_value (uint32_t, TypedValue);

	boost::shared_ptr<ARDOUR::AutomationControl> param_control (uint32_t) const;

	static TypedValue param_value (boost::shared_ptr<ARDOUR::AutomationControl>);

private:
	boost::shared_ptr<ARDOUR::PluginInsert>      _insert;
	boost::shared_ptr<PBD::ScopedConnectionList> _connections;

};

class ArdourMixerStrip
{
public:
	ArdourMixerStrip (boost::shared_ptr<ARDOUR::Stripable>);

	boost::shared_ptr<ARDOUR::Stripable> stripable () const;
	boost::shared_ptr<PBD::ScopedConnectionList> connections () const;

	int                plugin_count () const;
	ArdourMixerPlugin& nth_plugin (uint32_t);

	double gain () const;
	void   set_gain (double);

	double pan () const;
	void   set_pan (double);

	bool mute () const;
	void set_mute (bool);

	std::string name () const;

	float meter_level_db () const;

	static double to_db (double);
	static double from_db (double);

private:
	boost::shared_ptr<ARDOUR::Stripable>         _stripable;
	boost::shared_ptr<PBD::ScopedConnectionList> _connections;
	std::vector<ArdourMixerPlugin>               _plugins;

	void on_drop_plugin (uint32_t);

};

class ArdourMixer : public SurfaceComponent
{
public:
	ArdourMixer (ArdourSurface::ArdourWebsockets& surface)
	    : SurfaceComponent (surface){};
	virtual ~ArdourMixer (){};

	int start ();
	int stop ();

	uint32_t          strip_count () const;
	ArdourMixerStrip& nth_strip (uint32_t);
	void              on_drop_strip (uint32_t);

private:
	typedef std::vector<ArdourMixerStrip> StripsVector;
	StripsVector                         _strips;
};

#endif // _ardour_surface_websockets_mixer_h_
