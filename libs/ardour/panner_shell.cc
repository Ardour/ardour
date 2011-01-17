/*
    Copyright (C) 2004-2011 Paul Davis

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

#include <inttypes.h>

#include <cmath>
#include <cerrno>
#include <fstream>
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
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "evoral/Curve.hpp"

#include "ardour/audio_buffer.h"
#include "ardour/audio_buffer.h"
#include "ardour/automatable.h"
#include "ardour/buffer_set.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_manager.h"
#include "ardour/panner_shell.h"
#include "ardour/runtime_functions.h"
#include "ardour/session.h"
#include "ardour/utils.h"

#include "i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PannerShell::PannerShell (string name, Session& s, boost::shared_ptr<Pannable> p)
	: SessionObject (s, name)
        , _pannable (p)
{
	set_name (name);
}

PannerShell::~PannerShell ()
{
}

void
PannerShell::configure_io (ChanCount in, ChanCount out)
{
        uint32_t nouts = out.n_audio();
        uint32_t nins = in.n_audio();

	/* if new and old config don't need panning, or if
	   the config hasn't changed, we're done.
	*/

	if (_panner && _panner->in().n_audio() == nins && _panner->out().n_audio() == nouts) {
                return;
	}

	if (nouts < 2 || nins == 0) {
		/* no need for panning with less than 2 outputs or no inputs */
		if (_panner) {
                        _panner.reset ();
			Changed (); /* EMIT SIGNAL */
		}
		return;
	}

        PannerInfo* pi = PannerManager::instance().select_panner (in, out);

        if (pi == 0) {
                abort ();
        }

        _panner.reset (pi->descriptor.factory (_pannable, _session.get_speakers()));

        Changed (); /* EMIT SIGNAL */
}

XMLNode&
PannerShell::get_state (void)
{
	return state (true);
}

XMLNode&
PannerShell::state (bool full)
{
	XMLNode* node = new XMLNode ("PannerShell");

        if (_panner) {
		node->add_child_nocopy (_panner->state (full));
	}

	return *node;
}

int
PannerShell::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children ();
	XMLNodeConstIterator niter;
	const XMLProperty *prop;
	LocaleGuard lg (X_("POSIX"));

        _panner.reset ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("Panner")) {

			if ((prop = (*niter)->property (X_("type")))) {

                                list<PannerInfo*>::iterator p;
                                PannerManager& pm (PannerManager::instance());

				for (p = pm.panner_info.begin(); p != pm.panner_info.end(); ++p) {
					if (prop->value() == (*p)->descriptor.name) {

						/* note that we assume that all the stream panners
						   are of the same type. pretty good
						   assumption, but it's still an assumption.
						*/
                                                        
                                                _panner.reset ((*p)->descriptor.factory (_pannable, _session.get_speakers ()));
                                                
						if (_panner->set_state (**niter, version) == 0) {
                                                        return -1;
                                                }

						break;
					}
				}

				if (p == pm.panner_info.end()) {
					error << string_compose (_("Unknown panner plugin \"%1\" found in pan state - ignored"),
							  prop->value())
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

		if (gain_coeff == 0.0f) {

			/* gain was zero, so make it silent */

			dst.silence (nframes);

		} else if (gain_coeff == 1.0f){

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
PannerShell::run (BufferSet& inbufs, BufferSet& outbufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes)
{
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

        AutoState as = _panner->automation_state ();

	// If we shouldn't play automation defer to distribute_no_automation

	if (!(as & Play || ((as & Touch) && !_panner->touching()))) {

		// Speed quietning
		gain_t gain_coeff = 1.0;
                
		if (fabsf(_session.transport_speed()) > 1.5f && Config->get_quieten_at_speed ()) {
			gain_coeff = speed_quietning;
		}

		distribute_no_automation (inbufs, outbufs, nframes, gain_coeff);

	} else {

                /* setup the terrible silence so that we can mix into the outbuffers (slightly suboptimal -
                   better to copy the first set of data then mix after that, but hey, its 2011)
                */
                for (BufferSet::audio_iterator i = outbufs.audio_begin(); i != outbufs.audio_end(); ++i) {
                        i->silence(nframes);
                }
                
                _panner->distribute_automated (inbufs, outbufs, start_frame, end_frame, nframes, _session.pan_automation_buffer());
        }
}

