/*
 * Copyright (C) 2001-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
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
#include <fstream>

#include <errno.h>

#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>

#include "pbd/error.h"

#include "temporal/timeline.h"

#include "ardour/amp.h"
#include "ardour/automatable.h"
#include "ardour/event_type_map.h"
#include "ardour/gain_control.h"
#include "ardour/monitor_control.h"
#include "ardour/midi_track.h"
#include "ardour/pan_controllable.h"
#include "ardour/pannable.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/record_enable_control.h"
#include "ardour/session.h"
#include "ardour/uri_map.h"
#include "ardour/value_as_string.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* used for templates (previously: !full_state) */
bool Automatable::skip_saving_automation = false;

const string Automatable::xml_node_name = X_("Automation");

Automatable::Automatable(Session& session)
	: _a_session(session)
	, _automated_controls (new ControlList)
{
}

Automatable::Automatable (const Automatable& other)
	: ControlSet (other)
	, Slavable ()
	, _a_session (other._a_session)
	, _automated_controls (new ControlList)
{
	Glib::Threads::Mutex::Lock lm (other._control_lock);

	for (Controls::const_iterator i = other._controls.begin(); i != other._controls.end(); ++i) {
		boost::shared_ptr<Evoral::Control> ac (control_factory (i->first));
		add_control (ac);
	}
}

Automatable::~Automatable ()
{
	{
		RCUWriter<ControlList> writer (_automated_controls);
		boost::shared_ptr<ControlList> cl = writer.get_copy ();
		cl->clear ();
	}
	_automated_controls.flush ();

	Glib::Threads::Mutex::Lock lm (_control_lock);
	for (Controls::const_iterator li = _controls.begin(); li != _controls.end(); ++li) {
		boost::dynamic_pointer_cast<AutomationControl>(li->second)->drop_references ();
	}
}

int
Automatable::old_set_automation_state (const XMLNode& node)
{
	XMLProperty const * prop;

	if ((prop = node.property ("path")) != 0) {
		load_automation (prop->value());
	} else {
		warning << _("Automation node has no path property") << endmsg;
	}

	return 0;
}

int
Automatable::load_automation (const string& path)
{
	string fullpath;

	if (Glib::path_is_absolute (path)) { // legacy
		fullpath = path;
	} else {
		fullpath = _a_session.automation_dir();
		fullpath += path;
	}

	std::ifstream in (fullpath);

	if (in.bad()) {
		warning << string_compose(_("cannot open %2 to load automation data (%3)")
				, fullpath, strerror (errno)) << endmsg;
		return 1;
	}

	Glib::Threads::Mutex::Lock lm (control_lock());
	set<Evoral::Parameter> tosave;
	controls().clear ();

	while (!in.eof()) {
		Temporal::timepos_t when;
		double value;
		uint32_t port;

		in >> port;  if (in.bad()) { goto bad; }
		in >> when;  if (in.bad()) { goto bad; }
		in >> value; if (in.bad()) { goto bad; }

		Evoral::Parameter param(PluginAutomation, 0, port);
		/* FIXME: this is legacy and only used for plugin inserts?  I think? */
		boost::shared_ptr<Evoral::Control> c = control (param, true);
		c->list()->add (when, value);
		tosave.insert (param);
	}

	return 0;

bad:
	error << string_compose(_("cannot load automation data from %2"), fullpath) << endmsg;
	controls().clear ();
	return -1;
}

void
Automatable::add_control(boost::shared_ptr<Evoral::Control> ac)
{
	Evoral::Parameter param = ac->parameter();

	boost::shared_ptr<AutomationList> al = boost::dynamic_pointer_cast<AutomationList> (ac->list ());

	boost::shared_ptr<AutomationControl> actl (boost::dynamic_pointer_cast<AutomationControl> (ac));

	if ((!actl || !(actl->flags() & Controllable::NotAutomatable)) && al) {
		al->automation_state_changed.connect_same_thread (
			_list_connections,
			boost::bind (&Automatable::automation_list_automation_state_changed,
			             this, ac->parameter(), _1));
	}

	ControlSet::add_control (ac);

	if ((!actl || !(actl->flags() & Controllable::NotAutomatable)) && al) {
		if (!actl || !(actl->flags() & Controllable::HiddenControl)) {
			can_automate (param);
		}
		automation_list_automation_state_changed (param, al->automation_state ()); // sync everything up
	}
}

