/*
    Copyright (C) 2006-2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>

#include "pbd/convert.h"
#include "pbd/strsplit.h"

#include "ardour/dB.h"
#include "ardour/gain_control.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

GainControl::GainControl (Session& session, const Evoral::Parameter &param, boost::shared_ptr<AutomationList> al)
	: AutomationControl (session, param, ParameterDescriptor(param),
	                     al ? al : boost::shared_ptr<AutomationList> (new AutomationList (param)),
	                     param.type() == GainAutomation ? X_("gaincontrol") : X_("trimcontrol")) {

	alist()->reset_default (1.0);

	lower_db = accurate_coefficient_to_dB (_desc.lower);
	range_db = accurate_coefficient_to_dB (_desc.upper) - lower_db;
}

gain_t
GainControl::get_value_locked () const {

	/* read or write masters lock must be held */

	if (_masters.empty()) {
		return AutomationControl::get_value();
	}

	gain_t g = 1.0;

	for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		/* get current master value, scale by our current ratio with that master */
		g *= mr->second.master()->get_value () * mr->second.ratio();
	}

	return min (Config->get_max_gain(), g);
}

double
GainControl::get_value () const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return get_value_locked ();
}

void
GainControl::set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (writable()) {
		_set_value (val, group_override);
	}
}

void
GainControl::set_value_unchecked (double val)
{
	/* used only automation playback */
	_set_value (val, Controllable::NoGroup);
}

void
GainControl::_set_value (double val, Controllable::GroupControlDisposition group_override)
{
	val = std::max (std::min (val, (double)_desc.upper), (double)_desc.lower);

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);

		if (!_masters.empty()) {
			recompute_masters_ratios (val);
		}
	}

	AutomationControl::set_value (val, group_override);

	_session.set_dirty ();
}

double
GainControl::internal_to_interface (double v) const
{
	if (_desc.type == GainAutomation) {
		return gain_to_slider_position (v);
	} else {
		return (accurate_coefficient_to_dB (v) - lower_db) / range_db;
	}
}

double
GainControl::interface_to_internal (double v) const
{
	if (_desc.type == GainAutomation) {
		return slider_position_to_gain (v);
	} else {
		return dB_to_coefficient (lower_db + v * range_db);
	}
}

double
GainControl::internal_to_user (double v) const
{
	return accurate_coefficient_to_dB (v);
}

double
GainControl::user_to_internal (double u) const
{
	return dB_to_coefficient (u);
}

std::string
GainControl::get_user_string () const
{
	char theBuf[32]; sprintf( theBuf, _("%3.1f dB"), accurate_coefficient_to_dB (get_value()));
	return std::string(theBuf);
}

gain_t
GainControl::get_master_gain () const
{
	Glib::Threads::RWLock::ReaderLock sm (master_lock, Glib::Threads::TRY_LOCK);

	if (sm.locked()) {
		return get_master_gain_locked ();
	}

	return 1.0;
}

gain_t
GainControl::get_master_gain_locked () const
{
	/* Master lock MUST be held (read or write lock is acceptable) */

	gain_t g = 1.0;

	for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		/* get current master value, scale by our current ratio with that master */
		g *= mr->second.master()->get_value () * mr->second.ratio();
	}

	return g;
}

void
GainControl::add_master (boost::shared_ptr<VCA> vca)
{
	gain_t current_value;
	std::pair<Masters::iterator,bool> res;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();

		/* ratio will be recomputed below */

		res = _masters.insert (make_pair<uint32_t,MasterRecord> (vca->number(), MasterRecord (vca->gain_control(), 0.0)));

		if (res.second) {

			recompute_masters_ratios (current_value);

			/* note that we bind @param m as a weak_ptr<GainControl>, thus
			   avoiding holding a reference to the control in the binding
			   itself.
			*/

			vca->DropReferences.connect_same_thread (masters_connections, boost::bind (&GainControl::master_going_away, this, vca));

			/* Store the connection inside the MasterRecord, so that when we destroy it, the connection is destroyed
			   and we no longer hear about changes to the VCA.
			*/

			vca->gain_control()->Changed.connect_same_thread (res.first->second.connection, boost::bind (&PBD::Signal0<void>::operator(), &Changed));
		}
	}

	if (res.second) {
		VCAStatusChange (); /* EMIT SIGNAL */
	}
}

