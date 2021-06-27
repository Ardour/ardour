/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <glib.h>
#include <gtkmm/button.h>
#include <gtkmm/table.h>

#include "pbd/microseconds.h"

#include "temporal/time.h"

#include "idleometer.h"
#include "pbd/i18n.h"

using namespace Gtk;

IdleOMeter::IdleOMeter ()
	: ArdourDialog (_("Idle O Meter"))
{
	get_vbox()->set_spacing (8);
	Label* l = manage (new Label (_("<b>GUI Idle Timing Statistics</b>"), ALIGN_CENTER));
	l->set_use_markup ();

	HBox* hbox = manage (new HBox ());
	Table* t = manage (new Table ());
	hbox->pack_start (*t, true, false);

	Button* b = manage (new Button (_("Reset")));
	b->signal_clicked().connect (sigc::mem_fun(*this, &IdleOMeter::reset));

	get_vbox()->pack_start (*l, false, false);
	get_vbox()->pack_start (*hbox, false, false);
	get_vbox()->pack_start (*b, false, false);

	_label_cur.set_alignment (ALIGN_RIGHT, ALIGN_CENTER);
	_label_min.set_alignment (ALIGN_RIGHT, ALIGN_CENTER);
	_label_max.set_alignment (ALIGN_RIGHT, ALIGN_CENTER);
	_label_avg.set_alignment (ALIGN_RIGHT, ALIGN_CENTER);
	_label_dev.set_alignment (ALIGN_RIGHT, ALIGN_CENTER);
	_label_acq.set_alignment (ALIGN_CENTER, ALIGN_CENTER);

	int row = 0;
	t->attach (*manage (new Label (_("Current:"), ALIGN_RIGHT)), 0, 1, row, row + 1, FILL, SHRINK);
	t->attach (_label_cur, 1, 2, row, row + 1, FILL, SHRINK);
	++row;
	t->attach (*manage (new Label (_("Min:"),     ALIGN_RIGHT)), 0, 1, row, row + 1, FILL, SHRINK);
	t->attach (_label_min, 1, 2, row, row + 1, FILL, SHRINK);
	++row;
	t->attach (*manage (new Label (_("Max:"),     ALIGN_RIGHT)), 0, 1, row, row + 1, FILL, SHRINK);
	t->attach (_label_max, 1, 2, row, row + 1, FILL, SHRINK);
	++row;
	t->attach (*manage (new Label (_("Mean:"),    ALIGN_RIGHT)), 0, 1, row, row + 1, FILL, SHRINK);
	t->attach (_label_avg, 1, 2, row, row + 1, FILL, SHRINK);
	++row;
	t->attach (*manage (new Label (_("\u03c3:"),  ALIGN_RIGHT)), 0, 1, row, row + 1, FILL, SHRINK);
	t->attach (_label_dev, 1, 2, row, row + 1, FILL, SHRINK);
	++row;
	t->attach (*manage (new Label (_("Elapsed:"),  ALIGN_RIGHT)), 0, 1, row, row + 1, FILL, SHRINK);
	t->attach (_label_acq, 1, 2, row, row + 1, FILL, SHRINK);
}

IdleOMeter::~IdleOMeter ()
{
	_idle_connection.disconnect ();
}

bool
IdleOMeter::idle ()
{
	const int64_t now = PBD::get_microseconds ();
	const int64_t elapsed = now - _last;

	_max = std::max (_max, elapsed);
	_min = std::min (_min, elapsed);
	_last = now;
	_total += elapsed;
	++_cnt;

	const double cnt = _cnt;

	/* running variance */
	if (_cnt <= 1) {
		_var_m = elapsed;
		_var_s = 0;
	} else {
		const double var_m1 = _var_m;
		const double t = elapsed;
		_var_m += (t - _var_m) / cnt;
		_var_s += (t - _var_m) * (t - var_m1);
	}

	if (now - _last_display < 100000 || _cnt < 2) {
		return true;
	}

	const double avg = _total / cnt;
	const double stddev = sqrt (_var_s / (_cnt - 1.0));
	_last_display = now;

	char buf[128];

	snprintf (buf, sizeof(buf), "%8.2f ms", elapsed / 1000.0);
	_label_cur.set_text (buf);
	snprintf (buf, sizeof(buf), "%8.2f ms", _min / 1000.0);
	_label_min.set_text (buf);
	snprintf (buf, sizeof(buf), "%8.2f ms", _max / 1000.0);
	_label_max.set_text (buf);
	snprintf (buf, sizeof(buf), "%8.3f ms", avg / 1000.0);
	_label_avg.set_text (buf);
	snprintf (buf, sizeof(buf), "%8.3f ms", stddev / 1000.0);
	_label_dev.set_text (buf);
	_label_acq.set_text (Timecode::timecode_format_sampletime (now - _start, 1000000, 100, false));

	return true;
}

void
IdleOMeter::reset ()
{
	_last = PBD::get_microseconds ();
	_last_display = _last;
	_start = _last;
	_max = 0;
	_min = INT64_MAX;
	_cnt = 0;
	_total = _var_m = _var_s = 0;

	_label_cur.set_text ("-");
	_label_min.set_text ("-");
	_label_max.set_text ("-");
	_label_avg.set_text ("-");
	_label_dev.set_text ("-");
	_label_acq.set_text ("-");
}

void
IdleOMeter::on_show ()
{
	ArdourDialog::on_show ();
	reset ();
	_idle_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &IdleOMeter::idle));
}

void
IdleOMeter::on_hide ()
{
	_idle_connection.disconnect ();
	ArdourDialog::on_hide ();
}
