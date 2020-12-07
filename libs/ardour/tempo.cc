/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <algorithm>
#include <stdexcept>
#include <cmath>

#include <unistd.h>

#include <glibmm/threads.h>

#include "pbd/enumwriter.h"
#include "pbd/xml++.h"

#include "temporal/beats.h"

#include "ardour/debug.h"
#include "ardour/lmath.h"
#include "ardour/tempo.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

using Temporal::BBT_Time;

/* _default tempo is 4/4 qtr=120 */

Meter    TempoMap::_default_meter (4.0, 4.0);
Tempo    TempoMap::_default_tempo (120.0, 4.0, 120.0);

samplepos_t
MetricSection::sample_at_minute (const double& time) const
{
	return (samplepos_t) floor ((time * 60.0 * _sample_rate) + 0.5);
}

double
MetricSection::minute_at_sample (const samplepos_t sample) const
{
	return (sample / (double) _sample_rate) / 60.0;
}

/***********************************************************************/

bool
ARDOUR::bbt_time_to_string (const BBT_Time& bbt, std::string& str)
{
	char buf[256];
	int retval = snprintf (buf, sizeof(buf), "%" PRIu32 "|%" PRIu32 "|%" PRIu32, bbt.bars, bbt.beats,
	                       bbt.ticks);

	if (retval <= 0 || retval >= (int)sizeof(buf)) {
		return false;
	}

	str = buf;
	return true;
}

bool
ARDOUR::string_to_bbt_time (const std::string& str, BBT_Time& bbt)
{
	if (sscanf (str.c_str (), "%" PRIu32 "|%" PRIu32 "|%" PRIu32, &bbt.bars, &bbt.beats,
	            &bbt.ticks) == 3) {
		return true;
	}
	return false;
}


/***********************************************************************/

double
Meter::samples_per_grid (const Tempo& tempo, samplecnt_t sr) const
{
	/* This is tempo- and meter-sensitive. The number it returns
	   is based on the interval between any two lines in the
	   grid that is constructed from tempo and meter sections.

	   The return value IS NOT interpretable in terms of "beats".
	*/

	return (60.0 * sr) / (tempo.note_types_per_minute() * (_note_type / tempo.note_type()));
}

double
Meter::samples_per_bar (const Tempo& tempo, samplecnt_t sr) const
{
	return samples_per_grid (tempo, sr) * _divisions_per_bar;
}

/***********************************************************************/

void
MetricSection::add_state_to_node(XMLNode& node) const
{
	node.set_property ("pulse", _pulse);
	node.set_property ("frame", sample());
	node.set_property ("movable", !_initial);
	node.set_property ("lock-style", _position_lock_style);
}

int
MetricSection::set_state (const XMLNode& node, int /*version*/)
{
	node.get_property ("pulse", _pulse);

	samplepos_t sample;
	if (node.get_property ("frame", sample)) {
		set_minute (minute_at_sample (sample));
	}

	bool tmp;
	if (!node.get_property ("movable", tmp)) {
		error << _("TempoSection XML node has no \"movable\" property") << endmsg;
		throw failed_constructor();
	}
	_initial = !tmp;

	if (!node.get_property ("lock-style", _position_lock_style)) {
		if (!initial()) {
			_position_lock_style = MusicTime;
		} else {
			_position_lock_style = AudioTime;
		}
	}
	return 0;
}

/***********************************************************************/

const string TempoSection::xml_state_node_name = "Tempo";

TempoSection::TempoSection (const XMLNode& node, samplecnt_t sample_rate)
	: MetricSection (0.0, 0, MusicTime, true, sample_rate)
	, Tempo (TempoMap::default_tempo())
	, _c (0.0)
	, _active (true)
	, _locked_to_meter (false)
	, _clamped (false)
{
	BBT_Time bbt;
	std::string start_bbt;
	_legacy_bbt.bars = 0; // legacy session check compars .bars != 0; default BBT_Time c'tor uses 1.
	if (node.get_property ("start", start_bbt)) {
		if (string_to_bbt_time (start_bbt, bbt)) {
			/* legacy session - start used to be in bbt*/
			_legacy_bbt = bbt;
			set_pulse(-1.0);
			info << _("Legacy session detected. TempoSection XML node will be altered.") << endmsg;
		}
	}

	// Don't worry about return value, exception will be thrown on error
	MetricSection::set_state (node, Stateful::loading_state_version);

	if (node.get_property ("beats-per-minute", _note_types_per_minute)) {
		if (_note_types_per_minute < 0.0) {
			error << _("TempoSection XML node has an illegal \"beats_per_minute\" value") << endmsg;
			throw failed_constructor();
		}
	}

	if (node.get_property ("note-type", _note_type)) {
		if (_note_type < 1.0) {
			error << _("TempoSection XML node has an illegal \"note-type\" value") << endmsg;
			throw failed_constructor();
		}
	} else {
		/* older session, make note type be quarter by default */
		_note_type = 4.0;
	}

	if (!node.get_property ("clamped", _clamped)) {
		_clamped = false;
	}

	if (node.get_property ("end-beats-per-minute", _end_note_types_per_minute)) {
		if (_end_note_types_per_minute < 0.0) {
			info << _("TempoSection XML node has an illegal \"end-beats-per-minute\" value") << endmsg;
			throw failed_constructor();
		}
	}

	TempoSection::Type old_type;
	if (node.get_property ("tempo-type", old_type)) {
		/* sessions with a tempo-type node contain no end-beats-per-minute.
		   if the legacy node indicates a constant tempo, simply fill this in with the
		   start tempo. otherwise we need the next neighbour to know what it will be.
		*/

		if (old_type == TempoSection::Constant) {
			_end_note_types_per_minute = _note_types_per_minute;
		} else {
			_end_note_types_per_minute = -1.0;
		}
	}

	if (!node.get_property ("active", _active)) {
		warning << _("TempoSection XML node has no \"active\" property") << endmsg;
		_active = true;
	}

	if (!node.get_property ("locked-to-meter", _locked_to_meter)) {
		if (initial()) {
			set_locked_to_meter (true);
		} else {
			set_locked_to_meter (false);
		}
	}

	/* 5.5 marked initial tempo as not locked to meter. this should always be true anyway */
	if (initial()) {
		set_locked_to_meter (true);
	}
}

XMLNode&
TempoSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);

	MetricSection::add_state_to_node (*root);

	root->set_property ("beats-per-minute", _note_types_per_minute);
	root->set_property ("note-type", _note_type);
	root->set_property ("clamped", _clamped);
	root->set_property ("end-beats-per-minute", _end_note_types_per_minute);
	root->set_property ("active", _active);
	root->set_property ("locked-to-meter", _locked_to_meter);

	return *root;
}

/** returns the Tempo at the session-relative minute.
*/
Tempo
TempoSection::tempo_at_minute (const double& m) const
{
	const bool constant = type() == Constant || _c == 0.0 || (initial() && m < minute());
	if (constant) {
		return Tempo (note_types_per_minute(), note_type());
	}

	return Tempo (_tempo_at_time (m - minute()), _note_type, _end_note_types_per_minute);
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
	const bool constant = type() == Constant || _c == 0.0 || (initial() && p < pulse());
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
	const bool constant = type() == Constant || _c == 0.0 || (initial() && p < pulse());

	if (constant) {
		return Tempo (note_types_per_minute(), note_type());
	}

	return Tempo (_tempo_at_pulse (p - pulse()), _note_type, _end_note_types_per_minute);
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
	const bool constant = type() == Constant || _c == 0.0 || (initial() && m < minute());
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
	const bool constant = type() == Constant || _c == 0.0 || (initial() && m < minute());
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
	const bool constant = type() == Constant || _c == 0.0 || (initial() && p < pulse());
	if (constant) {
		return ((p - pulse()) / pulses_per_minute()) + minute();
	}

	return _time_at_pulse (p - pulse()) + minute();
}

/** returns thw whole-note pulse at session sample position f.
 *  @param f the sample position.
 *  @return the position in whole-note pulses corresponding to f
 *
 *  for use with musical units whose granularity is coarser than samples (e.g. ticks)
*/
double
TempoSection::pulse_at_sample (const samplepos_t f) const
{
	const bool constant = type() == Constant || _c == 0.0 || (initial() && f < sample());
	if (constant) {
		return (minute_at_sample (f - sample()) * pulses_per_minute()) + pulse();
	}

	return _pulse_at_time (minute_at_sample (f - sample())) + pulse();
}

