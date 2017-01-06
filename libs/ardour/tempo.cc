/*
    Copyright (C) 2000-2002 Paul Davis

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

#include <algorithm>
#include <stdexcept>
#include <cmath>

#include <unistd.h>

#include <glibmm/threads.h>

#include "pbd/enumwriter.h"
#include "pbd/xml++.h"
#include "evoral/Beats.hpp"

#include "ardour/debug.h"
#include "ardour/lmath.h"
#include "ardour/tempo.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

using Timecode::BBT_Time;

/* _default tempo is 4/4 qtr=120 */

Meter    TempoMap::_default_meter (4.0, 4.0);
Tempo    TempoMap::_default_tempo (120.0, 4.0);

framepos_t
MetricSection::frame_at_minute (const double& time) const
{
	return (framepos_t) floor ((time * 60.0 * _sample_rate) + 0.5);
}

double
MetricSection::minute_at_frame (const framepos_t& frame) const
{
	return (frame / (double) _sample_rate) / 60.0;
}

/***********************************************************************/

double
Meter::frames_per_grid (const Tempo& tempo, framecnt_t sr) const
{
	/* This is tempo- and meter-sensitive. The number it returns
	   is based on the interval between any two lines in the
	   grid that is constructed from tempo and meter sections.

	   The return value IS NOT interpretable in terms of "beats".
	*/

	return (60.0 * sr) / (tempo.note_types_per_minute() * (_note_type/tempo.note_type()));
}

double
Meter::frames_per_bar (const Tempo& tempo, framecnt_t sr) const
{
	return frames_per_grid (tempo, sr) * _divisions_per_bar;
}

/***********************************************************************/

const string TempoSection::xml_state_node_name = "Tempo";

TempoSection::TempoSection (const XMLNode& node, framecnt_t sample_rate)
	: MetricSection (0.0, 0, MusicTime, true, sample_rate)
	, Tempo (TempoMap::default_tempo())
	, _c (0.0)
	, _active (true)
	, _locked_to_meter (false)
{
	XMLProperty const * prop;
	LocaleGuard lg;
	BBT_Time bbt;
	double pulse;
	uint32_t frame;

	_legacy_bbt = BBT_Time (0, 0, 0);

	if ((prop = node.property ("start")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
			    &bbt.bars,
			    &bbt.beats,
			    &bbt.ticks) == 3) {
			/* legacy session - start used to be in bbt*/
			_legacy_bbt = bbt;
			pulse = -1.0;
			info << _("Legacy session detected. TempoSection XML node will be altered.") << endmsg;
		}
	}

	if ((prop = node.property ("pulse")) != 0) {
		if (sscanf (prop->value().c_str(), "%lf", &pulse) != 1) {
			error << _("TempoSection XML node has an illegal \"pulse\" value") << endmsg;
		}
	}

	set_pulse (pulse);

	if ((prop = node.property ("frame")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu32, &frame) != 1) {
			error << _("TempoSection XML node has an illegal \"frame\" value") << endmsg;
			throw failed_constructor();
		} else {
			set_minute (minute_at_frame (frame));
		}
	}

	/* XX replace old beats-per-minute name with note-types-per-minute */
	if ((prop = node.property ("beats-per-minute")) != 0) {
		if (sscanf (prop->value().c_str(), "%lf", &_note_types_per_minute) != 1 || _note_types_per_minute < 0.0) {
			error << _("TempoSection XML node has an illegal \"beats-per-minute\" value") << endmsg;
			throw failed_constructor();
		}
	}

	if ((prop = node.property ("note-type")) == 0) {
		/* older session, make note type be quarter by default */
		_note_type = 4.0;
	} else {
		if (sscanf (prop->value().c_str(), "%lf", &_note_type) != 1 || _note_type < 1.0) {
			error << _("TempoSection XML node has an illegal \"note-type\" value") << endmsg;
			throw failed_constructor();
	}
	}

	if ((prop = node.property ("movable")) == 0) {
		error << _("TempoSection XML node has no \"movable\" property") << endmsg;
		throw failed_constructor();
	}

	set_initial (!string_is_affirmative (prop->value()));

	if ((prop = node.property ("active")) == 0) {
		warning << _("TempoSection XML node has no \"active\" property") << endmsg;
		set_active(true);
	} else {
		set_active (string_is_affirmative (prop->value()));
	}

	if ((prop = node.property ("tempo-type")) == 0) {
		_type = Constant;
	} else {
		_type = Type (string_2_enum (prop->value(), _type));
	}

	if ((prop = node.property ("lock-style")) == 0) {
		if (!initial()) {
			set_position_lock_style (MusicTime);
		} else {
			set_position_lock_style (AudioTime);
		}
	} else {
		set_position_lock_style (PositionLockStyle (string_2_enum (prop->value(), position_lock_style())));
	}

	if ((prop = node.property ("locked-to-meter")) == 0) {
		set_locked_to_meter (false);
	} else {
		set_locked_to_meter (string_is_affirmative (prop->value()));
	}
}

XMLNode&
TempoSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);
	char buf[256];
	LocaleGuard lg;

	snprintf (buf, sizeof (buf), "%lf", pulse());
	root->add_property ("pulse", buf);
	snprintf (buf, sizeof (buf), "%li", frame());
	root->add_property ("frame", buf);
	snprintf (buf, sizeof (buf), "%lf", _note_types_per_minute);
	root->add_property ("beats-per-minute", buf);
	snprintf (buf, sizeof (buf), "%lf", _note_type);
	root->add_property ("note-type", buf);
	snprintf (buf, sizeof (buf), "%s", !initial()?"yes":"no");
	root->add_property ("movable", buf);
	snprintf (buf, sizeof (buf), "%s", active()?"yes":"no");
	root->add_property ("active", buf);
	root->add_property ("tempo-type", enum_2_string (_type));
	root->add_property ("lock-style", enum_2_string (position_lock_style()));
	root->add_property ("locked-to-meter", locked_to_meter()?"yes":"no");

	return *root;
}

void
TempoSection::set_type (Type type)
{
	_type = type;
}

/** returns the Tempo at the session-relative minute.
*/
Tempo
TempoSection::tempo_at_minute (const double& m) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && m < minute());
	if (constant) {
		return Tempo (note_types_per_minute(), note_type());
	}

	return Tempo (_tempo_at_time (m - minute()), _note_type);
}

/** returns the session relative minute where the supplied tempo in note types per minute occurs.
 *  @param ntpm the tempo in mote types per minute used to calculate the returned minute
 *  @param p the pulse used to calculate the returned minute for constant tempi
 *  @return the minute at the supplied tempo
 *
 *  note that the note_type is currently ignored in this function. see below.
 *
*/

/** if tempoA (120, 4.0) precedes tempoB (120, 8.0),
 *  there should be no ramp between the two even if we are ramped.
 *  in other words a ramp should only place a curve on note_types_per_minute.
 *  we should be able to use Tempo note type here, but the above
 *  complicates things a bit.
*/
double
TempoSection::minute_at_ntpm (const double& ntpm, const double& p) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && p < pulse());
	if (constant) {
		return ((p - pulse()) / pulses_per_minute()) + minute();
	}

	return _time_at_tempo (ntpm) + minute();
}

/** returns the Tempo at the supplied whole-note pulse.
 */
Tempo
TempoSection::tempo_at_pulse (const double& p) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && p < pulse());

	if (constant) {
		return Tempo (note_types_per_minute(), note_type());
	}

	return Tempo (_tempo_at_pulse (p - pulse()), _note_type);
}

/** returns the whole-note pulse where a tempo in note types per minute occurs.
 *  constant tempi require minute m.
 *  @param ntpm the note types per minute value used to calculate the returned pulse
 *  @param m the minute used to calculate the returned pulse if the tempo is constant
 *  @return the whole-note pulse at the supplied tempo
 *
 *  note that note_type is currently ignored in this function. see minute_at_tempo().
 *
 *  for constant tempi, this is anaologous to pulse_at_minute().
*/
double
TempoSection::pulse_at_ntpm (const double& ntpm, const double& m) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && m < minute());
	if (constant) {
		return ((m - minute()) * pulses_per_minute()) + pulse();
	}

	return _pulse_at_tempo (ntpm) + pulse();
}

/** returns the whole-note pulse at the supplied session-relative minute.
*/
double
TempoSection::pulse_at_minute (const double& m) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && m < minute());
	if (constant) {
		return ((m - minute()) * pulses_per_minute()) + pulse();
	}

	return _pulse_at_time (m - minute()) + pulse();
}

/** returns the session-relative minute at the supplied whole-note pulse.
*/
double
TempoSection::minute_at_pulse (const double& p) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && p < pulse());
	if (constant) {
		return ((p - pulse()) / pulses_per_minute()) + minute();
	}

	return _time_at_pulse (p - pulse()) + minute();
}

/** returns thw whole-note pulse at session frame position f.
 *  @param f the frame position.
 *  @return the position in whole-note pulses corresponding to f
 *
 *  for use with musical units whose granularity is coarser than frames (e.g. ticks)
*/
double
TempoSection::pulse_at_frame (const framepos_t& f) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && f < frame());
	if (constant) {
		return (minute_at_frame (f - frame()) * pulses_per_minute()) + pulse();
	}

	return _pulse_at_time (minute_at_frame (f - frame())) + pulse();
}

framepos_t
TempoSection::frame_at_pulse (const double& p) const
{
	const bool constant = _type == Constant || _c == 0.0 || (initial() && p < pulse());
	if (constant) {
		return frame_at_minute (((p - pulse()) / pulses_per_minute()) + minute());
	}

	return frame_at_minute (_time_at_pulse (p - pulse()) + minute());
}

/*
Ramp Overview

      |                     *
Tempo |                   *
Tt----|-----------------*|
Ta----|--------------|*  |
      |            * |   |
      |         *    |   |
      |     *        |   |
T0----|*             |   |
  *   |              |   |
      _______________|___|____
      time           a   t (next tempo)
      [        c         ] defines c

Duration in beats at time a is the integral of some Tempo function.
In our case, the Tempo function (Tempo at time t) is
T(t) = T0(e^(ct))

with function constant
c = log(Ta/T0)/a
so
a = log(Ta/T0)/c

The integral over t of our Tempo function (the beat function, which is the duration in beats at some time t) is:
b(t) = T0(e^(ct) - 1) / c

To find the time t at beat duration b, we use the inverse function of the beat function (the time function) which can be shown to be:
t(b) = log((c.b / T0) + 1) / c

The time t at which Tempo T occurs is a as above:
t(T) = log(T / T0) / c

The beat at which a Tempo T occurs is:
b(T) = (T - T0) / c

The Tempo at which beat b occurs is:
T(b) = b.c + T0

We define c for this tempo ramp by placing a new tempo section at some time t after this one.
Our problem is that we usually don't know t.
We almost always know the duration in beats between this and the new section, so we need to find c in terms of the beat function.
Where a = t (i.e. when a is equal to the time of the next tempo section), the beat function reveals:
t = b log (Ta / T0) / (T0 (e^(log (Ta / T0)) - 1))

By substituting our expanded t as a in the c function above, our problem is reduced to:
c = T0 (e^(log (Ta / T0)) - 1) / b

Of course the word 'beat' has been left loosely defined above.
In music, a beat is defined by the musical pulse (which comes from the tempo)
and the meter in use at a particular time (how many  pulse divisions there are in one bar).
It would be more accurate to substitute the work 'pulse' for 'beat' above.

Anyway ...

We can now store c for future time calculations.
If the following tempo section (the one that defines c in conjunction with this one)
is changed or moved, c is no longer valid.

The public methods are session-relative.

Most of this stuff is taken from this paper:

WHERE’S THE BEAT?
TOOLS FOR DYNAMIC TEMPO CALCULATIONS
Jan C. Schacher
Martin Neukom
Zurich University of Arts
Institute for Computer Music and Sound Technology

https://www.zhdk.ch/fileadmin/data_subsites/data_icst/Downloads/Timegrid/ICST_Tempopolyphony_ICMC07.pdf

*/

/** compute this ramp's function constant from some tempo-pulse point
 * @param end_npm end tempo (in note types per minute)
 * @param end_pulse duration (pulses into global start) of some other position.
 * @return the calculated function constant
*/
double
TempoSection::compute_c_func_pulse (const double& end_npm, const double& end_pulse) const
{
	if (note_types_per_minute() == end_npm || _type == Constant) {
		return 0.0;
	}

	double const log_tempo_ratio = log (end_npm / note_types_per_minute());
	return (note_types_per_minute() * expm1 (log_tempo_ratio)) / ((end_pulse - pulse()) * _note_type);
}

/** compute the function constant from some tempo-time point.
 * @param end_npm tempo (note types/min.)
 * @param end_minute distance (in minutes) from session origin
 * @return the calculated function constant
*/
double
TempoSection::compute_c_func_minute (const double& end_npm, const double& end_minute) const
{
	if (note_types_per_minute() == end_npm || _type == Constant) {
		return 0.0;
	}

	return c_func (end_npm, end_minute - minute());
}

/* position function */
double
TempoSection::a_func (double end_npm, double c) const
{
	return log (end_npm / note_types_per_minute()) / c;
}

/*function constant*/
double
TempoSection::c_func (double end_npm, double end_time) const
{
	return log (end_npm / note_types_per_minute()) / end_time;
}

/* tempo in note types per minute at time in minutes */
double
TempoSection::_tempo_at_time (const double& time) const
{
	return exp (_c * time) * note_types_per_minute();
}

/* time in minutes at tempo in note types per minute */
double
TempoSection::_time_at_tempo (const double& npm) const
{
	return log (npm / note_types_per_minute()) / _c;
}

/* pulse at tempo in note types per minute */
double
TempoSection::_pulse_at_tempo (const double& npm) const
{
	return ((npm - note_types_per_minute()) / _c) / _note_type;
}

/* tempo in note types per minute at pulse */
double
TempoSection::_tempo_at_pulse (const double& pulse) const
{
	return (pulse * _note_type * _c) + note_types_per_minute();
}

/* pulse at time in minutes */
double
TempoSection::_pulse_at_time (const double& time) const
{
	return (expm1 (_c * time) * (note_types_per_minute() / _c)) / _note_type;
}

/* time in minutes at pulse */
double
TempoSection::_time_at_pulse (const double& pulse) const
{
	return log1p ((_c * pulse * _note_type) / note_types_per_minute()) / _c;
}

/***********************************************************************/

const string MeterSection::xml_state_node_name = "Meter";

