/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "pbd/types_convert.h"

#include "ardour/rt_safe_delete.h"
#include "ardour/session.h"

#include "gui_thread.h"
#include "rta_manager.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RTAManager* RTAManager::_instance = 0;

RTAManager*
RTAManager::instance ()
{
	if (!_instance) {
		_instance = new RTAManager;
	}
	return _instance;
}

RTAManager::RTAManager ()
	: _active (false)
	, _speed (ARDOUR::DSP::PerceptualAnalyzer::Moderate)
	, _warp (ARDOUR::DSP::PerceptualAnalyzer::Medium)
{
}

RTAManager::~RTAManager ()
{
}

XMLNode&
RTAManager::get_state () const
{
	XMLNode* node = new XMLNode ("RTAManager");
	node->set_property (X_("rta-warp"), _warp);
	node->set_property (X_("rta-speed"), _speed);
	for (auto& r : _rta) {
		node->add_child (X_("RouteID"))->set_property (X_("id"), r.route ()->id ());
	}
	return *node;
}

void
RTAManager::set_session (ARDOUR::Session* s)
{
	if (!s) {
		return;
	}
	SessionHandlePtr::set_session (s);

	XMLNode* node = _session->instant_xml (X_("RTAManager"));
	if (node) {
		node->get_property ("rta-warp", _warp);
		node->get_property ("rta-speed", _speed);
		SettingsChanged (); /* EMIT SIGNAL */
	}

	if (_session->master_out ()) {
		attach (_session->master_out ());
	}

	if (node) {
		for (auto const& n : node->children ()) {
			if (n->name () != X_("RouteID")) {
				continue;
			}
			PBD::ID id;
			if (!n->get_property (X_("id"), id)) {
				continue;
			}
			std::shared_ptr<ARDOUR::Route> route = _session->route_by_id (id);
			if (route) {
				attach (route);
			}
		}
	}

	_update_connection = Timers::super_rapid_connect (sigc::mem_fun (*this, &RTAManager::run_rta));
}

void
RTAManager::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RTAManager::session_going_away);
	_update_connection.disconnect ();
	_rta.clear ();

	SessionHandlePtr::session_going_away ();
	_session = 0;
}

void
RTAManager::set_active (bool en)
{
	if (_active == en) {
		return;
	}
	_active = en;
	if (_active) {
		for (auto& r : _rta) {
			r.reset ();
			r.delivery ()->set_analysis_active (true);
		}
	} else {
		for (auto const& r : _rta) {
			r.delivery ()->set_analysis_active (false);
		}
	}
}

void
RTAManager::set_rta_speed (DSP::PerceptualAnalyzer::Speed s)
{
	if (_speed == s) {
		return;
	}
	_speed = s;
	for (auto& r : _rta) {
		r.set_rta_speed (s);
	}
	SettingsChanged (); /* EMIT SIGNAL */
}

void
RTAManager::set_rta_warp (DSP::PerceptualAnalyzer::Warp w)
{
	if (_warp == w) {
		return;
	}
	_warp = w;
	for (auto& r : _rta) {
		r.set_rta_warp (w);
	}
	SettingsChanged (); /* EMIT SIGNAL */
}

void
RTAManager::attach (std::shared_ptr<ARDOUR::Route> route)
{
	for (auto const& r : _rta) {
		if (r.route () == route) {
			return;
		}
	}
	try {
		_rta.emplace_back (route);
	} catch (...) {
		return;
	}

	_rta.back ().set_rta_speed (_speed);
	_rta.back ().set_rta_warp (_warp);
	_rta.back ().delivery ()->set_analysis_active (_active);

	route->gui_changed ("rta", this); /* EMIT SIGNAL */
	route->DropReferences.connect (*this, invalidator (*this), std::bind (&RTAManager::route_removed, this, std::weak_ptr<Route> (route)), gui_context ());
}

void
RTAManager::remove (std::shared_ptr<ARDOUR::Route> route)
{
	_rta.remove_if ([route] (RTAManager::RTA const& r) { return r.route () == route; });
	route->gui_changed ("rta", this); /* EMIT SIGNAL */

	if (_rta.empty ()) {
		SignalReady (); /* EMIT SIGNAL */
	}
}

bool
RTAManager::attached (std::shared_ptr<ARDOUR::Route> route) const
{
	return std::find_if (_rta.begin (), _rta.end (), [route] (RTAManager::RTA const& r) { return r.route () == route; }) != _rta.end ();
}

void
RTAManager::route_removed (std::weak_ptr<Route> wr)
{
	if (!_session || _session->inital_connect_or_deletion_in_progress ()) {
		return;
	}
	std::shared_ptr<Route> route (wr.lock ());
	if (route) {
		remove (route);
	}
}