samplepos_t
TempoSection::sample_at_pulse (const double& p) const
{
	const bool constant = type() == Constant || _c == 0.0 || (initial() && p < pulse());
	if (constant) {
		return sample_at_minute (((p - pulse()) / pulses_per_minute()) + minute());
	}

	return sample_at_minute (_time_at_pulse (p - pulse()) + minute());
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
TempoSection::compute_c_pulse (const double& end_npm, const double& end_pulse) const
{
	if (note_types_per_minute() == end_npm || type() == Constant) {
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
TempoSection::compute_c_minute (const double& end_npm, const double& end_minute) const
{
	if (note_types_per_minute() == end_npm || type() == Constant) {
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

MeterSection::MeterSection (const XMLNode& node, const samplecnt_t sample_rate)
	: MetricSection (0.0, 0, MusicTime, false, sample_rate), Meter (TempoMap::default_meter())
{
	pair<double, BBT_Time> start;
	start.first = 0.0;

	std::string bbt_str;
	if (node.get_property ("start", bbt_str)) {
		if (string_to_bbt_time (bbt_str, start.second)) {
			/* legacy session - start used to be in bbt*/
			info << _("Legacy session detected - MeterSection XML node will be altered.") << endmsg;
			set_pulse (-1.0);
		} else {
			error << _("MeterSection XML node has an illegal \"start\" value") << endmsg;
		}
	}

	MetricSection::set_state (node, Stateful::loading_state_version);

	node.get_property ("beat", start.first);

	if (node.get_property ("bbt", bbt_str)) {
		if (!string_to_bbt_time (bbt_str, start.second)) {
			error << _("MeterSection XML node has an illegal \"bbt\" value") << endmsg;
			throw failed_constructor();
		}
	} else {
		warning << _("MeterSection XML node has no \"bbt\" property") << endmsg;
	}

	set_beat (start);

	/* beats-per-bar is old; divisions-per-bar is new */

	if (!node.get_property ("divisions-per-bar", _divisions_per_bar)) {
		if (!node.get_property ("beats-per-bar", _divisions_per_bar)) {
			error << _("MeterSection XML node has no \"beats-per-bar\" or \"divisions-per-bar\" property") << endmsg;
			throw failed_constructor();
		}
	}

	if (_divisions_per_bar < 0.0) {
		error << _("MeterSection XML node has an illegal \"divisions-per-bar\" value") << endmsg;
		throw failed_constructor();
	}

	if (!node.get_property ("note-type", _note_type)) {
		error << _("MeterSection XML node has no \"note-type\" property") << endmsg;
		throw failed_constructor();
	}

	if (_note_type < 0.0) {
		error << _("MeterSection XML node has an illegal \"note-type\" value") << endmsg;
		throw failed_constructor();
	}
}

XMLNode&
MeterSection::get_state() const
{
	XMLNode *root = new XMLNode (xml_state_node_name);

	MetricSection::add_state_to_node (*root);

	std::string bbt_str;
	bbt_time_to_string (_bbt, bbt_str);
	root->set_property ("bbt", bbt_str);
	root->set_property ("beat", beat());
	root->set_property ("note-type", _note_type);
	root->set_property ("divisions-per-bar", _divisions_per_bar);

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
  Solved means ordered by sample or pulse with sample-accurate precision (see check_solved()).

  Music and Audio

  Music and audio-locked objects may seem interchangeable on the surface, but when translating
  between audio samples and beat, remember that a sample is only a quantised approximation
  of the actual time (in minutes) of a beat.
  Thus if a gui user points to the sample occupying the start of a music-locked object on 1|3|0, it does not
  mean that this sample is the actual location in time of 1|3|0.

  You cannot use a sample measurement to determine beat distance except under special circumstances
  (e.g. where the user has requested that a beat lie on a SMPTE sample or if the tempo is known to be constant over the duration).

  This means is that a user operating on a musical grid must supply the desired beat position and/or current beat quantization in order for the
  sample space the user is operating at to be translated correctly to the object.

  The current approach is to interpret the supplied sample using the grid division the user has currently selected.
  If the user has no musical grid set, they are actually operating in sample space (even SMPTE samples are rounded to audio sample), so
  the supplied audio sample is interpreted as the desired musical location (beat_at_sample()).

  tldr: Beat, being a function of time, has nothing to do with sample rate, but time quantization can get in the way of precision.

  When sample_at_beat() is called, the position calculation is performed in pulses and minutes.
  The result is rounded to audio samples.
  When beat_at_sample() is called, the sample is converted to minutes, with no rounding performed on the result.

  So :
  sample_at_beat (beat_at_sample (sample)) == sample
  but :
  beat_at_sample (sample_at_beat (beat)) != beat due to the time quantization of sample_at_beat().

  Doing the second one will result in a beat distance error of up to 0.5 audio samples.
  samples_between_quarter_notes () eliminats this effect when determining time duration
  from Beats distance, or instead work in quarter-notes and/or beats and convert to samples last.

  The above pointless example could instead do:
  beat_at_quarter_note (quarters_at (beat)) to avoid rounding.

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
		return a->sample() < b->sample();
	}
};

TempoMap::TempoMap (samplecnt_t fr)
{
	_sample_rate = fr;
	BBT_Time start (1, 1, 0);

	TempoSection *t = new TempoSection (0.0, 0.0, _default_tempo, AudioTime, fr);
	MeterSection *m = new MeterSection (0.0, 0.0, 0.0, start, _default_meter.divisions_per_bar(), _default_meter.note_divisor(), AudioTime, fr);

	t->set_initial (true);
	t->set_locked_to_meter (true);

	m->set_initial (true);

	/* note: sample time is correct (zero) for both of these */

	_metrics.push_back (t);
	_metrics.push_back (m);

}

TempoMap&
TempoMap::operator= (TempoMap const & other)
{
	if (&other != this) {
		Glib::Threads::RWLock::ReaderLock lr (other.lock);
		Glib::Threads::RWLock::WriterLock lm (lock);
		_sample_rate = other._sample_rate;

		Metrics::const_iterator d = _metrics.begin();
		while (d != _metrics.end()) {
			delete (*d);
			++d;
		}
		_metrics.clear();

		for (Metrics::const_iterator m = other._metrics.begin(); m != other._metrics.end(); ++m) {
			TempoSection const * const ts = dynamic_cast<TempoSection const * const> (*m);
			MeterSection const * const ms = dynamic_cast<MeterSection const * const> (*m);

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

samplepos_t
TempoMap::sample_at_minute (const double time) const
{
	return (samplepos_t) floor ((time * 60.0 * _sample_rate) + 0.5);
}

double
TempoMap::minute_at_sample (const samplepos_t sample) const
{
	return (sample / (double) _sample_rate) / 60.0;
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
			if (tempo.sample() == (*i)->sample()) {
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
				if (t->locked_to_meter() && meter.sample() == (*i)->sample()) {
					delete (*i);
					_metrics.erase (i);
					break;
				}
			}
		}
	}

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		if (dynamic_cast<MeterSection*> (*i) != 0) {
			if (meter.sample() == (*i)->sample()) {
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
			if ((ipm && tempo->pulse() == insert_tempo->pulse()) || (!ipm && tempo->sample() == insert_tempo->sample())) {

				if (tempo->initial()) {

					/* can't (re)move this section, so overwrite
					 * its data content (but not its properties as
					 * a section).
					 */

					*(dynamic_cast<Tempo*>(*i)) = *(dynamic_cast<Tempo*>(insert_tempo));
					(*i)->set_position_lock_style (AudioTime);
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

			if ((ipm && meter->beat() == insert_meter->beat()) || (!ipm && meter->sample() == insert_meter->sample())) {

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
					if ((ipm && meter->beat() > insert_meter->beat()) || (!ipm && meter->sample() > insert_meter->sample())) {
						break;
					}
				} else {
					if (prev_t && prev_t->locked_to_meter() && (!ipm && prev_t->sample() == insert_meter->sample())) {
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
					if ((ipm && tempo->pulse() > insert_tempo->pulse()) || (!ipm && tempo->sample() > insert_tempo->sample())
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
TempoMap::add_tempo (const Tempo& tempo, const double& pulse, const samplepos_t sample, PositionLockStyle pls)
{
	if (tempo.note_types_per_minute() <= 0.0) {
		warning << "Cannot add tempo. note types per minute must be greater than zero." << endmsg;
		return 0;
	}

	TempoSection* ts = 0;
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		/* here we default to not clamped for a new tempo section. preference? */
		ts = add_tempo_locked (tempo, pulse, minute_at_sample (sample), pls, true, false, false);

		recompute_map (_metrics);
	}

	PropertyChanged (PropertyChange ());

	return ts;
}

void
TempoMap::replace_tempo (TempoSection& ts, const Tempo& tempo, const double& pulse, const samplepos_t sample, PositionLockStyle pls)
{
	if (tempo.note_types_per_minute() <= 0.0) {
		warning << "Cannot replace tempo. note types per minute must be greater than zero." << endmsg;
		return;
	}

	bool const locked_to_meter = ts.locked_to_meter();
	bool const ts_clamped = ts.clamped();
	TempoSection* new_ts = 0;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection& first (first_tempo());
		if (!ts.initial()) {
			if (locked_to_meter) {
				{
					/* cannot move a meter-locked tempo section */
					*static_cast<Tempo*>(&ts) = tempo;
					recompute_map (_metrics);
				}
			} else {
				remove_tempo_locked (ts);
				new_ts = add_tempo_locked (tempo, pulse, minute_at_sample (sample), pls, true, locked_to_meter, ts_clamped);
				/* enforce clampedness of next tempo section */
				TempoSection* next_t = next_tempo_section_locked (_metrics, new_ts);
				if (next_t && next_t->clamped()) {
					next_t->set_note_types_per_minute (new_ts->end_note_types_per_minute());
				}
			}

		} else {
			first.set_pulse (0.0);
			first.set_minute (minute_at_sample (sample));
			first.set_position_lock_style (AudioTime);
			first.set_locked_to_meter (true);
			first.set_clamped (ts_clamped);
			{
				/* cannot move the first tempo section */
				*static_cast<Tempo*>(&first) = tempo;
			}
		}
		recompute_map (_metrics);
	}

	PropertyChanged (PropertyChange ());
}

TempoSection*
TempoMap::add_tempo_locked (const Tempo& tempo, double pulse, double minute
			    , PositionLockStyle pls, bool recompute, bool locked_to_meter, bool clamped)
{
	TempoSection* t = new TempoSection (pulse, minute, tempo, pls, _sample_rate);
	t->set_locked_to_meter (locked_to_meter);
	t->set_clamped (clamped);

	do_insert (t);

	TempoSection* prev_tempo = 0;
	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		TempoSection* const this_t = dynamic_cast<TempoSection*>(*i);
		if (this_t) {
			if (this_t == t) {
				if (prev_tempo && prev_tempo->type() == TempoSection::Ramp) {
					prev_tempo->set_end_note_types_per_minute (t->note_types_per_minute());
				}
				break;
			}
			prev_tempo = this_t;
		}
	}

	if (recompute) {
		if (pls == AudioTime) {
			solve_map_minute (_metrics, t, t->minute());
		} else {
			solve_map_pulse (_metrics, t, t->pulse());
		}
		recompute_meters (_metrics);
	}

	return t;
}

MeterSection*
TempoMap::add_meter (const Meter& meter, const Temporal::BBT_Time& where, samplepos_t sample, PositionLockStyle pls)
{
	MeterSection* m = 0;
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		m = add_meter_locked (meter, where, sample, pls, true);
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
TempoMap::replace_meter (const MeterSection& ms, const Meter& meter, const BBT_Time& where, samplepos_t sample, PositionLockStyle pls)
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		if (!ms.initial()) {
			remove_meter_locked (ms);
			add_meter_locked (meter, where, sample, pls, true);
		} else {
			MeterSection& first (first_meter());
			TempoSection& first_t (first_tempo());
			/* cannot move the first meter section */
			*static_cast<Meter*>(&first) = meter;
			first.set_position_lock_style (AudioTime);
			first.set_pulse (0.0);
			first.set_minute (minute_at_sample (sample));
			pair<double, BBT_Time> beat = make_pair (0.0, BBT_Time (1, 1, 0));
			first.set_beat (beat);
			first_t.set_minute (first.minute());
			first_t.set_locked_to_meter (true);
			first_t.set_pulse (0.0);
			first_t.set_position_lock_style (AudioTime);
			recompute_map (_metrics);
		}
	}

	PropertyChanged (PropertyChange ());
}

MeterSection*
TempoMap::add_meter_locked (const Meter& meter, const BBT_Time& bbt, samplepos_t sample, PositionLockStyle pls, bool recompute)
{
	double const minute_at_bbt = minute_at_bbt_locked (_metrics, bbt);
	const MeterSection& prev_m = meter_section_at_minute_locked  (_metrics, minute_at_bbt - minute_at_sample (1));
	double const pulse = ((bbt.bars - prev_m.bbt().bars) * (prev_m.divisions_per_bar() / prev_m.note_divisor())) + prev_m.pulse();
	/* the natural time of the BBT position */
	double const time_minutes = minute_at_pulse_locked (_metrics, pulse);

	if (pls == AudioTime) {
		/* add meter-locked tempo at the natural time in the current map (sample may differ). */
		Tempo const tempo_at_time = tempo_at_minute_locked (_metrics, time_minutes);
		TempoSection* mlt = add_tempo_locked (tempo_at_time, pulse, time_minutes, AudioTime, true, true, false);

		if (!mlt) {
			return 0;
		}
	}
	/* still using natural time for the position, ignoring lock style. */
	MeterSection* new_meter = new MeterSection (pulse, time_minutes, beat_at_bbt_locked (_metrics, bbt), bbt, meter.divisions_per_bar(), meter.note_divisor(), pls, _sample_rate);

	bool solved = false;

	do_insert (new_meter);

	if (recompute) {

		if (pls == AudioTime) {
			/* now set the audio locked meter's position to sample */
			solved = solve_map_minute (_metrics, new_meter, minute_at_sample (sample));
			/* we failed, most likely due to some impossible sample requirement wrt audio-locked tempi.
			   fudge sample so that the meter ends up at its BBT position instead.
			*/
			if (!solved) {
				solved = solve_map_minute (_metrics, new_meter, minute_at_sample (prev_m.sample() + 1));
			}
		} else {
			solved = solve_map_bbt (_metrics, new_meter, bbt);
			/* required due to resetting the pulse of meter-locked tempi above.
			   Arguably  solve_map_bbt() should use solve_map_pulse (_metrics, TempoSection) instead,
			   but afaict this cannot cause the map to be left unsolved (these tempi are all audio locked).
			*/
			recompute_map (_metrics);
		}
	}

	if (!solved && recompute) {
		/* if this has failed to solve, there is little we can do other than to ensure that
		 * the new map is valid and recalculated.
		 */
		remove_meter_locked (*new_meter);
		warning << "Adding meter may have left the tempo map unsolved." << endmsg;
		recompute_map (_metrics);
	}

	return new_meter;
}

void
TempoMap::change_initial_tempo (double note_types_per_minute, double note_type, double end_note_types_per_minute)
{
	Tempo newtempo (note_types_per_minute, note_type, end_note_types_per_minute);
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
TempoMap::change_existing_tempo_at (samplepos_t where, double note_types_per_minute, double note_type, double end_ntpm)
{
	Tempo newtempo (note_types_per_minute, note_type, end_ntpm);

	TempoSection* prev;
	TempoSection* first;
	Metrics::iterator i;

	/* find the TempoSection immediately preceding "where"
	 */

	for (first = 0, i = _metrics.begin(), prev = 0; i != _metrics.end(); ++i) {

		if ((*i)->sample() > where) {
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
					prev_t->set_c (prev_t->compute_c_minute (prev_t->end_note_types_per_minute(), t->minute()));
					if (!t->locked_to_meter()) {
						t->set_pulse (prev_t->pulse_at_ntpm (prev_t->end_note_types_per_minute(), t->minute()));
					}

				} else {
					prev_t->set_c (prev_t->compute_c_pulse (prev_t->end_note_types_per_minute(), t->pulse()));
					t->set_minute (prev_t->minute_at_ntpm (prev_t->end_note_types_per_minute(), t->pulse()));

				}
			}
			prev_t = t;
		}
	}
	assert (prev_t);
	prev_t->set_c (0.0);
}

/* tempos must be positioned correctly.
 * the current approach is to use a meter's bbt time as its base position unit.
 * an audio-locked meter requires a recomputation of pulse and beat (but not bbt),
 * while a music-locked meter requires recomputations of sample pulse and beat (but not bbt)
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
						if (t->locked_to_meter() && t->sample() == meter->sample()) {
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
TempoMap::recompute_map (Metrics& metrics, samplepos_t end)
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
TempoMap::metric_at (samplepos_t sample, Metrics::const_iterator* last) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	TempoMetric m (first_meter(), first_tempo());

	if (last) {
		*last = ++_metrics.begin();
	}

	/* at this point, we are *guaranteed* to have m.meter and m.tempo pointing
	   at something, because we insert the default tempo and meter during
	   TempoMap construction.

	   now see if we can find better candidates.
	*/

	for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {

		if ((*i)->sample() > sample) {
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

/** Returns the BBT (meter-based) beat corresponding to the supplied sample, possibly returning a negative value.
 * @param sample The session sample position.
 * @return The beat duration according to the tempo map at the supplied sample.
 *
 * If the supplied sample lies before the first meter, the returned beat duration will be negative.
 * The returned beat is obtained using the first meter and the continuation of the tempo curve (backwards).
 *
 * This function uses both tempo and meter.
 */
double
TempoMap::beat_at_sample (const samplecnt_t sample) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return beat_at_minute_locked (_metrics, minute_at_sample (sample));
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

	assert (prev_m);

	const double beat = prev_m->beat() + (ts.pulse_at_minute (minute) - prev_m->pulse()) * prev_m->note_divisor();

	/* audio locked meters fake their beat */
	if (next_m && next_m->beat() < beat) {
		return next_m->beat();
	}

	return beat;
}

/** Returns the sample corresponding to the supplied BBT (meter-based) beat.
 * @param beat The BBT (meter-based) beat.
 * @return The sample duration according to the tempo map at the supplied BBT (meter-based) beat.
 *
 * This function uses both tempo and meter.
 */
samplepos_t
TempoMap::sample_at_beat (const double& beat) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return sample_at_minute (minute_at_beat_locked (_metrics, beat));
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

			if (!t->active()) {
				continue;
			}

			if (prev_t && ((t->pulse() - prev_m->pulse()) * prev_m->note_divisor()) + prev_m->beat() > beat) {
				break;
			}
			prev_t = t;
		}

	}
	assert (prev_t);

	return prev_t->minute_at_pulse (((beat - prev_m->beat()) / prev_m->note_divisor()) + prev_m->pulse());
}

/** Returns a Tempo corresponding to the supplied sample position.
 * @param sample The audio sample.
 * @return a Tempo according to the tempo map at the supplied sample.
 *
 */
Tempo
TempoMap::tempo_at_sample (const samplepos_t sample) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_at_minute_locked (_metrics, minute_at_sample (sample));
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
				/* t is the section past sample */
				return prev_t->tempo_at_minute (minute);
			}
			prev_t = t;
		}
	}

	assert (prev_t);
	return Tempo (prev_t->note_types_per_minute(), prev_t->note_type(), prev_t->end_note_types_per_minute());
}

/** returns the sample at which the supplied tempo occurs, or
 *  the sample of the last tempo section (search exhausted)
 *  only the position of the first occurence will be returned
 *  (extend me)
*/
samplepos_t
TempoMap::sample_at_tempo (const Tempo& tempo) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return sample_at_minute (minute_at_tempo_locked (_metrics, tempo));
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



			if (t->note_types_per_minute() == tempo_bpm) {
				return t->minute();
			}

			if (prev_t) {
				const double prev_t_bpm = prev_t->note_types_per_minute();
				const double prev_t_end_bpm = prev_t->end_note_types_per_minute();
				if ((prev_t_bpm > tempo_bpm && prev_t_end_bpm < tempo_bpm)
				    || (prev_t_bpm < tempo_bpm && prev_t_end_bpm > tempo_bpm)
				    || (prev_t_end_bpm == tempo_bpm)) {

					return prev_t->minute_at_ntpm (tempo_bpm, t->pulse());
				}
			}
			prev_t = t;
		}
	}

	assert (prev_t);
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
				/* t is the section past sample */
				return prev_t->tempo_at_pulse (pulse);
			}
			prev_t = t;
		}
	}

	assert (prev_t);
	return Tempo (prev_t->note_types_per_minute(), prev_t->note_type(), prev_t->end_note_types_per_minute());
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

	assert (prev_t);
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

	return pulse_at_tempo_locked (_metrics, tempo) * 4.0;
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
				/*the previous ts is the one containing the sample */
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

	assert (prev_t);

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

	assert (prev_t);

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
TempoMap::beat_at_bbt (const Temporal::BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return beat_at_bbt_locked (_metrics, bbt);
}


