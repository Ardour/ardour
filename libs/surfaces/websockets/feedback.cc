/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/plugin_insert.h"
#include "ardour/meter.h"

#include "feedback.h"
#include "strips.h"
#include "globals.h"
#include "state.h"
#include "server.h"

using namespace ARDOUR;

typedef boost::function<void ()> SignalObserver;

int
ArdourFeedback::start ()
{
    observe_globals ();
    observe_strips ();

    // some things need polling like the strip meters
    Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (100); // ms
    _periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this,
        &ArdourFeedback::poll));
    periodic_timeout->attach (main_loop ()->get_context ());

    return 0;
}

int
ArdourFeedback::stop ()
{
    _periodic_connection.disconnect ();
    _signal_connections.drop_connections ();
    return 0;
}

bool
ArdourFeedback::poll () const
{
    for (uint32_t strip_n = 0; strip_n < strips ().strip_count (); ++strip_n) {
        // meters
        boost::shared_ptr<Stripable> strip = strips ().nth_strip (strip_n);
        boost::shared_ptr<PeakMeter> meter = strip->peak_meter ();
        float db = meter ? meter->meter_level (0, MeterMCP) : -193;
        update_all (Node::strip_meter, { strip_n }, static_cast<double>(db));
    }

    return true;
}

void
ArdourFeedback::observe_globals ()
{
    // tempo
    SignalObserver observer = [this] () {
        update_all (Node::tempo, {}, globals ().tempo ());
    };

    session ().tempo_map ().PropertyChanged.connect (_signal_connections, MISSING_INVALIDATOR,
        boost::bind<void> (observer), event_loop ());
}

void
ArdourFeedback::observe_strips ()
{
    for (uint32_t strip_n = 0; strip_n < strips ().strip_count (); ++strip_n) {
        boost::shared_ptr<Stripable> strip = strips ().nth_strip (strip_n);

        // gain
        SignalObserver observer = [this, strip_n] () {
            // fires multiple times (4x as of ardour 6.0)
            update_all (Node::strip_gain, { strip_n }, strips ().strip_gain (strip_n));
        };
        strip->gain_control ()->Changed.connect (_signal_connections, MISSING_INVALIDATOR,
            boost::bind<void> (observer), event_loop ());

        // pan
        observer = [this, strip_n] () {
            update_all (Node::strip_pan, { strip_n }, strips ().strip_pan (strip_n));
        };
        strip->pan_azimuth_control ()->Changed.connect (_signal_connections, MISSING_INVALIDATOR,
            boost::bind<void> (observer), event_loop ());

        // mute
        observer = [this, strip_n] () {
            update_all (Node::strip_mute, { strip_n }, strips ().strip_mute (strip_n));
        };
        strip->mute_control ()->Changed.connect (_signal_connections, MISSING_INVALIDATOR,
            boost::bind<void> (observer), event_loop ());

        observe_strip_plugins (strip_n, strip);
    }
}

void
ArdourFeedback::observe_strip_plugins (uint32_t strip_n, boost::shared_ptr<ARDOUR::Stripable> strip)
{
    for (uint32_t plugin_n = 0 ; ; ++plugin_n) {
        boost::shared_ptr<PluginInsert> insert = strips ().strip_plugin_insert (strip_n, plugin_n);
        if (!insert) {
            break;
        }

        SignalObserver observer = [this, strip_n, plugin_n] () {
            update_all (Node::strip_plugin_enable, { strip_n, plugin_n },
                strips ().strip_plugin_enabled (strip_n, plugin_n));
        };

        uint32_t bypass = insert->plugin ()->designated_bypass_port ();
        Evoral::Parameter param = Evoral::Parameter (PluginAutomation, 0, bypass);
        boost::shared_ptr<AutomationControl> control = insert->automation_control (param);

        if (control) {
            control->Changed.connect (_signal_connections, MISSING_INVALIDATOR,
                boost::bind<void> (observer), event_loop ());
        }

        observe_strip_plugin_param_values (strip_n, plugin_n, insert);
    }
}

void
ArdourFeedback::observe_strip_plugin_param_values (uint32_t strip_n,
    uint32_t plugin_n, boost::shared_ptr<ARDOUR::PluginInsert> insert)
{
    boost::shared_ptr<Plugin> plugin = insert->plugin ();

    for (uint32_t param_n = 0; param_n < plugin->parameter_count (); ++param_n) {
        boost::shared_ptr<AutomationControl> control = strips ().strip_plugin_param_control (
            strip_n, plugin_n, param_n);

        if (!control) {
            continue;
        }

        SignalObserver observer = [this, control, strip_n, plugin_n, param_n] () {
            update_all (Node::strip_plugin_param_value, { strip_n, plugin_n, param_n },
                ArdourStrips::plugin_param_value (control));
        };

        control->Changed.connect (_signal_connections, MISSING_INVALIDATOR,
                boost::bind<void> (observer), event_loop ());
    }
}

void
ArdourFeedback::update_all (std::string node, std::vector<uint32_t> addr, TypedValue val) const
{
    server ().update_all_clients ({ node, addr, { val }}, false);
}
