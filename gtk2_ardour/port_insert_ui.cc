/*
    Copyright (C) 2002-2007 Paul Davis

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

#include <gtkmm/messagedialog.h>
#include <glibmm/objectbase.h>

#include <gtkmm2ext/doi.h>

#include "ardour/audioengine.h"
#include "ardour/mtdm.h"
#include "ardour/port_insert.h"
#include "ardour/session.h"

#include "port_insert_ui.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

PortInsertUI::PortInsertUI (Gtk::Window* parent, ARDOUR::Session* sess, boost::shared_ptr<ARDOUR::PortInsert> pi)
        : _pi (pi)
        , latency_button (_("Measure Latency"))
        , input_selector (parent, sess, pi->input())
        , output_selector (parent, sess, pi->output())
{
        latency_hbox.pack_start (latency_button, false, false);
        latency_hbox.pack_start (latency_display, false, false);
	latency_hbox.set_spacing (4);

	output_selector.set_min_height_divisor (2);
	input_selector.set_min_height_divisor (2);

        notebook.append_page (output_selector, _("Send/Output"));
        notebook.append_page (input_selector, _("Return/Input"));

        notebook.set_current_page (0);

        set_spacing (12);
        pack_start (notebook, true, true);
        pack_start (latency_hbox, false, false);

        update_latency_display ();

        latency_button.signal_toggled().connect (mem_fun (*this, &PortInsertUI::latency_button_toggled));
	latency_button.set_name (X_("MeasureLatencyButton"));
}

void
PortInsertUI::update_latency_display ()
{
        framecnt_t const sample_rate = AudioEngine::instance()->sample_rate();
        if (sample_rate == 0) {
                latency_display.set_text (_("Disconnected from audio engine"));
        } else {
                char buf[64];
                snprintf (buf, sizeof (buf), "%10.3lf frames %10.3lf ms",
                          (float)_pi->latency(), (float)_pi->latency() * 1000.0f/sample_rate);
                latency_display.set_text(buf);
        }
}

bool
PortInsertUI::check_latency_measurement ()
{
        MTDM* mtdm = _pi->mtdm ();

        if (mtdm->resolve () < 0) {
                latency_display.set_text (_("No signal detected"));
                return true;
        }

        if (mtdm->err () > 0.3) {
                mtdm->invert ();
                mtdm->resolve ();
        }

        char buf[128];
        framecnt_t const sample_rate = AudioEngine::instance()->sample_rate();

        if (sample_rate == 0) {
                latency_display.set_text (_("Disconnected from audio engine"));
                _pi->stop_latency_detection ();
                return false;
        }

        snprintf (buf, sizeof (buf), "%10.3lf frames %10.3lf ms", mtdm->del (), mtdm->del () * 1000.0f/sample_rate);

        bool solid = true;

        if (mtdm->err () > 0.2) {
                strcat (buf, " ??");
                solid = false;
        }

        if (mtdm->inv ()) {
                strcat (buf, " (Inv)");
                solid = false;
        }

        if (solid) {
                _pi->set_measured_latency (rint (mtdm->del()));
                latency_button.set_active (false);
                strcat (buf, " (set)");
        }

        latency_display.set_text (buf);

        return true;
}

void
PortInsertUI::latency_button_toggled ()
{
        if (latency_button.get_active ()) {

                _pi->start_latency_detection ();
                latency_display.set_text (_("Detecting ..."));
                latency_timeout = Glib::signal_timeout().connect (mem_fun (*this, &PortInsertUI::check_latency_measurement), 250);

        } else {
                _pi->stop_latency_detection ();
                latency_timeout.disconnect ();
                update_latency_display ();
        }
}

void
PortInsertUI::redisplay ()
{
	input_selector.setup_ports (input_selector.other());
	output_selector.setup_ports (output_selector.other());
}

void
PortInsertUI::finished (IOSelector::Result r)
{
	input_selector.Finished (r);
	output_selector.Finished (r);
}


PortInsertWindow::PortInsertWindow (ARDOUR::Session* sess, boost::shared_ptr<ARDOUR::PortInsert> pi)
	: ArdourDialog ("port insert dialog"),
	  _portinsertui (this, sess, pi)
{

	set_name ("IOSelectorWindow");
	std::string title = _("Port Insert ");
	title += pi->name();
	set_title (title);

	get_vbox()->pack_start (_portinsertui);

	Gtk::Button* cancel_but = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	Gtk::Button* ok_but = add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

	cancel_but->signal_clicked().connect (sigc::mem_fun (*this, &PortInsertWindow::cancel));
	ok_but->signal_clicked().connect (sigc::mem_fun (*this, &PortInsertWindow::accept));

	signal_delete_event().connect (sigc::mem_fun (*this, &PortInsertWindow::wm_delete), false);
}

bool
PortInsertWindow::wm_delete (GdkEventAny* /*event*/)
{
	accept ();
	return false;
}

void
PortInsertWindow::on_map ()
{
	_portinsertui.redisplay ();
	Window::on_map ();
}


void
PortInsertWindow::cancel ()
{
	_portinsertui.finished (IOSelector::Cancelled);
	hide ();
}

void
PortInsertWindow::accept ()
{
	_portinsertui.finished (IOSelector::Accepted);
	hide ();
}

