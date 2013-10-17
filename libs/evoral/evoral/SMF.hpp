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

#include <cassert>

#include "evoral/visibility.h"
#include "evoral/types.hpp"

struct smf_struct;
struct smf_track_struct;
typedef smf_struct smf_t;
typedef smf_track_struct smf_track_t;

namespace Evoral {

#define THROW_FILE_ERROR throw(FileError)

/** Standard Midi File.
 * Currently only tempo-based time of a given PPQN is supported.
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

	SMF() : _smf(0), _smf_track(0), _empty(true) {};
	virtual ~SMF();

	int  open(const std::string& path, int track=1) THROW_FILE_ERROR;
	int  create(const std::string& path, int track=1, uint16_t ppqn=19200) THROW_FILE_ERROR;
	void close() THROW_FILE_ERROR;

	const std::string& file_path() const { return _file_path; };

	void seek_to_start() const;
	int  seek_to_track(int track);

	int read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf, event_id_t* note_id) const;

	uint16_t num_tracks() const;
	uint16_t ppqn()       const;
	bool     is_empty()   const { return _empty; }

	void begin_write();
	void append_event_delta(uint32_t delta_t, uint32_t size, const uint8_t* buf, event_id_t note_id);
	void end_write() THROW_FILE_ERROR;

	void flush() {};

	double round_to_file_precision (double val) const;

protected:
	void set_path (const std::string& p);

private:
	std::string  _file_path;
	smf_t*       _smf;
	smf_track_t* _smf_track;
	bool         _empty; ///< true iff file contains(non-empty) events
};

}; /* namespace Evoral */

#endif /* EVORAL_SMF_HPP */