void
GainControl::master_going_away (boost::weak_ptr<VCA> wv)
{
	boost::shared_ptr<VCA> v = wv.lock();
	if (v) {
		remove_master (v);
	}
}

void
GainControl::remove_master (boost::shared_ptr<VCA> vca)
{
	gain_t current_value;
	Masters::size_type erased = 0;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();
		erased = _masters.erase (vca->number());
		if (erased) {
			recompute_masters_ratios (current_value);
		}
	}

	if (erased) {
		VCAStatusChange (); /* EMIT SIGNAL */
	}
}

void
GainControl::clear_masters ()
{
	bool had_masters = false;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		if (!_masters.empty()) {
			had_masters = true;
		}
		_masters.clear ();
	}

	if (had_masters) {
		VCAStatusChange (); /* EMIT SIGNAL */
	}
}

void
GainControl::recompute_masters_ratios (double val)
{
	/* Master WRITE lock must be held */

	/* V' is the new gain value for this

	   Mv(n) is the return value of ::get_value() for the n-th master
	   Mr(n) is the return value of ::ratio() for the n-th master record

	   the slave should return V' on the next call to ::get_value().

	   but the value is determined by the masters, so we know:

	   V' = (Mv(1) * Mr(1)) * (Mv(2) * Mr(2)) * ... * (Mv(n) * Mr(n))

	   hence:

	   Mr(1) * Mr(2) * ... * (Mr(n) = V' / (Mv(1) * Mv(2) * ... * Mv(n))

	   if we make all ratios equal (i.e. each master contributes the same
	   fraction of its own gain level to make the final slave gain), then we
	   have:

	   pow (Mr(n), n) = V' / (Mv(1) * Mv(2) * ... * Mv(n))

	   which gives

	   Mr(n) = pow ((V' / (Mv(1) * Mv(2) * ... * Mv(n))), 1/n)

	   Mr(n) is the new ratio number for the slaves
	*/


	const double nmasters = _masters.size();
	double masters_total_gain_coefficient = 1.0;

	for (Masters::iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		masters_total_gain_coefficient *= mr->second.master()->get_value();
	}

	const double new_universal_ratio = pow ((val / masters_total_gain_coefficient), (1.0/nmasters));

	for (Masters::iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		mr->second.reset_ratio (new_universal_ratio);
	}
}

bool
GainControl::slaved_to (boost::shared_ptr<VCA> vca) const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return _masters.find (vca->number()) != _masters.end();
}

bool
GainControl::slaved () const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return !_masters.empty();
}

XMLNode&
GainControl::get_state ()
{
	XMLNode& node (AutomationControl::get_state());

	/* store VCA master IDs */

	string str;

	{
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
			if (!str.empty()) {
				str += ',';
			}
			str += PBD::to_string (mr->first, std::dec);
		}
	}

	if (!str.empty()) {
		node.add_property (X_("masters"), str);
	}

	return node;
}

int
GainControl::set_state (XMLNode const& node, int version)
{
	AutomationControl::set_state (node, version);

	XMLProperty const* prop = node.property (X_("masters"));

	/* Problem here if we allow VCA's to be slaved to other VCA's .. we
	 * have to load all VCAs first, then set up slave/master relationships
	 * once we have them all.
	 */

	if (prop) {
		masters_string = prop->value ();

		if (_session.vca_manager().vcas_loaded()) {
			vcas_loaded ();
		} else {
			_session.vca_manager().VCAsLoaded.connect_same_thread (vca_loaded_connection, boost::bind (&GainControl::vcas_loaded, this));
		}
	}

	return 0;
}

void
GainControl::vcas_loaded ()
{
	if (masters_string.empty()) {
		return;
	}

	vector<string> masters;
	split (masters_string, masters, ',');

	for (vector<string>::const_iterator m = masters.begin(); m != masters.end(); ++m) {
		boost::shared_ptr<VCA> vca = _session.vca_manager().vca_by_number (PBD::atoi (*m));
		if (vca) {
			add_master (vca);
		}
	}

	vca_loaded_connection.disconnect ();
	masters_string.clear ();
}