void
RTAManager::run_rta ()
{
	bool have_new_data = false;

	for (auto& r : _rta) {
		have_new_data |= r.run ();
	}

	if (have_new_data) {
		SignalReady (); /* EMIT SIGNAL */
	}
}

/* ****************************************************************************/

RTAManager::RTA::RTA (std::shared_ptr<ARDOUR::Route> r)
	: _route (r)
	, _rate (r->session ().nominal_sample_rate ())
	, _blocksize (_rate > 64000 ? 16384 : 8192)
	, _stepsize (_blocksize / 4) // must be >= PerceptualAnalyzer::fftlen (512) and <= blocksize and a power of two
	, _offset (0)
	, _speed (ARDOUR::DSP::PerceptualAnalyzer::Moderate)
	, _warp (ARDOUR::DSP::PerceptualAnalyzer::Medium)
{
	if (!init ()) {
		throw failed_constructor ();
	}
	_route->io_changed.connect (_route_connections, invalidator (*this), std::bind (&RTAManager::RTA::route_io_changed, this), gui_context ());
	_route->processors_changed.connect (_route_connections, invalidator (*this), std::bind (&RTAManager::RTA::route_io_changed, this), gui_context ());
}

RTAManager::RTA::~RTA ()
{
	_route_connections.drop_connections ();
	delivery ()->set_analysis_active (false);
	RTABufferListPtr unset;
	delivery ()->set_analysis_buffers (unset);
}

std::shared_ptr<Route>
RTAManager::RTA::route () const
{
	return _route;
}

std::shared_ptr<Delivery>
RTAManager::RTA::delivery () const
{
	return _route->main_outs ();
}

std::vector<ARDOUR::DSP::PerceptualAnalyzer*> const&
RTAManager::RTA::analyzers () const
{
	return _analyzers;
}

bool
RTAManager::RTA::init ()
{
	for (auto a : _analyzers) {
		delete a;
	}
	_analyzers.clear ();
	_ringbuffers.reset ();

	uint32_t n_audio    = delivery ()->input_streams ().n_audio ();
	bool     was_active = delivery ()->analysis_active ();

	delivery ()->set_analysis_active (false);

	if (n_audio == 0) {
		return false;
	}

	_ringbuffers = RTABufferListPtr (new RTABufferList (), std::bind (&rt_safe_delete<RTABufferList>, &_route->session (), _1));
	for (uint32_t n = 0; n < n_audio; ++n) {
		_ringbuffers->emplace_back (new RTARingBuffer (_rate), std::bind (&rt_safe_delete<RTARingBuffer>, &_route->session (), _1));
		_analyzers.emplace_back (new PerceptualAnalyzer (_rate, _blocksize));
		_analyzers.back ()->set_speed (_speed);
		_analyzers.back ()->set_speed (_warp);
	}

	assert (_analyzers.size () == _ringbuffers->size ());

	delivery ()->set_analysis_buffers (_ringbuffers);
	delivery ()->set_analysis_active (was_active);
	return true;
}

void
RTAManager::RTA::route_io_changed ()
{
	if (_analyzers.size () != delivery ()->input_streams ().n_audio ()) {
		init ();
	}
}

void
RTAManager::RTA::reset ()
{
	for (auto const& a : _analyzers) {
		a->reset ();
		memset (a->ipdata (), 0, _blocksize * sizeof (float));
	}
	for (auto& r : *_ringbuffers) {
		r->increment_read_idx (r->read_space ());
	}
}

void
RTAManager::RTA::set_rta_speed (ARDOUR::DSP::PerceptualAnalyzer::Speed s)
{
	_speed = s;
	for (auto& a : _analyzers) {
		a->set_speed (s);
	}
}

void
RTAManager::RTA::set_rta_warp (ARDOUR::DSP::PerceptualAnalyzer::Warp w)
{
	_warp = w;
	for (auto& a : _analyzers) {
		a->set_wfact (w);
	}
}

bool
RTAManager::RTA::run ()
{
	bool have_new_data = false;

	while (true) {
		for (auto& rb : *_ringbuffers) {
			if (rb->read_space () < _stepsize) {
				goto out;
			}
		}
		/* we can process all channels */
		auto rtaiter = _analyzers.begin ();
		for (auto& rb : *_ringbuffers) {
			rb->read ((*rtaiter)->ipdata () + _offset, _stepsize);
			(*rtaiter)->process (_stepsize);
			have_new_data |= (*rtaiter)->power ()->_valid;
			++rtaiter;
		}
		_offset = (_offset + _stepsize) % _blocksize;
	}
out:
	return have_new_data;
}
