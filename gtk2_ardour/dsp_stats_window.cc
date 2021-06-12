/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include "dsp_stats_ui.h"
#include "dsp_stats_window.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;

DspStatisticsWindow::DspStatisticsWindow()
	: ArdourWindow (_("Performance Meters"))
	, ui (new DspStatisticsGUI)
{
	ui->show ();
	add (*ui);
}

DspStatisticsWindow::~DspStatisticsWindow ()
{
	delete ui;
}

void
DspStatisticsWindow::set_session (Session* s)
{
	ui->set_session (s);
}

void
DspStatisticsWindow::on_show ()
{
	ArdourWindow::on_show ();
	ui->start_updating ();
}

void
DspStatisticsWindow::on_hide ()
{
	ArdourWindow::on_hide ();
	ui->stop_updating ();
}
