/* This file is part of Evoral.
 * Copyright(C) 2008 David Robillard <http://drobilla.net>
 * Copyright(C) 2000-2008 Paul Davis
 * Author: Hans Baier
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or(at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_SMF_HPP
#define EVORAL_SMF_HPP

#include <glibmm/threads.h>
#include <set>

#include "evoral/visibility.h"
#include "evoral/types.hpp"

struct smf_struct;
struct smf_track_struct;
struct smf_tempo_struct;
typedef smf_struct smf_t;
typedef smf_track_struct smf_track_t;
typedef smf_tempo_struct smf_tempo_t;

namespace Evoral {

#define THROW_FILE_ERROR throw(FileError)

/** Standard Midi File.
 * Currently only tempo-based time of a given PPQN is supported.
 *
 * For WRITING: this object specifically wraps a type0 file or a type1 file with only a
 * single track. It has no support at this time for a type1 file with multiple
 * tracks.
 *
 * For READING: this object can read a single arbitrary track from a type1
 * file, or the single track of a type0 file. It has no support at this time
 * for reading more than 1 track.
 */
class LIBEVORAL_API SMF {
public:
	class FileError : public std::exception {
	public:
		FileError (std::string const & n) : _file_name (n) {}
		~FileError () throw () {}
		const char* what() const throw() { return "Unknown SMF error"; }
		std::string file_name () const { return _file_name; }
	private:
		std::string _file_name;
	};

	SMF();
	virtual ~SMF();

	static bool test(const std::string& path);
	int  open(const std::string& path, int track=1) THROW_FILE_ERROR;
	// XXX 19200 = 10 * Timecode::BBT_Time::ticks_per_beat
	int  create(const std::string& path, int track=1, uint16_t ppqn=19200) THROW_FILE_ERROR;
	void close() THROW_FILE_ERROR;

	void seek_to_start() const;
	int  seek_to_track(int track);

	int read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf, event_id_t* note_id) const;

	uint16_t num_tracks() const;
	uint16_t ppqn()       const;
	bool     is_empty()   const { return _empty; }

	void begin_write();
	void append_event_delta(uint32_t delta_t, uint32_t size, const uint8_t* buf, event_id_t note_id);
	void end_write(std::string const &) THROW_FILE_ERROR;

	void flush() {};

	double round_to_file_precision (double val) const;

	bool is_type0 () const { return _type0; }
	std::set<uint8_t> channels () const { return _type0channels; }
	void track_names (std::vector<std::string>&) const;
	void instrument_names (std::vector<std::string>&) const;

	int num_tempos () const;

	/* This is exactly modelled on smf_tempo_t */
	struct Tempo {
		size_t time_pulses;
		double time_seconds;
		int    microseconds_per_quarter_note;
		int    numerator;
		int    denominator;
		int    clocks_per_click;
		int    notes_per_note;

		Tempo ()
			: time_pulses (0)
			, time_seconds (0)
			, microseconds_per_quarter_note (-1)
			, numerator (-1)
			, denominator (-1)
			, clocks_per_click (-1)
			, notes_per_note (-1) {}
		Tempo (smf_tempo_t*);

		double tempo() const {
			return 60.0 * (1000000.0 / (double) microseconds_per_quarter_note);
		}
	};

	Tempo* tempo_at_smf_pulse (size_t smf_pulse) const;
	Tempo* tempo_at_seconds (double seconds) const;
	Tempo* nth_tempo (size_t n) const;

  private:
	smf_t*       _smf;
	smf_track_t* _smf_track;
	bool         _empty; ///< true iff file contains(non-empty) events
	mutable Glib::Threads::Mutex _smf_lock;

	bool              _type0;
	std::set<uint8_t> _type0channels;
};

}; /* namespace Evoral */

#endif /* EVORAL_SMF_HPP */
