/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#include <glibmm/threads.h>

#include "component.h"
#include "typed_value.h"

namespace ArdourSurface {

class ArdourMixerNotFoundException : public std::runtime_error
{
public:
	ArdourMixerNotFoundException (std::string const & what)
		: std::runtime_error (what)
		, _what (what)
	{}

	~ArdourMixerNotFoundException() throw() {}

	const char* what() const throw() { return _what.c_str(); }

private:
	std::string _what;
};

class ArdourMixerPlugin : public PBD::ScopedConnectionList
{
public:
	ArdourMixerPlugin (boost::shared_ptr<ARDOUR::PluginInsert>);
	~ArdourMixerPlugin ();

	boost::shared_ptr<ARDOUR::PluginInsert> insert () const;
	
	bool enabled () const;
	void set_enabled (bool);

	uint32_t   param_count () const;
	TypedValue param_value (uint32_t);
	void       set_param_value (uint32_t, TypedValue);

	boost::shared_ptr<ARDOUR::AutomationControl> param_control (uint32_t) const;

	static TypedValue param_value (boost::shared_ptr<ARDOUR::AutomationControl>);

private:
	boost::shared_ptr<ARDOUR::PluginInsert> _insert;
};

class ArdourMixerStrip : public PBD::ScopedConnectionList
{
public:
	ArdourMixerStrip (boost::shared_ptr<ARDOUR::Stripable>, PBD::EventLoop*);
	~ArdourMixerStrip ();
	
	boost::shared_ptr<ARDOUR::Stripable> stripable () const;

	typedef std::map<uint32_t, boost::shared_ptr<ArdourMixerPlugin> > PluginMap;

	PluginMap&         plugins ();
	ArdourMixerPlugin& plugin (uint32_t);

	double gain () const;
	void   set_gain (double);

	bool   has_pan () const;
	double pan () const;
	void   set_pan (double);

	bool mute () const;
	void set_mute (bool);

	std::string name () const;

	float meter_level_db () const;

	static double to_db (double);
	static double from_db (double);

	static int to_velocity (double);
	static double from_velocity (int);

private:
	boost::shared_ptr<ARDOUR::Stripable> _stripable;

	PluginMap _plugins;

	bool is_midi () const;

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

	typedef std::map<uint32_t, boost::shared_ptr<ArdourMixerStrip> > StripMap;

	StripMap&         strips ();
	ArdourMixerStrip& strip (uint32_t);
	void              on_drop_strip (uint32_t);

	Glib::Threads::Mutex& mutex ();

private:
	StripMap             _strips;
	Glib::Threads::Mutex _mutex;
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_mixer_h_
