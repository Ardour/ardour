/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018-2019 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_midi_source_h__
#define __ardour_midi_source_h__

#include <string>
#include <time.h>
#include <glibmm/threads.h>
#include <boost/enable_shared_from_this.hpp>

#include "pbd/stateful.h"
#include "pbd/xml++.h"

#include "evoral/Sequence.h"

#include "temporal/range.h"

#include "ardour/ardour.h"
#include "ardour/buffer.h"
#include "ardour/midi_cursor.h"
#include "ardour/source.h"
#include "ardour/beats_samples_converter.h"

namespace ARDOUR {

class MidiChannelFilter;
class MidiModel;
class MidiStateTracker;

template<typename T> class MidiRingBuffer;

/** Source for MIDI data */
class LIBARDOUR_API MidiSource : virtual public Source
{
  public:
	typedef Temporal::Beats TimeType;

	MidiSource (Session& session, std::string name, Source::Flag flags = Source::Flag(0));
	MidiSource (Session& session, const XMLNode&);
	virtual ~MidiSource ();

	/** Write the data in the given time range to another MidiSource
	 * @param lock Reference to the Mutex to lock before modification
	 * @param newsrc MidiSource to which data will be written. Should be a
	 *        new, empty source. If it already has contents, the results are
	 *        undefined. Source must be writable.
	 * @param begin time of earliest event that can be written.
	 * @param end time of latest event that can be written.
	 * @return zero on success, non-zero if the write failed for any reason.
	 */
	int write_to (const Lock&                   lock,
	              boost::shared_ptr<MidiSource> newsrc,
	              Temporal::Beats               begin = Temporal::Beats(),
	              Temporal::Beats               end   = std::numeric_limits<Temporal::Beats>::max());

	/** Export the midi data in the given time range to another MidiSource
	 * @param lock Reference to the Mutex to lock before modification
	 * @param newsrc MidiSource to which data will be written. Should be a
	 *        new, empty source. If it already has contents, the results are
	 *        undefined. Source must be writable.
	 * @param begin time of earliest event that can be written.
	 * @param end time of latest event that can be written.
	 * @return zero on success, non-zero if the write failed for any reason.
	 */
	int export_write_to (const Lock&                   lock,
	                     boost::shared_ptr<MidiSource> newsrc,
	                     Temporal::Beats               begin,
	                     Temporal::Beats               end);

	/** Read the data in a given time range from the MIDI source.
	 * All time stamps in parameters are in audio samples (even if the source has tempo time).
	 * @param lock Reference to the Mutex to lock before modification
	 * @param dst Ring buffer where read events are written.
	 * @param source_start Start position of the SOURCE in this read context.
	 * @param start Start of range to be read.
	 * @param cnt Length of range to be read (in audio samples).
	 * @param loop_range If non-null, all event times will be mapped into this loop range.
	 * @param cursor Cached iterator to start copying events
	 * @param filter Channel filter to apply or NULL to disable filter
	 * @param tracker an optional pointer to MidiStateTracker object, for note on/off tracking.
	 * @param filtered Parameters whose MIDI messages will not be returned.
	 */
	virtual timecnt_t midi_read (const Lock&                       lock,
	                             Evoral::EventSink<samplepos_t>&    dst,
	                             timepos_t const &                  source_start,
	                             timecnt_t const &                  start,
	                             timecnt_t const &                  cnt,
	                             Temporal::Range*                   loop_range,
	                             MidiCursor&                        cursor,
	                             MidiStateTracker*                  tracker,
	                             MidiChannelFilter*                 filter,
	                             const std::set<Evoral::Parameter>& filtered);

	/** Write data from a MidiRingBuffer to this source.
	 * @param lock Reference to the Mutex to lock before modification
	 * @param source Source to read from.
	 * @param source_start This source's start position in session samples.
	 * @param cnt The length of time to write.
	 */
	virtual timecnt_t midi_write (const Lock&                  lock,
	                                MidiRingBuffer<samplepos_t>& source,
	                                timepos_t const &            source_start,
	                                timecnt_t const &            cnt);

	/** Append a single event with a timestamp in beats.
	 *
	 * Caller must ensure that the event is later than the last written event.
	 */
	virtual void append_event_beats(const Lock&                           lock,
	                                const Evoral::Event<Temporal::Beats>& ev) = 0;

	/** Append a single event with a timestamp in samples.
	 *
	 * Caller must ensure that the event is later than the last written event.
	 */
	virtual void append_event_samples(const Lock&                      lock,
	                                 const Evoral::Event<samplepos_t>& ev,
	                                 samplepos_t                       source_start) = 0;

	virtual void mark_streaming_midi_write_started (const Lock& lock, NoteMode mode);
	virtual void mark_streaming_write_started (const Lock& lock);
	virtual void mark_streaming_write_completed (const Lock& lock);

