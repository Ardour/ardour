/*
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <inttypes.h>

#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <locale.h>
#include <unistd.h>
#include <float.h>
#include <iomanip>

#include <glibmm.h>

#include "pbd/cartesian.h"
#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/stateful.h"
#include "pbd/xml++.h"

#include "evoral/Curve.h"

#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_manager.h"
#include "ardour/panner_shell.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/speakers.h"

#include "pbd/i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PannerShell::PannerShell (string name, Session& s, boost::shared_ptr<Pannable> p, bool is_send)
	: SessionObject (s, name)
	, _pannable_route (p)
	, _is_send (is_send)
	, _panlinked (true)
	, _bypassed (false)
	, _current_panner_uri("")
	, _user_selected_panner_uri("")
	, _panner_gui_uri("")
	, _force_reselect (false)
{
	if (is_send) {
		_pannable_internal.reset(new Pannable (s));
		if (Config->get_link_send_and_route_panner()) {
			_panlinked = true;
		} else {
			_panlinked = false;
		}
	}
	set_name (name);
}

PannerShell::~PannerShell ()
{
	DEBUG_TRACE(DEBUG::Destruction, string_compose ("panner shell %3 for %1 destructor, panner is %4, pannable is %2\n", _name, _pannable_route, this, _panner));
}

void
PannerShell::configure_io (ChanCount in, ChanCount out)
{
	uint32_t nouts = out.n_audio();
	uint32_t nins = in.n_audio();

	/* if new and old config don't need panning, or if
	   the config hasn't changed, we're done.
	*/

	if (!_force_reselect && _panner && (_panner->in().n_audio() == nins) && (_panner->out().n_audio() == nouts)) {
		return;
	}
	_force_reselect = false;

	if (nouts < 2 || nins == 0) {
		/* no need for panning with less than 2 outputs or no inputs */
		if (_panner) {
			_panner.reset ();
			_current_panner_uri = "";
			_panner_gui_uri = "";
			if (!_is_send || !_panlinked) {
				pannable()->set_panner(_panner);
			}
			Changed (); /* EMIT SIGNAL */
		}
		return;
	}

	PannerInfo* pi = PannerManager::instance().select_panner (in, out, _user_selected_panner_uri);
	if (!pi) {
		fatal << _("No panner found: check that panners are being discovered correctly during startup.") << endmsg;
		abort(); /*NOTREACHED*/
	}
	if (Stateful::loading_state_version < 6000 && pi->descriptor.in == 2) {
		_user_selected_panner_uri = pi->descriptor.panner_uri;
	}

	DEBUG_TRACE (DEBUG::Panning, string_compose (_("select panner: %1\n"), pi->descriptor.name.c_str()));

	boost::shared_ptr<Speakers> speakers = _session.get_speakers ();

	if (nouts != speakers->size()) {
		/* hmm, output count doesn't match session speaker count so
		   create a new speaker set.
		*/
		Speakers* s = new Speakers ();
		s->setup_default_speakers (nouts);
		speakers.reset (s);
	}

	/* TODO  don't allow to link  _is_send if internal & route panners are different types */
	Panner* p = pi->descriptor.factory (pannable(), speakers);
	// boost_debug_shared_ptr_mark_interesting (p, "Panner");
	_panner.reset (p);
	_panner->configure_io (in, out);
	_current_panner_uri = pi->descriptor.panner_uri;
	_panner_gui_uri = pi->descriptor.gui_uri;

	if (!_is_send || !_panlinked) {
		pannable()->set_panner(_panner);
	}
	Changed (); /* EMIT SIGNAL */
}

XMLNode&
PannerShell::get_state ()
{
	XMLNode* node = new XMLNode ("PannerShell");

	node->set_property (X_("bypassed"), _bypassed);
	node->set_property (X_("user-panner"), _user_selected_panner_uri);
	node->set_property (X_("linked-to-route"), _panlinked);

	if (_panner && _is_send) {
		node->add_child_nocopy (_panner->get_state ());
	}

	return *node;
}

