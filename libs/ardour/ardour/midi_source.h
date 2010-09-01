/*
    Copyright (C) 2006 Paul Davis
	Written by Dave Robillard, 2006

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
#include <glibmm/thread.h>
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
class MidiSource : virtual public Source
{
  public:
	typedef double TimeType;

	MidiSource (Session& session, std::string name, Source::Flag flags = Source::Flag(0));
	MidiSource (Session& session, const XMLNode&);
	virtual ~MidiSource ();

        boost::shared_ptr<MidiSource> clone (Evoral::MusicalTime begin = Evoral::MinMusicalTime, 
                                             Evoral::MusicalTime end = Evoral::MaxMusicalTime);

	/** Read the data in a given time range from the MIDI source.
	 * All time stamps in parameters are in audio frames (even if the source has tempo time).
	 * \param dst Ring buffer where read events are written
	 * \param source_start Start position of the SOURCE in this read context
	 * \param start Start of range to be read
	 * \param cnt Length of range to be read (in audio frames)
	 * \param tracker an optional pointer to MidiStateTracker object, for note on/off tracking
	 */
	virtual nframes_t midi_read (Evoral::EventSink<nframes_t>& dst,
				     sframes_t source_start,
				     sframes_t start, nframes_t cnt,
				     MidiStateTracker*,
				     std::set<Evoral::Parameter> const &) const;

	virtual nframes_t midi_write (MidiRingBuffer<nframes_t>& src,
	                              sframes_t source_start,
	                              nframes_t cnt);

	virtual void append_event_unlocked_beats(const Evoral::Event<Evoral::MusicalTime>& ev) = 0;

	virtual void append_event_unlocked_frames(const Evoral::Event<nframes_t>& ev,
			sframes_t source_start) = 0;

	virtual bool       empty () const;
	virtual framecnt_t length (framepos_t pos) const;
	virtual void       update_length (framepos_t pos, framecnt_t cnt);

	virtual void mark_streaming_midi_write_started (NoteMode mode, sframes_t start_time);
	virtual void mark_streaming_write_started ();
	virtual void mark_streaming_write_completed ();

	virtual void session_saved();

	std::string captured_for() const               { return _captured_for; }
	void        set_captured_for (std::string str) { _captured_for = str; }

	uint32_t read_data_count()  const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

	static PBD::Signal1<void,MidiSource*> MidiSourceCreated;

	// Signal a range of recorded data is available for reading from model()
	mutable PBD::Signal2<void,sframes_t,nframes_t> ViewDataRangeReady;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool length_mutable() const { return true; }

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

	virtual nframes_t read_unlocked (Evoral::EventSink<nframes_t>& dst,
					 sframes_t position,
					 sframes_t start, nframes_t cnt,
					 MidiStateTracker* tracker) const = 0;

	virtual nframes_t write_unlocked (MidiRingBuffer<nframes_t>& dst,
			sframes_t position,
			nframes_t cnt) = 0;

	std::string      _captured_for;
	mutable uint32_t _read_data_count;  ///< modified in read()
	mutable uint32_t _write_data_count; ///< modified in write()

	boost::shared_ptr<MidiModel> _model;
	bool                         _writing;

	mutable Evoral::Sequence<Evoral::MusicalTime>::const_iterator _model_iter;
	mutable bool                                                  _model_iter_valid;

	mutable double    _length_beats;
	mutable sframes_t _last_read_end;
	sframes_t         _last_write_end;

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