	/** Mark write starting with the given time parameters.
	 *
	 * This is called by MidiDiskStream::process before writing to the capture
	 * buffer which will be later read by midi_read().
	 *
	 * @param position The timeline position the source now starts at.
	 * @param capture_length The current length of the capture, which may not
	 * be zero if record is armed while rolling.
	 * @param loop_length The loop length if looping, otherwise zero.
	 */
	void mark_write_starting_now (samplecnt_t position,
	                              samplecnt_t capture_length,
	                              samplecnt_t loop_length);

	/* like ::mark_streaming_write_completed() but with more arguments to
	 * allow control over MIDI-specific behaviour. Expected to be used only
	 * when recording actual MIDI input, rather then when importing files
	 * etc.
	 */
	virtual void mark_midi_streaming_write_completed (
		const Lock&                                        lock,
		Evoral::Sequence<Temporal::Beats>::StuckNoteOption stuck_option,
		Temporal::Beats                                    when = Temporal::Beats());

	virtual void session_saved();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool length_mutable() const { return true; }

	void     set_length_beats(TimeType l) { _length_beats = l; }
	TimeType length_beats() const         { return _length_beats; }

	virtual void load_model(const Glib::Threads::Mutex::Lock& lock, bool force_reload=false) = 0;
	virtual void destroy_model(const Glib::Threads::Mutex::Lock& lock) = 0;

	/** Reset cached information (like iterators) when things have changed.
	 * @param lock Source lock, which must be held by caller.
	 */
	void invalidate(const Glib::Threads::Mutex::Lock& lock);

	/** Thou shalt not emit this directly, use invalidate() instead. */
	mutable PBD::Signal1<void, bool> Invalidated;

	void set_note_mode(const Glib::Threads::Mutex::Lock& lock, NoteMode mode);

	boost::shared_ptr<MidiModel> model() { return _model; }
	void set_model(const Glib::Threads::Mutex::Lock& lock, boost::shared_ptr<MidiModel>);
	void drop_model(const Glib::Threads::Mutex::Lock& lock);

	Evoral::ControlList::InterpolationStyle interpolation_of (Evoral::Parameter) const;
	void set_interpolation_of (Evoral::Parameter, Evoral::ControlList::InterpolationStyle);
	void copy_interpolation_from (boost::shared_ptr<MidiSource>);
	void copy_interpolation_from (MidiSource *);

	AutoState automation_state_of (Evoral::Parameter) const;
	void set_automation_state_of (Evoral::Parameter, AutoState);
	void copy_automation_state_from (boost::shared_ptr<MidiSource>);
	void copy_automation_state_from (MidiSource *);

	/** Emitted when a different MidiModel is set */
	PBD::Signal0<void> ModelChanged;
	/** Emitted when a parameter's interpolation style is changed */
	PBD::Signal2<void, Evoral::Parameter, Evoral::ControlList::InterpolationStyle> InterpolationChanged;
	/** Emitted when a parameter's automation state is changed */
	PBD::Signal2<void, Evoral::Parameter, AutoState> AutomationStateChanged;

  protected:
	virtual void flush_midi(const Lock& lock) = 0;

	virtual timecnt_t read_unlocked (const Lock&                     lock,
	                                 Evoral::EventSink<samplepos_t>& dst,
	                                 timepos_t const &               position,
	                                 timecnt_t const &               start,
	                                 timecnt_t const &               cnt,
	                                 Temporal::Range*                loop_range,
	                                 MidiStateTracker*               tracker,
	                                 MidiChannelFilter*              filter) const = 0;

	/** Write data to this source from a MidiRingBuffer.
	 * @param lock Reference to the Mutex to lock before modification
	 * @param source Buffer to read from.
	 * @param position This source's start position in session samples.
	 * @param cnt The duration of this block to write for.
	 */
	virtual timecnt_t write_unlocked (const Lock&                 lock,
	                                  MidiRingBuffer<samplepos_t>& source,
	                                  timepos_t const &            position,
	                                  timecnt_t const &            cnt) = 0;

	boost::shared_ptr<MidiModel> _model;
	bool                         _writing;

	Temporal::Beats _length_beats;

	/** The total duration of the current capture. */
	samplepos_t _capture_length;

	/** Length of transport loop during current capture, or zero. */
	samplepos_t _capture_loop_length;

	/** Map of interpolation styles to use for Parameters; if they are not in this map,
	 *  the correct interpolation style can be obtained from EventTypeMap::interpolation_of ()
	 */
	typedef std::map<Evoral::Parameter, Evoral::ControlList::InterpolationStyle> InterpolationStyleMap;
	InterpolationStyleMap _interpolation_style;

	/** Map of automation states to use for Parameters; if they are not in this map,
	 *  the correct automation state is Off.
	 */
	typedef std::map<Evoral::Parameter, AutoState> AutomationStateMap;
	AutomationStateMap  _automation_state;
};

}

#endif /* __ardour_midi_source_h__ */