string
Automatable::describe_parameter (Evoral::Parameter param)
{
	/* derived classes like PluginInsert should override this */

	if (param == Evoral::Parameter(GainAutomation)) {
		return _("Fader");
	} else if (param.type() == BusSendLevel) {
		return _("Send");
	} else if (param.type() == TrimAutomation) {
		return _("Trim");
	} else if (param.type() == MainOutVolume) {
		return _("Master Volume");
	} else if (param.type() == MuteAutomation) {
		return _("Mute");
	} else if (param.type() == PanAzimuthAutomation) {
		return _("Azimuth");
	} else if (param.type() == PanWidthAutomation) {
		return _("Width");
	} else if (param.type() == PanElevationAutomation) {
		return _("Elevation");
	} else if (param.type() == MidiCCAutomation) {
		return string_compose("Controller %1 [%2]", param.id(), int(param.channel()) + 1);
	} else if (param.type() == MidiPgmChangeAutomation) {
		return string_compose("Program [%1]", int(param.channel()) + 1);
	} else if (param.type() == MidiPitchBenderAutomation) {
		return string_compose("Bender [%1]", int(param.channel()) + 1);
	} else if (param.type() == MidiChannelPressureAutomation) {
		return string_compose("Pressure [%1]", int(param.channel()) + 1);
	} else if (param.type() == MidiNotePressureAutomation) {
		return string_compose("PolyPressure [%1]", int(param.channel()) + 1);
	} else if (param.type() == PluginPropertyAutomation) {
		return string_compose("Property %1", URIMap::instance().id_to_uri(param.id()));
	} else {
		return EventTypeMap::instance().to_symbol(param);
	}
}

void
Automatable::can_automate (Evoral::Parameter what)
{
	_can_automate_list.insert (what);
}

std::vector<Evoral::Parameter>
Automatable::all_automatable_params () const
{
	return std::vector<Evoral::Parameter> (_can_automate_list.begin (), _can_automate_list.end ());
}

/** \a legacy_param is used for loading legacy sessions where an object (IO, Panner)
 * had a single automation parameter, with it's type implicit.  Derived objects should
 * pass that type and it will be used for the untyped AutomationList found.
 */
int
Automatable::set_automation_xml_state (const XMLNode& node, Evoral::Parameter legacy_param)
{
	Glib::Threads::Mutex::Lock lm (control_lock());

	/* Don't clear controls, since some may be special derived Controllable classes */

	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		/*if (sscanf ((*niter)->name().c_str(), "parameter-%" PRIu32, &param) != 1) {
		  error << string_compose (_("%2: badly formatted node name in XML automation state, ignored"), _name) << endmsg;
		  continue;
		  }*/

		if ((*niter)->name() == "AutomationList") {

			XMLProperty const * id_prop = (*niter)->property("automation-id");

			Evoral::Parameter param = (id_prop
					? EventTypeMap::instance().from_symbol(id_prop->value())
					: legacy_param);

			if (param.type() == NullAutomation) {
				warning << "Automation has null type" << endl;
				continue;
			}

			if (!id_prop) {
				warning << "AutomationList node without automation-id property, "
					<< "using default: " << EventTypeMap::instance().to_symbol(legacy_param) << endmsg;
			}

			if (_can_automate_list.find (param) == _can_automate_list.end ()) {
				boost::shared_ptr<AutomationControl> actl = automation_control (param);
				if (actl && (*niter)->children().size() > 0 && Config->get_limit_n_automatables () > 0) {
					actl->clear_flag (Controllable::NotAutomatable);
					if (!(actl->flags() & Controllable::HiddenControl) && actl->name() != X_("hidden")) {
						can_automate (param);
					}
					info << "Marked parmater as automatable" << endl;
				} else {
					warning << "Ignored automation data for non-automatable parameter" << endl;
					continue;
				}
			}


			boost::shared_ptr<AutomationControl> existing = automation_control (param);

			if (existing) {
				existing->alist()->set_state (**niter, Stateful::loading_state_version);
			} else {
				boost::shared_ptr<Evoral::Control> newcontrol = control_factory(param);
				add_control (newcontrol);
				boost::shared_ptr<AutomationList> al (new AutomationList(**niter, param));
				newcontrol->set_list(al);
			}

		} else {
			error << "Expected AutomationList node, got '" << (*niter)->name() << "'" << endmsg;
		}
	}

	return 0;
}