double
TempoMap::beat_at_bbt_locked (const Metrics& metrics, const Temporal::BBT_Time& bbt) const
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

	assert (prev_m);

	const double remaining_bars = bbt.bars - prev_m->bbt().bars;
	const double remaining_bars_in_beats = remaining_bars * prev_m->divisions_per_bar();
	const double ret = remaining_bars_in_beats + prev_m->beat() + (bbt.beats - 1) + (bbt.ticks / Temporal::ticks_per_beat);

	return ret;
}

/** Returns the BBT time corresponding to the supplied BBT (meter-based) beat.
 * @param beat The BBT (meter-based) beat.
 * @return The BBT time (meter-based) at the supplied meter-based beat.
 *
 */
Temporal::BBT_Time
TempoMap::bbt_at_beat (const double& beat)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return bbt_at_beat_locked (_metrics, beat);
}

Temporal::BBT_Time
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
	const double remaining_ticks = (remaining_beats - floor (remaining_beats)) * Temporal::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 - based mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= Temporal::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= Temporal::ticks_per_beat;
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
TempoMap::quarter_note_at_bbt (const Temporal::BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return pulse_at_bbt_locked (_metrics, bbt) * 4.0;
}

double
TempoMap::quarter_note_at_bbt_rt (const Temporal::BBT_Time& bbt)
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::quarter_note_at_bbt_rt() could not lock tempo map");
	}

	return pulse_at_bbt_locked (_metrics, bbt) * 4.0;
}

double
TempoMap::pulse_at_bbt_locked (const Metrics& metrics, const Temporal::BBT_Time& bbt) const
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

	assert (prev_m);

	const double remaining_bars = bbt.bars - prev_m->bbt().bars;
	const double remaining_pulses = remaining_bars * prev_m->divisions_per_bar() / prev_m->note_divisor();
	const double ret = remaining_pulses + prev_m->pulse() + (((bbt.beats - 1) + (bbt.ticks / Temporal::ticks_per_beat)) / prev_m->note_divisor());

	return ret;
}

/** Returns the BBT time corresponding to the supplied quarter-note beat.
 * @param qn the quarter-note beat.
 * @return The BBT time (meter-based) at the supplied meter-based beat.
 *
 * quarter-notes ignore meter and are based on pulse (the musical unit of MetricSection).
 *
 */
Temporal::BBT_Time
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
Temporal::BBT_Time
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
	const double remaining_ticks = (remaining_beats - floor (remaining_beats)) * Temporal::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= Temporal::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= Temporal::ticks_per_beat;
	}

	if (ret.beats >= prev_m->divisions_per_bar() + 1) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

/** Returns the BBT time corresponding to the supplied sample position.
 * @param sample the position in audio samples.
 * @return the BBT time at the sample position .
 *
 */
