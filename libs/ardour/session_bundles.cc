/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 GZharun <grygoriiz@wavesglobal.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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

#include <cstdio>

#include <boost/algorithm/string/erase.hpp>

#include "pbd/i18n.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/bundle.h"
#include "ardour/session.h"
#include "ardour/user_bundle.h"

using namespace ARDOUR;
using std::string;

void
Session::add_bundle (boost::shared_ptr<Bundle> bundle, bool emit_signal)
{
	{
		RCUWriter<BundleList> writer (_bundles);
		boost::shared_ptr<BundleList> b = writer.get_copy ();
		b->push_back (bundle);
	}

	if (emit_signal) {
		BundleAddedOrRemoved (); /* EMIT SIGNAL */
		set_dirty();
	}
}

void
Session::remove_bundle (boost::shared_ptr<Bundle> bundle)
{
	bool removed = false;

	{
		RCUWriter<BundleList> writer (_bundles);
		boost::shared_ptr<BundleList> b = writer.get_copy ();
		BundleList::iterator i = find (b->begin(), b->end(), bundle);

		if (i != b->end()) {
			b->erase (i);
			removed = true;
		}
	}

	if (removed) {
		 BundleAddedOrRemoved (); /* EMIT SIGNAL */
	}

	set_dirty();
}

boost::shared_ptr<Bundle>
Session::bundle_by_name (string name) const
{
	boost::shared_ptr<BundleList> b = _bundles.reader ();

	for (BundleList::const_iterator i = b->begin(); i != b->end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return boost::shared_ptr<Bundle> ();
}

void
Session::setup_bundles ()
{

	{
		RCUWriter<BundleList> writer (_bundles);
		boost::shared_ptr<BundleList> b = writer.get_copy ();
		for (BundleList::iterator i = b->begin(); i != b->end();) {
			if (boost::dynamic_pointer_cast<UserBundle>(*i)) {
				++i;
				continue;
			}
			i = b->erase(i);
		}
	}

	std::vector<string> inputs[DataType::num_types];
	std::vector<string> outputs[DataType::num_types];

	for (uint32_t i = 0; i < DataType::num_types; ++i) {
		get_physical_ports (inputs[i], outputs[i], DataType (DataType::Symbol (i)),
		                    MidiPortFlags (0), /* no specific inclusions */
		                    MidiPortFlags (MidiPortControl|MidiPortVirtual) /* exclude control & virtual ports */
			);
	}

	/* now add virtual Vkeybd, compare to PortGroupList::gather */
	if (_midi_ports) {
		boost::shared_ptr<Port> ap = boost::dynamic_pointer_cast<Port> (vkbd_output_port ());
		inputs[DataType::MIDI].push_back (AudioEngine::instance()->make_port_name_non_relative (ap->name ()));

		/* JACK semantics prevent us directly calling the
		   pretty-name/metadata API from a server callback, and this is
		   called from a port registration callback. So defer to the
		   auto-connect thread, which does this sort of thing anyway.
		*/

		g_atomic_int_set (&_update_pretty_names, 1);
		auto_connect_thread_wakeup ();
	}

	/* Create a set of Bundle objects that map
	   to the physical I/O currently available.  We create both
	   mono and stereo bundles, so that the common cases of mono
	   and stereo tracks get bundles to put in their mixer strip
	   in / out menus.  There may be a nicer way of achieving that;
	   it doesn't really scale that well to higher channel counts
	*/

	/* mono output bundles */

	for (uint32_t np = 0; np < outputs[DataType::AUDIO].size(); ++np) {
		char buf[64];
		std::string pn = _engine.get_pretty_name_by_name (outputs[DataType::AUDIO][np]);
		if (!pn.empty()) {
			snprintf (buf, sizeof (buf), _("out %s"), pn.c_str());
		} else {
			snprintf (buf, sizeof (buf), _("out %" PRIu32), np+1);
		}

		boost::shared_ptr<Bundle> c (new Bundle (buf, true));
		c->add_channel (_("mono"), DataType::AUDIO);
		c->set_port (0, outputs[DataType::AUDIO][np]);

		add_bundle (c, false);
	}

	/* stereo output bundles */

	for (uint32_t np = 0; np < outputs[DataType::AUDIO].size(); np += 2) {
		if (np + 1 < outputs[DataType::AUDIO].size()) {
			char buf[32];
			snprintf (buf, sizeof(buf), _("out %" PRIu32 "+%" PRIu32), np + 1, np + 2);
			boost::shared_ptr<Bundle> c (new Bundle (buf, true));
			c->add_channel (_("L"), DataType::AUDIO);
			c->set_port (0, outputs[DataType::AUDIO][np]);
			c->add_channel (_("R"), DataType::AUDIO);
			c->set_port (1, outputs[DataType::AUDIO][np + 1]);

			add_bundle (c, false);
		}
	}

	/* mono input bundles */

	for (uint32_t np = 0; np < inputs[DataType::AUDIO].size(); ++np) {
		char buf[64];
		std::string pn = _engine.get_pretty_name_by_name (inputs[DataType::AUDIO][np]);
		if (!pn.empty()) {
			snprintf (buf, sizeof (buf), _("in %s"), pn.c_str());
		} else {
			snprintf (buf, sizeof (buf), _("in %" PRIu32), np+1);
		}

		boost::shared_ptr<Bundle> c (new Bundle (buf, false));
		c->add_channel (_("mono"), DataType::AUDIO);
		c->set_port (0, inputs[DataType::AUDIO][np]);

		add_bundle (c, false);
	}

	/* stereo input bundles */

	for (uint32_t np = 0; np < inputs[DataType::AUDIO].size(); np += 2) {
		if (np + 1 < inputs[DataType::AUDIO].size()) {
			char buf[32];
			snprintf (buf, sizeof(buf), _("in %" PRIu32 "+%" PRIu32), np + 1, np + 2);

			boost::shared_ptr<Bundle> c (new Bundle (buf, false));
			c->add_channel (_("L"), DataType::AUDIO);
			c->set_port (0, inputs[DataType::AUDIO][np]);
			c->add_channel (_("R"), DataType::AUDIO);
			c->set_port (1, inputs[DataType::AUDIO][np + 1]);

			add_bundle (c, false);
		}
	}

	/* MIDI input bundles */

	for (uint32_t np = 0; np < inputs[DataType::MIDI].size(); ++np) {
		string n = inputs[DataType::MIDI][np];

		std::string pn = _engine.get_pretty_name_by_name (n);
		if (!pn.empty()) {
			n = pn;
		} else {
			boost::erase_first (n, X_("alsa_pcm:"));
		}
		boost::shared_ptr<Bundle> c (new Bundle (n, false));
		c->add_channel ("", DataType::MIDI);
		c->set_port (0, inputs[DataType::MIDI][np]);
		add_bundle (c, false);
	}

	/* MIDI output bundles */

	for (uint32_t np = 0; np < outputs[DataType::MIDI].size(); ++np) {
		string n = outputs[DataType::MIDI][np];
		std::string pn = _engine.get_pretty_name_by_name (n);
		if (!pn.empty()) {
			n = pn;
		} else {
			boost::erase_first (n, X_("alsa_pcm:"));
		}
		boost::shared_ptr<Bundle> c (new Bundle (n, true));
		c->add_channel ("", DataType::MIDI);
		c->set_port (0, outputs[DataType::MIDI][np]);
		add_bundle (c, false);
	}

	// we trust the backend to only calls us if there's a change
	BundleAddedOrRemoved (); /* EMIT SIGNAL */
}
