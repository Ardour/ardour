/*
    Copyright (C) 2000 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "ardour/automation_control.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
Route::set_control (AutomationType type, double val, PBD::Controllable::GroupControlDisposition group_override)
{
	boost::shared_ptr<RouteList> rl;

	switch (type) {
	case GainAutomation:
		/* route must mediate group control */
		set_gain (val, group_override);
		break;

	case TrimAutomation:
		/* route must mediate group control */
		set_trim (val, group_override);
		break;

	case RecEnableAutomation:
		/* session must mediate group control */
		rl.reset (new RouteList);
		rl->push_back (shared_from_this());
		_session.set_record_enabled (rl, val >= 0.5 ? true : false, Session::rt_cleanup, group_override);
		break;

	case SoloAutomation:
		/* session must mediate group control */
		rl.reset (new RouteList);
		rl->push_back (shared_from_this());
		if (Config->get_solo_control_is_listen_control()) {
			_session.set_listen (rl, val >= 0.5 ? true : false, Session::rt_cleanup, group_override);
		} else {
			_session.set_solo (rl, val >= 0.5 ? true : false, Session::rt_cleanup, group_override);
		}
		break;

	case MuteAutomation:
		/* session must mediate group control */
		rl.reset (new RouteList);
		rl->push_back (shared_from_this());
		_session.set_mute (rl, val >= 0.5 ? true : false, Session::rt_cleanup, group_override);
		return;
		break;

	default:
		/* Not a route automation control */
		fatal << string_compose (_("programming error: %1%2\n"), X_("illegal type of route automation control passed to Route::set_control(): "), enum_2_string(type)) << endmsg;
		/*NOTREACHED*/
		return;
	}
}


Route::RouteAutomationControl::RouteAutomationControl (const std::string& name,
                                                       AutomationType atype,
                                                       boost::shared_ptr<AutomationList> alist,
                                                       boost::shared_ptr<Route> r)
	: AutomationControl (r->session(), Evoral::Parameter (atype),
	                     ParameterDescriptor (Evoral::Parameter (atype)),
	                     alist, name)
	, _route (r)
{
}

double
Route::BooleanRouteAutomationControl::get_masters_value_locked () const
{
	/* masters (read/write) lock must be held */

	/* if any master is enabled (val > 0.0) then we consider the master
	   value to be 1.0
	*/

	for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		if (mr->second.master()->get_value()) {
			return 1.0;
		}
	}

	return 0.0;
}



Route::GainControllable::GainControllable (Session& s, AutomationType atype, boost::shared_ptr<Route> r)
	: GainControl (s, Evoral::Parameter(atype))
	, _route (r)
{

}

Route::SoloControllable::SoloControllable (std::string name, boost::shared_ptr<Route> r)
	: BooleanRouteAutomationControl (name, SoloAutomation, boost::shared_ptr<AutomationList>(), r)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(SoloAutomation)));
	gl->set_interpolation(Evoral::ControlList::Discrete);
	set_list (gl);
}

void
Route::SoloControllable::set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (writable()) {
		_set_value (val, group_override);
	}
}

void
Route::SoloControllable::_set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return;
	}
	r->set_control (SoloAutomation, val, group_override);
}

void
Route::SoloControllable::set_value_unchecked (double val)
{
	/* Used only by automation playback */

	_set_value (val, Controllable::NoGroup);
}

double
Route::SoloControllable::get_value () const
{
	std::cerr << "RSC get value\n";

	if (slaved()) {
		std::cerr << "slaved solo control, get master value ... ";
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		double v = get_masters_value_locked () ? GAIN_COEFF_UNITY : GAIN_COEFF_ZERO;
		std::cerr << v << std::endl;
	}

	if (_list && ((AutomationList*)_list.get())->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	boost::shared_ptr<Route> r = _route.lock ();

	if (!r) {
		return 0;
	}

	if (Config->get_solo_control_is_listen_control()) {
		return r->listening_via_monitor() ? GAIN_COEFF_UNITY : GAIN_COEFF_ZERO;
	} else {
		return r->self_soloed() ? GAIN_COEFF_UNITY : GAIN_COEFF_ZERO;
	}
}

Route::MuteControllable::MuteControllable (std::string name, boost::shared_ptr<Route> r)
	: BooleanRouteAutomationControl (name, MuteAutomation, boost::shared_ptr<AutomationList>(), r)
	, _route (r)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(MuteAutomation)));
	gl->set_interpolation(Evoral::ControlList::Discrete);
	set_list (gl);
}