BBT_Time
TempoMap::bbt_at_sample (samplepos_t sample) const
{
	if (sample < 0) {
		BBT_Time bbt;
		bbt.bars = 1;
		bbt.beats = 1;
		bbt.ticks = 0;
#ifndef NDEBUG
		warning << string_compose (_("tempo map was asked for BBT time at sample %1\n"), sample) << endmsg;
#endif
		return bbt;
	}

	const double minute =  minute_at_sample (sample);

	Glib::Threads::RWLock::ReaderLock lm (lock);

	return bbt_at_minute_locked (_metrics, minute);
}

BBT_Time
TempoMap::bbt_at_sample_rt (samplepos_t sample) const
{
	const double minute =  minute_at_sample (sample);

	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::bbt_at_sample_rt() could not lock tempo map");
	}

	return bbt_at_minute_locked (_metrics, minute);
}

Temporal::BBT_Time
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

	assert (prev_m);

	double beat = prev_m->beat() + (ts.pulse_at_minute (minute) - prev_m->pulse()) * prev_m->note_divisor();

	/* handle sample before first meter */
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
	const double remaining_ticks = (remaining_beats - floor (remaining_beats)) * Temporal::ticks_per_beat;

	BBT_Time ret;

	ret.ticks = (uint32_t) floor (remaining_ticks + 0.5);
	ret.beats = (uint32_t) floor (remaining_beats);
	ret.bars = total_bars;

	/* 0 0 0 to 1 1 0 - based mapping*/
	++ret.bars;
	++ret.beats;

	if (ret.ticks >= Temporal::ticks_per_beat) {
		++ret.beats;
		ret.ticks -= Temporal::ticks_per_beat;
	}

	if (ret.beats >= prev_m->divisions_per_bar() + 1) {
		++ret.bars;
		ret.beats = 1;
	}

	return ret;
}

/** Returns the sample position corresponding to the supplied BBT time.
 * @param bbt the position in BBT time.
 * @return the sample position at bbt.
 *
 */
samplepos_t
TempoMap::sample_at_bbt (const BBT_Time& bbt)
{
	if (bbt.bars < 1) {
#ifndef NDEBUG
		warning << string_compose (_("tempo map asked for sample time at bar < 1  (%1)\n"), bbt) << endmsg;
#endif
		return 0;
	}

	if (bbt.beats < 1) {
		throw std::logic_error ("beats are counted from one");
	}

	double minute;
	{
		Glib::Threads::RWLock::ReaderLock lm (lock);
		minute = minute_at_bbt_locked (_metrics, bbt);
	}

	return sample_at_minute (minute);
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
 * Returns the quarter-note beat position corresponding to the supplied sample.
 *
 * @param sample the position in samples.
 * @return The quarter-note position of the supplied sample. Ignores meter.
 *
*/
double
TempoMap::quarter_note_at_sample (const samplepos_t sample) const
{
	const double minute =  minute_at_sample (sample);

	Glib::Threads::RWLock::ReaderLock lm (lock);

	return pulse_at_minute_locked (_metrics, minute) * 4.0;
}

double
TempoMap::quarter_note_at_sample_rt (const samplepos_t sample) const
{
	const double minute =  minute_at_sample (sample);

	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		throw std::logic_error ("TempoMap::quarter_note_at_sample_rt() could not lock tempo map");
	}

	return pulse_at_minute_locked (_metrics, minute) * 4.0;
}

/**
 * Returns the sample position corresponding to the supplied quarter-note beat.
 *
 * @param quarter_note the quarter-note position.
 * @return the sample position of the supplied quarter-note. Ignores meter.
 *
 *
*/
samplepos_t
TempoMap::sample_at_quarter_note (const double quarter_note) const
{
	double minute;
	{
		Glib::Threads::RWLock::ReaderLock lm (lock);

		minute = minute_at_pulse_locked (_metrics, quarter_note / 4.0);
	}

	return sample_at_minute (minute);
}

/** Returns the quarter-note beats corresponding to the supplied BBT (meter-based) beat.
 * @param beat The BBT (meter-based) beat.
 * @return The quarter-note position of the supplied BBT (meter-based) beat.
 *
 * a quarter-note may be compared with and assigned to Temporal::Beats.
 *
 */
double
TempoMap::quarter_note_at_beat (const double beat) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return pulse_at_beat_locked (_metrics, beat) * 4.0;
}

/** Returns the BBT (meter-based) beat position corresponding to the supplied quarter-note beats.
 * @param quarter_note The position in quarter-note beats.
 * @return the BBT (meter-based) beat position of the supplied quarter-note beats.
 *
 * a quarter-note is the musical unit of Temporal::Beats.
 *
 */
double
TempoMap::beat_at_quarter_note (const double quarter_note) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return beat_at_pulse_locked (_metrics, quarter_note / 4.0);
}

/** Returns the duration in samples between two supplied quarter-note beat positions.
 * @param start the first position in quarter-note beats.
 * @param end the end position in quarter-note beats.
 * @return the sample distance ober the quarter-note beats duration.
 *
 * use this rather than e.g.
 * sample_at-quarter_note (end_beats) - sample_at_quarter_note (start_beats).
 * samples_between_quarter_notes() doesn't round to audio samples as an intermediate step,
 *
 */
samplecnt_t
TempoMap::samples_between_quarter_notes (const double start, const double end) const
{
	double minutes;

	{
		Glib::Threads::RWLock::ReaderLock lm (lock);
		minutes = minutes_between_quarter_notes_locked (_metrics, start, end);
	}

	return sample_at_minute (minutes);
}

double
TempoMap::minutes_between_quarter_notes_locked (const Metrics& metrics, const double start, const double end) const
{

	return minute_at_pulse_locked (metrics, end / 4.0) - minute_at_pulse_locked (metrics, start / 4.0);
}

double
TempoMap::quarter_notes_between_samples (const samplecnt_t start, const samplecnt_t end) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return quarter_notes_between_samples_locked (_metrics, start, end);
}

double
TempoMap::quarter_notes_between_samples_locked (const Metrics& metrics, const samplecnt_t start, const samplecnt_t end) const
{
	const TempoSection* prev_t = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t && t->sample() > start) {
				break;
			}
			prev_t = t;
		}
	}
	assert (prev_t);
	const double start_qn = prev_t->pulse_at_sample (start);

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;

		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (!t->active()) {
				continue;
			}
			if (prev_t && t->sample() > end) {
				break;
			}
			prev_t = t;
		}
	}
	const double end_qn = prev_t->pulse_at_sample (end);

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

				/* precision check ensures tempo and samples align.*/
				if (t->sample() != sample_at_minute (prev_t->minute_at_ntpm (prev_t->end_note_types_per_minute(), t->pulse()))) {
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
				const TempoSection* t = &tempo_section_at_minute_locked (metrics, minute_at_sample (m->sample() - 1));
				const samplepos_t nascent_m_sample = sample_at_minute (t->minute_at_pulse (m->pulse()));
				/* Here we check that a preceding section of music doesn't overlap a subsequent one.
				*/
				if (t && (nascent_m_sample > m->sample() || nascent_m_sample < 0)) {
					return false;
				}
			}

			prev_m = m;
		}

	}

	return true;
}

