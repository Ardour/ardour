/*
 * Copyright (C) 2020-2021 Luciano Iam <oss@lucianoiam.com>
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

#ifndef _ardour_surface_websockets_feedback_h_
#define _ardour_surface_websockets_feedback_h_

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <glibmm/threads.h>

#include "pbd/abstract_ui.h"

#include "component.h"
#include "typed_value.h"
#include "mixer.h"

namespace ArdourSurface {

class FeedbackHelperUI : public AbstractUI<BaseUI::BaseRequestObject>
{
public:
	FeedbackHelperUI ();
	~FeedbackHelperUI () {};

protected:
	virtual void do_request (BaseUI::BaseRequestObject*);

};

class ArdourFeedback : public SurfaceComponent
{
public:
	ArdourFeedback (ArdourSurface::ArdourWebsockets& surface)
	    : SurfaceComponent (surface){};
	virtual ~ArdourFeedback (){};

	int start ();
	int stop ();

	void update_all (std::string, TypedValue) const;
	void update_all (std::string, uint32_t, TypedValue) const;
	void update_all (std::string, uint32_t, uint32_t, TypedValue) const;
	void update_all (std::string, uint32_t, uint32_t, uint32_t, TypedValue) const;

private:
	Glib::Threads::Mutex      _client_state_lock;
	PBD::ScopedConnectionList _transport_connections;
	sigc::connection          _periodic_connection;

	// Only needed for server event loop integration method #3
	mutable FeedbackHelperUI  _helper;

	PBD::EventLoop* event_loop () const;

	bool poll () const;

	void observe_transport ();
	void observe_mixer ();
	void observe_strip_plugins (uint32_t, ArdourMixerStrip::PluginMap&);
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_feedback_h_