int
PannerShell::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children ();
	XMLNodeConstIterator niter;
	bool yn;
	std::string str;

	if (node.get_property (X_("bypassed"), yn)) {
		set_bypassed (yn);
	}

	if (node.get_property (X_("linked-to-route"), yn)) {
		_panlinked = yn;
	}

	node.get_property (X_("user-panner"), _user_selected_panner_uri);

	_panner.reset ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("Panner")) {
			if ((*niter)->get_property (X_("uri"), str)) {
				PannerInfo* p = PannerManager::instance().get_by_uri(str);
				if (p) {
					_panner.reset (p->descriptor.factory (
								_is_send ? _pannable_internal : _pannable_route, _session.get_speakers ()));
					_current_panner_uri = p->descriptor.panner_uri;
					_panner_gui_uri = p->descriptor.gui_uri;
					if (_is_send) {
						if (!_panlinked) {
							_pannable_internal->set_panner(_panner);
						} else {
							_force_reselect = true;
						}
					} else {
						_pannable_route->set_panner(_panner);
					}
					if (_panner->set_state (**niter, version) == 0) {
						return -1;
					}
				}
			}

			else /* backwards compatibility */
			if ((*niter)->get_property (X_("type"), str)) {

				list<PannerInfo*>::iterator p;
				PannerManager& pm (PannerManager::instance());

				for (p = pm.panner_info.begin(); p != pm.panner_info.end(); ++p) {
					if (str == (*p)->descriptor.name) {

						/* note that we assume that all the stream panners
						   are of the same type. pretty good
						   assumption, but it's still an assumption.
						*/

						_panner.reset ((*p)->descriptor.factory (
									_is_send ? _pannable_internal : _pannable_route, _session.get_speakers ()));
						_current_panner_uri = (*p)->descriptor.panner_uri;
						_panner_gui_uri = (*p)->descriptor.gui_uri;

						if (_is_send) {
							if (!_panlinked) {
								_pannable_internal->set_panner(_panner);
							} else {
								_force_reselect = true;
							}
						} else {
							_pannable_route->set_panner(_panner);
						}

						if (_panner->set_state (**niter, version) == 0) {
							return -1;
						}

						break;
					}
				}

				if (p == pm.panner_info.end()) {
					error << string_compose (_("Unknown panner plugin \"%1\" found in pan state - ignored"),
					                         str)
					      << endmsg;
				}

			} else {
				error << _("panner plugin node has no type information!")
				      << endmsg;
				return -1;
			}
		}
	}

	return 0;
}


void
PannerShell::distribute_no_automation (BufferSet& inbufs, BufferSet& outbufs, pframes_t nframes, gain_t gain_coeff)
{
	if (outbufs.count().n_audio() == 0) {
		// Don't want to lose audio...
		assert(inbufs.count().n_audio() == 0);
		return;
	}

	if (outbufs.count().n_audio() == 1) {

		/* just one output: no real panning going on */

		AudioBuffer& dst = outbufs.get_audio(0);

		if (gain_coeff == GAIN_COEFF_ZERO) {

			/* gain was zero, so make it silent */

			dst.silence (nframes);

		} else if (gain_coeff == GAIN_COEFF_UNITY){

			/* mix all input buffers into the output */

			// copy the first
			dst.read_from(inbufs.get_audio(0), nframes);

			// accumulate starting with the second
			if (inbufs.count().n_audio() > 0) {
				BufferSet::audio_iterator i = inbufs.audio_begin();
				for (++i; i != inbufs.audio_end(); ++i) {
					dst.merge_from(*i, nframes);
				}
			}

		} else {

			/* mix all buffers into the output, scaling them all by the gain */

			// copy the first
			dst.read_from(inbufs.get_audio(0), nframes);

			// accumulate (with gain) starting with the second
			if (inbufs.count().n_audio() > 0) {
				BufferSet::audio_iterator i = inbufs.audio_begin();
				for (++i; i != inbufs.audio_end(); ++i) {
					dst.accumulate_with_gain_from(*i, nframes, gain_coeff);
				}
			}

		}

		return;
	}

	/* multiple outputs ... we must have a panner */

	assert (_panner);

	/* setup silent buffers so that we can mix into the outbuffers (slightly suboptimal -
	   better to copy the first set of data then mix after that, but hey, its 2011)
	*/

	for (BufferSet::audio_iterator b = outbufs.audio_begin(); b != outbufs.audio_end(); ++b) {
		(*b).silence (nframes);
	}

	_panner->distribute (inbufs, outbufs, gain_coeff, nframes);
}