bool
TempoMap::set_active_tempi (const Metrics& metrics, const samplepos_t sample)
{
	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		TempoSection* t;
		if ((*i)->is_tempo()) {
			t = static_cast<TempoSection*> (*i);
			if (t->locked_to_meter()) {
				t->set_active (true);
			} else if (t->position_lock_style() == AudioTime) {
				if (t->sample() < sample) {
					t->set_active (false);
					t->set_pulse (-1.0);
				} else if (t->sample() > sample) {
					t->set_active (true);
				} else if (t->sample() == sample) {
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

				if (t->sample() == sample_at_minute (minute)) {
					return false;
				}

				const bool tlm = t->position_lock_style() == MusicTime;

				if (prev_t && !section_prev && ((sml && tlm && t->pulse() > section->pulse()) || (!tlm && t->minute() > minute))) {
					section_prev = prev_t;

					section_prev->set_c (section_prev->compute_c_minute (section_prev->end_note_types_per_minute(), minute));
					if (!section->locked_to_meter()) {
						section->set_pulse (section_prev->pulse_at_ntpm (section_prev->end_note_types_per_minute(), minute));
					}
					prev_t = section;
				}

				if (t->position_lock_style() == MusicTime) {
					prev_t->set_c (prev_t->compute_c_pulse (prev_t->end_note_types_per_minute(), t->pulse()));
					t->set_minute (prev_t->minute_at_ntpm (prev_t->end_note_types_per_minute(), t->pulse()));
				} else {
					prev_t->set_c (prev_t->compute_c_minute (prev_t->end_note_types_per_minute(), t->minute()));
					if (!t->locked_to_meter()) {
						t->set_pulse (prev_t->pulse_at_ntpm (prev_t->end_note_types_per_minute(), t->minute()));
					}
				}
			}
			prev_t = t;
		}
	}

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
					prev_t->set_c (prev_t->compute_c_pulse (prev_t->end_note_types_per_minute(), t->pulse()));
					t->set_minute (prev_t->minute_at_ntpm (prev_t->end_note_types_per_minute(), t->pulse()));
				} else {
					prev_t->set_c (prev_t->compute_c_minute (prev_t->end_note_types_per_minute(), t->minute()));
					if (!t->locked_to_meter()) {
						t->set_pulse (prev_t->pulse_at_ntpm (prev_t->end_note_types_per_minute(), t->minute()));
					}
				}
			}
			prev_t = t;
		}
	}

	if (section_prev) {
		section_prev->set_c (section_prev->compute_c_pulse (section_prev->end_note_types_per_minute(), pulse));
		section->set_minute (section_prev->minute_at_ntpm (section_prev->end_note_types_per_minute(), pulse));
	}

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
		if (!set_active_tempi (imaginary, sample_at_minute (minute))) {
			return false;
		}
	}

	TempoSection* meter_locked_tempo = 0;

	for (Metrics::const_iterator ii = imaginary.begin(); ii != imaginary.end(); ++ii) {
		TempoSection* t;
		if ((*ii)->is_tempo()) {
			t = static_cast<TempoSection*> (*ii);
			if (t->locked_to_meter() && t->sample() == section->sample()) {
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
						if (t->locked_to_meter() && t->sample() == m->sample()) {
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
		assert (prev_m);

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
TempoMap::copy_metrics_and_point (const Metrics& metrics, Metrics& copy, TempoSection* section) const
{
	TempoSection* ret = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			TempoSection const * const t = dynamic_cast<TempoSection const * const> (*i);
			if (t == section) {
				ret = new TempoSection (*t);
				copy.push_back (ret);
				continue;
			}

			TempoSection* cp = new TempoSection (*t);
			copy.push_back (cp);
		} else {
			MeterSection const * const m = dynamic_cast<MeterSection const * const> (*i);
			MeterSection* cp = new MeterSection (*m);
			copy.push_back (cp);
		}
	}

	return ret;
}

MeterSection*
TempoMap::copy_metrics_and_point (const Metrics& metrics, Metrics& copy, MeterSection* section) const
{
	MeterSection* ret = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		if ((*i)->is_tempo()) {
			TempoSection const * const t = dynamic_cast<TempoSection const * const> (*i);
			TempoSection* cp = new TempoSection (*t);
			copy.push_back (cp);
		} else {
			MeterSection const * const m = dynamic_cast<MeterSection const * const> (*i);
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
* This is for a gui that needs to know the pulse or sample of a tempo section if it were to be moved to some bbt time,
* taking any possible reordering as a consequence of this into account.
* @param section - the section to be altered
* @param bbt - the BBT time  where the altered tempo will fall
* @return returns - the position in pulses and samples (as a pair) where the new tempo section will lie.
*/
pair<double, samplepos_t>
TempoMap::predict_tempo_position (TempoSection* section, const BBT_Time& bbt)
{
	Metrics future_map;
	pair<double, samplepos_t> ret = make_pair (0.0, 0);

	Glib::Threads::RWLock::ReaderLock lm (lock);

	TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, section);

	const double beat = beat_at_bbt_locked (future_map, bbt);

	if (section->position_lock_style() == AudioTime) {
		tempo_copy->set_position_lock_style (MusicTime);
	}

	if (solve_map_pulse (future_map, tempo_copy, pulse_at_beat_locked (future_map, beat))) {
		ret.first = tempo_copy->pulse();
		ret.second = tempo_copy->sample();
	} else {
		ret.first = section->pulse();
		ret.second = section->sample();
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
 * @param sample - the new position in samples for the tempo
 * @param sub_num - the snap division to use if using musical time.
 *
 * if sub_num is non-zero, the sample position is used to calculate an exact
 * musical position.
 * sub_num   | effect
 * -1        | snap to bars (meter-based)
 *  0        | no snap - use audio sample for musical position
 *  1        | snap to meter-based (BBT) beat
 * >1        | snap to quarter-note subdivision (i.e. 4 will snap to sixteenth notes)
 *
 * this follows the snap convention in the gui.
 * if sub_num is zero, the musical position will be taken from the supplied sample.
 */
void
TempoMap::gui_set_tempo_position (TempoSection* ts, const samplepos_t sample, const int& sub_num)
{
	Metrics future_map;

	if (ts->position_lock_style() == MusicTime) {
		{
			/* if we're snapping to a musical grid, set the pulse exactly instead of via the supplied sample. */
			Glib::Threads::RWLock::WriterLock lm (lock);
			TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);

			tempo_copy->set_position_lock_style (AudioTime);

			if (solve_map_minute (future_map, tempo_copy, minute_at_sample (sample))) {
				const double beat = exact_beat_at_sample_locked (future_map, sample, sub_num);
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


			if (sub_num != 0) {
				/* We're moving the object that defines the grid while snapping to it...
				 * Placing the ts at the beat corresponding to the requested sample may shift the
				 * grid in such a way that the mouse is left hovering over a completerly different division,
				 * causing jittering when the mouse next moves (esp. large tempo deltas).
				 * We fudge around this by doing this in the musical domain and then swapping back for the recompute.
				 */
				const double qn = exact_qn_at_sample_locked (_metrics, sample, sub_num);
				tempo_copy->set_position_lock_style (MusicTime);
				if (solve_map_pulse (future_map, tempo_copy, qn / 4.0)) {
					ts->set_position_lock_style (MusicTime);
					solve_map_pulse (_metrics, ts, qn / 4.0);
					ts->set_position_lock_style (AudioTime);
					recompute_meters (_metrics);
				}
			} else {
				if (solve_map_minute (future_map, tempo_copy, minute_at_sample (sample))) {
					solve_map_minute (_metrics, ts, minute_at_sample (sample));
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

	MetricPositionChanged (PropertyChange ()); // Emit Signal
}

/** moves a MeterSection to a specified position.
 * @param ms - the section to be moved
 * @param sample - the new position in samples for the meter
 *
 * as a meter cannot snap to anything but bars,
 * the supplied sample is rounded to the nearest bar, possibly
 * leaving the meter position unchanged.
 */
void
TempoMap::gui_set_meter_position (MeterSection* ms, const samplepos_t sample)
{
	Metrics future_map;

	if (ms->position_lock_style() == AudioTime) {

		{
			Glib::Threads::RWLock::WriterLock lm (lock);
			MeterSection* copy = copy_metrics_and_point (_metrics, future_map, ms);

			if (solve_map_minute (future_map, copy, minute_at_sample (sample))) {
				solve_map_minute (_metrics, ms, minute_at_sample (sample));
				recompute_tempi (_metrics);
			}
		}
	} else {
		{
			Glib::Threads::RWLock::WriterLock lm (lock);
			MeterSection* copy = copy_metrics_and_point (_metrics, future_map, ms);

			const double beat = beat_at_minute_locked (_metrics, minute_at_sample (sample));
			const Temporal::BBT_Time bbt = bbt_at_beat_locked (_metrics, beat);

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

	MetricPositionChanged (PropertyChange ()); // Emit Signal
}

bool
TempoMap::gui_change_tempo (TempoSection* ts, const Tempo& bpm)
{
	Metrics future_map;
	bool can_solve = false;
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);

		if (tempo_copy->type() == TempoSection::Constant) {
			tempo_copy->set_end_note_types_per_minute (bpm.note_types_per_minute());
			tempo_copy->set_note_types_per_minute (bpm.note_types_per_minute());
		} else {
			tempo_copy->set_note_types_per_minute (bpm.note_types_per_minute());
			tempo_copy->set_end_note_types_per_minute (bpm.end_note_types_per_minute());
		}

		if (ts->clamped()) {
			TempoSection* prev = 0;
			if ((prev = previous_tempo_section_locked (future_map, tempo_copy)) != 0) {
				prev->set_end_note_types_per_minute (tempo_copy->note_types_per_minute());
			}
		}

		recompute_tempi (future_map);

		if (check_solved (future_map)) {
			if (ts->type() == TempoSection::Constant) {
				ts->set_end_note_types_per_minute (bpm.note_types_per_minute());
				ts->set_note_types_per_minute (bpm.note_types_per_minute());
			} else {
				ts->set_end_note_types_per_minute (bpm.end_note_types_per_minute());
				ts->set_note_types_per_minute (bpm.note_types_per_minute());
			}

			if (ts->clamped()) {
				TempoSection* prev = 0;
				if ((prev = previous_tempo_section_locked (_metrics, ts)) != 0) {
					prev->set_end_note_types_per_minute (ts->note_types_per_minute());
				}
			}

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
		MetricPositionChanged (PropertyChange ()); // Emit Signal
	}

	return can_solve;
}

void
TempoMap::gui_stretch_tempo (TempoSection* ts, const samplepos_t sample, const samplepos_t end_sample, const double start_qnote, const double end_qnote)
{
	/*
	  Ts (future prev_t)   Tnext
	  |                    |
	  |     [drag^]        |
	  |----------|----------
	        e_f  qn_beats(sample)
	*/

	Metrics future_map;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		if (!ts) {
			return;
		}

		TempoSection* ts_copy = copy_metrics_and_point (_metrics, future_map, ts);

		if (!ts_copy) {
			return;
		}

		/* minimum allowed measurement distance in samples */
		samplepos_t const min_dframe = 2;

		double new_bpm;
		if (ts_copy->clamped()) {
			TempoSection* next_t = next_tempo_section_locked (future_map, ts_copy);
			TempoSection* prev_to_ts_copy = previous_tempo_section_locked (future_map, ts_copy);
			assert (prev_to_ts_copy);
			/* the change in samples is the result of changing the slope of at most 2 previous tempo sections.
			 * constant to constant is straightforward, as the tempo prev to ts_copy has constant slope.
			 */
			double contribution = 0.0;
			if (next_t && prev_to_ts_copy->type() == TempoSection::Ramp) {
				contribution = (ts_copy->pulse() - prev_to_ts_copy->pulse()) / (double) (next_t->pulse() - prev_to_ts_copy->pulse());
			}
			samplepos_t const fr_off = end_sample - sample;
			sampleoffset_t const ts_copy_sample_contribution = fr_off - (contribution * (double) fr_off);

			if (sample > prev_to_ts_copy->sample() + min_dframe && (sample + ts_copy_sample_contribution) > prev_to_ts_copy->sample() + min_dframe) {
				new_bpm = ts_copy->note_types_per_minute() * ((start_qnote - (prev_to_ts_copy->pulse() * 4.0))
									     / (end_qnote - (prev_to_ts_copy->pulse() * 4.0)));
			} else {
				new_bpm = ts_copy->note_types_per_minute();
			}
		} else {
			if (sample > ts_copy->sample() + min_dframe && end_sample > ts_copy->sample() + min_dframe) {

				new_bpm = ts_copy->note_types_per_minute() * ((sample - ts_copy->sample())
									     / (double) (end_sample - ts_copy->sample()));
			} else {
				new_bpm = ts_copy->note_types_per_minute();
			}

			new_bpm = min (new_bpm, (double) 1000.0);
		}
		/* don't clamp and proceed here.
		   testing has revealed that this can go negative,
		   which is an entirely different thing to just being too low.
		*/

		if (new_bpm < 0.5) {
			goto out;
		}

		ts_copy->set_note_types_per_minute (new_bpm);

		if (ts_copy->clamped()) {
			TempoSection* prev = 0;
			if ((prev = previous_tempo_section_locked (future_map, ts_copy)) != 0) {
				prev->set_end_note_types_per_minute (ts_copy->note_types_per_minute());
			}
		}

		recompute_tempi (future_map);
		recompute_meters (future_map);

		if (check_solved (future_map)) {
			ts->set_note_types_per_minute (new_bpm);

			if (ts->clamped()) {
				TempoSection* prev = 0;
				if ((prev = previous_tempo_section_locked (_metrics, ts)) != 0) {
					prev->set_end_note_types_per_minute (ts->note_types_per_minute());
				}
			}

			recompute_tempi (_metrics);
			recompute_meters (_metrics);
		}
	}


out:
	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}
	MetricPositionChanged (PropertyChange ()); // Emit Signal


}
void
TempoMap::gui_stretch_tempo_end (TempoSection* ts, const samplepos_t sample, const samplepos_t end_sample)
{
	/*
	  Ts (future prev_t)   Tnext
	  |                    |
	  |     [drag^]        |
	  |----------|----------
	        e_f  qn_beats(sample)
	*/

	Metrics future_map;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		if (!ts) {
			return;
		}

		TempoSection* prev_t = copy_metrics_and_point (_metrics, future_map, ts);

		if (!prev_t) {
			return;
		}

		/* minimum allowed measurement distance in samples */
		samplepos_t const min_dframe = 2;
		double new_bpm;

		if (sample > prev_t->sample() + min_dframe && end_sample > prev_t->sample() + min_dframe) {
			new_bpm = prev_t->end_note_types_per_minute() * ((prev_t->sample() - sample)
										 / (double) (prev_t->sample() - end_sample));
		} else {
			new_bpm = prev_t->end_note_types_per_minute();
		}

		new_bpm = min (new_bpm, (double) 1000.0);

		if (new_bpm < 0.5) {
			goto out;
		}

		prev_t->set_end_note_types_per_minute (new_bpm);

		TempoSection* next = 0;
		if ((next = next_tempo_section_locked (future_map, prev_t)) != 0) {
			if (next->clamped()) {
				next->set_note_types_per_minute (prev_t->end_note_types_per_minute());
			}
		}

		recompute_tempi (future_map);
		recompute_meters (future_map);

		if (check_solved (future_map)) {
			ts->set_end_note_types_per_minute (new_bpm);

			TempoSection* true_next = 0;
			if ((true_next = next_tempo_section_locked (_metrics, ts)) != 0) {
				if (true_next->clamped()) {
					true_next->set_note_types_per_minute (ts->end_note_types_per_minute());
				}
			}

			recompute_tempi (_metrics);
			recompute_meters (_metrics);
		}
	}


out:
	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}

	MetricPositionChanged (PropertyChange ()); // Emit Signal
}

bool
TempoMap::gui_twist_tempi (TempoSection* ts, const Tempo& bpm, const samplepos_t sample, const samplepos_t end_sample)
{
	TempoSection* next_t = 0;
	TempoSection* next_to_next_t = 0;
	Metrics future_map;
	bool can_solve = false;

	/* minimum allowed measurement distance in samples */
	samplepos_t const min_dframe = 2;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		if (!ts) {
			return false;
		}

		TempoSection* tempo_copy = copy_metrics_and_point (_metrics, future_map, ts);
		TempoSection* prev_to_prev_t = 0;
		const sampleoffset_t fr_off = end_sample - sample;

		if (!tempo_copy) {
			return false;
		}

		if (tempo_copy->pulse() > 0.0) {
			prev_to_prev_t = const_cast<TempoSection*>(&tempo_section_at_minute_locked (future_map, minute_at_sample (tempo_copy->sample() - 1)));
		}

		for (Metrics::const_iterator i = future_map.begin(); i != future_map.end(); ++i) {
			if ((*i)->is_tempo() && (*i)->minute() >  tempo_copy->minute()) {
				next_t = static_cast<TempoSection*> (*i);
				break;
			}
		}

		if (!next_t) {
			return false;
		}

		for (Metrics::const_iterator i = future_map.begin(); i != future_map.end(); ++i) {
			if ((*i)->is_tempo() && (*i)->minute() >  next_t->minute()) {
				next_to_next_t = static_cast<TempoSection*> (*i);
				break;
			}
		}

		if (!next_to_next_t) {
			return false;
		}

		double prev_contribution = 0.0;

		if (next_t && prev_to_prev_t && prev_to_prev_t->type() == TempoSection::Ramp) {
			prev_contribution = (tempo_copy->sample() - prev_to_prev_t->sample()) / (double) (next_t->sample() - prev_to_prev_t->sample());
		}

		const sampleoffset_t tempo_copy_sample_contribution = fr_off - (prev_contribution * (double) fr_off);


		samplepos_t old_tc_minute = tempo_copy->minute();
		double old_next_minute = next_t->minute();
		double old_next_to_next_minute = next_to_next_t->minute();

		double new_bpm;
		double new_next_bpm;
		double new_copy_end_bpm;

		if (sample > tempo_copy->sample() + min_dframe && (sample + tempo_copy_sample_contribution) > tempo_copy->sample() + min_dframe) {
			new_bpm = tempo_copy->note_types_per_minute() * ((sample - tempo_copy->sample())
										       / (double) (end_sample - tempo_copy->sample()));
		} else {
			new_bpm = tempo_copy->note_types_per_minute();
		}

		/* don't clamp and proceed here.
		   testing has revealed that this can go negative,
		   which is an entirely different thing to just being too low.
		*/
		if (new_bpm < 0.5) {
			return false;
		}

		new_bpm = min (new_bpm, (double) 1000.0);

		tempo_copy->set_note_types_per_minute (new_bpm);
		if (tempo_copy->type() == TempoSection::Constant) {
			tempo_copy->set_end_note_types_per_minute (new_bpm);
		}

		recompute_tempi (future_map);

		if (check_solved (future_map)) {

			if (!next_t) {
				return false;
			}

			ts->set_note_types_per_minute (new_bpm);
			if (ts->type() == TempoSection::Constant) {
				ts->set_end_note_types_per_minute (new_bpm);
			}

			recompute_map (_metrics);

			can_solve = true;
		}

		if (next_t->type() == TempoSection::Constant || next_t->c() == 0.0) {
			if (sample > tempo_copy->sample() + min_dframe && end_sample > tempo_copy->sample() + min_dframe) {

				new_next_bpm = next_t->note_types_per_minute() * ((next_to_next_t->minute() - old_next_minute)
										  / (double) ((old_next_to_next_minute) - old_next_minute));

			} else {
				new_next_bpm = next_t->note_types_per_minute();
			}

			next_t->set_note_types_per_minute (new_next_bpm);
			recompute_tempi (future_map);

			if (check_solved (future_map)) {
				for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
					if ((*i)->is_tempo() && (*i)->minute() >  ts->minute()) {
						next_t = static_cast<TempoSection*> (*i);
						break;
					}
				}

				if (!next_t) {
					return false;
				}
				next_t->set_note_types_per_minute (new_next_bpm);
				recompute_map (_metrics);
				can_solve = true;
			}
		} else {
			double next_sample_ratio = 1.0;
			double copy_sample_ratio = 1.0;

			if (next_to_next_t) {
				next_sample_ratio = (next_to_next_t->minute() - old_next_minute) / (old_next_to_next_minute -  old_next_minute);

				copy_sample_ratio = ((old_tc_minute - next_t->minute()) / (double) (old_tc_minute - old_next_minute));
			}

			new_next_bpm = next_t->note_types_per_minute() * next_sample_ratio;
			new_copy_end_bpm = tempo_copy->end_note_types_per_minute() * copy_sample_ratio;

			tempo_copy->set_end_note_types_per_minute (new_copy_end_bpm);

			if (next_t->clamped()) {
				next_t->set_note_types_per_minute (new_copy_end_bpm);
			} else {
				next_t->set_note_types_per_minute (new_next_bpm);
			}

			recompute_tempi (future_map);

			if (check_solved (future_map)) {
				for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
					if ((*i)->is_tempo() && (*i)->minute() >  ts->minute()) {
						next_t = static_cast<TempoSection*> (*i);
						break;
					}
				}

				if (!next_t) {
					return false;
				}

				if (next_t->clamped()) {
					next_t->set_note_types_per_minute (new_copy_end_bpm);
				} else {
					next_t->set_note_types_per_minute (new_next_bpm);
				}

				ts->set_end_note_types_per_minute (new_copy_end_bpm);
				recompute_map (_metrics);
				can_solve = true;
			}
		}
	}

	Metrics::const_iterator d = future_map.begin();
	while (d != future_map.end()) {
		delete (*d);
		++d;
	}

	MetricPositionChanged (PropertyChange ()); // Emit Signal

	return can_solve;
}

/** Returns the sample position of the musical position zero */
samplepos_t
TempoMap::music_origin ()
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return first_tempo().sample();
}

/** Returns the exact bbt-based beat corresponding to the bar, beat or quarter note subdivision nearest to
 * the supplied sample, possibly returning a negative value.
 *
 * @param sample  The session sample position.
 * @param sub_num The subdivision to use when rounding the beat.
 *                A value of -1 indicates rounding to BBT bar. 1 indicates rounding to BBT beats.
 *                Positive integers indicate quarter note (non BBT) divisions.
 *                0 indicates that the returned beat should not be rounded (equivalent to quarter_note_at_sample()).
 * @return The beat position of the supplied sample.
 *
 * when working to a musical grid, the use of sub_nom indicates that
 * the position should be interpreted musically.
 *
 * it effectively snaps to meter bars, meter beats or quarter note divisions
 * (as per current gui convention) and returns a musical position independent of frame rate.
 *
 * If the supplied sample lies before the first meter, the return will be negative,
 * in which case the returned beat uses the first meter (for BBT subdivisions) and
 * the continuation of the tempo curve (backwards).
 *
 * This function is sensitive to tempo and meter.
 */
double
TempoMap::exact_beat_at_sample (const samplepos_t sample, const int32_t sub_num) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return exact_beat_at_sample_locked (_metrics, sample, sub_num);
}

double
TempoMap::exact_beat_at_sample_locked (const Metrics& metrics, const samplepos_t sample, const int32_t divisions) const
{
	return beat_at_pulse_locked (_metrics, exact_qn_at_sample_locked (metrics, sample, divisions) / 4.0);
}

/** Returns the exact quarter note corresponding to the bar, beat or quarter note subdivision nearest to
 * the supplied sample, possibly returning a negative value.
 *
 * @param sample  The session sample position.
 * @param sub_num The subdivision to use when rounding the quarter note.
 *                A value of -1 indicates rounding to BBT bar. 1 indicates rounding to BBT beats.
 *                Positive integers indicate quarter note (non BBT) divisions.
 *                0 indicates that the returned quarter note should not be rounded (equivalent to quarter_note_at_sample()).
 * @return The quarter note position of the supplied sample.
 *
 * When working to a musical grid, the use of sub_nom indicates that
 * the sample position should be interpreted musically.
 *
 * it effectively snaps to meter bars, meter beats or quarter note divisions
 * (as per current gui convention) and returns a musical position independent of frame rate.
 *
 * If the supplied sample lies before the first meter, the return will be negative,
 * in which case the returned quarter note uses the first meter (for BBT subdivisions) and
 * the continuation of the tempo curve (backwards).
 *
 * This function is tempo-sensitive.
 */
double
TempoMap::exact_qn_at_sample (const samplepos_t sample, const int32_t sub_num) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return exact_qn_at_sample_locked (_metrics, sample, sub_num);
}