MeterSection::MeterSection (const XMLNode& node, const framecnt_t sample_rate)
	: MetricSection (0.0, 0, MusicTime, false, sample_rate), Meter (TempoMap::default_meter())
{
	XMLProperty const * prop;
	LocaleGuard lg;
	BBT_Time bbt;
	double pulse = 0.0;
	double beat = 0.0;
	framepos_t frame = 0;
	pair<double, BBT_Time> start;

	if ((prop = node.property ("start")) != 0) {
		if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		    &bbt.bars,
		    &bbt.beats,
		    &bbt.ticks) < 3) {
			error << _("MeterSection XML node has an illegal \"start\" value") << endmsg;
		} else {
			/* legacy session - start used to be in bbt*/
			info << _("Legacy session detected - MeterSection XML node will be altered.") << endmsg;
			pulse = -1.0;
		}
	}

	if ((prop = node.property ("pulse")) != 0) {
		if (sscanf (prop->value().c_str(), "%lf", &pulse) != 1) {
			error << _("MeterSection XML node has an illegal \"pulse\" value") << endmsg;
		}
	}
	set_pulse (pulse);

	if ((prop = node.property ("beat")) != 0) {
		if (sscanf (prop->value().c_str(), "%lf", &beat) != 1) {
			error << _("MeterSection XML node has an illegal \"beat\" value") << endmsg;
		}
	}

	start.first = beat;

	if ((prop = node.property ("bbt")) == 0) {
		warning << _("MeterSection XML node has no \"bbt\" property") << endmsg;
	} else if (sscanf (prop->value().c_str(), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		    &bbt.bars,
		    &bbt.beats,
		    &bbt.ticks) < 3) {
		error << _("MeterSection XML node has an illegal \"bbt\" value") << endmsg;
		throw failed_constructor();
	}

	start.second = bbt;
	set_beat (start);

	if ((prop = node.property ("frame")) != 0) {
		if (sscanf (prop->value().c_str(), "%li", &frame) != 1) {
			error << _("MeterSection XML node has an illegal \"frame\" value") << endmsg;
			throw failed_constructor();
		} else {
			set_minute (minute_at_frame (frame));
		}
	}

	/* beats-per-bar is old; divisions-per-bar is new */

	if ((prop = node.property ("divisions-per-bar")) == 0) {
		if ((prop = node.property ("beats-per-bar")) == 0) {
			error << _("MeterSection XML node has no \"beats-per-bar\" or \"divisions-per-bar\" property") << endmsg;
			throw failed_constructor();
		}
	}
	if (sscanf (prop->value().c_str(), "%lf", &_divisions_per_bar) != 1 || _divisions_per_bar < 0.0) {
		error << _("MeterSection XML node has an illegal \"divisions-per-bar\" value") << endmsg;
		throw failed_constructor();
	}

	if ((prop = node.property ("note-type")) == 0) {
		error << _("MeterSection XML node has no \"note-type\" property") << endmsg;
		throw failed_constructor();
	}
	if (sscanf (prop->value().c_str(), "%lf", &_note_type) != 1 || _note_type < 0.0) {
		error << _("MeterSection XML node has an illegal \"note-type\" value") << endmsg;
		throw failed_constructor();
	}

	if ((prop = node.property ("movable")) == 0) {
		error << _("MeterSection XML node has no \"movable\" property") << endmsg;
		throw failed_constructor();
	}

	set_initial (!string_is_affirmative (prop->value()));

	if ((prop = node.property ("lock-style")) == 0) {
		warning << _("MeterSection XML node has no \"lock-style\" property") << endmsg;
		if (!initial()) {
			set_position_lock_style (MusicTime);
		} else {
			set_position_lock_style (AudioTime);
		}
	} else {
		set_position_lock_style (PositionLockStyle (string_2_enum (prop->value(), position_lock_style())));
	}
}

XMLNode&
MeterSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);
	char buf[256];
	LocaleGuard lg;

	snprintf (buf, sizeof (buf), "%lf", pulse());
	root->add_property ("pulse", buf);
	snprintf (buf, sizeof (buf), "%" PRIu32 "|%" PRIu32 "|%" PRIu32,
		  bbt().bars,
		  bbt().beats,
		  bbt().ticks);
	root->add_property ("bbt", buf);
	snprintf (buf, sizeof (buf), "%lf", beat());
	root->add_property ("beat", buf);
	snprintf (buf, sizeof (buf), "%lf", _note_type);
	root->add_property ("note-type", buf);
	snprintf (buf, sizeof (buf), "%li", frame());
	root->add_property ("frame", buf);
	root->add_property ("lock-style", enum_2_string (position_lock_style()));
	snprintf (buf, sizeof (buf), "%lf", _divisions_per_bar);
	root->add_property ("divisions-per-bar", buf);
	snprintf (buf, sizeof (buf), "%s", !initial()?"yes":"no");
	root->add_property ("movable", buf);

	return *root;
}

/***********************************************************************/
/*
  Tempo Map Overview

  Tempo determines the rate of musical pulse determined by its components
        note types per minute - the rate per minute of the whole note divisor _note_type
	note type             - the division of whole notes (pulses) which occur at the rate of note types per minute.
  Meter divides the musical pulse into measures and beats according to its components
        divisions_per_bar
	note_divisor

  TempoSection - translates between time, musical pulse and tempo.
        has a musical location in whole notes (pulses).
	has a time location in minutes.
	Note that 'beats' in Tempo::note_types_per_minute() are in fact note types per minute.
	(In the rest of tempo map,'beat' usually refers to accumulated BBT beats (pulse and meter based).

  MeterSection - translates between BBT, meter-based beat and musical pulse.
        has a musical location in whole notes (pulses)
	has a musical location in meter-based beats
	has a musical location in BBT time
	has a time location expressed in minutes.

  TempoSection and MeterSection may be locked to either audio or music (position lock style).
  The lock style determines the location type to be kept as a reference when location is recalculated.

  The first tempo and meter are special. they must move together, and are locked to audio.
  Audio locked tempi which lie before the first meter are made inactive.

  Recomputing the map is the process where the 'missing' location types are calculated.
        We construct the tempo map by first using the locked location type of each section
	to determine non-locked location types (pulse or minute position).
        We then use this map to find the pulse or minute position of each meter (again depending on lock style).

  Having done this, we can now traverse the Metrics list by pulse or minute
  to query its relevant meter/tempo.

  It is important to keep the _metrics in an order that makes sense.
  Because ramped MusicTime and AudioTime tempos can interact with each other,
  reordering is frequent. Care must be taken to keep _metrics in a solved state.
  Solved means ordered by frame or pulse with frame-accurate precision (see check_solved()).

  Music and Audio

  Music and audio-locked objects may seem interchangeable on the surface, but when translating
  between audio samples and beat, remember that a sample is only a quantised approximation
  of the actual time (in minutes) of a beat.
  Thus if a gui user points to the frame occupying the start of a music-locked object on 1|3|0, it does not
  mean that this frame is the actual location in time of 1|3|0.

  You cannot use a frame measurement to determine beat distance except under special circumstances
  (e.g. where the user has requested that a beat lie on a SMPTE frame or if the tempo is known to be constant over the duration).

  This means is that a user operating on a musical grid must supply the desired beat position and/or current beat quantization in order for the
  sample space the user is operating at to be translated correctly to the object.

  The current approach is to interpret the supplied frame using the grid division the user has currently selected.
  If the user has no musical grid set, they are actually operating in sample space (even SMPTE frames are rounded to audio frame), so
  the supplied audio frame is interpreted as the desired musical location (beat_at_frame()).

  tldr: Beat, being a function of time, has nothing to do with sample rate, but time quantization can get in the way of precision.

  When frame_at_beat() is called, the position calculation is performed in pulses and minutes.
  The result is rounded to audio frames.
  When beat_at_frame() is called, the frame is converted to minutes, with no rounding performed on the result.

  So :
  frame_at_beat (beat_at_frame (frame)) == frame
  but :
  beat_at_frame (frame_at_beat (beat)) != beat due to the time quantization of frame_at_beat().

  Doing the second one will result in a beat distance error of up to 0.5 audio samples.
  frames_between_quarter_notes () eliminats this effect when determining time duration
  from Beats distance, or instead work in quarter-notes and/or beats and convert to frames last.

  The above pointless example could instead do:
  beat_at_quarter_note (quarter_note_at_beat (beat)) to avoid rounding.

  The Shaggs - Things I Wonder
  https://www.youtube.com/watch?v=9wQK6zMJOoQ

*/
struct MetricSectionSorter {
    bool operator() (const MetricSection* a, const MetricSection* b) {
	    return a->pulse() < b->pulse();
    }
};

struct MetricSectionFrameSorter {
    bool operator() (const MetricSection* a, const MetricSection* b) {
	    return a->frame() < b->frame();
    }
};

TempoMap::TempoMap (framecnt_t fr)
{
	_frame_rate = fr;
	BBT_Time start (1, 1, 0);

	TempoSection *t = new TempoSection (0.0, 0.0, _default_tempo.note_types_per_minute(), _default_tempo.note_type(), TempoSection::Ramp, AudioTime, fr);
	MeterSection *m = new MeterSection (0.0, 0.0, 0.0, start, _default_meter.divisions_per_bar(), _default_meter.note_divisor(), AudioTime, fr);

	t->set_initial (true);
	t->set_locked_to_meter (true);

	m->set_initial (true);

	/* note: frame time is correct (zero) for both of these */

	_metrics.push_back (t);
	_metrics.push_back (m);

}

TempoMap::TempoMap (TempoMap const & other)
{
	_frame_rate = other._frame_rate;
	for (Metrics::const_iterator m = other._metrics.begin(); m != other._metrics.end(); ++m) {
		TempoSection* ts = dynamic_cast<TempoSection*> (*m);
		MeterSection* ms = dynamic_cast<MeterSection*> (*m);

		if (ts) {
			TempoSection* new_section = new TempoSection (*ts);
			_metrics.push_back (new_section);
		} else {
			MeterSection* new_section = new MeterSection (*ms);
			_metrics.push_back (new_section);
		}
	}
}

TempoMap&
TempoMap::operator= (TempoMap const & other)
{
	if (&other != this) {
		_frame_rate = other._frame_rate;

		Metrics::const_iterator d = _metrics.begin();
		while (d != _metrics.end()) {
			delete (*d);
			++d;
		}
		_metrics.clear();

		for (Metrics::const_iterator m = other._metrics.begin(); m != other._metrics.end(); ++m) {
			TempoSection* ts = dynamic_cast<TempoSection*> (*m);
			MeterSection* ms = dynamic_cast<MeterSection*> (*m);

			if (ts) {
				TempoSection* new_section = new TempoSection (*ts);
				_metrics.push_back (new_section);
			} else {
				MeterSection* new_section = new MeterSection (*ms);
				_metrics.push_back (new_section);
			}
		}
	}

	PropertyChanged (PropertyChange());

	return *this;
}

TempoMap::~TempoMap ()
{
	Metrics::const_iterator d = _metrics.begin();
	while (d != _metrics.end()) {
		delete (*d);
		++d;
	}
	_metrics.clear();
}

framepos_t
TempoMap::frame_at_minute (const double time) const
{
	return (framepos_t) floor ((time * 60.0 * _frame_rate) + 0.5);
}

double
TempoMap::minute_at_frame (const framepos_t frame) const
{
	return (frame / (double) _frame_rate) / 60.0;
}

void
TempoMap::remove_tempo (const TempoSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		if ((removed = remove_tempo_locked (tempo))) {
			if (complete_operation) {
				recompute_map (_metrics);
			}
		}
	}

	if (removed && complete_operation) {
		PropertyChanged (PropertyChange ());
	}
}

bool
TempoMap::remove_tempo_locked (const TempoSection& tempo)
{
	Metrics::iterator i;

	for (i = _metrics.begin(); i != _metrics.end(); ++i) {
		if (dynamic_cast<TempoSection*> (*i) != 0) {
			if (tempo.frame() == (*i)->frame()) {
				if (!(*i)->initial()) {
					delete (*i);
					_metrics.erase (i);
					return true;
				}
			}
		}
	}

	return false;
}

void
TempoMap::remove_meter (const MeterSection& tempo, bool complete_operation)
{
	bool removed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		if ((removed = remove_meter_locked (tempo))) {
			if (complete_operation) {
				recompute_map (_metrics);
			}
		}
	}

	if (removed && complete_operation) {
		PropertyChanged (PropertyChange ());
	}
}

bool
TempoMap::remove_meter_locked (const MeterSection& meter)
{

	if (meter.position_lock_style() == AudioTime) {
		/* remove meter-locked tempo */
		for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
			TempoSection* t = 0;
			if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
				if (t->locked_to_meter() && meter.frame() == (*i)->frame()) {
					delete (*i);
					_metrics.erase (i);
					break;
				}
			}
		}
	}

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if (dynamic_cast<MeterSection*> (*i) != 0) {
			if (meter.frame() == (*i)->frame()) {
				if (!(*i)->initial()) {
					delete (*i);
					_metrics.erase (i);
					return true;
				}
			}
		}
	}

	return false;
}

void
TempoMap::do_insert (MetricSection* section)
{
	bool need_add = true;
	/* we only allow new meters to be inserted on beat 1 of an existing
	 * measure.
	 */
	MeterSection* m = 0;
	if ((m = dynamic_cast<MeterSection*>(section)) != 0) {

		if ((m->bbt().beats != 1) || (m->bbt().ticks != 0)) {

			pair<double, BBT_Time> corrected = make_pair (m->beat(), m->bbt());
			corrected.second.beats = 1;
			corrected.second.ticks = 0;
			corrected.first = beat_at_bbt_locked (_metrics, corrected.second);
			warning << string_compose (_("Meter changes can only be positioned on the first beat of a bar. Moving from %1 to %2"),
						   m->bbt(), corrected.second) << endmsg;
			//m->set_pulse (corrected);
		}
	}

	/* Look for any existing MetricSection that is of the same type and
	   in the same bar as the new one, and remove it before adding
	   the new one. Note that this means that if we find a matching,
	   existing section, we can break out of the loop since we're
	   guaranteed that there is only one such match.
	*/

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {

		TempoSection* const tempo = dynamic_cast<TempoSection*> (*i);
		TempoSection* const insert_tempo = dynamic_cast<TempoSection*> (section);
		MeterSection* const meter = dynamic_cast<MeterSection*> (*i);
		MeterSection* const insert_meter = dynamic_cast<MeterSection*> (section);

		if (tempo && insert_tempo) {

			/* Tempo sections */
			bool const ipm = insert_tempo->position_lock_style() == MusicTime;
			if ((ipm && tempo->pulse() == insert_tempo->pulse()) || (!ipm && tempo->frame() == insert_tempo->frame())) {

				if (tempo->initial()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section).
					 */

					*(dynamic_cast<Tempo*>(*i)) = *(dynamic_cast<Tempo*>(insert_tempo));
					(*i)->set_position_lock_style (AudioTime);
					TempoSection* t;
					if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {
						t->set_type (insert_tempo->type());
					}
					need_add = false;
				} else {
					delete (*i);
					_metrics.erase (i);
				}
				break;
			}

		} else if (meter && insert_meter) {

			/* Meter Sections */

			bool const ipm = insert_meter->position_lock_style() == MusicTime;

			if ((ipm && meter->beat() == insert_meter->beat()) || (!ipm && meter->frame() == insert_meter->frame())) {

				if (meter->initial()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section
					 */

					*(dynamic_cast<Meter*>(*i)) = *(dynamic_cast<Meter*>(insert_meter));
					(*i)->set_position_lock_style (AudioTime);
					need_add = false;
				} else {
					delete (*i);
					_metrics.erase (i);
				}

				break;
			}
		} else {
			/* non-matching types, so we don't care */
		}
	}

	/* Add the given MetricSection, if we didn't just reset an existing
	 * one above
	 */

	if (need_add) {
		MeterSection* const insert_meter = dynamic_cast<MeterSection*> (section);
		TempoSection* const insert_tempo = dynamic_cast<TempoSection*> (section);
		Metrics::iterator i;

		if (insert_meter) {
			TempoSection* prev_t = 0;

			for (i = _metrics.begin(); i != _metrics.end(); ++i) {
				MeterSection* const meter = dynamic_cast<MeterSection*> (*i);
				bool const ipm = insert_meter->position_lock_style() == MusicTime;

				if (meter) {
					if ((ipm && meter->beat() > insert_meter->beat()) || (!ipm && meter->frame() > insert_meter->frame())) {
						break;
					}
				} else {
					if (prev_t && prev_t->locked_to_meter() && (!ipm && prev_t->frame() == insert_meter->frame())) {
						break;
					}

					prev_t = dynamic_cast<TempoSection*> (*i);
				}
			}
		} else if (insert_tempo) {
			for (i = _metrics.begin(); i != _metrics.end(); ++i) {
				TempoSection* const tempo = dynamic_cast<TempoSection*> (*i);

				if (tempo) {
					bool const ipm = insert_tempo->position_lock_style() == MusicTime;
					const bool lm = insert_tempo->locked_to_meter();
					if ((ipm && tempo->pulse() > insert_tempo->pulse()) || (!ipm && tempo->frame() > insert_tempo->frame())
					    || (lm && tempo->pulse() > insert_tempo->pulse())) {
						break;
					}
				}
			}
		}

		_metrics.insert (i, section);
		//dump (std::cout);
	}
}
/* user supplies the exact pulse if pls == MusicTime */
TempoSection*
TempoMap::add_tempo (const Tempo& tempo, const double& pulse, const framepos_t& frame, ARDOUR::TempoSection::Type type, PositionLockStyle pls)
{
	if (tempo.note_types_per_minute() <= 0.0) {
		warning << "Cannot add tempo. note types per minute must be greater than zero." << endmsg;
		return 0;
	}

	TempoSection* ts = 0;
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		ts = add_tempo_locked (tempo, pulse, minute_at_frame (frame), type, pls, true);
	}


	PropertyChanged (PropertyChange ());

	return ts;
}

