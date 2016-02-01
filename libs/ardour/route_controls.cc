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
		return;
		break;

	case TrimAutomation:
		/* route must mediate group control */
		set_trim (val, group_override);
		return;
		break;

	case RecEnableAutomation:
		/* session must mediate group control */
		rl.reset (new RouteList);
		rl->push_back (shared_from_this());
		_session.set_record_enabled (rl, val >= 0.5 ? true : false, Session::rt_cleanup, group_override);
		return;
		break;

	case SoloAutomation:
		/* session must mediate group control */
		rl.reset (new RouteList);
		rl->push_back (shared_from_this());
		if (Config->get_solo_control_is_listen_control()) {
			_session.set_listen (rl, val >= 0.5 ? true : false, Session::rt_cleanup, group_override);
		} else {
			_session.set_solo (rl, val >= 0.5 ? true : false);
		}

		return;
		break;

	case MuteAutomation:
		/* session must mediate group control */
		rl.reset (new RouteList);
		rl->push_back (shared_from_this());
		_session.set_mute (rl, !muted(), Session::rt_cleanup, group_override);
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

Route::RouteAutomationControl::RouteAutomationControl (const std::string& name,
                                                       AutomationType atype,
                                                       const ParameterDescriptor& desc,
                                                       boost::shared_ptr<AutomationList> alist,
                                                       boost::shared_ptr<Route> r)
	: AutomationControl (r->session(), Evoral::Parameter (atype), desc, alist, name)
	, _route (r)
{
}

Route::GainControllable::GainControllable (Session& s, AutomationType atype, boost::shared_ptr<Route> r)
	: GainControl (s, Evoral::Parameter(atype))
	, _route (r)
{

}

Route::SoloControllable::SoloControllable (std::string name, boost::shared_ptr<Route> r)
	: RouteAutomationControl (name, SoloAutomation, boost::shared_ptr<AutomationList>(), r)
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
	const bool bval = ((val >= 0.5) ? true : false);

	boost::shared_ptr<RouteList> rl (new RouteList);

	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return;
	}

	rl->push_back (r);

	if (Config->get_solo_control_is_listen_control()) {
		_session.set_listen (rl, bval, Session::rt_cleanup, group_override);
	} else {
		_session.set_solo (rl, bval, Session::rt_cleanup, group_override);
	}
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
	: RouteAutomationControl (name, MuteAutomation, boost::shared_ptr<AutomationList>(), r)
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
	const bool bval = ((val >= 0.5) ? true : false);

	boost::shared_ptr<Route> r = _route.lock ();
	if (!r) {
		return;
	}

	if (_list && ((AutomationList*)_list.get())->automation_playback()) {
		// Set superficial/automation value to drive controller (and possibly record)
		set_superficial_value (bval);
		// Playing back automation, set route mute directly
		r->set_mute (bval, Controllable::NoGroup);
	} else {
		// Set from user, queue mute event
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (r);
		_session.set_mute (rl, bval, Session::rt_cleanup, group_override);
	}
}

double
Route::MuteControllable::get_value () const
{
	if (_list && ((AutomationList*)_list.get())->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	// Not playing back automation, get the actual route mute value
	boost::shared_ptr<Route> r = _route.lock ();
	return (r && r->muted()) ? GAIN_COEFF_UNITY : GAIN_COEFF_ZERO;
}

Route::PhaseControllable::PhaseControllable (std::string name, boost::shared_ptr<Route> r)
	: RouteAutomationControl (name, PhaseAutomation, boost::shared_ptr<AutomationList>(), r)
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