XMLNode&
Automatable::get_automation_xml_state ()
{
	Glib::Threads::Mutex::Lock lm (control_lock());
	XMLNode* node = new XMLNode (Automatable::xml_node_name);

	if (controls().empty()) {
		return *node;
	}

	for (Controls::iterator li = controls().begin(); li != controls().end(); ++li) {
		boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(li->second->list());
		if (l) {
			node->add_child_nocopy (l->get_state ());
		}
	}

	return *node;
}

void
Automatable::set_parameter_automation_state (Evoral::Parameter param, AutoState s)
{
	Glib::Threads::Mutex::Lock lm (control_lock());

	boost::shared_ptr<AutomationControl> c = automation_control (param, true);

	if (c && (s != c->automation_state())) {
		c->set_automation_state (s);
		_a_session.set_dirty ();
		AutomationStateChanged(); /* Emit signal */
	}
}

AutoState
Automatable::get_parameter_automation_state (Evoral::Parameter param)
{
	AutoState result = Off;

	boost::shared_ptr<AutomationControl> c = automation_control(param);

	if (c) {
		result = c->automation_state();
	}

	return result;
}

void
Automatable::protect_automation ()
{
	typedef set<Evoral::Parameter> ParameterSet;
	const ParameterSet& automated_params = what_can_be_automated ();

	for (ParameterSet::const_iterator i = automated_params.begin(); i != automated_params.end(); ++i) {

		boost::shared_ptr<Evoral::Control> c = control(*i);
		boost::shared_ptr<AutomationList> l = boost::dynamic_pointer_cast<AutomationList>(c->list());

		switch (l->automation_state()) {
		case Write:
			l->set_automation_state (Off);
			break;
		case Latch:
			/* fallthrough */
		case Touch:
			l->set_automation_state (Play);
			break;
		default:
			break;
		}
	}
}

void
Automatable::non_realtime_locate (samplepos_t now)
{
	bool rolling = _a_session.transport_rolling ();

	for (Controls::iterator li = controls().begin(); li != controls().end(); ++li) {

		boost::shared_ptr<AutomationControl> c
				= boost::dynamic_pointer_cast<AutomationControl>(li->second);
		if (c) {
			boost::shared_ptr<AutomationList> l
				= boost::dynamic_pointer_cast<AutomationList>(c->list());

			if (!l) {
				continue;
			}

			bool am_touching = c->touching ();
			if (rolling && am_touching) {
			/* when locating while rolling, and writing automation,
			 * start a new write pass.
			 * compare to compare to non_realtime_transport_stop()
			 */
				const bool list_did_write = !l->in_new_write_pass ();
#warning NUTEMPO check use of domain in arbitrary irrelevant time
				c->stop_touch (timepos_t::zero (Temporal::AudioTime)); // time is irrelevant
				l->stop_touch (timepos_t::zero (Temporal::AudioTime));
				c->commit_transaction (list_did_write);
				l->write_pass_finished (timepos_t (now), Config->get_automation_thinning_factor ());

				if (l->automation_state () == Write) {
					l->set_automation_state (Touch);
				}
				if (l->automation_playback ()) {
					c->set_value_unchecked (c->list ()->eval (timepos_t (now)));
				}
			}

			l->start_write_pass (timepos_t (now));

			if (rolling && am_touching) {
				c->start_touch (timepos_t (now));
			}
		}
	}
}

void
Automatable::non_realtime_transport_stop (samplepos_t now, bool /*flush_processors*/)
{
	for (Controls::iterator li = controls().begin(); li != controls().end(); ++li) {
		boost::shared_ptr<AutomationControl> c =
			boost::dynamic_pointer_cast<AutomationControl>(li->second);
		if (!c) {
			continue;
		}

		boost::shared_ptr<AutomationList> l =
			boost::dynamic_pointer_cast<AutomationList>(c->list());
		if (!l) {
			continue;
		}

		/* Stop any active touch gesture just before we mark the write pass
		   as finished.  If we don't do this, the transport can end up stopped with
		   an AutomationList thinking that a touch is still in progress and,
		   when the transport is re-started, a touch will magically
		   be happening without it ever have being started in the usual way.
		*/
		const bool list_did_write = !l->in_new_write_pass ();

		c->stop_touch (timepos_t (now));
		l->stop_touch (timepos_t (now));

		c->commit_transaction (list_did_write);

		l->write_pass_finished (timepos_t (now), Config->get_automation_thinning_factor ());

		if (l->automation_state () == Write) {
			l->set_automation_state (Touch);
		}

		if (l->automation_playback ()) {
			c->set_value_unchecked (c->list ()->eval (timepos_t (now)));
		}
	}
}