void
TempoMap::replace_tempo (TempoSection& ts, const Tempo& tempo, const double& pulse, const framepos_t& frame, TempoSection::Type type, PositionLockStyle pls)
{
	if (tempo.note_types_per_minute() <= 0.0) {
		warning << "Cannot replace tempo. note types per minute must be greater than zero." << endmsg;
		return;
	}

	const bool locked_to_meter = ts.locked_to_meter();

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection& first (first_tempo());
		if (!ts.initial()) {
			if (ts.locked_to_meter()) {
				ts.set_type (type);
				{
					/* cannot move a meter-locked tempo section */
					*static_cast<Tempo*>(&ts) = tempo;
					recompute_map (_metrics);
				}
			} else {
				remove_tempo_locked (ts);
				add_tempo_locked (tempo, pulse, minute_at_frame (frame), type, pls, true, locked_to_meter);
			}
		} else {
			first.set_type (type);
			first.set_pulse (0.0);
			first.set_minute (minute_at_frame (frame));
			first.set_position_lock_style (AudioTime);
			{
				/* cannot move the first tempo section */
				*static_cast<Tempo*>(&first) = tempo;
				recompute_map (_metrics);
			}
		}
	}

	PropertyChanged (PropertyChange ());
}

TempoSection*
TempoMap::add_tempo_locked (const Tempo& tempo, double pulse, double minute
			    , TempoSection::Type type, PositionLockStyle pls, bool recompute, bool locked_to_meter)
{
	TempoSection* t = new TempoSection (pulse, minute, tempo.note_types_per_minute(), tempo.note_type(), type, pls, _frame_rate);
	t->set_locked_to_meter (locked_to_meter);
	bool solved = false;

	do_insert (t);

	if (recompute) {
		if (pls == AudioTime) {
			solved = solve_map_minute (_metrics, t, t->minute());
		} else {
			solved = solve_map_pulse (_metrics, t, t->pulse());
		}
		recompute_meters (_metrics);
	}

	if (!solved && recompute) {
		recompute_map (_metrics);
	}

	return t;
}

MeterSection*
TempoMap::add_meter (const Meter& meter, const double& beat, const Timecode::BBT_Time& where, framepos_t frame, PositionLockStyle pls)
{
	MeterSection* m = 0;
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		m = add_meter_locked (meter, beat, where, frame, pls, true);
	}


#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TempoMap)) {
		dump (std::cerr);
	}
#endif

	PropertyChanged (PropertyChange ());
	return m;
}

void
TempoMap::replace_meter (const MeterSection& ms, const Meter& meter, const BBT_Time& where, framepos_t frame, PositionLockStyle pls)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		const double beat = beat_at_bbt_locked (_metrics, where);

		if (!ms.initial()) {
			remove_meter_locked (ms);
			add_meter_locked (meter, beat, where, frame, pls, true);
		} else {
			MeterSection& first (first_meter());
			TempoSection& first_t (first_tempo());
			/* cannot move the first meter section */
			*static_cast<Meter*>(&first) = meter;
			first.set_position_lock_style (AudioTime);
			first.set_pulse (0.0);
			first.set_minute (minute_at_frame (frame));
			pair<double, BBT_Time> beat = make_pair (0.0, BBT_Time (1, 1, 0));
			first.set_beat (beat);
			first_t.set_minute (first.minute());
			first_t.set_pulse (0.0);
			first_t.set_position_lock_style (AudioTime);
			recompute_map (_metrics);
		}
	}

	PropertyChanged (PropertyChange ());
}

MeterSection*
TempoMap::add_meter_locked (const Meter& meter, double beat, const BBT_Time& where, framepos_t frame, PositionLockStyle pls, bool recompute)
{
	const MeterSection& prev_m = meter_section_at_minute_locked  (_metrics, minute_at_beat_locked (_metrics, beat) - minute_at_frame (1));
	const double pulse = ((where.bars - prev_m.bbt().bars) * (prev_m.divisions_per_bar() / prev_m.note_divisor())) + prev_m.pulse();
	const double time_minutes = minute_at_pulse_locked (_metrics, pulse);
	TempoSection* mlt = 0;

	if (pls == AudioTime) {
		/* add meter-locked tempo */
		mlt = add_tempo_locked (tempo_at_minute_locked (_metrics, time_minutes), pulse, minute_at_frame (frame), TempoSection::Ramp, AudioTime, true, true);

		if (!mlt) {
			return 0;
		}

	}

	MeterSection* new_meter = new MeterSection (pulse, minute_at_frame (frame), beat, where, meter.divisions_per_bar(), meter.note_divisor(), pls, _frame_rate);

	bool solved = false;

	do_insert (new_meter);

	if (recompute) {

		if (pls == AudioTime) {
			solved = solve_map_minute (_metrics, new_meter, minute_at_frame (frame));
			/* we failed, most likely due to some impossible frame requirement wrt audio-locked tempi.
			   fudge frame so that the meter ends up at its BBT position instead.
			*/
			if (!solved) {
				solved = solve_map_minute (_metrics, new_meter, minute_at_frame (prev_m.frame() + 1));
			}
		} else {
			solved = solve_map_bbt (_metrics, new_meter, where);
			/* required due to resetting the pulse of meter-locked tempi above.
			   Arguably  solve_map_bbt() should use solve_map_pulse (_metrics, TempoSection) instead,
			   but afaict this cannot cause the map to be left unsolved (these tempi are all audio locked).
			*/
			recompute_map (_metrics);
		}
	}

	if (!solved && recompute) {
		/* if this has failed to solve, there is little we can do other than to ensure that
		   the new map is recalculated.
		*/
		warning << "Adding meter may have left the tempo map unsolved." << endmsg;
		recompute_map (_metrics);
	}

	return new_meter;
}

void
TempoMap::change_initial_tempo (double note_types_per_minute, double note_type)
{
	Tempo newtempo (note_types_per_minute, note_type);
	TempoSection* t;

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
			if (!t->active()) {
				continue;
			}
			{
				Glib::Threads::RWLock::WriterLock lm (lock);
				*((Tempo*) t) = newtempo;
				recompute_map (_metrics);
			}
			PropertyChanged (PropertyChange ());
			break;
		}
	}
}

void
TempoMap::change_existing_tempo_at (framepos_t where, double note_types_per_minute, double note_type)
{
	Tempo newtempo (note_types_per_minute, note_type);

	TempoSection* prev;
	TempoSection* first;
	Metrics::iterator i;

	/* find the TempoSection immediately preceding "where"
	 */

	for (first = 0, i = _metrics.begin(), prev = 0; i != _metrics.end(); ++i) {

		if ((*i)->frame() > where) {
			break;
		}

		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {
			if (!t->active()) {
				continue;
			}
			if (!first) {
				first = t;
			}
			prev = t;
		}
	}

	if (!prev) {
		if (!first) {
			error << string_compose (_("no tempo sections defined in tempo map - cannot change tempo @ %1"), where) << endmsg;
			return;
		}

		prev = first;
	}

	/* reset */

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		/* cannot move the first tempo section */
		*((Tempo*)prev) = newtempo;
		recompute_map (_metrics);
	}

	PropertyChanged (PropertyChange ());
}

const MeterSection&
TempoMap::first_meter () const
{
	const MeterSection *m = 0;

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if ((m = dynamic_cast<const MeterSection *> (*i)) != 0) {
			return *m;
		}
	}

	fatal << _("programming error: no meter section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *m;
}

MeterSection&
TempoMap::first_meter ()
{
	MeterSection *m = 0;

	/* CALLER MUST HOLD LOCK */

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if ((m = dynamic_cast<MeterSection *> (*i)) != 0) {
			return *m;
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *m;
}

const TempoSection&
TempoMap::first_tempo () const
{
	const TempoSection *t = 0;

	/* CALLER MUST HOLD LOCK */

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if ((t = dynamic_cast<const TempoSection *> (*i)) != 0) {
			if (!t->active()) {
				continue;
			}
			if (t->initial()) {
				return *t;
			}
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *t;
}

TempoSection&
TempoMap::first_tempo ()
{
	TempoSection *t = 0;

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if ((t = dynamic_cast<TempoSection *> (*i)) != 0) {
			if (!t->active()) {
				continue;
			}
			if (t->initial()) {
				return *t;
			}
		}
	}

	fatal << _("programming error: no tempo section in tempo map!") << endmsg;
	abort(); /*NOTREACHED*/
	return *t;
}
void
TempoMap::recompute_tempi (Metrics& metrics)
{
	TempoSection* prev_t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (t->initial()) {
				if (!prev_t) {
					t->set_pulse (0.0);
					prev_t = t;
					continue;
				}
			}
			if (prev_t) {
				if (t->position_lock_style() == AudioTime) {
					prev_t->set_c (prev_t->compute_c_func_minute (t->note_types_per_minute(), t->minute()));
					if (!t->locked_to_meter()) {
						t->set_pulse (prev_t->pulse_at_ntpm (t->note_types_per_minute(), t->minute()));
					}

				} else {
					prev_t->set_c (prev_t->compute_c_func_pulse (t->note_types_per_minute(), t->pulse()));
					t->set_minute (prev_t->minute_at_ntpm (t->note_types_per_minute(), t->pulse()));

				}
			}
			prev_t = t;
		}
	}
	assert (prev_t);
	prev_t->set_c (0.0);
}

/* tempos must be positioned correctly.
   the current approach is to use a meter's bbt time as its base position unit.
   an audio-locked meter requires a recomputation of pulse and beat (but not bbt),
   while a music-locked meter requires recomputations of frame pulse and beat (but not bbt)
*/
void
TempoMap::recompute_meters (Metrics& metrics)
{
	MeterSection* meter = 0;
	MeterSection* prev_m = 0;

	for (Metrics::const_iterator mi = metrics.begin(); mi != metrics.end(); ++mi) {
		if (!(*mi)->is_tempo()) {
			meter = static_cast<MeterSection*> (*mi);
			if (meter->position_lock_style() == AudioTime) {
				double pulse = 0.0;
				pair<double, BBT_Time> b_bbt;
				TempoSection* meter_locked_tempo = 0;
				for (Metrics::const_iterator ii = metrics.begin(); ii != metrics.end(); ++ii) {
					TempoSection* t;
					if ((*ii)->is_tempo()) {
						t = static_cast<TempoSection*> (*ii);
						if ((t->locked_to_meter() || t->initial()) && t->frame() == meter->frame()) {
							meter_locked_tempo = t;
							break;
						}
					}
				}

				if (prev_m) {
					double beats = (meter->bbt().bars - prev_m->bbt().bars) * prev_m->divisions_per_bar();
					if (beats + prev_m->beat() != meter->beat()) {
						/* reordering caused a bbt change */

						beats = meter->beat() - prev_m->beat();
						b_bbt = make_pair (beats + prev_m->beat()
								   , BBT_Time ((beats / prev_m->divisions_per_bar()) + prev_m->bbt().bars, 1, 0));
						pulse = prev_m->pulse() + (beats / prev_m->note_divisor());

					} else if (!meter->initial()) {
						b_bbt = make_pair (meter->beat(), meter->bbt());
						pulse = prev_m->pulse() + (beats / prev_m->note_divisor());
					}
				} else {
					b_bbt = make_pair (0.0, BBT_Time (1, 1, 0));
				}
				if (meter_locked_tempo) {
					meter_locked_tempo->set_pulse (pulse);
				}
				meter->set_beat (b_bbt);
				meter->set_pulse (pulse);

			} else {
				/* MusicTime */
				double pulse = 0.0;
				pair<double, BBT_Time> b_bbt;
				if (prev_m) {
					const double beats = (meter->bbt().bars - prev_m->bbt().bars) * prev_m->divisions_per_bar();
					if (beats + prev_m->beat() != meter->beat()) {
						/* reordering caused a bbt change */
						b_bbt = make_pair (beats + prev_m->beat()
								   , BBT_Time ((beats / prev_m->divisions_per_bar()) + prev_m->bbt().bars, 1, 0));
					} else {
						b_bbt = make_pair (beats + prev_m->beat(), meter->bbt());
					}
					pulse = (beats / prev_m->note_divisor()) + prev_m->pulse();
				} else {
					/* shouldn't happen - the first is audio-locked */
					pulse = pulse_at_beat_locked (metrics, meter->beat());
					b_bbt = make_pair (meter->beat(), meter->bbt());
				}

				meter->set_beat (b_bbt);
				meter->set_pulse (pulse);
				meter->set_minute (minute_at_pulse_locked (metrics, pulse));
			}

			prev_m = meter;
		}
	}
}

void
TempoMap::recompute_map (Metrics& metrics, framepos_t end)
{
	/* CALLER MUST HOLD WRITE LOCK */

	DEBUG_TRACE (DEBUG::TempoMath, string_compose ("recomputing tempo map, zero to %1\n", end));

	if (end == 0) {
		/* silly call from Session::process() during startup
		 */
		return;
	}

	recompute_tempi (metrics);
	recompute_meters (metrics);
}

TempoMetric
TempoMap::metric_at (framepos_t frame, Metrics::const_iterator* last) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {

		if ((*i)->frame() > frame) {
			break;
		}

		m.set_metric(*i);

		if (last) {
			*last = i;
		}
	}

	return m;
}

/* XX meters only */
TempoMetric
TempoMap::metric_at (BBT_Time bbt) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		MeterSection* mw;
		if (!(*i)->is_tempo()) {
			mw = static_cast<MeterSection*> (*i);
			BBT_Time section_start (mw->bbt());

			if (section_start.bars > bbt.bars || (section_start.bars == bbt.bars && section_start.beats > bbt.beats)) {
				break;
			}

			m.set_metric (*i);
		}
	}

	return m;
}

/** Returns the BBT (meter-based) beat corresponding to the supplied frame, possibly returning a negative value.
 * @param frame The session frame position.
 * @return The beat duration according to the tempo map at the supplied frame.
 *
 * If the supplied frame lies before the first meter, the returned beat duration will be negative.
 * The returned beat is obtained using the first meter and the continuation of the tempo curve (backwards).
 *
 * This function uses both tempo and meter.
 */
double
TempoMap::beat_at_frame (const framecnt_t& frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return beat_at_minute_locked (_metrics, minute_at_frame (frame));
}

/* This function uses both tempo and meter.*/
double
TempoMap::beat_at_minute_locked (const Metrics& metrics, const double& minute) const
{
	const TempoSection& ts = tempo_section_at_minute_locked (metrics, minute);
	MeterSection* prev_m = 0;
	MeterSection* next_m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			if (prev_m && (*i)->minute() > minute) {
				next_m = static_cast<MeterSection*> (*i);
				break;
			}
			prev_m = static_cast<MeterSection*> (*i);
		}
	}

	const double beat = prev_m->beat() + (ts.pulse_at_minute (minute) - prev_m->pulse()) * prev_m->note_divisor();

	/* audio locked meters fake their beat */
	if (next_m && next_m->beat() < beat) {
		return next_m->beat();
	}

	return beat;
}

/** Returns the frame corresponding to the supplied BBT (meter-based) beat.
 * @param beat The BBT (meter-based) beat.
 * @return The frame duration according to the tempo map at the supplied BBT (meter-based) beat.
 *
 * This function uses both tempo and meter.
 */
framepos_t
TempoMap::frame_at_beat (const double& beat) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return frame_at_minute (minute_at_beat_locked (_metrics, beat));
}

/* meter & tempo section based */
double
TempoMap::minute_at_beat_locked (const Metrics& metrics, const double& beat) const
{
	MeterSection* prev_m = 0;
	TempoSection* prev_t = 0;

	MeterSection* m;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m && m->beat() > beat) {
				break;
			}
			prev_m = m;
		}
	}
	assert (prev_m);

	TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (prev_t && ((t->pulse() - prev_m->pulse()) * prev_m->note_divisor()) + prev_m->beat() > beat) {
				break;
			}
			prev_t = t;
		}

	}
	assert (prev_t);

	return prev_t->minute_at_pulse (((beat - prev_m->beat()) / prev_m->note_divisor()) + prev_m->pulse());
}

/** Returns a Tempo corresponding to the supplied frame position.
 * @param frame The audio frame.
 * @return a Tempo according to the tempo map at the supplied frame.
 *
 */
Tempo
TempoMap::tempo_at_frame (const framepos_t& frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_at_minute_locked (_metrics, minute_at_frame (frame));
}