void
PannerShell::run (BufferSet& inbufs, BufferSet& outbufs, samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes)
{
	if (inbufs.count().n_audio() == 0) {
		/* Input has no audio buffers (e.g. Aux Send in a MIDI track at a
		   point with no audio because there is no preceding instrument)
		*/
		outbufs.silence(nframes, 0);
		return;
	}

	if (outbufs.count().n_audio() == 0) {
		// Failing to deliver audio we were asked to deliver is a bug
		assert(inbufs.count().n_audio() == 0);
		return;
	}

	if (outbufs.count().n_audio() == 1) {

		/* one output only: no panner */

		AudioBuffer& dst = outbufs.get_audio(0);

		// FIXME: apply gain automation?

		// copy the first
		dst.read_from (inbufs.get_audio(0), nframes);

		// accumulate starting with the second
		BufferSet::audio_iterator i = inbufs.audio_begin();
		for (++i; i != inbufs.audio_end(); ++i) {
			dst.merge_from (*i, nframes);
		}

		return;
	}

	// More than 1 output

	AutoState as = pannable ()->automation_state ();

	// If we shouldn't play automation defer to distribute_no_automation

	if (!((as & Play) || ((as & (Touch | Latch)) && !pannable ()->touching ()))) {

		distribute_no_automation (inbufs, outbufs, nframes, 1.0);

	} else {

		/* setup the terrible silence so that we can mix into the outbuffers (slightly suboptimal -
		   better to copy the first set of data then mix after that, but hey, its 2011)
		*/
		for (BufferSet::audio_iterator i = outbufs.audio_begin(); i != outbufs.audio_end(); ++i) {
			i->silence(nframes);
		}

		_panner->distribute_automated (inbufs, outbufs, start_sample, end_sample, nframes, _session.pan_automation_buffer());
	}
}

void
PannerShell::set_bypassed (bool yn)
{
	if (yn == _bypassed) {
		return;
	}

	_bypassed = yn;
	_session.set_dirty ();
	Changed (); /* EMIT SIGNAL */
}

bool
PannerShell::bypassed () const
{
	return _bypassed;
}

/* set custom-panner config
 *
 * This function is intended to be only called from
 * Route::set_custom_panner()
 * which will trigger IO-reconfigutaion if this fn return true
 */
bool
PannerShell::set_user_selected_panner_uri (std::string const uri)
{
	if (uri == _user_selected_panner_uri) return false;
	_user_selected_panner_uri = uri;
	if (uri == _current_panner_uri) return false;
	_force_reselect = true;
	return true;
}

bool
PannerShell::select_panner_by_uri (std::string const uri)
{
	if (uri == _user_selected_panner_uri) return false;
	_user_selected_panner_uri = uri;
	if (uri == _current_panner_uri) return false;
	_force_reselect = true;
	if (_panner) {
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
			ChanCount in = _panner->in();
			ChanCount out = _panner->out();
			configure_io(in, out);
			if (!_is_send || !_panlinked) {
				pannable()->set_panner(_panner);
			}
			_session.set_dirty ();
	}
	return true;
}

void
PannerShell::set_linked_to_route (bool onoff)
{
	assert(_is_send);
	if (onoff == _panlinked) {
		return;
	}

	/* set _pannable-_has_state = true
	 * this way the panners will pick it up
	 * when it is re-created
	 */
	if (pannable()) {
		XMLNode state = pannable()->get_state();
		pannable()->set_state(state, Stateful::loading_state_version);
	}

	_panlinked = onoff;

	_force_reselect = true;
	if (_panner) {
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
			ChanCount in = _panner->in();
			ChanCount out = _panner->out();
			configure_io(in, out);
			if (!_panlinked) {
				pannable()->set_panner(_panner);
			}
			_session.set_dirty ();
	}
	PannableChanged();
}
