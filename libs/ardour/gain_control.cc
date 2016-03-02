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
	AutomationControl::set_value (std::max (std::min (val, (double)_desc.upper), (double)_desc.lower), group_override);
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
	Glib::Threads::Mutex::Lock sm (master_lock, Glib::Threads::TRY_LOCK);

	if (sm.locked()) {
		return get_master_gain_locked ();
	}

	return 1.0;
}

gain_t
GainControl::get_master_gain_locked () const
{
	/* Master lock MUST be held */

	gain_t g = 1.0;

	for (Masters::const_iterator m = _masters.begin(); m != _masters.end(); ++m) {
		g *= (*m)->get_value ();
	}

	return g;
}

void
GainControl::add_master (boost::shared_ptr<VCA> vca)
{
	gain_t old_master_val;
	gain_t new_master_val;
	std::pair<set<boost::shared_ptr<GainControl> >::iterator,bool> res;

	{
		Glib::Threads::Mutex::Lock lm (master_lock);
		old_master_val = get_master_gain_locked ();
		res = _masters.insert (vca->control());
		_masters_numbers.insert (vca->number());
		new_master_val = get_master_gain_locked ();

		/* note that we bind @param m as a weak_ptr<GainControl>, thus
		   avoiding holding a reference to the control in the binding
		   itself.
		*/

		vca->DropReferences.connect_same_thread (masters_connections, boost::bind (&GainControl::master_going_away, this, vca));

	}

	if (old_master_val != new_master_val) {
		Changed(); /* EMIT SIGNAL */
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
	gain_t old_master_val;
	gain_t new_master_val;
	set<boost::shared_ptr<GainControl> >::size_type erased = 0;

	{
		Glib::Threads::Mutex::Lock lm (master_lock);
		old_master_val = get_master_gain_locked ();
		erased = _masters.erase (vca->control());
		_masters_numbers.erase (vca->number());
		new_master_val = get_master_gain_locked ();
	}

	if (old_master_val != new_master_val) {
		Changed(); /* EMIT SIGNAL */
	}

	if (erased) {
		VCAStatusChange (); /* EMIT SIGNAL */
	}
}

void
GainControl::clear_masters ()
{
	gain_t old_master_val;
	gain_t new_master_val;
	bool had_masters = false;

	{
		Glib::Threads::Mutex::Lock lm (master_lock);
		old_master_val = get_master_gain_locked ();
		if (!_masters.empty()) {
			had_masters = true;
		}
		_masters.clear ();
		_masters_numbers.clear ();
		new_master_val = get_master_gain_locked ();
	}

	if (old_master_val != new_master_val) {
		Changed(); /* EMIT SIGNAL */
	}

	if (had_masters) {
		VCAStatusChange (); /* EMIT SIGNAL */
	}
}

bool
GainControl::slaved_to (boost::shared_ptr<VCA> vca) const
{
	Glib::Threads::Mutex::Lock lm (master_lock);
	return find (_masters.begin(), _masters.end(), vca->control()) != _masters.end();
}

XMLNode&
GainControl::get_state ()
{
	XMLNode& node (AutomationControl::get_state());

	/* store VCA master IDs */

	string str;

	{
		Glib::Threads::Mutex::Lock lm (master_lock);
		for (set<uint32_t>::const_iterator m = _masters_numbers.begin(); m != _masters_numbers.end(); ++m) {
			if (!str.empty()) {
				str += ',';
			}
			str += PBD::to_string (*m, std::dec);
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

	/* XXXProblem here if we allow VCA's to be slaved to other VCA's .. we
	 * have to load all VCAs first, then call ::set_state() so that
	 * vca_by_number() will succeed.
	 */

	if (prop) {
		vector<string> masters;
		split (prop->value(), masters, ',');

		for (vector<string>::const_iterator m = masters.begin(); m != masters.end(); ++m) {
			boost::shared_ptr<VCA> vca = _session.vca_manager().vca_by_number (PBD::atoi (*m));
			if (vca) {
				add_master (vca);
			}
		}
	}

	return 0;
}