Tempo
TempoMap::tempo_at_minute_locked (const Metrics& metrics, const double& minute) const
{
	TempoSection* prev_t = 0;

	TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if ((prev_t) && t->minute() > minute) {
				/* t is the section past frame */
				return prev_t->tempo_at_minute (minute);
			}
			prev_t = t;
		}
	}

	return Tempo (prev_t->note_types_per_minute(), prev_t->note_type());
}

/** returns the frame at which the supplied tempo occurs, or
 *  the frame of the last tempo section (search exhausted)
 *  only the position of the first occurence will be returned
 *  (extend me)
*/
framepos_t
TempoMap::frame_at_tempo (const Tempo& tempo) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return frame_at_minute (minute_at_tempo_locked (_metrics, tempo));
}

double
TempoMap::minute_at_tempo_locked (const Metrics& metrics, const Tempo& tempo) const
{
	TempoSection* prev_t = 0;
	const double tempo_bpm = tempo.note_types_per_minute();

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);

			if (!t->active()) {
				continue;
			}

			const double t_bpm = t->note_types_per_minute();

			if (t_bpm == tempo_bpm) {
				return t->minute();
			}

			if (prev_t) {
				const double prev_t_bpm = prev_t->note_types_per_minute();

				if ((t_bpm > tempo_bpm && prev_t_bpm < tempo_bpm) || (t_bpm < tempo_bpm && prev_t_bpm > tempo_bpm)) {
					return prev_t->minute_at_ntpm (prev_t->note_types_per_minute(), prev_t->pulse());
				}
			}
			prev_t = t;
		}
	}

	return prev_t->minute();
}

Tempo
TempoMap::tempo_at_pulse_locked (const Metrics& metrics, const double& pulse) const
{
	TempoSection* prev_t = 0;

	TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if ((prev_t) && t->pulse() > pulse) {
				/* t is the section past frame */
				return prev_t->tempo_at_pulse (pulse);
			}
			prev_t = t;
		}
	}

	return Tempo (prev_t->note_types_per_minute(), prev_t->note_type());
}

double
TempoMap::pulse_at_tempo_locked (const Metrics& metrics, const Tempo& tempo) const
{
	TempoSection* prev_t = 0;
	const double tempo_bpm = tempo.note_types_per_minute();

	Metrics::const_iterator i;

	for (i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);

			if (!t->active()) {
				continue;
			}

			const double t_bpm = t->note_types_per_minute();

			if (t_bpm == tempo_bpm) {
				return t->pulse();
			}

			if (prev_t) {
				const double prev_t_bpm = prev_t->note_types_per_minute();

				if ((t_bpm > tempo_bpm && prev_t_bpm < tempo_bpm) || (t_bpm < tempo_bpm && prev_t_bpm > tempo_bpm)) {
					return prev_t->pulse_at_ntpm (prev_t->note_types_per_minute(), prev_t->minute());
				}
			}
			prev_t = t;
		}
	}

	return prev_t->pulse();
}

/** Returns a Tempo corresponding to the supplied position in quarter-note beats.
 * @param qn the position in quarter note beats.
 * @return the Tempo at the supplied quarter-note.
 */
Tempo
TempoMap::tempo_at_quarter_note (const double& qn) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_at_pulse_locked (_metrics, qn / 4.0);
}

/** Returns the position in quarter-note beats corresponding to the supplied Tempo.
 * @param tempo the tempo.
 * @return the position in quarter-note beats where the map bpm
 * is equal to that of the Tempo. currently ignores note_type.
 */
double
TempoMap::quarter_note_at_tempo (const Tempo& tempo) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return pulse_at_tempo_locked (_metrics, tempo) * 4.0;;
}

/** Returns the whole-note pulse corresponding to the supplied  BBT (meter-based) beat.
 * @param metrics the list of metric sections used to calculate the pulse.
 * @param beat The BBT (meter-based) beat.
 * @return the whole-note pulse at the supplied BBT (meter-based) beat.
 *
 * a pulse or whole note is the base musical position of a MetricSection.
 * it is equivalent to four quarter notes.
 *
 */
double
TempoMap::pulse_at_beat_locked (const Metrics& metrics, const double& beat) const
{
	const MeterSection* prev_m = &meter_section_at_beat_locked (metrics, beat);

	return prev_m->pulse() + ((beat - prev_m->beat()) / prev_m->note_divisor());
}

/** Returns the BBT (meter-based) beat corresponding to the supplied whole-note pulse .
 * @param metrics the list of metric sections used to calculate the beat.
 * @param pulse the whole-note pulse.
 * @return the meter-based beat at the supplied whole-note pulse.
 *
 * a pulse or whole note is the base musical position of a MetricSection.
 * it is equivalent to four quarter notes.
 */
double
TempoMap::beat_at_pulse_locked (const Metrics& metrics, const double& pulse) const
{
	MeterSection* prev_m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m;
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m && m->pulse() > pulse) {
				break;
			}
			prev_m = m;
		}
	}
	assert (prev_m);

	double const ret = ((pulse - prev_m->pulse()) * prev_m->note_divisor()) + prev_m->beat();
	return ret;
}

/* tempo section based */
double
TempoMap::pulse_at_minute_locked (const Metrics& metrics, const double& minute) const
{
	/* HOLD (at least) THE READER LOCK */
	TempoSection* prev_t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t && t->minute() > minute) {
				/*the previous ts is the one containing the frame */
				const double ret = prev_t->pulse_at_minute (minute);
				/* audio locked section in new meter*/
				if (t->pulse() < ret) {
					return t->pulse();
				}
				return ret;
			}
			prev_t = t;
		}
	}

	/* treated as constant for this ts */
	const double pulses_in_section = ((minute - prev_t->minute()) * prev_t->note_types_per_minute()) / prev_t->note_type();

	return pulses_in_section + prev_t->pulse();
}

/* tempo section based */
double
TempoMap::minute_at_pulse_locked (const Metrics& metrics, const double& pulse) const
{
	/* HOLD THE READER LOCK */

	const TempoSection* prev_t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t && t->pulse() > pulse) {
				return prev_t->minute_at_pulse (pulse);
			}

			prev_t = t;
		}
	}
	/* must be treated as constant, irrespective of _type */
	double const dtime = ((pulse - prev_t->pulse()) * prev_t->note_type()) / prev_t->note_types_per_minute();

	return dtime + prev_t->minute();
}

/** Returns the BBT (meter-based) beat corresponding to the supplied BBT time.
 * @param bbt The BBT time (meter-based).
 * @return bbt The BBT beat (meter-based) at the supplied BBT time.
 *
 */
double
TempoMap::beat_at_bbt (const Timecode::BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return beat_at_bbt_locked (_metrics, bbt);
}


double
TempoMap::beat_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time& bbt) const
{
	/* CALLER HOLDS READ LOCK */

	MeterSection* prev_m = 0;

	/* because audio-locked meters have 'fake' integral beats,
	   there is no pulse offset here.
	*/
	MeterSection* m;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m) {
				const double bars_to_m = (m->beat() - prev_m->beat()) / prev_m->divisions_per_bar();
				if ((bars_to_m + (prev_m->bbt().bars - 1)) > (bbt.bars - 1)) {
					break;
				}
			}
			prev_m = m;
		}
	}

	const double remaining_bars = bbt.bars - prev_m->bbt().bars;
	const double remaining_bars_in_beats = remaining_bars * prev_m->divisions_per_bar();
	const double ret = remaining_bars_in_beats + prev_m->beat() + (bbt.beats - 1) + (bbt.ticks / BBT_Time::ticks_per_beat);

	return ret;
}

/** Returns the BBT time corresponding to the supplied BBT (meter-based) beat.
 * @param beat The BBT (meter-based) beat.
 * @return The BBT time (meter-based) at the supplied meter-based beat.
 *
 */
Timecode::BBT_Time
TempoMap::bbt_at_beat (const double& beat)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return bbt_at_beat_locked (_metrics, beat);
}

Timecode::BBT_Time
TempoMap::bbt_at_beat_locked (const Metrics& metrics, const double& b) const
{
	/* CALLER HOLDS READ LOCK */
	MeterSection* prev_m = 0;
	const double beats = max (0.0, b);

	MeterSection* m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m) {
				if (m->beat() > beats) {
					/* this is the meter after the one our beat is on*/
					break;
				}
			}

			prev_m = m;
		}
	}
	assert (prev_m);

	const double beats_in_ms = beats - prev_m->beat();
	const uint32_t bars_in_ms = (uint32_t) floor (beats_in_ms / prev_m->divisions_per_bar());
	const uint32_t total_bars = bars_in_ms + (prev_m->bbt().bars - 1);
	const double remaining_beats = beats_in_ms - (bars_in_ms * prev_m->divisions_per_bar());
	const double remaining_ticks = (remaining_beats - floor (remaining_beats)) * BBT_Time::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 - based mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= BBT_Time::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= BBT_Time::ticks_per_beat;
	}

	if (ret.beats >= prev_m->divisions_per_bar() + 1) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

/** Returns the quarter-note beat corresponding to the supplied BBT time (meter-based).
 * @param bbt The BBT time (meter-based).
 * @return the quarter note beat at the supplied BBT time
 *
 * quarter-notes ignore meter and are based on pulse (the musical unit of MetricSection).
 *
 * while the input uses meter, the output does not.
 */
double
TempoMap::quarter_note_at_bbt (const Timecode::BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return pulse_at_bbt_locked (_metrics, bbt) * 4.0;
}

double
TempoMap::quarter_note_at_bbt_rt (const Timecode::BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::quarter_note_at_bbt_rt() could not lock tempo map");
	}

	return pulse_at_bbt_locked (_metrics, bbt) * 4.0;
}

double
TempoMap::pulse_at_bbt_locked (const Metrics& metrics, const Timecode::BBT_Time& bbt) const
{
	/* CALLER HOLDS READ LOCK */

	MeterSection* prev_m = 0;

	/* because audio-locked meters have 'fake' integral beats,
	   there is no pulse offset here.
	*/
	MeterSection* m;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m) {
				if (m->bbt().bars > bbt.bars) {
					break;
				}
			}
			prev_m = m;
		}
	}

	const double remaining_bars = bbt.bars - prev_m->bbt().bars;
	const double remaining_pulses = remaining_bars * prev_m->divisions_per_bar() / prev_m->note_divisor();
	const double ret = remaining_pulses + prev_m->pulse() + (((bbt.beats - 1) + (bbt.ticks / BBT_Time::ticks_per_beat)) / prev_m->note_divisor());

	return ret;
}

/** Returns the BBT time corresponding to the supplied quarter-note beat.
 * @param qn the quarter-note beat.
 * @return The BBT time (meter-based) at the supplied meter-based beat.
 *
 * quarter-notes ignore meter and are based on pulse (the musical unit of MetricSection).
 *
 */
Timecode::BBT_Time
TempoMap::bbt_at_quarter_note (const double& qn)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return bbt_at_pulse_locked (_metrics, qn / 4.0);
}

/** Returns the BBT time (meter-based) corresponding to the supplied whole-note pulse position.
 * @param metrics The list of metric sections used to determine the result.
 * @param pulse The whole-note pulse.
 * @return The BBT time at the supplied whole-note pulse.
 *
 * a pulse or whole note is the basic musical position of a MetricSection.
 * it is equivalent to four quarter notes.
 * while the output uses meter, the input does not.
 */
Timecode::BBT_Time
TempoMap::bbt_at_pulse_locked (const Metrics& metrics, const double& pulse) const
{
	MeterSection* prev_m = 0;

	MeterSection* m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);

			if (prev_m) {
				double const pulses_to_m = m->pulse() - prev_m->pulse();
				if (prev_m->pulse() + pulses_to_m > pulse) {
					/* this is the meter after the one our beat is on*/
					break;
				}
			}

			prev_m = m;
		}
	}

	assert (prev_m);

	const double beats_in_ms = (pulse - prev_m->pulse()) * prev_m->note_divisor();
	const uint32_t bars_in_ms = (uint32_t) floor (beats_in_ms / prev_m->divisions_per_bar());
	const uint32_t total_bars = bars_in_ms + (prev_m->bbt().bars - 1);
	const double remaining_beats = beats_in_ms - (bars_in_ms * prev_m->divisions_per_bar());
	const double remaining_ticks = (remaining_beats - floor (remaining_beats)) * BBT_Time::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= BBT_Time::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= BBT_Time::ticks_per_beat;
	}

	if (ret.beats >= prev_m->divisions_per_bar() + 1) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

/** Returns the BBT time corresponding to the supplied frame position.
 * @param frame the position in audio samples.
 * @return the BBT time at the frame position .
 *
 */
BBT_Time
TempoMap::bbt_at_frame (framepos_t frame)
{
	if (frame < 0) {
		BBT_Time bbt;
		bbt.bars = 1;
		bbt.beats = 1;
		bbt.ticks = 0;
		warning << string_compose (_("tempo map was asked for BBT time at frame %1\n"), frame) << endmsg;
		return bbt;
	}
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return bbt_at_minute_locked (_metrics, minute_at_frame (frame));
}

BBT_Time
TempoMap::bbt_at_frame_rt (framepos_t frame)
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::bbt_at_frame_rt() could not lock tempo map");
	}

	return bbt_at_minute_locked (_metrics, minute_at_frame (frame));
}

Timecode::BBT_Time
TempoMap::bbt_at_minute_locked (const Metrics& metrics, const double& minute) const
{
	if (minute < 0) {
		BBT_Time bbt;
		bbt.bars = 1;
		bbt.beats = 1;
		bbt.ticks = 0;
		return bbt;
	}

	const TempoSection& ts = tempo_section_at_minute_locked (metrics, minute);
	MeterSection* prev_m = 0;
	MeterSection* next_m = 0;

	MeterSection* m;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m && m->minute() > minute) {
				next_m = m;
				break;
			}
			prev_m = m;
		}
	}

	double beat = prev_m->beat() + (ts.pulse_at_minute (minute) - prev_m->pulse()) * prev_m->note_divisor();

	/* handle frame before first meter */
	if (minute < prev_m->minute()) {
		beat = 0.0;
	}
	/* audio locked meters fake their beat */
	if (next_m && next_m->beat() < beat) {
		beat = next_m->beat();
	}

	beat = max (0.0, beat);

	const double beats_in_ms = beat - prev_m->beat();
	const uint32_t bars_in_ms = (uint32_t) floor (beats_in_ms / prev_m->divisions_per_bar());
	const uint32_t total_bars = bars_in_ms + (prev_m->bbt().bars - 1);
	const double remaining_beats = beats_in_ms - (bars_in_ms * prev_m->divisions_per_bar());
	const double remaining_ticks = (remaining_beats - floor (remaining_beats)) * BBT_Time::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 - based mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= BBT_Time::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= BBT_Time::ticks_per_beat;
	}

	if (ret.beats >= prev_m->divisions_per_bar() + 1) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

/** Returns the frame position corresponding to the supplied BBT time.
 * @param bbt the position in BBT time.
 * @return the frame position at bbt.
 *
 */
framepos_t
TempoMap::frame_at_bbt (const BBT_Time& bbt)
{
	if (bbt.bars < 1) {
		warning << string_compose (_("tempo map asked for frame time at bar < 1  (%1)\n"), bbt) << endmsg;
		return 0;
	}

	if (bbt.beats < 1) {
		throw std::logic_error ("beats are counted from one");
	}
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return frame_at_minute (minute_at_bbt_locked (_metrics, bbt));
}

/* meter & tempo section based */
double
TempoMap::minute_at_bbt_locked (const Metrics& metrics, const BBT_Time& bbt) const
{
	/* HOLD THE READER LOCK */

	const double ret = minute_at_beat_locked (metrics, beat_at_bbt_locked (metrics, bbt));
	return ret;
}

/**
 * Returns the quarter-note beat position corresponding to the supplied frame.
 *
 * @param frame the position in frames.
 * @return The quarter-note position of the supplied frame. Ignores meter.
 *
*/
double
TempoMap::quarter_note_at_frame (const framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const double ret = quarter_note_at_minute_locked (_metrics, minute_at_frame (frame));

	return ret;
}

double
TempoMap::quarter_note_at_minute_locked (const Metrics& metrics, const double minute) const
{
	const double ret = pulse_at_minute_locked (metrics, minute) * 4.0;

	return ret;
}

