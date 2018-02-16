/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __gtkardour_dsp_load_gauge_h__
#define __gtkardour_dsp_load_gauge_h__

#include <pangomm.h>

#include "ardour_gauge.h"

class DspLoadGauge : public ArdourGauge
{
public:
	DspLoadGauge ();

	void set_xrun_count (const unsigned int xruns);
	void set_dsp_load (const double load);

	void set_xrun_while_recording () {_xrun_while_recording = true;}

protected:
	bool alert () const;
	ArdourGauge::Status indicator () const;
	float level () const;
	std::string tooltip_text ();

private:
	bool on_button_release_event (GdkEventButton*);

	float _dsp_load;
	unsigned int _xrun_count;
	bool _xrun_while_recording;
};

#endif
