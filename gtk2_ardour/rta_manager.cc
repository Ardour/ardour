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
	return *node;
}

void
RTAManager::set_session (ARDOUR::Session* s)
{
	if (!s) {
		return;
	}
	SessionHandlePtr::set_session (s);

	XMLNode* node = _session->instant_xml(X_("RTAManager"));
	if (node) {
		node->get_property ("rta-warp", _warp);
		node->get_property ("rta-speed", _speed);
		SettingsChanged (); /* EMIT SIGNAL */
	}

	if (_session->master_out ()) {
		attach (_session->master_out ());
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
			r.analyzer.reset ();
			//r.ringbuffer->increment_read_idx (r.ringbuffer->read_space ());
			memset (r.analyzer.ipdata (), 0, r.blocksize * sizeof (float));
			r.route->main_outs ()->set_analysis_active (true);
		}
	} else {
		for (auto const& r : _rta) {
			r.route->main_outs ()->set_analysis_active (false);
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
		r.analyzer.set_speed (s);
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
		r.analyzer.set_wfact (w);
	}
	SettingsChanged (); /* EMIT SIGNAL */
}

void
RTAManager::attach (std::shared_ptr<ARDOUR::Route> route)
{
	for (auto const& r : _rta) {
		if (r.route == route) {
			return;
		}
	}
	_rta.emplace_back (route);
	_rta.back ().analyzer.set_speed (_speed);
	_rta.back ().analyzer.set_wfact (_warp);
	_rta.back ().route->main_outs ()->set_analysis_active (_active);

	route->DropReferences.connect (*this, invalidator (*this), std::bind (&RTAManager::route_removed, this, std::weak_ptr<Route>(route)), gui_context());
}

void
RTAManager::remove (std::shared_ptr<ARDOUR::Route> route)
{
	_rta.remove_if ([route] (RTAManager::RTA const& r) { return r.route == route; });
}

bool
RTAManager::attached (std::shared_ptr<ARDOUR::Route> route) const
{
	return std::find_if (_rta.begin (), _rta.end (), [route] (RTAManager::RTA const& r) { return r.route == route; }) != _rta.end ();
}

void
RTAManager::route_removed (std::weak_ptr<Route> wr)
{
	if (!_session || _session->inital_connect_or_deletion_in_progress ()) {
		return;
	}
  std::shared_ptr<Route> route (wr.lock());
  if (route) {
		remove (route);
  }
}

void
RTAManager::run_rta ()
{
	bool have_new_data = false;

	for (auto& r : _rta) {
		const size_t blocksize = r.blocksize;
		const size_t stepsize  = r.stepsize;
		RTA::RTABufferList& rbl = *r.ringbuffers;
		// TODO max of all
		while (rbl[0]->read_space () >= stepsize) {
			rbl[0]->read (r.analyzer.ipdata () + r.offset, stepsize);
			r.analyzer.process (stepsize);
			r.offset = (r.offset + stepsize) % blocksize;
			have_new_data |= r.analyzer.power ()->_valid;
		}
	}

	if (have_new_data) {
		SignalReady (); /* EMIT SIGNAL */
	}
}

/* ****************************************************************************/

RTAManager::RTA::RTA (std::shared_ptr<ARDOUR::Route> r)
	: route (r)
	, rate (r->session ().nominal_sample_rate ())
	, blocksize (rate > 64000 ? 16384 : 8192)
	, stepsize (blocksize / 4) // must be >= PerceptualAnalyzer::fftlen (512) and <= blocksize and a power of two
	, offset (0)
	, analyzer (rate, blocksize)
{
	ringbuffers = RTABufferListPtr (new RTABufferList (), std::bind (&rt_safe_delete<RTABufferList>, &r->session (), _1));
	for (uint32_t n = 0; n < route->main_outs ()->output_buffers().count().n_audio(); ++n) {
		ringbuffers->emplace_back (new RTARingBuffer (rate), std::bind (&rt_safe_delete<RTARingBuffer>, &r->session (), _1));
	}

	route->main_outs ()->set_analysis_buffers (ringbuffers);
	route->main_outs ()->set_analysis_active (true);
	analyzer.set_speed (ARDOUR::DSP::PerceptualAnalyzer::Moderate);
}

RTAManager::RTA::~RTA ()
{
	route->main_outs ()->set_analysis_active (false);
	RTABufferListPtr unset;
	route->main_outs ()->set_analysis_buffers (unset); // XXX need shared_ptr LIST!
}