double
TempoMap::quarter_note_at_frame_rt (const framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::quarter_note_at_frame_rt() could not lock tempo map");
	}

	const double ret = pulse_at_minute_locked (_metrics, minute_at_frame (frame)) * 4.0;

	return ret;
}

/**
 * Returns the frame position corresponding to the supplied quarter-note beat.
 *
 * @param quarter_note the quarter-note position.
 * @return the frame position of the supplied quarter-note. Ignores meter.
 *
 *
*/
framepos_t
TempoMap::frame_at_quarter_note (const double quarter_note) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const framepos_t ret = frame_at_minute (minute_at_quarter_note_locked (_metrics, quarter_note));

	return ret;
}

double
TempoMap::minute_at_quarter_note_locked (const Metrics& metrics, const double quarter_note) const
{
	const double ret = minute_at_pulse_locked (metrics, quarter_note / 4.0);

	return ret;
}

/** Returns the quarter-note beats corresponding to the supplied BBT (meter-based) beat.
 * @param beat The BBT (meter-based) beat.
 * @return The quarter-note position of the supplied BBT (meter-based) beat.
 *
 * a quarter-note may be compared with and assigned to Evoral::Beats.
 *
 */
double
TempoMap::quarter_note_at_beat (const double beat) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const double ret = quarter_note_at_beat_locked (_metrics, beat);

	return ret;
}

double
TempoMap::quarter_note_at_beat_locked (const Metrics& metrics, const double beat) const
{
	const double ret = pulse_at_beat_locked (metrics, beat) * 4.0;

	return ret;
}

/** Returns the BBT (meter-based) beat position corresponding to the supplied quarter-note beats.
 * @param quarter_note The position in quarter-note beats.
 * @return the BBT (meter-based) beat position of the supplied quarter-note beats.
 *
 * a quarter-note is the musical unit of Evoral::Beats.
 *
 */
double
TempoMap::beat_at_quarter_note (const double quarter_note) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const double ret = beat_at_quarter_note_locked (_metrics, quarter_note);

	return ret;
}

double
TempoMap::beat_at_quarter_note_locked (const Metrics& metrics, const double quarter_note) const
{

	return beat_at_pulse_locked (metrics, quarter_note / 4.0);
}

/** Returns the duration in frames between two supplied quarter-note beat positions.
 * @param start the first position in quarter-note beats.
 * @param end the end position in quarter-note beats.
 * @return the frame distance ober the quarter-note beats duration.
 *
 * use this rather than e.g.
 * frame_at-quarter_note (end_beats) - frame_at_quarter_note (start_beats).
 * frames_between_quarter_notes() doesn't round to audio frames as an intermediate step,
 *
 */
framecnt_t
TempoMap::frames_between_quarter_notes (const double start, const double end) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return frame_at_minute (minutes_between_quarter_notes_locked (_metrics, start, end));
}

double
TempoMap::minutes_between_quarter_notes_locked (const Metrics& metrics, const double start, const double end) const
{

	return minute_at_pulse_locked (metrics, end / 4.0) - minute_at_pulse_locked (metrics, start / 4.0);
}

double
TempoMap::quarter_notes_between_frames (const framecnt_t start, const framecnt_t end) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return quarter_notes_between_frames_locked (_metrics, start, end);
}

double
TempoMap::quarter_notes_between_frames_locked (const Metrics& metrics, const framecnt_t start, const framecnt_t end) const
{
	const TempoSection* prev_t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t && t->frame() > start) {
				break;
			}
			prev_t = t;
		}
	}
	assert (prev_t);
	const double start_qn = prev_t->pulse_at_frame (start);

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t && t->frame() > end) {
				break;
			}
			prev_t = t;
		}
	}
	const double end_qn = prev_t->pulse_at_frame (end);

	return (end_qn - start_qn) * 4.0;
}

bool
TempoMap::check_solved (const Metrics& metrics) const
{
	TempoSection* prev_t = 0;
	MeterSection* prev_m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		MeterSection* m;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t) {
				/* check ordering */
				if ((t->minute() <= prev_t->minute()) || (t->pulse() <= prev_t->pulse())) {
					return false;
				}

				/* precision check ensures tempo and frames align.*/
				if (t->frame() != frame_at_minute (prev_t->minute_at_ntpm (t->note_types_per_minute(), t->pulse()))) {
					if (!t->locked_to_meter()) {
						return false;
					}
				}

				/* gradient limit - who knows what it should be?
				   things are also ok (if a little chaotic) without this
				*/
				if (fabs (prev_t->c()) > 1000.0) {
					//std::cout << "c : " << prev_t->c() << std::endl;
					return false;
				}
			}
			prev_t = t;
		}

		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m && m->position_lock_style() == AudioTime) {
				const TempoSection* t = &tempo_section_at_minute_locked (metrics, minute_at_frame (m->frame() - 1));
				const framepos_t nascent_m_frame = frame_at_minute (t->minute_at_pulse (m->pulse()));
				/* Here we check that a preceding section of music doesn't overlap a subsequent one.
				*/
				if (t && (nascent_m_frame > m->frame() || nascent_m_frame < 0)) {
					return false;
				}
			}

			prev_m = m;
		}

	}

	return true;
}

bool
TempoMap::set_active_tempi (const Metrics& metrics, const framepos_t& frame)
{
	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (t->locked_to_meter()) {
				t->set_active (true);
			} else if (t->position_lock_style() == AudioTime) {
				if (t->frame() < frame) {
					t->set_active (false);
					t->set_pulse (-1.0);
				} else if (t->frame() > frame) {
					t->set_active (true);
				} else if (t->frame() == frame) {
					return false;
				}
			}
		}
	}
	return true;
}

bool
TempoMap::solve_map_minute (Metrics& imaginary, TempoSection* section, const double& minute)
{
	TempoSection* prev_t = 0;
	TempoSection* section_prev = 0;
	double first_m_minute = 0.0;
	const bool sml = section->locked_to_meter();

	/* can't move a tempo before the first meter */
	for (Metrics::iterator i = imaginary.begin(); i != imaginary.end(); ++i) {
		MeterSection* m;
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (m->initial()) {
				first_m_minute = m->minute();
				break;
			}
		}
	}
	if (!section->initial() && minute <= first_m_minute) {
		return false;
	}

	section->set_active (true);
	section->set_minute (minute);

	for (Metrics::iterator i = imaginary.begin(); i != imaginary.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);

			if (!t->active()) {
				continue;
			}

			if (prev_t) {

				if (t == section) {
					continue;
				}

				if (t->frame() == frame_at_minute (minute)) {
					return false;
				}

				const bool tlm = t->position_lock_style() == MusicTime;

				if (prev_t && !section_prev && ((sml && tlm && t->pulse() > section->pulse()) || (!tlm && t->minute() > minute))) {
					section_prev = prev_t;

					section_prev->set_c (section_prev->compute_c_func_minute (section->note_types_per_minute(), minute));
					if (!section->locked_to_meter()) {
						section->set_pulse (section_prev->pulse_at_ntpm (section->note_types_per_minute(), minute));
					}
					prev_t = section;
				}

				if (t->position_lock_style() == MusicTime) {
					prev_t->set_c (prev_t->compute_c_func_pulse (t->note_types_per_minute(), t->pulse()));
					t->set_minute (prev_t->minute_at_ntpm (t->note_types_per_minute(), t->pulse()));
				} else {
					prev_t->set_c (prev_t->compute_c_func_minute (t->note_types_per_minute(), t->minute()));
					if (!t->locked_to_meter()) {
						t->set_pulse (prev_t->pulse_at_ntpm (t->note_types_per_minute(), t->minute()));
					}
				}
			}
			prev_t = t;
		}
	}

#if (0)
	recompute_tempi (imaginary);

	if (check_solved (imaginary)) {
		return true;
	} else {
		dunp (imaginary, std::cout);
	}
#endif

	MetricSectionFrameSorter fcmp;
	imaginary.sort (fcmp);

	recompute_tempi (imaginary);

	if (check_solved (imaginary)) {
		return true;
	}

	return false;
}

bool
TempoMap::solve_map_pulse (Metrics& imaginary, TempoSection* section, const double& pulse)
{
	TempoSection* prev_t = 0;
	TempoSection* section_prev = 0;

	section->set_pulse (pulse);

	for (Metrics::iterator i = imaginary.begin(); i != imaginary.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (t->initial()) {
				t->set_pulse (0.0);
				prev_t = t;
				continue;
			}
			if (prev_t) {
				if (t == section) {
					section_prev = prev_t;
					continue;
				}

				if (t->position_lock_style() == MusicTime) {
					prev_t->set_c (prev_t->compute_c_func_pulse (t->note_types_per_minute(), t->pulse()));
					t->set_minute (prev_t->minute_at_ntpm (t->note_types_per_minute(), t->pulse()));
				} else {
					prev_t->set_c (prev_t->compute_c_func_minute (t->note_types_per_minute(), t->minute()));
					if (!t->locked_to_meter()) {
						t->set_pulse (prev_t->pulse_at_ntpm (t->note_types_per_minute(), t->minute()));
					}
				}
			}
			prev_t = t;
		}
	}

	if (section_prev) {
		section_prev->set_c (section_prev->compute_c_func_pulse (section->note_types_per_minute(), pulse));
		section->set_minute (section_prev->minute_at_ntpm (section->note_types_per_minute(), pulse));
	}

#if (0)
	recompute_tempi (imaginary);

	if (check_solved (imaginary)) {
		return true;
	} else {
		dunp (imaginary, std::cout);
	}
#endif

	MetricSectionSorter cmp;
	imaginary.sort (cmp);

	recompute_tempi (imaginary);
	/* Reordering
	 * XX need a restriction here, but only for this case,
	 * as audio locked tempos don't interact in the same way.
	 *
	 * With music-locked tempos, the solution to cross-dragging can fly off the screen
	 * e.g.
	 * |50 bpm                        |250 bpm |60 bpm
	 *                drag 250 to the pulse after 60->
	 * a clue: dragging the second 60 <- past the 250 would cause no such problem.
	 */
	if (check_solved (imaginary)) {
		return true;
	}

	return false;
}

bool
TempoMap::solve_map_minute (Metrics& imaginary, MeterSection* section, const double& minute)
{
	/* disallow moving first meter past any subsequent one, and any initial meter before the first one */
	const MeterSection* other =  &meter_section_at_minute_locked (imaginary, minute);
	if ((section->initial() && !other->initial()) || (other->initial() && !section->initial() && other->minute() >= minute)) {
		return false;
	}

	if (section->initial()) {
		/* lock the first tempo to our first meter */
		if (!set_active_tempi (imaginary, frame_at_minute (minute))) {
			return false;
		}
	}

	TempoSection* meter_locked_tempo = 0;

	for (Metrics::const_iterator ii = imaginary.begin(); ii != imaginary.end(); ++ii) {
		TempoSection* t;
		if ((*ii)->is_tempo()) {
			t = static_cast<TempoSection*> (*ii);
			if ((t->locked_to_meter() || t->initial()) && t->frame() == section->frame()) {
				meter_locked_tempo = t;
				break;
			}
		}
	}

	if (!meter_locked_tempo) {
		return false;
	}

	MeterSection* prev_m = 0;
	Metrics future_map;
	TempoSection* tempo_copy = copy_metrics_and_point (imaginary, future_map, meter_locked_tempo);
	bool solved = false;

	for (Metrics::iterator i = imaginary.begin(); i != imaginary.end(); ++i) {
		MeterSection* m;
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (m == section){
				if (prev_m && !section->initial()) {
					const double beats = (pulse_at_minute_locked (imaginary, minute) - prev_m->pulse()) * prev_m->note_divisor();
					if (beats + prev_m->beat() < section->beat()) {
						/* set the section pulse according to its musical position,
						 * as an earlier time than this has been requested.
						*/
						const double new_pulse = ((section->beat() - prev_m->beat())
									  / prev_m->note_divisor()) + prev_m->pulse();

						tempo_copy->set_position_lock_style (MusicTime);
						if ((solved = solve_map_pulse (future_map, tempo_copy, new_pulse))) {
							meter_locked_tempo->set_position_lock_style (MusicTime);
							section->set_position_lock_style (MusicTime);
							section->set_pulse (new_pulse);
							solve_map_pulse (imaginary, meter_locked_tempo, new_pulse);
							meter_locked_tempo->set_position_lock_style (AudioTime);
							section->set_position_lock_style (AudioTime);
							section->set_minute (meter_locked_tempo->minute());

						} else {
							solved = false;
						}

						Metrics::const_iterator d = future_map.begin();
						while (d != future_map.end()) {
							delete (*d);
							++d;
						}

						if (!solved) {
							return false;
						}
					} else {
						/* all is ok. set section's locked tempo if allowed.
						   possibly disallow if there is an adjacent audio-locked tempo.
						   XX this check could possibly go. its never actually happened here.
						*/
						MeterSection* meter_copy = const_cast<MeterSection*>
							(&meter_section_at_minute_locked (future_map, section->minute()));

						meter_copy->set_minute (minute);

						if ((solved = solve_map_minute (future_map, tempo_copy, minute))) {
							section->set_minute (minute);
							meter_locked_tempo->set_pulse (((section->beat() - prev_m->beat())
												/ prev_m->note_divisor()) + prev_m->pulse());
							solve_map_minute (imaginary, meter_locked_tempo, minute);
						} else {
							solved = false;
						}

						Metrics::const_iterator d = future_map.begin();
						while (d != future_map.end()) {
							delete (*d);
							++d;
						}

						if (!solved) {
							return false;
						}
					}
				} else {
					/* initial (first meter atm) */

					tempo_copy->set_minute (minute);
					tempo_copy->set_pulse (0.0);

					if ((solved = solve_map_minute (future_map, tempo_copy, minute))) {
						section->set_minute (minute);
						meter_locked_tempo->set_minute (minute);
						meter_locked_tempo->set_pulse (0.0);
						solve_map_minute (imaginary, meter_locked_tempo, minute);
					} else {
						solved = false;
					}

					Metrics::const_iterator d = future_map.begin();
					while (d != future_map.end()) {
						delete (*d);
						++d;
					}

					if (!solved) {
						return false;
					}

					pair<double, BBT_Time> b_bbt = make_pair (0.0, BBT_Time (1, 1, 0));
					section->set_beat (b_bbt);
					section->set_pulse (0.0);

				}
				break;
			}

			prev_m = m;
		}
	}

	MetricSectionFrameSorter fcmp;
	imaginary.sort (fcmp);

	recompute_meters (imaginary);

	return true;
}