double
TempoMap::exact_qn_at_sample_locked (const Metrics& metrics, const samplepos_t sample, const int32_t sub_num) const
{
	double qn = pulse_at_minute_locked (metrics, minute_at_sample (sample)) * 4.0;

	if (sub_num > 1) {
		qn = floor (qn) + (floor (((qn - floor (qn)) * (double) sub_num) + 0.5) / sub_num);
	} else if (sub_num == 1) {
		/* the gui requested exact musical (BBT) beat */
		qn = pulse_at_beat_locked (metrics, (floor (beat_at_minute_locked (metrics, minute_at_sample (sample)) + 0.5))) * 4.0;
	} else if (sub_num == -1) {
		/* snap to  bar */
		Temporal::BBT_Time bbt = bbt_at_pulse_locked (metrics, qn / 4.0);
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

/** returns the sample duration of the supplied BBT time at a specified sample position in the tempo map.
 * @param pos the sample position in the tempo map.
 * @param bbt the distance in BBT time from pos to calculate.
 * @param dir the rounding direction..
 * @return the duration in samples between pos and bbt
*/
samplecnt_t
TempoMap::bbt_duration_at (samplepos_t pos, const BBT_Time& bbt, int dir)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	BBT_Time pos_bbt = bbt_at_minute_locked (_metrics, minute_at_sample (pos));

	const double divisions = meter_section_at_minute_locked (_metrics, minute_at_sample (pos)).divisions_per_bar();

	if (dir > 0) {
		pos_bbt.bars += bbt.bars;

		pos_bbt.ticks += bbt.ticks;
		if ((double) pos_bbt.ticks > Temporal::ticks_per_beat) {
			pos_bbt.beats += 1;
			pos_bbt.ticks -= Temporal::ticks_per_beat;
		}

		pos_bbt.beats += bbt.beats;
		if ((double) pos_bbt.beats > divisions) {
			pos_bbt.bars += 1;
			pos_bbt.beats -= divisions;
		}
		const samplecnt_t pos_bbt_sample = sample_at_minute (minute_at_bbt_locked (_metrics, pos_bbt));

		return pos_bbt_sample - pos;

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
				pos_bbt.ticks = Temporal::ticks_per_beat - (bbt.ticks - pos_bbt.ticks);
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

		return pos - sample_at_minute (minute_at_bbt_locked (_metrics, pos_bbt));
	}

	return 0;
}

MusicSample
TempoMap::round_to_bar (samplepos_t fr, RoundMode dir)
{
	return round_to_type (fr, dir, Bar);
}

MusicSample
TempoMap::round_to_beat (samplepos_t fr, RoundMode dir)
{
	return round_to_type (fr, dir, Beat);
}

MusicSample
TempoMap::round_to_quarter_note_subdivision (samplepos_t fr, int sub_num, RoundMode dir)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	uint32_t ticks = (uint32_t) floor (max (0.0, pulse_at_minute_locked (_metrics, minute_at_sample (fr))) * Temporal::ticks_per_beat * 4.0);
	uint32_t beats = (uint32_t) floor (ticks / Temporal::ticks_per_beat);
	uint32_t ticks_one_subdivisions_worth = (uint32_t) Temporal::ticks_per_beat / sub_num;

	ticks -= beats * Temporal::ticks_per_beat;

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

		/* NOTE: this code intentionally limits the rounding so we don't advance to the next beat.
		 * For the purposes of "jump-to-next-subdivision", we DO want to advance to the next beat.
		 * And since the "prev" direction DOES move beats, I assume this code is unintended.
		 * But I'm keeping it around, commened out, until we determine there are no terrible consequences.
		 */
#if 0
		if (ticks >= Temporal::ticks_per_beat) {
			ticks -= Temporal::ticks_per_beat;
		}
#endif

	} else if (dir < 0) {

		/* round to previous (or same iff dir == RoundDownMaybe) */

		uint32_t difference = ticks % ticks_one_subdivisions_worth;

		if (difference == 0 && dir == RoundDownAlways) {
			/* right on the subdivision, but force-rounding down,
			   so the difference is just the subdivision ticks */
			difference = ticks_one_subdivisions_worth;
		}

		if (ticks < difference) {
			ticks = Temporal::ticks_per_beat - ticks;
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

			if (ticks > Temporal::ticks_per_beat) {
				++beats;
				ticks -= Temporal::ticks_per_beat;
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("fold beat to %1\n", beats));
			}

		} else if (rem > 0) {

			/* closer to previous subdivision, so shift backward */

			if (rem > ticks) {
				if (beats == 0) {
					/* can't go backwards past zero, so ... */
					return MusicSample (0, 0);
				}
				/* step back to previous beat */
				--beats;
				ticks = lrint (Temporal::ticks_per_beat - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("step back beat to %1\n", beats));
			} else {
				ticks = lrint (ticks - rem);
				DEBUG_TRACE (DEBUG::SnapBBT, string_compose ("moved backward to %1\n", ticks));
			}
		} else {
			/* on the subdivision, do nothing */
		}
	}

	MusicSample ret (0, 0);
	ret.sample = sample_at_minute (minute_at_pulse_locked (_metrics, (beats + (ticks / Temporal::ticks_per_beat)) / 4.0));
	ret.division = sub_num;

	return ret;
}

