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
#include <sigc++/signal.h>
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

	/** Read the data in a given time range from the MIDI source.
	 * All time stamps in parameters are in audio frames (even if the source has tempo time).
	 * \param dst Ring buffer where read events are written
	 * \param source_start Start position of the SOURCE in this read context
	 * \param start Start of range to be read
	 * \param cnt Length of range to be read (in audio frames)
	 * \param stamp_offset Offset to add to event times written to dst
	 * \param negative_stamp_offset Offset to subtract from event times written to dst
	 * \param tracker an optional pointer to MidiStateTracker object, for note on/off tracking
	 */
	virtual nframes_t midi_read (MidiRingBuffer<nframes_t>& dst,
				     sframes_t source_start,
				     sframes_t start, nframes_t cnt,
				     sframes_t stamp_offset, sframes_t negative_stamp_offset, MidiStateTracker*) const;

	virtual nframes_t midi_write (MidiRingBuffer<nframes_t>& src,
			sframes_t source_start,
			nframes_t cnt);

	virtual void append_event_unlocked_beats(const Evoral::Event<Evoral::MusicalTime>& ev) = 0;

	virtual void append_event_unlocked_frames(const Evoral::Event<nframes_t>& ev,
			sframes_t source_start) = 0;

	virtual sframes_t length (sframes_t pos) const;
	virtual void      update_length (sframes_t pos, sframes_t cnt);

	virtual void mark_streaming_midi_write_started (NoteMode mode, sframes_t start_time);
	virtual void mark_streaming_write_started ();
	virtual void mark_streaming_write_completed ();

	virtual void session_saved();

	std::string captured_for() const               { return _captured_for; }
	void        set_captured_for (std::string str) { _captured_for = str; }

	uint32_t read_data_count()  const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

	static sigc::signal<void,MidiSource*> MidiSourceCreated;

	// Signal a range of recorded data is available for reading from model()
	mutable sigc::signal<void,sframes_t,nframes_t> ViewDataRangeReady;

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
	void set_model(boost::shared_ptr<MidiModel> m) { _model = m; }
	void drop_model() { _model.reset(); }

  protected:
	virtual void flush_midi() = 0;

	virtual nframes_t read_unlocked (MidiRingBuffer<nframes_t>& dst,
					 sframes_t position,
					 sframes_t start, nframes_t cnt,
					 sframes_t stamp_offset, sframes_t negative_stamp_offset,
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

	mutable double    _length_beats;
	mutable sframes_t _last_read_end;
	sframes_t         _last_write_end;

  private:
	bool file_changed (std::string path);
};

}

#endif /* __ardour_midi_source_h__ */