bool
TempoMap::solve_map_bbt (Metrics& imaginary, MeterSection* section, const BBT_Time& when)
{
	/* disallow setting section to an existing meter's bbt */
	for (Metrics::iterator i = imaginary.begin(); i != imaginary.end(); ++i) {
		MeterSection* m;
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (m != section && m->bbt().bars == when.bars) {
				return false;
			}
		}
	}

	MeterSection* prev_m = 0;
	MeterSection* section_prev = 0;

	for (Metrics::iterator i = imaginary.begin(); i != imaginary.end(); ++i) {
		MeterSection* m;
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);

			if (m == section) {
				continue;
			}

			pair<double, BBT_Time> b_bbt;
			double new_pulse = 0.0;

			if (prev_m && m->bbt().bars > when.bars && !section_prev){
				section_prev = prev_m;

				const double beats = (when.bars - section_prev->bbt().bars) * section_prev->divisions_per_bar();
				const double pulse = (beats / section_prev->note_divisor()) + section_prev->pulse();
				pair<double, BBT_Time> b_bbt = make_pair (beats + section_prev->beat(), when);

				section->set_beat (b_bbt);
				section->set_pulse (pulse);
				section->set_minute (minute_at_pulse_locked (imaginary, pulse));
				prev_m = section;
			}

			if (m->position_lock_style() == AudioTime) {
				TempoSection* meter_locked_tempo = 0;

				for (Metrics::const_iterator ii = imaginary.begin(); ii != imaginary.end(); ++ii) {
					TempoSection* t;
					if ((*ii)->is_tempo()) {
						t = static_cast<TempoSection*> (*ii);
						if ((t->locked_to_meter() || t->initial()) && t->frame() == m->frame()) {
							meter_locked_tempo = t;
							break;
						}
					}
				}

				if (!meter_locked_tempo) {
					return false;
				}

				if (prev_m) {
					double beats = ((m->bbt().bars - prev_m->bbt().bars) * prev_m->divisions_per_bar());

					if (beats + prev_m->beat() != m->beat()) {
						/* tempo/ meter change caused a change in beat (bar). */

						/* the user has requested that the previous section of music overlaps this one.
						   we have no choice but to change the bar number here, as being locked to audio means
						   we must stay where we are on the timeline.
						*/
						beats = m->beat() - prev_m->beat();
						b_bbt = make_pair (beats + prev_m->beat()
								   , BBT_Time ((beats / prev_m->divisions_per_bar()) + prev_m->bbt().bars, 1, 0));
						new_pulse = prev_m->pulse() + (beats / prev_m->note_divisor());

					} else if (!m->initial()) {
						b_bbt = make_pair (m->beat(), m->bbt());
						new_pulse = prev_m->pulse() + (beats / prev_m->note_divisor());
					}
				} else {
					b_bbt = make_pair (0.0, BBT_Time (1, 1, 0));
				}

				meter_locked_tempo->set_pulse (new_pulse);
				m->set_beat (b_bbt);
				m->set_pulse (new_pulse);

			} else {
				/* MusicTime */
				const double beats = ((m->bbt().bars - prev_m->bbt().bars) * prev_m->divisions_per_bar());
				if (beats + prev_m->beat() != m->beat()) {
					/* tempo/ meter change caused a change in beat (bar). */
					b_bbt = make_pair (beats + prev_m->beat()
							   , BBT_Time ((beats / prev_m->divisions_per_bar()) + prev_m->bbt().bars, 1, 0));
				} else {
					b_bbt = make_pair (beats + prev_m->beat()
							   , m->bbt());
				}
				new_pulse = (beats / prev_m->note_divisor()) + prev_m->pulse();
				m->set_beat (b_bbt);
				m->set_pulse (new_pulse);
				m->set_minute (minute_at_pulse_locked (imaginary, new_pulse));
			}

			prev_m = m;
		}
	}

	if (!section_prev) {

		const double beats = (when.bars - prev_m->bbt().bars) * prev_m->divisions_per_bar();
		const double pulse = (beats / prev_m->note_divisor()) + prev_m->pulse();
		pair<double, BBT_Time> b_bbt = make_pair (beats + prev_m->beat(), when);

		section->set_beat (b_bbt);
		section->set_pulse (pulse);
		section->set_minute (minute_at_pulse_locked (imaginary, pulse));
	}

	MetricSectionSorter cmp;
	imaginary.sort (cmp);

	recompute_meters (imaginary);

	return true;
}

/** places a copy of _metrics into copy and returns a pointer
 *  to section's equivalent in copy.
 */
TempoSection*
TempoMap::copy_metrics_and_point (const Metrics& metrics, Metrics& copy, TempoSection* section)
{
	TempoSection* ret = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		MeterSection* m;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (t == section) {
				ret = new TempoSection (*t);
				copy.push_back (ret);
				continue;
			}

			TempoSection* cp = new TempoSection (*t);
			copy.push_back (cp);
		}
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection *> (*i);
			MeterSection* cp = new MeterSection (*m);
			copy.push_back (cp);
		}
	}

	return ret;
}

MeterSection*
TempoMap::copy_metrics_and_point (const Metrics& metrics, Metrics& copy, MeterSection* section)
{
	MeterSection* ret = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		MeterSection* m;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			TempoSection* cp = new TempoSection (*t);
			copy.push_back (cp);
		}

		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection *> (*i);
			if (m == section) {
				ret = new MeterSection (*m);
				copy.push_back (ret);
				continue;
			}
			MeterSection* cp = new MeterSection (*m);
			copy.push_back (cp);
		}
	}

	return ret;
}

/** answers the question "is this a valid beat position for this tempo section?".
 *  it returns true if the tempo section can be moved to the requested bbt position,
 *  leaving the tempo map in a solved state.
 * @param ts the tempo section to be moved
 * @param bbt the requested new position for the tempo section
 * @return true if the tempo section can be moved to the position, otherwise false.
 */
bool
TempoMap::can_solve_bbt (TempoSection* ts, const BBT_Time& bbt)
{
	Metrics copy;
	TempoSection* tempo_copy = 0;

	{
		Glib::Threads::RWLock::ReaderLock lm (lock);
		tempo_copy = copy_metrics_and_point (_metrics, copy, ts);
		if (!tempo_copy) {
			return false;
		}
	}

	const bool ret = solve_map_pulse (copy, tempo_copy, pulse_at_bbt_locked (copy, bbt));

	Metrics::const_iterator d = copy.begin();
	while (d != copy.end()) {
		delete (*d);
		++d;
	}

	return ret;
}

/**
* This is for a gui that needs to know the pulse or frame of a tempo section if it were to be moved to some bbt time,
* taking any possible reordering as a consequence of this into account.
* @param section - the section to be altered
* @param bbt - the BBT time  where the altered tempo will fall
* @return returns - the position in pulses and frames (as a pair) where the new tempo section will lie.
*/
pair<double, framepos_t>
TempoMap::predict_tempo_position (TempoSection* section, const BBT_Time& bbt)
{
	Metrics future_map;
	pair<double, framepos_t> ret = make_pair (0.0, 0);

	Glib::Threads::RWLock::ReaderLock lm (lock);

	TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, section);

	const double beat = beat_at_bbt_locked (future_map, bbt);

	if (section->position_lock_style() == AudioTime) {
		tempo_copy->set_position_lock_style (MusicTime);
	}

	if (solve_map_pulse (future_map, tempo_copy, pulse_at_beat_locked (future_map, beat))) {
		ret.first = tempo_copy->pulse();
		ret.second = tempo_copy->frame();
	} else {
		ret.first = section->pulse();
		ret.second = section->frame();
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}
	return ret;
}

/** moves a TempoSection to a specified position.
 * @param ts - the section to be moved
 * @param frame - the new position in frames for the tempo
 * @param sub_num - the snap division to use if using musical time.
 *
 * if sub_num is non-zero, the frame position is used to calculate an exact
 * musical position.
 * sub_num   | effect
 * -1        | snap to bars (meter-based)
 *  0        | no snap - use audio frame for musical position
 *  1        | snap to meter-based (BBT) beat
 * >1        | snap to quarter-note subdivision (i.e. 4 will snap to sixteenth notes)
 *
 * this follows the snap convention in the gui.
 * if sub_num is zero, the musical position will be taken from the supplied frame.
 */
void
TempoMap::gui_set_tempo_position (TempoSection* ts, const framepos_t& frame, const int& sub_num)
{
	Metrics future_map;

	if (ts->position_lock_style() == MusicTime) {
		{
			/* if we're snapping to a musical grid, set the pulse exactly instead of via the supplied frame. */
			Glib::Threads::RWLock::WriterLock lm (lock);
			TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);

			tempo_copy->set_position_lock_style (AudioTime);

			if (solve_map_minute (future_map, tempo_copy, minute_at_frame (frame))) {
				const double beat = exact_beat_at_frame_locked (future_map, frame, sub_num);
				const double pulse = pulse_at_beat_locked (future_map, beat);

				if (solve_map_pulse (future_map, tempo_copy, pulse)) {
					solve_map_pulse (_metrics, ts, pulse);
					recompute_meters (_metrics);
				}
			}
		}

	} else {

		{
			Glib::Threads::RWLock::WriterLock lm (lock);
			TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);

			if (solve_map_minute (future_map, tempo_copy, minute_at_frame (frame))) {
				if (sub_num != 0) {
					/* We're moving the object that defines the grid while snapping to it...
					 * Placing the ts at the beat corresponding to the requested frame may shift the
					 * grid in such a way that the mouse is left hovering over a completerly different division,
					 * causing jittering when the mouse next moves (esp. large tempo deltas).
					 *
					 * This alters the snap behaviour slightly in that we snap to beat divisions
					 * in the future map rather than the existing one.
					 */
					const double qn = exact_qn_at_frame_locked (future_map, frame, sub_num);
					const framepos_t snapped_frame = frame_at_minute (minute_at_quarter_note_locked (future_map, qn));

					if (solve_map_minute (future_map, tempo_copy, minute_at_frame (snapped_frame))) {
						solve_map_minute (_metrics, ts, minute_at_frame (snapped_frame));
						ts->set_pulse (qn / 4.0);
						recompute_meters (_metrics);
					}
				} else {
					solve_map_minute (_metrics, ts, minute_at_frame (frame));
					recompute_meters (_metrics);
				}
			}
		}
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}

	MetricPositionChanged (); // Emit Signal
}

/** moves a MeterSection to a specified position.
 * @param ms - the section to be moved
 * @param frame - the new position in frames for the meter
 *
 * as a meter cannot snap to anything but bars,
 * the supplied frame is rounded to the nearest bar, possibly
 * leaving the meter position unchanged.
 */
void
TempoMap::gui_set_meter_position (MeterSection* ms, const framepos_t& frame)
{
	Metrics future_map;

	if (ms->position_lock_style() == AudioTime) {

		{
			Glib::Threads::RWLock::WriterLock lm (lock);
			MeterSection* copy = copy_metrics_and_point (_metrics, future_map, ms);

			if (solve_map_minute (future_map, copy, minute_at_frame (frame))) {
				solve_map_minute (_metrics, ms, minute_at_frame (frame));
				recompute_tempi (_metrics);
			}
		}
	} else {
		{
			Glib::Threads::RWLock::WriterLock lm (lock);
			MeterSection* copy = copy_metrics_and_point (_metrics, future_map, ms);

			const double beat = beat_at_minute_locked (_metrics, minute_at_frame (frame));
			const Timecode::BBT_Time bbt = bbt_at_beat_locked (_metrics, beat);

			if (solve_map_bbt (future_map, copy, bbt)) {
				solve_map_bbt (_metrics, ms, bbt);
				recompute_tempi (_metrics);
			}
		}
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}

	MetricPositionChanged (); // Emit Signal
}

bool
TempoMap::gui_change_tempo (TempoSection* ts, const Tempo& bpm)
{
	Metrics future_map;
	bool can_solve = false;
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);
		tempo_copy->set_note_types_per_minute (bpm.note_types_per_minute());
		recompute_tempi (future_map);

		if (check_solved (future_map)) {
			ts->set_note_types_per_minute (bpm.note_types_per_minute());
			recompute_map (_metrics);
			can_solve = true;
		}
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}
	if (can_solve) {
		MetricPositionChanged (); // Emit Signal
	}
	return can_solve;
}

void
TempoMap::gui_stretch_tempo (TempoSection* ts, const framepos_t& frame, const framepos_t& end_frame)
{
	/*
	  Ts (future prev_t)   Tnext
	  |                    |
	  |     [drag^]        |
	  |----------|----------
	        e_f  qn_beats(frame)
	*/

	Metrics future_map;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		if (!ts) {
			return;
		}

		TempoSection* prev_t = copy_metrics_and_point (_metrics, future_map, ts);
		TempoSection* prev_to_prev_t = 0;
		const frameoffset_t fr_off = end_frame - frame;

		assert (prev_t);

		if (prev_t->pulse() > 0.0) {
			prev_to_prev_t = const_cast<TempoSection*>(&tempo_section_at_minute_locked (future_map, minute_at_frame (prev_t->frame() - 1)));
		}

		TempoSection* next_t = 0;
		for (Metrics::iterator i = future_map.begin(); i != future_map.end(); ++i) {
			TempoSection* t = 0;
			if ((*i)->is_tempo()) {
				t = static_cast<TempoSection*> (*i);
				if (t->frame() > ts->frame()) {
					next_t = t;
					break;
				}
			}
		}
		/* minimum allowed measurement distance in frames */
		const framepos_t min_dframe = 2;

		/* the change in frames is the result of changing the slope of at most 2 previous tempo sections.
		   constant to constant is straightforward, as the tempo prev to prev_t has constant slope.
		*/
		double contribution = 0.0;

		if (next_t && prev_to_prev_t && prev_to_prev_t->type() == TempoSection::Ramp) {
			contribution = (prev_t->frame() - prev_to_prev_t->frame()) / (double) (next_t->frame() - prev_to_prev_t->frame());
		}

		const frameoffset_t prev_t_frame_contribution = fr_off - (contribution * (double) fr_off);

		const double start_pulse = prev_t->pulse_at_minute (minute_at_frame (frame));
		const double end_pulse = prev_t->pulse_at_minute (minute_at_frame (end_frame));

		double new_bpm;

		if (prev_t->type() == TempoSection::Constant || prev_t->c() == 0.0) {

			if (prev_t->position_lock_style() == MusicTime) {
				if (prev_to_prev_t && prev_to_prev_t->type() == TempoSection::Ramp) {
					if (frame > prev_to_prev_t->frame() + min_dframe && (frame + prev_t_frame_contribution) > prev_to_prev_t->frame() + min_dframe) {

						new_bpm = prev_t->note_types_per_minute() * ((frame - prev_to_prev_t->frame())
											/ (double) ((frame + prev_t_frame_contribution) - prev_to_prev_t->frame()));
					} else {
						new_bpm = prev_t->note_types_per_minute();
					}
				} else {
					/* prev to prev is irrelevant */

					if (start_pulse > prev_t->pulse() && end_pulse > prev_t->pulse()) {
						new_bpm = prev_t->note_types_per_minute() * ((start_pulse - prev_t->pulse()) / (end_pulse - prev_t->pulse()));
					} else {
						new_bpm = prev_t->note_types_per_minute();
					}
				}
			} else {
				/* AudioTime */
				if (prev_to_prev_t && prev_to_prev_t->type() == TempoSection::Ramp) {
					if (frame > prev_to_prev_t->frame() + min_dframe && end_frame > prev_to_prev_t->frame() + min_dframe) {

						new_bpm = prev_t->note_types_per_minute() * ((frame - prev_to_prev_t->frame())
											/ (double) ((end_frame) - prev_to_prev_t->frame()));
					} else {
						new_bpm = prev_t->note_types_per_minute();
					}
				} else {
					/* prev_to_prev_t is irrelevant */

					if (frame > prev_t->frame() + min_dframe && end_frame > prev_t->frame() + min_dframe) {
						new_bpm = prev_t->note_types_per_minute() * ((frame - prev_t->frame()) / (double) (end_frame - prev_t->frame()));
					} else {
						new_bpm = prev_t->note_types_per_minute();
					}
				}
			}
		} else {

			double frame_ratio = 1.0;
			double pulse_ratio = 1.0;
			const double pulse_pos = frame;

			if (prev_to_prev_t) {
				if (pulse_pos > prev_to_prev_t->frame() + min_dframe && (pulse_pos - fr_off) > prev_to_prev_t->frame() + min_dframe) {
					frame_ratio = (((pulse_pos - fr_off) - prev_to_prev_t->frame()) / (double) ((pulse_pos) - prev_to_prev_t->frame()));
				}
				if (end_pulse > prev_to_prev_t->pulse() && start_pulse > prev_to_prev_t->pulse()) {
					pulse_ratio = ((start_pulse - prev_to_prev_t->pulse()) / (end_pulse - prev_to_prev_t->pulse()));
				}
			} else {
				if (pulse_pos > prev_t->frame() + min_dframe && (pulse_pos - fr_off) > prev_t->frame() + min_dframe) {
					frame_ratio = (((pulse_pos - fr_off) - prev_t->frame()) / (double) ((pulse_pos) - prev_t->frame()));
				}
				pulse_ratio = (start_pulse / end_pulse);
			}
			new_bpm = prev_t->note_types_per_minute() * (pulse_ratio * frame_ratio);
		}

		/* don't clamp and proceed here.
		   testing has revealed that this can go negative,
		   which is an entirely different thing to just being too low.
		*/
		if (new_bpm < 0.5) {
			return;
		}
		new_bpm = min (new_bpm, (double) 1000.0);
		prev_t->set_note_types_per_minute (new_bpm);
		recompute_tempi (future_map);
		recompute_meters (future_map);

		if (check_solved (future_map)) {
			ts->set_note_types_per_minute (new_bpm);
			recompute_tempi (_metrics);
			recompute_meters (_metrics);
		}
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}

	MetricPositionChanged (); // Emit Signal
}

