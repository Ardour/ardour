/*
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_tempo_curve_h__
#define __gtk_ardour_tempo_curve_h__

#include <string>
#include <glib.h>

#include <sigc++/signal.h>

#include "ardour/ardour.h"
#include "pbd/signals.h"

#include "canvas/types.h"
#include "canvas/framed_curve.h"
#include "canvas/text.h"

namespace Temporal {
	class TempoPoint;
}
class PublicEditor;

class TempoCurve : public sigc::trackable
{
public:
	TempoCurve (PublicEditor& editor, ArdourCanvas::Item &, guint32 rgba, Temporal::TempoPoint const & temp, bool handle_events , ArdourCanvas::Distance marker_width);
	~TempoCurve ();

	static PBD::Signal1<void,TempoCurve*> CatchDeletion;

	static void setup_sizes (const double timebar_height);

	ArdourCanvas::Item& the_item() const;

	void set_duration (ARDOUR::samplecnt_t duration);
	void set_color_rgba (uint32_t rgba);

	void hide ();
	void show ();

	Temporal::TempoPoint const & tempo () const { return _tempo; }

	void set_max_tempo (const double& max) { _max_tempo = max; }
	void set_min_tempo (const double& min) { _min_tempo = min; }

protected:
	PublicEditor& editor;

	ArdourCanvas::Item*        _parent;
	ArdourCanvas::Container*    group;
	ArdourCanvas::Points        points;
	ArdourCanvas::FramedCurve* _curve;

	ARDOUR::samplecnt_t    _duration;
	ArdourCanvas::Distance _marker_width;
	uint32_t               _color;

	void reposition ();

private:
	/* disallow copy construction */
	TempoCurve (TempoCurve const &);

	TempoCurve & operator= (TempoCurve const &);

	double _min_tempo;
	double _max_tempo;

	Temporal::TempoPoint const & _tempo;
	ArdourCanvas::Text*   _start_text;
	ArdourCanvas::Text*   _end_text;
};
#endif /* __gtk_ardour_tempo_curve_h__ */
