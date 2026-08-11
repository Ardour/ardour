/*
 * Copyright (C) 2026 Ada <ada@6bit.com>
 * Copyright (C) 2026 Joshua Perry <josh@6bit.com>
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

#include <stdexcept>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"
#include "control_protocol/control_protocol.h"

#include "komplete_kontrol_a.h"

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

static ControlProtocol*
new_komplete_kontrol_a (Session* s, std::string const& /* config */)
{
	KompleteKontrolA* kka = 0;

	try {
		kka = new KompleteKontrolA (*s);
	} catch (std::exception& e) {
		PBD::error << "Failed to instantiate Komplete Kontrol A-Series: " << e.what () << endmsg;
		delete kka;
		return 0;
	}

	/* Honour the result.  Ardour's activate() calls set_active() again, and
	 * our set_active() re-runs start() whenever the protocol is not already
	 * active -- so handing back an object that failed to start means the
	 * device is opened twice and every failure is logged twice.  Reporting
	 * the failure here instead gives one error and lets activate() fail
	 * cleanly, which is also what makes the GUI checkbox behave.
	 */
	if (kka->set_active (true)) {
		delete kka;
		return 0;
	}

	return kka;
}

static void
delete_komplete_kontrol_a (ControlProtocol* cp)
{
	delete cp;
}

static std::map<std::string, std::vector<std::string> >
enumerate_komplete_kontrol_a ()
{
	return { { "Native Instruments", { "Komplete Kontrol A25",
	                                   "Komplete Kontrol A49",
	                                   "Komplete Kontrol A61" } } };
}

static ControlProtocolDescriptor komplete_kontrol_a_descriptor = {
	/* name       */ "NI Komplete Kontrol A-Series",
	/* id         */ "uri://ardour.org/surfaces/komplete_kontrol_a:0",
	/* module     */ 0,
	/* available  */ KompleteKontrolA::available,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_komplete_kontrol_a,
	/* destroy    */ delete_komplete_kontrol_a,
	/* enumerate  */ enumerate_komplete_kontrol_a,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &komplete_kontrol_a_descriptor; }