/** Returns the exact bbt-based beat corresponding to the bar, beat or quarter note subdivision nearest to
 * the supplied frame, possibly returning a negative value.
 *
 * @param frame  The session frame position.
 * @param sub_num The subdivision to use when rounding the beat.
 *                A value of -1 indicates rounding to BBT bar. 1 indicates rounding to BBT beats.
 *                Positive integers indicate quarter note (non BBT) divisions.
 *                0 indicates that the returned beat should not be rounded (equivalent to quarter_note_at_frame()).
 * @return The beat position of the supplied frame.
 *
 * when working to a musical grid, the use of sub_nom indicates that
 * the position should be interpreted musically.
 *
 * it effectively snaps to meter bars, meter beats or quarter note divisions
 * (as per current gui convention) and returns a musical position independent of frame rate.
 *
 * If the supplied frame lies before the first meter, the return will be negative,
 * in which case the returned beat uses the first meter (for BBT subdivisions) and
 * the continuation of the tempo curve (backwards).
 *
 * This function is sensitive to tempo and meter.
 */
double
TempoMap::exact_beat_at_frame (const framepos_t& frame, const int32_t sub_num) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return exact_beat_at_frame_locked (_metrics, frame, sub_num);
}

double
TempoMap::exact_beat_at_frame_locked (const Metrics& metrics, const framepos_t& frame, const int32_t divisions) const
{
	return beat_at_pulse_locked (_metrics, exact_qn_at_frame_locked (metrics, frame, divisions) / 4.0);
}

/** Returns the exact quarter note corresponding to the bar, beat or quarter note subdivision nearest to
 * the supplied frame, possibly returning a negative value.
 *
 * @param frame  The session frame position.
 * @param sub_num The subdivision to use when rounding the quarter note.
 *                A value of -1 indicates rounding to BBT bar. 1 indicates rounding to BBT beats.
 *                Positive integers indicate quarter note (non BBT) divisions.
 *                0 indicates that the returned quarter note should not be rounded (equivalent to quarter_note_at_frame()).
 * @return The quarter note position of the supplied frame.
 *
 * When working to a musical grid, the use of sub_nom indicates that
 * the frame position should be interpreted musically.
 *
 * it effectively snaps to meter bars, meter beats or quarter note divisions
 * (as per current gui convention) and returns a musical position independent of frame rate.
 *
 * If the supplied frame lies before the first meter, the return will be negative,
 * in which case the returned quarter note uses the first meter (for BBT subdivisions) and
 * the continuation of the tempo curve (backwards).
 *
 * This function is tempo-sensitive.
 */
double
TempoMap::exact_qn_at_frame (const framepos_t& frame, const int32_t sub_num) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return exact_qn_at_frame_locked (_metrics, frame, sub_num);
}

double
TempoMap::exact_qn_at_frame_locked (const Metrics& metrics, const framepos_t& frame, const int32_t sub_num) const
{
	double qn = quarter_note_at_minute_locked (metrics, minute_at_frame (frame));

	if (sub_num > 1) {
		qn = floor (qn) + (floor (((qn - floor (qn)) * (double) sub_num) + 0.5) / sub_num);
	} else if (sub_num == 1) {
		/* the gui requested exact musical (BBT) beat */
		qn = quarter_note_at_beat_locked (metrics, floor (beat_at_minute_locked (metrics, minute_at_frame (frame)) + 0.5));
	} else if (sub_num == -1) {
		/* snap to  bar */
		Timecode::BBT_Time bbt = bbt_at_pulse_locked (metrics, qn / 4.0);
		bbt.beats = 1;
		bbt.ticks = 0;

		const double prev_b = pulse_at_bbt_locked (metrics, bbt) * 4.0;
		++bbt.bars;
		const double next_b = pulse_at_bbt_locked (metrics, bbt) * 4.0;

		if ((qn - prev_b) > (next_b - prev_b) / 2.0) {
			qn = next_b;
		} else {
			qn = prev_b;
		}
	}

	return qn;
}

/** returns the frame duration of the supplied BBT time at a specified frame position in the tempo map.
 * @param pos the frame position in the tempo map.
 * @param bbt the distance in BBT time from pos to calculate.
 * @param dir the rounding direction..
 * @return the duration in frames between pos and bbt
*/
framecnt_t
TempoMap::bbt_duration_at (framepos_t pos, const BBT_Time& bbt, int dir)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	BBT_Time pos_bbt = bbt_at_minute_locked (_metrics, minute_at_frame (pos));

	const double divisions = meter_section_at_minute_locked (_metrics, minute_at_frame (pos)).divisions_per_bar();

	if (dir > 0) {
		pos_bbt.bars += bbt.bars;

		pos_bbt.ticks += bbt.ticks;
		if ((double) pos_bbt.ticks > BBT_Time::ticks_per_beat) {
			pos_bbt.beats += 1;
			pos_bbt.ticks -= BBT_Time::ticks_per_beat;
		}

		pos_bbt.beats += bbt.beats;
		if ((double) pos_bbt.beats > divisions) {
			pos_bbt.bars += 1;
			pos_bbt.beats -= divisions;
		}
		const framecnt_t pos_bbt_frame = frame_at_minute (minute_at_bbt_locked (_metrics, pos_bbt));

		return pos_bbt_frame - pos;

	} else {

		if (pos_bbt.bars <= bbt.bars) {
			pos_bbt.bars = 1;
		} else {
			pos_bbt.bars -= bbt.bars;
		}

		if (pos_bbt.ticks < bbt.ticks) {
			if (pos_bbt.bars > 1) {
				if (pos_bbt.beats == 1) {
					pos_bbt.bars--;
					pos_bbt.beats = divisions;
				} else {
					pos_bbt.beats--;
				}
				pos_bbt.ticks = BBT_Time::ticks_per_beat - (bbt.ticks - pos_bbt.ticks);
			} else {
				pos_bbt.beats = 1;
				pos_bbt.ticks = 0;
			}
		} else {
			pos_bbt.ticks -= bbt.ticks;
		}

		if (pos_bbt.beats <= bbt.beats) {
			if (pos_bbt.bars > 1) {
				pos_bbt.bars--;
				pos_bbt.beats = divisions - (bbt.beats - pos_bbt.beats);
			} else {
				pos_bbt.beats = 1;
			}
		} else {
			pos_bbt.beats -= bbt.beats;
		}

		return pos - frame_at_minute (minute_at_bbt_locked (_metrics, pos_bbt));
	}

	return 0;
}

framepos_t
TempoMap::round_to_bar (framepos_t fr, RoundMode dir)
{
	return round_to_type (fr, dir, Bar);
}

framepos_t
TempoMap::round_to_beat (framepos_t fr, RoundMode dir)
{
	return round_to_type (fr, dir, Beat);
}

framepos_t
TempoMap::round_to_beat_subdivision (framepos_t fr, int sub_num, RoundMode dir)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	uint32_t ticks = (uint32_t) floor (max (0.0, beat_at_minute_locked (_metrics, minute_at_frame (fr))) * BBT_Time::ticks_per_beat);
	uint32_t beats = (uint32_t) floor (ticks / BBT_Time::ticks_per_beat);
	uint32_t ticks_one_subdivisions_worth = (uint32_t) BBT_Time::ticks_per_beat / sub_num;

	ticks -= beats * BBT_Time::ticks_per_beat;

	if (dir > 0) {
		/* round to next (or same iff dir == RoundUpMaybe) */

		uint32_t mod = ticks % ticks_one_subdivisions_worth;

		if (mod == 0 && dir == RoundUpMaybe) {
			/* right on the subdivision, which is fine, so do nothing */

		} else if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			ticks += ticks_one_subdivisions_worth;

		} else {
			/* not on subdivision, compute distance to next subdivision */

			ticks += ticks_one_subdivisions_worth - mod;
		}

		if (ticks >= BBT_Time::ticks_per_beat) {
			ticks -= BBT_Time::ticks_per_beat;
		}
	} else if (dir < 0) {

		/* round to previous (or same iff dir == RoundDownMaybe) */

		uint32_t difference = ticks % ticks_one_subdivisions_worth;

		if (difference == 0 && dir == RoundDownAlways) {
			/* right on the subdivision, but force-rounding down,
			   so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;
		}

		if (ticks < difference) {
			ticks = BBT_Time::ticks_per_beat - ticks;
		} else {
			ticks -= difference;
		}

	} else {
		/* round to nearest */
		double rem;

		/* compute the distance to the previous and next subdivision */

		if ((rem = fmod ((double) ticks, (double) ticks_one_subdivisions_worth)) > ticks_one_subdivisions_worth/2.0) {

			/* closer to the next subdivision, so shift forward */

			ticks = lrint (ticks + (ticks_one_subdivisions_worth - rem));

			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved forward to %1\n", ticks));

			if (ticks > BBT_Time::ticks_per_beat) {
				++beats;
				ticks -= BBT_Time::ticks_per_beat;
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", beats));
			}

		} else if (rem > 0) {

			/* closer to previous subdivision, so shift backward */

			if (rem > ticks) {
				if (beats == 0) {
					/* can't go backwards past zero, so ... */
					return 0;
				}
				/* step back to previous beat */
				--beats;
				ticks = lrint (BBT_Time::ticks_per_beat - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", beats));
			} else {
				ticks = lrint (ticks - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}

	const framepos_t ret_frame = frame_at_minute (minute_at_beat_locked (_metrics, beats + (ticks / BBT_Time::ticks_per_beat)));

	return ret_frame;
}

framepos_t
TempoMap::round_to_quarter_note_subdivision (framepos_t fr, int sub_num, RoundMode dir)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	uint32_t ticks = (uint32_t) floor (max (0.0, quarter_note_at_minute_locked (_metrics, minute_at_frame (fr))) * BBT_Time::ticks_per_beat);
	uint32_t beats = (uint32_t) floor (ticks / BBT_Time::ticks_per_beat);
	uint32_t ticks_one_subdivisions_worth = (uint32_t) BBT_Time::ticks_per_beat / sub_num;

	ticks -= beats * BBT_Time::ticks_per_beat;

	if (dir > 0) {
		/* round to next (or same iff dir == RoundUpMaybe) */

		uint32_t mod = ticks % ticks_one_subdivisions_worth;

		if (mod == 0 && dir == RoundUpMaybe) {
			/* right on the subdivision, which is fine, so do nothing */

		} else if (mod == 0) {
			/* right on the subdivision, so the difference is just the subdivision ticks */
			ticks += ticks_one_subdivisions_worth;

		} else {
			/* not on subdivision, compute distance to next subdivision */

			ticks += ticks_one_subdivisions_worth - mod;
		}

//NOTE:  this code intentionally limits the rounding so we don't advance to the next beat.
//  For the purposes of "jump-to-next-subdivision", we DO want to advance to the next beat.
//	And since the "prev" direction DOES move beats, I assume this code is unintended.
//  But I'm keeping it around, until we determine there are no terrible consequences.
//		if (ticks >= BBT_Time::ticks_per_beat) {
//			ticks -= BBT_Time::ticks_per_beat;
//		}

	} else if (dir < 0) {

		/* round to previous (or same iff dir == RoundDownMaybe) */

		uint32_t difference = ticks % ticks_one_subdivisions_worth;

		if (difference == 0 && dir == RoundDownAlways) {
			/* right on the subdivision, but force-rounding down,
			   so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;
		}

		if (ticks < difference) {
			ticks = BBT_Time::ticks_per_beat - ticks;
		} else {
			ticks -= difference;
		}

	} else {
		/* round to nearest */
		double rem;

		/* compute the distance to the previous and next subdivision */

		if ((rem = fmod ((double) ticks, (double) ticks_one_subdivisions_worth)) > ticks_one_subdivisions_worth/2.0) {

			/* closer to the next subdivision, so shift forward */

			ticks = lrint (ticks + (ticks_one_subdivisions_worth - rem));

			DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved forward to %1\n", ticks));

			if (ticks > BBT_Time::ticks_per_beat) {
				++beats;
				ticks -= BBT_Time::ticks_per_beat;
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", beats));
			}

		} else if (rem > 0) {

			/* closer to previous subdivision, so shift backward */

			if (rem > ticks) {
				if (beats == 0) {
					/* can't go backwards past zero, so ... */
					return 0;
				}
				/* step back to previous beat */
				--beats;
				ticks = lrint (BBT_Time::ticks_per_beat - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", beats));
			} else {
				ticks = lrint (ticks - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}

	const framepos_t ret_frame = frame_at_minute (minute_at_quarter_note_locked (_metrics, beats + (ticks / BBT_Time::ticks_per_beat)));

	return ret_frame;
}

framepos_t
TempoMap::round_to_type (framepos_t frame, RoundMode dir, BBTPointType type)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const double beat_at_framepos = max (0.0, beat_at_minute_locked (_metrics, minute_at_frame (frame)));
	BBT_Time bbt (bbt_at_beat_locked (_metrics, beat_at_framepos));

	switch (type) {
	case Bar:
		if (dir < 0) {
			/* find bar previous to 'frame' */
			if (bbt.bars > 0)
				--bbt.bars;
			bbt.beats = 1;
			bbt.ticks = 0;
			return frame_at_minute (minute_at_bbt_locked (_metrics, bbt));

		} else if (dir > 0) {
			/* find bar following 'frame' */
			++bbt.bars;
			bbt.beats = 1;
			bbt.ticks = 0;
			return frame_at_minute (minute_at_bbt_locked (_metrics, bbt));
		} else {
			/* true rounding: find nearest bar */
			framepos_t raw_ft = frame_at_minute (minute_at_bbt_locked (_metrics, bbt));
			bbt.beats = 1;
			bbt.ticks = 0;
			framepos_t prev_ft = frame_at_minute (minute_at_bbt_locked (_metrics, bbt));
			++bbt.bars;
			framepos_t next_ft = frame_at_minute (minute_at_bbt_locked (_metrics, bbt));

			if ((raw_ft - prev_ft) > (next_ft - prev_ft) / 2) {
				return next_ft;
			} else {
				return prev_ft;
			}
		}

		break;

	case Beat:
		if (dir < 0) {
			return frame_at_minute (minute_at_beat_locked (_metrics, floor (beat_at_framepos)));
		} else if (dir > 0) {
			return frame_at_minute (minute_at_beat_locked (_metrics, ceil (beat_at_framepos)));
		} else {
			return frame_at_minute (minute_at_beat_locked (_metrics, floor (beat_at_framepos + 0.5)));
		}
		break;
	}

	return 0;
}

void
TempoMap::get_grid (vector<TempoMap::BBTPoint>& points,
		    framepos_t lower, framepos_t upper, uint32_t bar_mod)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	int32_t cnt = ceil (beat_at_minute_locked (_metrics, minute_at_frame (lower)));
	framecnt_t pos = 0;
	/* although the map handles negative beats, bbt doesn't. */
	if (cnt < 0.0) {
		cnt = 0.0;
	}

	if (minute_at_beat_locked (_metrics, cnt) >= minute_at_frame (upper)) {
		return;
	}
	if (bar_mod == 0) {
		while (pos >= 0 && pos < upper) {
			pos = frame_at_minute (minute_at_beat_locked (_metrics, cnt));
			const TempoSection tempo = tempo_section_at_minute_locked (_metrics, minute_at_frame (pos));
			const MeterSection meter = meter_section_at_minute_locked (_metrics, minute_at_frame (pos));
			const BBT_Time bbt = bbt_at_beat_locked (_metrics, cnt);

			points.push_back (BBTPoint (meter, tempo_at_minute_locked (_metrics, minute_at_frame (pos)), pos, bbt.bars, bbt.beats, tempo.c()));
			++cnt;
		}
	} else {
		BBT_Time bbt = bbt_at_minute_locked (_metrics, minute_at_frame (lower));
		bbt.beats = 1;
		bbt.ticks = 0;

		if (bar_mod != 1) {
			bbt.bars -= bbt.bars % bar_mod;
			++bbt.bars;
		}

		while (pos >= 0 && pos < upper) {
			pos = frame_at_minute (minute_at_bbt_locked (_metrics, bbt));
			const TempoSection tempo = tempo_section_at_minute_locked (_metrics, minute_at_frame (pos));
			const MeterSection meter = meter_section_at_minute_locked (_metrics, minute_at_frame (pos));
			points.push_back (BBTPoint (meter, tempo_at_minute_locked (_metrics, minute_at_frame (pos)), pos, bbt.bars, bbt.beats, tempo.c()));
			bbt.bars += bar_mod;
		}
	}
}

const TempoSection&
TempoMap::tempo_section_at_frame (framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_section_at_minute_locked (_metrics, minute_at_frame (frame));
}

TempoSection&
TempoMap::tempo_section_at_frame (framepos_t frame)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_section_at_minute_locked (_metrics, minute_at_frame (frame));
}

const TempoSection&
TempoMap::tempo_section_at_minute_locked (const Metrics& metrics, double minute) const
{
	TempoSection* prev = 0;

	TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev && t->minute() > minute) {
				break;
			}

			prev = t;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return *prev;
}
TempoSection&
TempoMap::tempo_section_at_minute_locked (const Metrics& metrics, double minute)
{
	TempoSection* prev = 0;

	TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev && t->minute() > minute) {
				break;
			}

			prev = t;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return *prev;
}
const TempoSection&
TempoMap::tempo_section_at_beat_locked (const Metrics& metrics, const double& beat) const
{
	TempoSection* prev_t = 0;
	const MeterSection* prev_m = &meter_section_at_beat_locked (metrics, beat);

	TempoSection* t;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (prev_t && ((t->pulse() - prev_m->pulse()) * prev_m->note_divisor()) + prev_m->beat() > beat) {
				break;
			}
			prev_t = t;
		}

	}
	return *prev_t;
}

/* don't use this to calculate length (the tempo is only correct for this frame).
   do that stuff based on the beat_at_frame and frame_at_beat api
*/
double
TempoMap::frames_per_quarter_note_at (const framepos_t& frame, const framecnt_t& sr) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	const TempoSection* ts_at = 0;
	const TempoSection* ts_after = 0;
	Metrics::const_iterator i;
	TempoSection* t;

	for (i = _metrics.begin(); i != _metrics.end(); ++i) {

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (ts_at && (*i)->frame() > frame) {
				ts_after = t;
				break;
			}
			ts_at = t;
		}
	}
	assert (ts_at);

	if (ts_after) {
		return  (60.0 * _frame_rate) / ts_at->tempo_at_minute (minute_at_frame (frame)).quarter_notes_per_minute();
	}
	/* must be treated as constant tempo */
	return ts_at->frames_per_quarter_note (_frame_rate);
}

