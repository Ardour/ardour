/*
    Copyright (C) 2006 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_midi_source_h__
#define __ardour_midi_source_h__

#include <string>
#include <time.h>
#include <glibmm/threads.h>
#include <boost/enable_shared_from_this.hpp>
#include "pbd/stateful.h"
#include "pbd/xml++.h"
#include "evoral/Sequence.hpp"
#include "ardour/ardour.h"
#include "ardour/buffer.h"
#include "ardour/source.h"
#include "ardour/beats_frames_converter.h"

namespace ARDOUR {

class MidiStateTracker;
class MidiModel;
template<typename T> class MidiRingBuffer;

/** Source for MIDI data */
class MidiSource : virtual public Source, public boost::enable_shared_from_this<MidiSource>
{
  public:
	typedef double TimeType;

	MidiSource (Session& session, std::string name, Source::Flag flags = Source::Flag(0));
	MidiSource (Session& session, const XMLNode&);
	virtual ~MidiSource ();

	/** Write the data in the given time range to another MidiSource
	 * \param newsrc MidiSource to which data will be written. Should be a
	 *        new, empty source. If it already has contents, the results are
	 *        undefined. Source must be writable.
	 *
	 * \param begin time of earliest event that can be written.
	 * \param end time of latest event that can be written.
	 *
	 * Returns zero on success, non-zero if the write failed for any
	 * reason.
	 *
	 */
	int write_to (boost::shared_ptr<MidiSource> newsrc,
		      Evoral::MusicalTime begin = Evoral::MinMusicalTime,
		      Evoral::MusicalTime end = Evoral::MaxMusicalTime);

	/** Read the data in a given time range from the MIDI source.
	 * All time stamps in parameters are in audio frames (even if the source has tempo time).
	 * \param dst Ring buffer where read events are written
	 * \param source_start Start position of the SOURCE in this read context
	 * \param start Start of range to be read
	 * \param cnt Length of range to be read (in audio frames)
	 * \param tracker an optional pointer to MidiStateTracker object, for note on/off tracking
	 */
	virtual framecnt_t midi_read (Evoral::EventSink<framepos_t>& dst,
	                              framepos_t                     source_start,
	                              framepos_t                     start,
	                              framecnt_t                     cnt,
	                              MidiStateTracker*              tracker,
	                              std::set<Evoral::Parameter> const &) const;

	/** Write data from a MidiRingBuffer to this source.
	 *  @param source Source to read from.
	 *  @param source_start This source's start position in session frames.
	 *  @param cnt The length of time to write.
	 */
	virtual framecnt_t midi_write (MidiRingBuffer<framepos_t>& src,
	                               framepos_t                  source_start,
	                               framecnt_t                  cnt);

	virtual void append_event_unlocked_beats(const Evoral::Event<Evoral::MusicalTime>& ev) = 0;

	virtual void append_event_unlocked_frames(const Evoral::Event<framepos_t>& ev,
	                                          framepos_t                       source_start) = 0;

	virtual bool       empty () const;
	virtual framecnt_t length (framepos_t pos) const;
	virtual void       update_length (framecnt_t);

	virtual void mark_streaming_midi_write_started (NoteMode mode);
	virtual void mark_streaming_write_started ();
	virtual void mark_streaming_write_completed ();

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
	void mark_write_starting_now (framecnt_t position,
	                              framecnt_t capture_length,
	                              framecnt_t loop_length);

	/* like ::mark_streaming_write_completed() but with more arguments to
	 * allow control over MIDI-specific behaviour. Expected to be used only
	 * when recording actual MIDI input, rather then when importing files
	 * etc.
	 */
	virtual void mark_midi_streaming_write_completed (
		Evoral::Sequence<Evoral::MusicalTime>::StuckNoteOption stuck_option,
		Evoral::MusicalTime                                    when = 0);

	virtual void session_saved();

	std::string captured_for() const               { return _captured_for; }
	void        set_captured_for (std::string str) { _captured_for = str; }

	static PBD::Signal1<void,MidiSource*> MidiSourceCreated;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool length_mutable() const { return true; }

	void set_length_beats(double l) { _length_beats = l; }
	double length_beats() const { return _length_beats; }

	virtual void load_model(bool lock=true, bool force_reload=false) = 0;
	virtual void destroy_model() = 0;

	/** This must be called with the source lock held whenever the
	 *  source/model contents have been changed (reset iterators/cache/etc).
	 */
	void invalidate();

	void set_note_mode(NoteMode mode);

	boost::shared_ptr<MidiModel> model() { return _model; }
	void set_model (boost::shared_ptr<MidiModel>);
	void drop_model();

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
	virtual void flush_midi() = 0;

	virtual framecnt_t read_unlocked (Evoral::EventSink<framepos_t>& dst,
	                                  framepos_t                     position,
	                                  framepos_t                     start,
	                                  framecnt_t                     cnt,
	                                  MidiStateTracker*              tracker) const = 0;

	/** Write data to this source from a MidiRingBuffer.
	 *  @param source Buffer to read from.
	 *  @param position This source's start position in session frames.
	 *  @param cnt The duration of this block to write for.
	 */
	virtual framecnt_t write_unlocked (MidiRingBuffer<framepos_t>& source,
	                                   framepos_t                  position,
	                                   framecnt_t                  cnt) = 0;

	std::string _captured_for;

	boost::shared_ptr<MidiModel> _model;
	bool                         _writing;

	mutable Evoral::Sequence<Evoral::MusicalTime>::const_iterator _model_iter;
	mutable bool                                                  _model_iter_valid;

	mutable double     _length_beats;
	mutable framepos_t _last_read_end;

	/** The total duration of the current capture. */
	framepos_t _capture_length;

	/** Length of transport loop during current capture, or zero. */
	framepos_t _capture_loop_length;

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