void
Automatable::automation_run (samplepos_t start, pframes_t nframes, bool only_active)
{
	if (only_active) {
		boost::shared_ptr<ControlList> cl = _automated_controls.reader ();
		for (ControlList::const_iterator ci = cl->begin(); ci != cl->end(); ++ci) {
			(*ci)->automation_run (start, nframes);
		}
		return;
	}

	for (Controls::iterator li = controls().begin(); li != controls().end(); ++li) {
		boost::shared_ptr<AutomationControl> c =
			boost::dynamic_pointer_cast<AutomationControl>(li->second);
		if (!c) {
			continue;
		}
		c->automation_run (start, nframes);
	}
}

void
Automatable::automation_list_automation_state_changed (Evoral::Parameter param, AutoState as)
{
	{
		boost::shared_ptr<AutomationControl> c (automation_control(param));
		assert (c && c->list());

		RCUWriter<ControlList> writer (_automated_controls);
		boost::shared_ptr<ControlList> cl = writer.get_copy ();

		ControlList::iterator fi = std::find (cl->begin(), cl->end(), c);
		if (fi != cl->end()) {
			cl->erase (fi);
		}
		switch (as) {
			/* all potential  automation_playback() states */
			case Play:
			case Touch:
			case Latch:
				cl->push_back (c);
				break;
			case Off:
			case Write:
				break;
		}
	}
	_automated_controls.flush();
}

boost::shared_ptr<Evoral::Control>
Automatable::control_factory(const Evoral::Parameter& param)
{
	Evoral::Control*                  control   = NULL;
	bool                              make_list = true;
	ParameterDescriptor               desc(param);
	boost::shared_ptr<AutomationList> list;

	if (parameter_is_midi (param.type())) {
		MidiTrack* mt = dynamic_cast<MidiTrack*>(this);
		if (mt) {
			control = new MidiTrack::MidiControl(mt, param);
			make_list = false;  // No list, this is region "automation"
		}
	} else if (param.type() == PluginAutomation) {
		PluginInsert* pi = dynamic_cast<PluginInsert*>(this);
		if (pi) {
			pi->plugin(0)->get_parameter_descriptor(param.id(), desc);
			control = new PluginInsert::PluginControl(pi, param, desc);
		} else {
			warning << "PluginAutomation for non-Plugin" << endl;
		}
	} else if (param.type() == PluginPropertyAutomation) {
		PluginInsert* pi = dynamic_cast<PluginInsert*>(this);
		if (pi) {
			desc = pi->plugin(0)->get_property_descriptor(param.id());
			if (desc.datatype != Variant::NOTHING) {
				if (!Variant::type_is_numeric(desc.datatype)) {
					make_list = false;  // Can't automate non-numeric data yet
				} else {
					list = boost::shared_ptr<AutomationList>(new AutomationList(param, desc, Temporal::AudioTime));
				}
				control = new PluginInsert::PluginPropertyControl(pi, param, desc, list);
			}
		} else {
			warning << "PluginPropertyAutomation for non-Plugin" << endl;
		}
	} else if (param.type() == GainAutomation) {
		control = new GainControl(_a_session, param);
	} else if (param.type() == TrimAutomation) {
		control = new GainControl(_a_session, param);
	} else if (param.type() == MainOutVolume) {
		control = new GainControl(_a_session, param);
	} else if (param.type() == BusSendLevel) {
		control = new GainControl(_a_session, param);
	} else if (param.type() == PanAzimuthAutomation || param.type() == PanWidthAutomation || param.type() == PanElevationAutomation) {
		Pannable* pannable = dynamic_cast<Pannable*>(this);
		if (pannable) {
			control = new PanControllable (_a_session, describe_parameter (param), pannable, param);
		} else {
			warning << "PanAutomation for non-Pannable" << endl;
		}
	} else if (param.type() == RecEnableAutomation) {
		Recordable* re = dynamic_cast<Recordable*> (this);
		if (re) {
			control = new RecordEnableControl (_a_session, X_("recenable"), *re);
		}
	} else if (param.type() == MonitoringAutomation) {
		Monitorable* m = dynamic_cast<Monitorable*>(this);
		if (m) {
			control = new MonitorControl (_a_session, X_("monitor"), *m);
		}
	} else if (param.type() == SoloAutomation) {
		Soloable* s = dynamic_cast<Soloable*>(this);
		Muteable* m = dynamic_cast<Muteable*>(this);
		if (s && m) {
			control = new SoloControl (_a_session, X_("solo"), *s, *m);
		}
	} else if (param.type() == MuteAutomation) {
		Muteable* m = dynamic_cast<Muteable*>(this);
		if (m) {
			control = new MuteControl (_a_session, X_("mute"), *m);
		}
	}

	if (make_list && !list) {
#warning NUTEMPO what time domain to use here?
		list = boost::shared_ptr<AutomationList>(new AutomationList(param, desc, Temporal::AudioTime));
	}

	if (!control) {
		control = new AutomationControl(_a_session, param, desc, list);
	}

	return boost::shared_ptr<Evoral::Control>(control);
}