MusicSample
TempoMap::round_to_type (samplepos_t sample, RoundMode dir, BBTPointType type)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	const double minute = minute_at_sample (sample);
	const double beat_at_samplepos = max (0.0, beat_at_minute_locked (_metrics, minute));
	BBT_Time bbt (bbt_at_beat_locked (_metrics, beat_at_samplepos));
	MusicSample ret (0, 0);

	switch (type) {
	case Bar:
		ret.division = -1;

		if (dir < 0) {
			/* find bar previous to 'sample' */
			if (bbt.bars > 0)
				--bbt.bars;
			bbt.beats = 1;
			bbt.ticks = 0;

			ret.sample = sample_at_minute (minute_at_bbt_locked (_metrics, bbt));

			return ret;

		} else if (dir > 0) {
			/* find bar following 'sample' */
			++bbt.bars;
			bbt.beats = 1;
			bbt.ticks = 0;

			ret.sample = sample_at_minute (minute_at_bbt_locked (_metrics, bbt));

			return ret;
		} else {
			/* true rounding: find nearest bar */
			samplepos_t raw_ft = sample_at_minute (minute_at_bbt_locked (_metrics, bbt));
			bbt.beats = 1;
			bbt.ticks = 0;
			samplepos_t prev_ft = sample_at_minute (minute_at_bbt_locked (_metrics, bbt));
			++bbt.bars;
			samplepos_t next_ft = sample_at_minute (minute_at_bbt_locked (_metrics, bbt));

			if ((raw_ft - prev_ft) > (next_ft - prev_ft) / 2) {
				ret.sample = next_ft;

				return ret;
			} else {
				--bbt.bars;
				ret.sample = prev_ft;

				return ret;
			}
		}

		break;

	case Beat:
		ret.division = 1;

		if (dir < 0) {
			ret.sample = sample_at_minute (minute_at_beat_locked (_metrics, floor (beat_at_samplepos)));

			return ret;
		} else if (dir > 0) {
			ret.sample = sample_at_minute (minute_at_beat_locked (_metrics, ceil (beat_at_samplepos)));

			return ret;
		} else {
			ret.sample = sample_at_minute (minute_at_beat_locked (_metrics, floor (beat_at_samplepos + 0.5)));

			return ret;
		}
		break;
	}

	return MusicSample (0, 0);
}

void
TempoMap::get_grid (vector<TempoMap::BBTPoint>& points,
		    samplepos_t lower, samplepos_t upper, uint32_t bar_mod)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	int32_t cnt = ceil (beat_at_minute_locked (_metrics, minute_at_sample (lower)));
	/* although the map handles negative beats, bbt doesn't. */
	if (cnt < 0.0) {
		cnt = 0.0;
	}

	if (minute_at_beat_locked (_metrics, cnt) >= minute_at_sample (upper)) {
		return;
	}
	if (bar_mod == 0) {
		while (true) {
			samplecnt_t pos = sample_at_minute (minute_at_beat_locked (_metrics, cnt));
			if (pos >= upper) {
				break;
			}
			const MeterSection meter = meter_section_at_minute_locked (_metrics, minute_at_sample (pos));
			const BBT_Time bbt = bbt_at_beat_locked (_metrics, cnt);
			const double qn = pulse_at_beat_locked (_metrics, cnt) * 4.0;

			if (pos >= lower) {
				points.push_back (BBTPoint (meter, tempo_at_minute_locked (_metrics, minute_at_sample (pos)), pos, bbt.bars, bbt.beats, qn));
			}
			++cnt;
		}
	} else {
		BBT_Time bbt = bbt_at_minute_locked (_metrics, minute_at_sample (lower));
		bbt.beats = 1;
		bbt.ticks = 0;

		if (bar_mod != 1) {
			bbt.bars -= bbt.bars % bar_mod;
			++bbt.bars;
		}

		while (true) {
			samplecnt_t pos = sample_at_minute (minute_at_bbt_locked (_metrics, bbt));
			if (pos >= upper) {
				break;
			}
			const MeterSection meter = meter_section_at_minute_locked (_metrics, minute_at_sample (pos));
			const double qn = pulse_at_bbt_locked (_metrics, bbt) * 4.0;

			if (pos >= lower) {
				points.push_back (BBTPoint (meter, tempo_at_minute_locked (_metrics, minute_at_sample (pos)), pos, bbt.bars, bbt.beats, qn));
			}
			bbt.bars += bar_mod;
		}
	}
}

void
TempoMap::midi_clock_beat_at_of_after (samplepos_t const pos, samplepos_t& clk_pos, uint32_t& clk_beat)
{
	/* Sequences are always assumed to start on a MIDI Beat of 0 (ie, the downbeat).
	 * Each MIDI Beat spans 6 MIDI Clocks. In other words, each MIDI Beat is a 16th note
	 * (since there are 24 MIDI Clocks in a quarter note, therefore 4 MIDI Beats also fit in a quarter).
	 * So, a master can sync playback to a resolution of any particular 16th note
	 *
	 * from http://midi.teragonaudio.com/tech/midispec/seq.htm
	 */
	Glib::Threads::RWLock::ReaderLock lm (lock);

	/* pulse is a whole note */
	clk_beat = ceil (16.0 * (pulse_at_minute_locked  (_metrics, minute_at_sample (pos))));
	clk_pos =  sample_at_minute (minute_at_pulse_locked (_metrics, clk_beat / 16.0));
	assert (clk_pos >= pos);
}

const TempoSection&
TempoMap::tempo_section_at_sample (samplepos_t sample) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_section_at_minute_locked (_metrics, minute_at_sample (sample));
}

TempoSection&
TempoMap::tempo_section_at_sample (samplepos_t sample)
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return tempo_section_at_minute_locked (_metrics, minute_at_sample (sample));
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

			if (!t->active()) {
				continue;
			}

			if (prev_t && ((t->pulse() - prev_m->pulse()) * prev_m->note_divisor()) + prev_m->beat() > beat) {
				break;
			}
			prev_t = t;
		}

	}

	if (prev_t == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return *prev_t;
}