const MeterSection&
TempoMap::meter_section_at_frame (framepos_t frame) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return meter_section_at_minute_locked (_metrics, minute_at_frame (frame));
}

const MeterSection&
TempoMap::meter_section_at_minute_locked (const Metrics& metrics, double minute) const
{
	Metrics::const_iterator i;
	MeterSection* prev = 0;

	MeterSection* m;

	for (i = metrics.begin(); i != metrics.end(); ++i) {

		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);

			if (prev && (*i)->minute() > minute) {
				break;
			}

			prev = m;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return *prev;
}

const MeterSection&
TempoMap::meter_section_at_beat_locked (const Metrics& metrics, const double& beat) const
{
	MeterSection* prev_m = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		MeterSection* m;
		if (!(*i)->is_tempo()) {
			m = static_cast<MeterSection*> (*i);
			if (prev_m && m->beat() > beat) {
				break;
			}
			prev_m = m;
		}

	}
	return *prev_m;
}

const MeterSection&
TempoMap::meter_section_at_beat (double beat) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return meter_section_at_beat_locked (_metrics, beat);
}

const Meter&
TempoMap::meter_at_frame (framepos_t frame) const
{
	TempoMetric m (metric_at (frame));
	return m.meter();
}

void
TempoMap::fix_legacy_session ()
{
	MeterSection* prev_m = 0;
	TempoSection* prev_t = 0;

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		MeterSection* m;
		TempoSection* t;

		if ((m = dynamic_cast<MeterSection*>(*i)) != 0) {
			if (m->initial()) {
				pair<double, BBT_Time> bbt = make_pair (0.0, BBT_Time (1, 1, 0));
				m->set_beat (bbt);
				m->set_pulse (0.0);
				m->set_minute (0.0);
				m->set_position_lock_style (AudioTime);
				prev_m = m;
				continue;
			}
			if (prev_m) {
				pair<double, BBT_Time> start = make_pair (((m->bbt().bars - 1) * prev_m->note_divisor())
									  + (m->bbt().beats - 1)
									  + (m->bbt().ticks / BBT_Time::ticks_per_beat)
									  , m->bbt());
				m->set_beat (start);
				const double start_beat = ((m->bbt().bars - 1) * prev_m->note_divisor())
					+ (m->bbt().beats - 1)
					+ (m->bbt().ticks / BBT_Time::ticks_per_beat);
				m->set_pulse (start_beat / prev_m->note_divisor());
			}
			prev_m = m;
		} else if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {

			if (!t->active()) {
				continue;
			}

			if (t->initial()) {
				t->set_pulse (0.0);
				t->set_minute (0.0);
				t->set_position_lock_style (AudioTime);
				prev_t = t;
				continue;
			}

			if (prev_t) {
				const double beat = ((t->legacy_bbt().bars - 1) * ((prev_m) ? prev_m->note_divisor() : 4.0))
					+ (t->legacy_bbt().beats - 1)
					+ (t->legacy_bbt().ticks / BBT_Time::ticks_per_beat);
				if (prev_m) {
					t->set_pulse (beat / prev_m->note_divisor());
				} else {
					/* really shouldn't happen but.. */
					t->set_pulse (beat / 4.0);
				}
			}
			prev_t = t;
		}
	}
}

XMLNode&
TempoMap::get_state ()
{
	Metrics::const_iterator i;
	XMLNode *root = new XMLNode ("TempoMap");

	{
		Glib::Threads::RWLock::ReaderLock lm (lock);
		for (i = _metrics.begin(); i != _metrics.end(); ++i) {
			root->add_child_nocopy ((*i)->get_state());
		}
	}

	return *root;
}

int
TempoMap::set_state (const XMLNode& node, int /*version*/)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		XMLNodeList nlist;
		XMLNodeConstIterator niter;
		Metrics old_metrics (_metrics);
		_metrics.clear();

		nlist = node.children();

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			XMLNode* child = *niter;

			if (child->name() == TempoSection::xml_state_node_name) {

				try {
					TempoSection* ts = new TempoSection (*child, _frame_rate);
					_metrics.push_back (ts);
				}

				catch (failed_constructor& err){
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					_metrics = old_metrics;
					old_metrics.clear();
					break;
				}

			} else if (child->name() == MeterSection::xml_state_node_name) {

				try {
					MeterSection* ms = new MeterSection (*child, _frame_rate);
					_metrics.push_back (ms);
				}

				catch (failed_constructor& err) {
					error << _("Tempo map: could not set new state, restoring old one.") << endmsg;
					_metrics = old_metrics;
					old_metrics.clear();
					break;
				}
			}
		}

		if (niter == nlist.end()) {
			MetricSectionSorter cmp;
			_metrics.sort (cmp);
		}

		/* check for legacy sessions where bbt was the base musical unit for tempo */
		for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
			TempoSection* t;
			if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
				if (t->legacy_bbt().bars != 0) {
					fix_legacy_session();
					break;
				}
				break;
			}
		}

		/* check for multiple tempo/meters at the same location, which
		   ardour2 somehow allowed.
		*/

		Metrics::iterator prev = _metrics.end();
		for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
			if (prev != _metrics.end()) {
				MeterSection* ms;
				MeterSection* prev_m;
				TempoSection* ts;
				TempoSection* prev_t;
				if ((prev_m = dynamic_cast<MeterSection*>(*prev)) != 0 && (ms = dynamic_cast<MeterSection*>(*i)) != 0) {
					if (prev_m->pulse() == ms->pulse()) {
						cerr << string_compose (_("Multiple meter definitions found at %1"), prev_m->pulse()) << endmsg;
						error << string_compose (_("Multiple meter definitions found at %1"), prev_m->pulse()) << endmsg;
						return -1;
					}
				} else if ((prev_t = dynamic_cast<TempoSection*>(*prev)) != 0 && (ts = dynamic_cast<TempoSection*>(*i)) != 0) {
					if (prev_t->pulse() == ts->pulse()) {
						cerr << string_compose (_("Multiple tempo definitions found at %1"), prev_t->pulse()) << endmsg;
						error << string_compose (_("Multiple tempo definitions found at %1"), prev_t->pulse()) << endmsg;
						return -1;
					}
				}
			}
			prev = i;
		}

		recompute_map (_metrics);

		Metrics::const_iterator d = old_metrics.begin();
		while (d != old_metrics.end()) {
			delete (*d);
			++d;
		}
		old_metrics.clear ();
	}

	PropertyChanged (PropertyChange ());

	return 0;
}

void
TempoMap::dump (std::ostream& o) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);
	const MeterSection* m;
	const TempoSection* t;
	const TempoSection* prev_t = 0;

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {

		if ((t = dynamic_cast<const TempoSection*>(*i)) != 0) {
			o << "Tempo @ " << *i << t->note_types_per_minute() << " BPM (pulse = 1/" << t->note_type()
			  << " type= " << enum_2_string (t->type()) << ") "  << " at pulse= " << t->pulse()
			  << " minute= " << t->minute() << " frame= " << t->frame() << " (initial? " << t->initial() << ')'
			  << " pos lock: " << enum_2_string (t->position_lock_style()) << std::endl;
			if (prev_t) {
				o <<  "  current      : " << t->note_types_per_minute()
				  << " | " << t->pulse() << " | " << t->frame() << " | " << t->minute() << std::endl;
				o << "  previous     : " << prev_t->note_types_per_minute()
				  << " | " << prev_t->pulse() << " | " << prev_t->frame() << " | " << prev_t->minute() << std::endl;
				o << "  calculated   : " << prev_t->tempo_at_pulse (t->pulse())
				  << " | " << prev_t->pulse_at_ntpm (t->note_types_per_minute(), t->minute())
				  << " | " << frame_at_minute (prev_t->minute_at_ntpm (t->note_types_per_minute(), t->pulse()))
				  << " | " << prev_t->minute_at_ntpm (t->note_types_per_minute(), t->pulse()) << std::endl;
			}
			prev_t = t;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			o << "Meter @ " << *i << ' ' << m->divisions_per_bar() << '/' << m->note_divisor() << " at " << m->bbt()
			  << " frame= " << m->frame() << " pulse: " << m->pulse() <<  " beat : " << m->beat()
			  << " pos lock: " << enum_2_string (m->position_lock_style()) << " (initial? " << m->initial() << ')' << endl;
		}
	}
	o << "------" << std::endl;
}

int
TempoMap::n_tempos() const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	int cnt = 0;

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			cnt++;
		}
	}

	return cnt;
}

int
TempoMap::n_meters() const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	int cnt = 0;

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if (!(*i)->is_tempo()) {
			cnt++;
		}
	}

	return cnt;
}

void
TempoMap::insert_time (framepos_t where, framecnt_t amount)
{
	for (Metrics::reverse_iterator i = _metrics.rbegin(); i != _metrics.rend(); ++i) {
		if ((*i)->frame() >= where && !(*i)->initial ()) {
			MeterSection* ms;
			TempoSection* ts;

			if ((ms = dynamic_cast <MeterSection*>(*i)) != 0) {
				gui_set_meter_position (ms, (*i)->frame() + amount);
			}

			if ((ts = dynamic_cast <TempoSection*>(*i)) != 0) {
				gui_set_tempo_position (ts, (*i)->frame() + amount, 0);
			}
		}
	}

	PropertyChanged (PropertyChange ());
}

bool
TempoMap::remove_time (framepos_t where, framecnt_t amount)
{
	bool moved = false;

	std::list<MetricSection*> metric_kill_list;

	TempoSection* last_tempo = NULL;
	MeterSection* last_meter = NULL;
	bool tempo_after = false; // is there a tempo marker at the first sample after the removed range?
	bool meter_after = false; // is there a meter marker likewise?
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
			if ((*i)->frame() >= where && (*i)->frame() < where+amount) {
				metric_kill_list.push_back(*i);
				TempoSection *lt = dynamic_cast<TempoSection*> (*i);
				if (lt)
					last_tempo = lt;
				MeterSection *lm = dynamic_cast<MeterSection*> (*i);
				if (lm)
					last_meter = lm;
			}
			else if ((*i)->frame() >= where) {
				// TODO: make sure that moved tempo/meter markers are rounded to beat/bar boundaries
				(*i)->set_minute ((*i)->minute() - minute_at_frame (amount));
				if ((*i)->frame() == where) {
					// marker was immediately after end of range
					tempo_after = dynamic_cast<TempoSection*> (*i);
					meter_after = dynamic_cast<MeterSection*> (*i);
				}
				moved = true;
			}
		}

		//find the last TEMPO and METER metric (if any) and move it to the cut point so future stuff is correct
		if (last_tempo && !tempo_after) {
			metric_kill_list.remove(last_tempo);
			last_tempo->set_minute (minute_at_frame (where));
			moved = true;
		}
		if (last_meter && !meter_after) {
			metric_kill_list.remove(last_meter);
			last_meter->set_minute (minute_at_frame (where));
			moved = true;
		}

		//remove all the remaining metrics
		for (std::list<MetricSection*>::iterator i = metric_kill_list.begin(); i != metric_kill_list.end(); ++i) {
			_metrics.remove(*i);
			moved = true;
		}

		if (moved) {
			recompute_map (_metrics);
		}
	}
	PropertyChanged (PropertyChange ());
	return moved;
}

/** Add some (fractional) Beats to a session frame position, and return the result in frames.
 *  pos can be -ve, if required.
 */
framepos_t
TempoMap::framepos_plus_qn (framepos_t frame, Evoral::Beats beats) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	const double frame_qn = quarter_note_at_minute_locked (_metrics, minute_at_frame (frame));

	return frame_at_minute (minute_at_quarter_note_locked (_metrics, frame_qn + beats.to_double()));
}

framepos_t
TempoMap::framepos_plus_bbt (framepos_t pos, BBT_Time op) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	BBT_Time pos_bbt = bbt_at_beat_locked (_metrics, beat_at_minute_locked (_metrics, minute_at_frame (pos)));
	pos_bbt.ticks += op.ticks;
	if (pos_bbt.ticks >= BBT_Time::ticks_per_beat) {
		++pos_bbt.beats;
		pos_bbt.ticks -= BBT_Time::ticks_per_beat;
	}
	pos_bbt.beats += op.beats;
	/* the meter in effect will start on the bar */
	double divisions_per_bar = meter_section_at_beat (beat_at_bbt_locked (_metrics, BBT_Time (pos_bbt.bars + op.bars, 1, 0))).divisions_per_bar();
	while (pos_bbt.beats >= divisions_per_bar + 1) {
		++pos_bbt.bars;
		divisions_per_bar = meter_section_at_beat (beat_at_bbt_locked (_metrics, BBT_Time (pos_bbt.bars + op.bars, 1, 0))).divisions_per_bar();
		pos_bbt.beats -= divisions_per_bar;
	}
	pos_bbt.bars += op.bars;

	return frame_at_minute (minute_at_bbt_locked (_metrics, pos_bbt));
}

/** Count the number of beats that are equivalent to distance when going forward,
    starting at pos.
*/
Evoral::Beats
TempoMap::framewalk_to_qn (framepos_t pos, framecnt_t distance) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return Evoral::Beats (quarter_notes_between_frames_locked (_metrics, pos, pos + distance));
}

struct bbtcmp {
    bool operator() (const BBT_Time& a, const BBT_Time& b) {
	    return a < b;
    }
};

std::ostream&
operator<< (std::ostream& o, const Meter& m) {
	return o << m.divisions_per_bar() << '/' << m.note_divisor();
}

std::ostream&
operator<< (std::ostream& o, const Tempo& t) {
	return o << t.note_types_per_minute() << " 1/" << t.note_type() << "'s per minute";
}

std::ostream&
operator<< (std::ostream& o, const MetricSection& section) {

	o << "MetricSection @ " << section.frame() << ' ';

	const TempoSection* ts;
	const MeterSection* ms;

	if ((ts = dynamic_cast<const TempoSection*> (&section)) != 0) {
		o << *((const Tempo*) ts);
	} else if ((ms = dynamic_cast<const MeterSection*> (&section)) != 0) {
		o << *((const Meter*) ms);
	}

	return o;
}