void
Route::MuteControllable::set_superficial_value(bool muted)
{
	/* Note we can not use AutomationControl::set_value here since it will emit
	   Changed(), but the value will not be correct to the observer. */

	const bool to_list = _list && ((AutomationList*)_list.get ())->automation_write ();
	const double where = _session.audible_frame ();
	if (to_list) {
		/* Note that we really need this:
		 *  if (as == Touch && _list->in_new_write_pass ()) {
		 *       alist->start_write_pass (_session.audible_frame ());
		 *  }
		 * here in the case of the user calling from a GUI or whatever.
		 * Without the ability to distinguish between user and
		 * automation-initiated changes, we lose the "touch mute"
		 * behaviour we have in AutomationController::toggled ().
		 */
		_list->set_in_write_pass (true, false, where);
	}

	Control::set_double (muted, where, to_list);
}

void
Route::MuteControllable::set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (writable()) {
		_set_value (val, group_override);
	}
}

void
Route::MuteControllable::set_value_unchecked (double val)
{
	/* used only automation playback */
	_set_value (val, Controllable::NoGroup);
}

void
Route::MuteControllable::_set_value (double val, Controllable::GroupControlDisposition group_override)
{
	boost::shared_ptr<Route> r = _route.lock ();

	if (!r) {
		return;
	}

	if (_list && ((AutomationList*)_list.get())->automation_playback()) {
		// Set superficial/automation value to drive controller (and possibly record)
		const bool bval = ((val >= 0.5) ? true : false);
		set_superficial_value (bval);
		// Playing back automation, set route mute directly
		r->set_mute (bval, Controllable::NoGroup);
	} else {
		r->set_control (MuteAutomation, val, group_override);
	}
}

double
Route::MuteControllable::get_value () const
{
	if (slaved()) {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_masters_value_locked () ? GAIN_COEFF_UNITY : GAIN_COEFF_ZERO;
	}

	if (_list && ((AutomationList*)_list.get())->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	// Not playing back automation, get the actual route mute value
	boost::shared_ptr<Route> r = _route.lock ();
	return (r && r->muted()) ? GAIN_COEFF_UNITY : GAIN_COEFF_ZERO;
}

Route::PhaseControllable::PhaseControllable (std::string name, boost::shared_ptr<Route> r)
	: BooleanRouteAutomationControl (name, PhaseAutomation, boost::shared_ptr<AutomationList>(), r)
	, _current_phase (0)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(PhaseAutomation)));
	gl->set_interpolation(Evoral::ControlList::Discrete);
	set_list (gl);
}

void
Route::PhaseControllable::set_value (double v, PBD::Controllable::GroupControlDisposition /* group_override */)
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (r->phase_invert().size()) {
		if (v == 0 || (v < 1 && v > 0.9) ) {
			r->set_phase_invert (_current_phase, false);
		} else {
			r->set_phase_invert (_current_phase, true);
		}
	}
}

double
Route::PhaseControllable::get_value () const
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return 0.0;
	}
	return (double) r->phase_invert (_current_phase);
}

void
Route::PhaseControllable::set_channel (uint32_t c)
{
	_current_phase = c;
}

uint32_t
Route::PhaseControllable::channel () const
{
	return _current_phase;
}

Route::SoloIsolateControllable::SoloIsolateControllable (std::string name, boost::shared_ptr<Route> r)
	: BooleanRouteAutomationControl (name, SoloIsolateAutomation, boost::shared_ptr<AutomationList>(), r)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(SoloIsolateAutomation)));
	gl->set_interpolation(Evoral::ControlList::Discrete);
	set_list (gl);
}


double
Route::SoloIsolateControllable::get_value () const
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return 0.0; /* "false" */
	}

	return r->solo_isolated() ? 1.0 : 0.0;
}

void
Route::SoloIsolateControllable::set_value (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	_set_value (val, gcd);
}

void
Route::SoloIsolateControllable::_set_value (double val, PBD::Controllable::GroupControlDisposition)
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return;
	}

	/* no group semantics yet */
	r->set_solo_isolated (val >= 0.5 ? true : false);
}

Route::SoloSafeControllable::SoloSafeControllable (std::string name, boost::shared_ptr<Route> r)
	: BooleanRouteAutomationControl (name, SoloSafeAutomation, boost::shared_ptr<AutomationList>(), r)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(SoloSafeAutomation)));
	gl->set_interpolation(Evoral::ControlList::Discrete);
	set_list (gl);
}

void
Route::SoloSafeControllable::set_value (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	_set_value (val, gcd);
}

void
Route::SoloSafeControllable::_set_value (double val, PBD::Controllable::GroupControlDisposition)
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return;
	}

	/* no group semantics yet */
	r->set_solo_safe (val >= 0.5 ? true : false);
}

double
Route::SoloSafeControllable::get_value () const
{
	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return 0.0; /* "false" */
	}

	return r->solo_safe() ? 1.0 : 0.0;
}