boost::shared_ptr<AutomationControl>
Automatable::automation_control (PBD::ID const & id) const
{
	Controls::const_iterator li;

	for (li = _controls.begin(); li != _controls.end(); ++li) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (li->second);
		if (ac && (ac->id() == id)) {
			return ac;
		}
	}

	return boost::shared_ptr<AutomationControl>();
}

boost::shared_ptr<AutomationControl>
Automatable::automation_control (const Evoral::Parameter& id, bool create)
{
	return boost::dynamic_pointer_cast<AutomationControl>(Evoral::ControlSet::control(id, create));
}

boost::shared_ptr<const AutomationControl>
Automatable::automation_control (const Evoral::Parameter& id) const
{
	return boost::dynamic_pointer_cast<const AutomationControl>(Evoral::ControlSet::control(id));
}

void
Automatable::clear_controls ()
{
	_control_connections.drop_connections ();
	ControlSet::clear_controls ();
}

bool
Automatable::find_next_event (timepos_t const & start, timepos_t const & end, Evoral::ControlEvent& next_event, bool only_active) const
{
	next_event.when = start <= end ? std::numeric_limits<double>::max() : 0;

	if (only_active) {
		boost::shared_ptr<ControlList> cl = _automated_controls.reader ();
		for (ControlList::const_iterator ci = cl->begin(); ci != cl->end(); ++ci) {
			if ((*ci)->automation_playback()) {
				if (start <= end) {
					find_next_ac_event (*ci, start, end, next_event);
				} else {
					find_prev_ac_event (*ci, start, end, next_event);
				}
			}
		}
	} else {
		for (Controls::const_iterator li = _controls.begin(); li != _controls.end(); ++li) {
			boost::shared_ptr<AutomationControl> c
				= boost::dynamic_pointer_cast<AutomationControl>(li->second);
			if (c) {
				if (start <= end) {
					find_next_ac_event (c, start, end, next_event);
				} else {
					find_prev_ac_event (c, start, end, next_event);
				}
			}
		}
	}
	return next_event.when != (start <= end ? std::numeric_limits<double>::max() : 0);
}

void
Automatable::find_next_ac_event (boost::shared_ptr<AutomationControl> c, timepos_t const & start, timepos_t const & end, Evoral::ControlEvent& next_event) const
{
	assert (start <= end);

	boost::shared_ptr<SlavableAutomationControl> sc
		= boost::dynamic_pointer_cast<SlavableAutomationControl>(c);

	if (sc) {
		sc->find_next_event (start, end, next_event);
	}

	boost::shared_ptr<const Evoral::ControlList> alist (c->list());
	Evoral::ControlEvent cp (start, 0.0f);
	if (!alist) {
		return;
	}

	Evoral::ControlList::const_iterator i = upper_bound (alist->begin(), alist->end(), &cp, Evoral::ControlList::time_comparator);

	if (i != alist->end() && (*i)->when < end) {
		if ((*i)->when < next_event.when) {
			next_event.when = (*i)->when;
		}
	}
}

void
Automatable::find_prev_ac_event (boost::shared_ptr<AutomationControl> c, timepos_t const & start, timepos_t const & end, Evoral::ControlEvent& next_event) const
{
	assert (start > end);
	boost::shared_ptr<SlavableAutomationControl> sc
		= boost::dynamic_pointer_cast<SlavableAutomationControl>(c);

	if (sc) {
		sc->find_next_event (start, end, next_event);
	}

	boost::shared_ptr<const Evoral::ControlList> alist (c->list());
	if (!alist) {
		return;
	}

	Evoral::ControlEvent cp (end, 0.0f);
	Evoral::ControlList::const_iterator i = upper_bound (alist->begin(), alist->end(), &cp, Evoral::ControlList::time_comparator);

	while (i != alist->end() && (*i)->when < start) {
		if ((*i)->when > next_event.when) {
			next_event.when = (*i)->when;
		}
		++i;
	}
}