TempoSection*
TempoMap::previous_tempo_section (TempoSection* ts) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return previous_tempo_section_locked (_metrics, ts);

}

TempoSection*
TempoMap::previous_tempo_section_locked (const Metrics& metrics, TempoSection* ts) const
{
	if (!ts) {
		return 0;
	}

	TempoSection* prev = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((*i)->is_tempo()) {
			TempoSection* t = static_cast<TempoSection*> (*i);

			if (!t->active()) {
				continue;
			}

			if (prev && t == ts) {

				return prev;
			}

			prev = t;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return 0;
}

TempoSection*
TempoMap::next_tempo_section (TempoSection* ts) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return next_tempo_section_locked (_metrics, ts);
}

TempoSection*
TempoMap::next_tempo_section_locked (const Metrics& metrics, TempoSection* ts) const
{
	if (!ts) {
		return 0;
	}

	TempoSection* prev = 0;

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((*i)->is_tempo()) {
			TempoSection* t = static_cast<TempoSection*> (*i);

			if (!t->active()) {
				continue;
			}

			if (prev && prev == ts) {

				return t;
			}

			prev = t;
		}
	}

	if (prev == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
	}

	return 0;
}
/* don't use this to calculate length (the tempo is only correct for this sample).
 * do that stuff based on the beat_at_sample and sample_at_beat api
 */
double
TempoMap::samples_per_quarter_note_at (const samplepos_t sample, const samplecnt_t sr) const
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
			if (ts_at && (*i)->sample() > sample) {
				ts_after = t;
				break;
			}
			ts_at = t;
		}
	}
	assert (ts_at);

	if (ts_after) {
		return  (60.0 * _sample_rate) / ts_at->tempo_at_minute (minute_at_sample (sample)).quarter_notes_per_minute();
	}
	/* must be treated as constant tempo */
	return ts_at->samples_per_quarter_note (_sample_rate);
}

const MeterSection&
TempoMap::meter_section_at_sample (samplepos_t sample) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	return meter_section_at_minute_locked (_metrics, minute_at_sample (sample));
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

	if (prev_m == 0) {
		fatal << endmsg;
		abort(); /*NOTREACHED*/
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
TempoMap::meter_at_sample (samplepos_t sample) const
{
	TempoMetric m (metric_at (sample));
	return m.meter();
}

void
TempoMap::fix_legacy_session ()
{
	MeterSection* prev_m = 0;
	TempoSection* prev_t = 0;
	bool have_initial_t = false;

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
									  + (m->bbt().ticks / Temporal::ticks_per_beat)
									  , m->bbt());
				m->set_beat (start);
				const double start_beat = ((m->bbt().bars - 1) * prev_m->note_divisor())
					+ (m->bbt().beats - 1)
					+ (m->bbt().ticks / Temporal::ticks_per_beat);
				m->set_pulse (start_beat / prev_m->note_divisor());
			}
			prev_m = m;
		} else if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {

			if (!t->active()) {
				continue;
			}
			/* Ramp type never existed in the era of this tempo section */
			t->set_end_note_types_per_minute (t->note_types_per_minute());

			if (t->initial()) {
				t->set_pulse (0.0);
				t->set_minute (0.0);
				t->set_position_lock_style (AudioTime);
				prev_t = t;
				have_initial_t = true;
				continue;
			}

			if (prev_t) {
				/* some 4.x sessions have no initial (non-movable) tempo. */
				if (!have_initial_t) {
					prev_t->set_pulse (0.0);
					prev_t->set_minute (0.0);
					prev_t->set_position_lock_style (AudioTime);
					prev_t->set_initial (true);
					prev_t->set_locked_to_meter (true);
					have_initial_t = true;
				}

				const double beat = ((t->legacy_bbt().bars - 1) * ((prev_m) ? prev_m->note_divisor() : 4.0))
					+ (t->legacy_bbt().beats - 1)
					+ (t->legacy_bbt().ticks / Temporal::ticks_per_beat);
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
void
TempoMap::fix_legacy_end_session ()
{
	TempoSection* prev_t = 0;

	for (Metrics::iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
		TempoSection* t;

		if ((t = dynamic_cast<TempoSection*>(*i)) != 0) {

			if (!t->active()) {
				continue;
			}

			if (prev_t) {
				if (prev_t->end_note_types_per_minute() < 0.0) {
					prev_t->set_end_note_types_per_minute (t->note_types_per_minute());
				}
			}

			prev_t = t;
		}
	}

	if (prev_t) {
		prev_t->set_end_note_types_per_minute (prev_t->note_types_per_minute());
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
					TempoSection* ts = new TempoSection (*child, _sample_rate);
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
					MeterSection* ms = new MeterSection (*child, _sample_rate);
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

		/* check for legacy sessions where bbt was the base musical unit for tempo */
		for (Metrics::const_iterator i = _metrics.begin(); i != _metrics.end(); ++i) {
			TempoSection* t;
			if ((t = dynamic_cast<TempoSection*> (*i)) != 0) {
				if (t->legacy_bbt().bars != 0) {
					fix_legacy_session();
					break;
				}

				if (t->end_note_types_per_minute() < 0.0) {
					fix_legacy_end_session();
					break;
				}
			}
		}

		if (niter == nlist.end()) {
			MetricSectionSorter cmp;
			_metrics.sort (cmp);
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
					if (prev_m->beat() == ms->beat()) {
						cerr << string_compose (_("Multiple meter definitions found at %1"), prev_m->beat()) << endmsg;
						error << string_compose (_("Multiple meter definitions found at %1"), prev_m->beat()) << endmsg;
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
			o << "Tempo @ " << *i << " start : " << t->note_types_per_minute() << " end : " << t->end_note_types_per_minute() << " BPM (pulse = 1/" << t->note_type()
			  << " type= " << enum_2_string (t->type()) << ") "  << " at pulse= " << t->pulse()
			  << " minute= " << t->minute() << " sample= " << t->sample() << " (initial? " << t->initial() << ')'
			  << " pos lock: " << enum_2_string (t->position_lock_style()) << std::endl;
			if (prev_t) {
				o <<  "  current start  : " << t->note_types_per_minute()
				  <<  "  current end  : " << t->end_note_types_per_minute()
				  << " | " << t->pulse() << " | " << t->sample() << " | " << t->minute() << std::endl;
				o << "  previous     : " << prev_t->note_types_per_minute()
				  << " | " << prev_t->pulse() << " | " << prev_t->sample() << " | " << prev_t->minute() << std::endl;
				o << "  calculated   : " << prev_t->tempo_at_pulse (t->pulse())
				  << " | " << prev_t->pulse_at_ntpm (prev_t->end_note_types_per_minute(), t->minute())
				  << " | " << sample_at_minute (prev_t->minute_at_ntpm (prev_t->end_note_types_per_minute(), t->pulse()))
				  << " | " << prev_t->minute_at_ntpm (prev_t->end_note_types_per_minute(), t->pulse()) << std::endl;
			}
			prev_t = t;
		} else if ((m = dynamic_cast<const MeterSection*>(*i)) != 0) {
			o << "Meter @ " << *i << ' ' << m->divisions_per_bar() << '/' << m->note_divisor() << " at " << m->bbt()
			  << " sample= " << m->sample() << " pulse: " << m->pulse() <<  " beat : " << m->beat()
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
TempoMap::insert_time (samplepos_t where, samplecnt_t amount)
{
	for (Metrics::reverse_iterator i = _metrics.rbegin(); i != _metrics.rend(); ++i) {
		if ((*i)->sample() >= where && !(*i)->initial ()) {
			MeterSection* ms;
			TempoSection* ts;

			if ((ms = dynamic_cast <MeterSection*>(*i)) != 0) {
				gui_set_meter_position (ms, (*i)->sample() + amount);
			}

			if ((ts = dynamic_cast <TempoSection*>(*i)) != 0) {
				gui_set_tempo_position (ts, (*i)->sample() + amount, 0);
			}
		}
	}

	PropertyChanged (PropertyChange ());
}

bool
TempoMap::remove_time (samplepos_t where, samplecnt_t amount)
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
			if ((*i)->sample() >= where && (*i)->sample() < where+amount) {
				metric_kill_list.push_back(*i);
				TempoSection *lt = dynamic_cast<TempoSection*> (*i);
				if (lt)
					last_tempo = lt;
				MeterSection *lm = dynamic_cast<MeterSection*> (*i);
				if (lm)
					last_meter = lm;
			}
			else if ((*i)->sample() >= where) {
				// TODO: make sure that moved tempo/meter markers are rounded to beat/bar boundaries
				(*i)->set_minute ((*i)->minute() - minute_at_sample (amount));
				if ((*i)->sample() == where) {
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
			last_tempo->set_minute (minute_at_sample (where));
			moved = true;
		}
		if (last_meter && !meter_after) {
			metric_kill_list.remove(last_meter);
			last_meter->set_minute (minute_at_sample (where));
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

/** Add some (fractional) Beats to a session sample position, and return the result in samples.
 *  pos can be -ve, if required.
 */
samplepos_t
TempoMap::samplepos_plus_qn (samplepos_t sample, Temporal::Beats beats) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);
	const double sample_qn = pulse_at_minute_locked (_metrics, minute_at_sample (sample)) * 4.0;

	return sample_at_minute (minute_at_pulse_locked (_metrics, (sample_qn + beats.to_double()) / 4.0));
}

samplepos_t
TempoMap::samplepos_plus_bbt (samplepos_t pos, BBT_Time op) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	BBT_Time pos_bbt = bbt_at_beat_locked (_metrics, beat_at_minute_locked (_metrics, minute_at_sample (pos)));
	pos_bbt.ticks += op.ticks;
	if (pos_bbt.ticks >= Temporal::ticks_per_beat) {
		++pos_bbt.beats;
		pos_bbt.ticks -= Temporal::ticks_per_beat;
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

	return sample_at_minute (minute_at_bbt_locked (_metrics, pos_bbt));
}

/** Count the number of beats that are equivalent to distance when going forward,
 * starting at pos.
 */
Temporal::Beats
TempoMap::framewalk_to_qn (samplepos_t pos, samplecnt_t distance) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	return Temporal::Beats (quarter_notes_between_samples_locked (_metrics, pos, pos + distance));
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

	o << "MetricSection @ " << section.sample() << ' ';

	const TempoSection* ts;
	const MeterSection* ms;

	if ((ts = dynamic_cast<const TempoSection*> (&section)) != 0) {
		o << *((const Tempo*) ts);
	} else if ((ms = dynamic_cast<const MeterSection*> (&section)) != 0) {
		o << *((const Meter*) ms);
	}

	return o;
}
