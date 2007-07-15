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

#include <vector>

#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <pbd/mountpoint.h>
#include <pbd/pathscanner.h>
#include <pbd/stl_delete.h>
#include <pbd/strsplit.h>

#include <glibmm/miscutils.h>

#include <ardour/smf_source.h>
#include <ardour/session.h>
#include <ardour/midi_ring_buffer.h>
#include <ardour/midi_util.h>
#include <ardour/tempo.h>
#include <ardour/audioengine.h>

#include "i18n.h"

using namespace ARDOUR;

string SMFSource::_search_path;

/*sigc::signal<void,struct tm*, time_t> SMFSource::HeaderPositionOffsetChanged;
bool                                  SMFSource::header_position_negative;
uint64_t                              SMFSource::header_position_offset;
*/

SMFSource::SMFSource (Session& s, std::string path, Flag flags)
	: MidiSource (s, region_name_from_path(path, false))
	, _channel(0)
	, _flags (Flag(flags | Writable)) // FIXME: this needs to be writable for now
	, _allow_remove_if_empty(true)
	, _timeline_position (0)
	, _fd (0)
	, _last_ev_time(0)
	, _track_size(4) // 4 bytes for the ever-present EOT event
	, _header_size(22)
{
	/* constructor used for new internal-to-session files. file cannot exist */

	if (init (path, false)) {
		throw failed_constructor ();
	}
	
	if (open()) {
		throw failed_constructor ();
	}
	
	assert(_name.find("/") == string::npos);
}

SMFSource::SMFSource (Session& s, const XMLNode& node)
	: MidiSource (s, node)
	, _channel(0)
	, _flags (Flag (Writable|CanRename))
	, _allow_remove_if_empty(true)
	, _timeline_position (0)
	, _fd (0)
	, _last_ev_time(0)
	, _track_size(4) // 4 bytes for the ever-present EOT event
	, _header_size(22)
{
	/* constructor used for existing internal-to-session files. file must exist */

	if (set_state (node)) {
		throw failed_constructor ();
	}
	
	if (init (_name, true)) {
		throw failed_constructor ();
	}
	
	if (open()) {
		throw failed_constructor ();
	}
	
	assert(_name.find("/") == string::npos);
}

SMFSource::~SMFSource ()
{
	if (removable()) {
		unlink (_path.c_str());
	}
}

bool
SMFSource::removable () const
{
	return (_flags & Removable) && ((_flags & RemoveAtDestroy) || 
				      ((_flags & RemovableIfEmpty) && is_empty (_path)));
}

int
SMFSource::init (string pathstr, bool must_exist)
{
	bool is_new = false;

	if (!find (pathstr, must_exist, is_new)) {
		cerr << "cannot find " << pathstr << " with me = " << must_exist << endl;
		return -1;
	}

	if (is_new && must_exist) {
		return -1;
	}

	assert(_name.find("/") == string::npos);
	return 0;
}

int
SMFSource::open()
{
	//cerr << "Opening SMF file " << path() << " writeable: " << writable() << endl;

	assert(writable()); // FIXME;

	_fd = fopen(path().c_str(), "r+");

	// File already exists
	if (_fd) {
		fseek(_fd, _header_size - 4, 0);
		uint32_t track_size_be = 0;
		fread(&track_size_be, 4, 1, _fd);
		_track_size = GUINT32_FROM_BE(track_size_be);
		//cerr << "SMF - read track size " << _track_size << endl;

	// We're making a new file
	} else {
		_fd = fopen(path().c_str(), "w+");
		_track_size = 0;

		// write a tentative header just to pad things out so writing happens in the right spot
		flush_header();
		// FIXME: write the footer here too so it's a valid SMF (screw up writing ATM though)
	}

	return (_fd == 0) ? -1 : 0;
}

int
SMFSource::update_header (nframes_t when, struct tm&, time_t)
{
	_timeline_position = when;
	return flush_header();
}

int
SMFSource::flush_header ()
{
	// FIXME: write timeline position somehow?
	
	cerr << "SMF Flushing header\n";

	assert(_fd);

	const uint16_t type     = GUINT16_TO_BE(0);     // SMF Type 0 (single track)
	const uint16_t ntracks  = GUINT16_TO_BE(1);     // Number of tracks (always 1 for Type 0)
	const uint16_t division = GUINT16_TO_BE(_ppqn); // Pulses per beat

	char data[6];
	memcpy(data, &type, 2);
	memcpy(data+2, &ntracks, 2);
	memcpy(data+4, &division, 2);

	_fd = freopen(path().c_str(), "r+", _fd);
	assert(_fd);
	fseek(_fd, 0, 0);
	write_chunk("MThd", 6, data);
	//if (_track_size > 0) {
		write_chunk_header("MTrk", _track_size); 
	//}

	fflush(_fd);

	return 0;
}

int
SMFSource::flush_footer()
{
	cerr << "SMF - Writing EOT\n";

	fseek(_fd, 0, SEEK_END);
	write_var_len(1); // whatever...
	char eot[4] = { 0xFF, 0x2F, 0x00 }; // end-of-track meta-event
	fwrite(eot, 1, 4, _fd);
	fflush(_fd);
	return 0;
}

/** Returns the offset of the first event in the file with a time past @a start,
 * relative to the start of the source.
 *
 * Returns -1 if not found.
 */
/*
long
SMFSource::find_first_event_after(nframes_t start)
{
	// FIXME: obviously this is slooow
	
	fseek(_fd, _header_size, 0);

	while ( ! feof(_fd) ) {
		const uint32_t delta_time = read_var_len();

		if (delta_time > start)
			return delta_time;
	}

	return -1;
}
*/

/** Read an event from the current position in file.
 *
 * File position MUST be at the beginning of a delta time, or this will die very messily.
 * ev.buffer must be of size ev.size, and large enough for the event.  The returned event
 * will have it's time field set to it's delta time, in SMF tempo-based ticks, using the
 * rate given by ppqn() (it is the caller's responsibility to calculate a real time).
 *
 * Returns event length (including status byte) on success, 0 if event was
 * skipped (eg a meta event), or -1 on EOF (or end of track).
 */
int
SMFSource::read_event(jack_midi_event_t& ev) const
{
	// - 4 is for the EOT event, which we don't actually want to read
	//if (feof(_fd) || ftell(_fd) >= _header_size + _track_size - 4) {
	if (feof(_fd)) {
		return -1;
	}

	uint32_t delta_time = read_var_len();
	assert(!feof(_fd));
	int status = fgetc(_fd);
	assert(status != EOF); // FIXME die gracefully
	if (status == 0xFF) {
		assert(!feof(_fd));
		int type = fgetc(_fd);
		if ((unsigned char)type == 0x2F) {
			//cerr << "SMF - hit EOT" << endl;
			return -1; // we hit the logical EOF anyway...
		} else {
			ev.size = 0;
			ev.time = delta_time; // this is needed regardless
			return 0;
		}
	}
	
	ev.time = delta_time;
	ev.size = midi_event_size((unsigned char)status) + 1;

	if (ev.buffer == NULL)
		ev.buffer = (Byte*)malloc(sizeof(Byte) * ev.size);

	ev.buffer[0] = (unsigned char)status;
	fread(ev.buffer+1, 1, ev.size - 1, _fd);

	/*printf("SMF - read event, delta = %u, size = %zu, data = ",
		delta_time, ev.size);
	for (size_t i=0; i < ev.size; ++i) {
		printf("%X ", ev.buffer[i]);
	}
	printf("\n");*/
	
	return ev.size;
}

/** All stamps in audio frames */
nframes_t
SMFSource::read_unlocked (MidiRingBuffer& dst, nframes_t start, nframes_t cnt, nframes_t stamp_offset) const
{
	//cerr << "SMF - read " << start << ", count=" << cnt << ", offset=" << stamp_offset << endl;

	// 64 bits ought to be enough for anybody
	uint64_t time = 0; // in SMF ticks, 1 tick per _ppqn

	_read_data_count = 0;

	// FIXME: ugh
	unsigned char ev_buf[MidiBuffer::max_event_size()];
	jack_midi_event_t ev; // time in SMF ticks
	ev.time = 0;
	ev.size = MidiBuffer::max_event_size();
	ev.buffer = ev_buf;

	// FIXME: don't seek to start every read
	fseek(_fd, _header_size, 0);
	
	// FIXME: assumes tempo never changes after start
	const double frames_per_beat = _session.tempo_map().tempo_at(_timeline_position).frames_per_beat(
			_session.engine().frame_rate());
	
	uint64_t start_ticks = (uint64_t)((start / frames_per_beat) * _ppqn);

	while (!feof(_fd)) {
		int ret = read_event(ev);
		if (ret == -1) { // EOF
			//cerr << "SMF - EOF\n";
			break;
		}

		if (ret == 0) { // meta-event (skipped)
			//cerr << "SMF - META\n";
			time += ev.time; // just accumulate delta time and ignore event
			continue;
		}

		time += ev.time; // accumulate delta time
		ev.time = time; // set ev.time to actual time (relative to source start)

		if (ev.time >= start_ticks) {
			if (ev.time < start_ticks + (cnt / frames_per_beat)) {
				break;
			} else {
				ev.time = (nframes_t)(((ev.time / (double)_ppqn) * frames_per_beat)) + stamp_offset;
				// write event time in absolute frames
				dst.write(ev.time, ev.size, ev.buffer);
			}
		}

		_read_data_count += ev.size;
	}
	
	return cnt;
}

/** All stamps in audio frames */
nframes_t
SMFSource::write_unlocked (MidiRingBuffer& src, nframes_t cnt)
{
	_write_data_count = 0;

	boost::shared_ptr<MidiBuffer> buf_ptr(new MidiBuffer(1024)); // FIXME: size?
	MidiBuffer& buf = *buf_ptr.get();
	src.read(buf, /*_length*/0, _length + cnt); // FIXME?

	fseek(_fd, 0, SEEK_END);

	// FIXME: assumes tempo never changes after start
	const double frames_per_beat = _session.tempo_map().tempo_at(_timeline_position).frames_per_beat(
			_session.engine().frame_rate());
	
	for (size_t i=0; i < buf.size(); ++i) {
		MidiEvent& ev = buf[i];
		assert(ev.time >= _timeline_position);
		ev.time -= _timeline_position;
		assert(ev.time >= _last_ev_time);
		const uint32_t delta_time = (uint32_t)((ev.time - _last_ev_time) / frames_per_beat * _ppqn);
		
		/*printf("SMF - writing event, delta = %u, size = %zu, data = ",
			delta_time, ev.size);
		for (size_t i=0; i < ev.size; ++i) {
			printf("%X ", ev.buffer[i]);
		}
		printf("\n");
		*/
		size_t stamp_size = write_var_len(delta_time);
		fwrite(ev.buffer, 1, ev.size, _fd);

		_track_size += stamp_size + ev.size;
		_write_data_count += ev.size;
		
		_last_ev_time = ev.time;
	}

	fflush(_fd);

	const nframes_t oldlen = _length;
	update_length(oldlen, cnt);

	_model->append(buf);

	ViewDataRangeReady (buf_ptr, oldlen, cnt); /* EMIT SIGNAL */
	
	return cnt;
}

XMLNode&
SMFSource::get_state ()
{
	XMLNode& root (MidiSource::get_state());
	char buf[16];
	snprintf (buf, sizeof (buf), "0x%x", (int)_flags);
	root.add_property ("flags", buf);
	return root;
}

int
SMFSource::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if (MidiSource::set_state (node)) {
		return -1;
	}

	if ((prop = node.property (X_("flags"))) != 0) {

		int ival;
		sscanf (prop->value().c_str(), "0x%x", &ival);
		_flags = Flag (ival);

	} else {

		_flags = Flag (0);

	}

	assert(_name.find("/") == string::npos);

	return 0;
}

void
SMFSource::mark_for_remove ()
{
	if (!writable()) {
		return;
	}
	_flags = Flag (_flags | RemoveAtDestroy);
}

void
SMFSource::mark_streaming_write_completed ()
{
	if (!writable()) {
		return;
	}
	
	flush_footer();

#if 0
	Glib::Mutex::Lock lm (_lock);


	next_peak_clear_should_notify = true;

	if (_peaks_built || pending_peak_builds.empty()) {
		_peaks_built = true;
		 PeaksReady (); /* EMIT SIGNAL */
	}
#endif
}

void
SMFSource::mark_take (string id)
{
	if (writable()) {
		_take_id = id;
	}
}

int
SMFSource::move_to_trash (const string trash_dir_name)
{
	string newpath;

	if (!writable()) {
		return -1;
	}

	/* don't move the file across filesystems, just
	   stick it in the 'trash_dir_name' directory
	   on whichever filesystem it was already on.
	*/

	newpath = Glib::path_get_dirname (_path);
	newpath = Glib::path_get_dirname (newpath);

	newpath += '/';
	newpath += trash_dir_name;
	newpath += '/';
	newpath += Glib::path_get_basename (_path);

	if (access (newpath.c_str(), F_OK) == 0) {

		/* the new path already exists, try versioning */
		
		char buf[PATH_MAX+1];
		int version = 1;
		string newpath_v;

		snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), version);
		newpath_v = buf;

		while (access (newpath_v.c_str(), F_OK) == 0 && version < 999) {
			snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), ++version);
			newpath_v = buf;
		}
		
		if (version == 999) {
			PBD::error << string_compose (_("there are already 1000 files with names like %1; versioning discontinued"),
					  newpath)
			      << endmsg;
		} else {
			newpath = newpath_v;
		}

	} else {

		/* it doesn't exist, or we can't read it or something */

	}

	if (::rename (_path.c_str(), newpath.c_str()) != 0) {
		PBD::error << string_compose (_("cannot rename midi file source from %1 to %2 (%3)"),
				  _path, newpath, strerror (errno))
		      << endmsg;
		return -1;
	}
#if 0
	if (::unlink (peakpath.c_str()) != 0) {
		PBD::error << string_compose (_("cannot remove peakfile %1 for %2 (%3)"),
				  peakpath, _path, strerror (errno))
		      << endmsg;
		/* try to back out */
		rename (newpath.c_str(), _path.c_str());
		return -1;
	}
	    
	_path = newpath;
	peakpath = "";
#endif	
	/* file can not be removed twice, since the operation is not idempotent */

	_flags = Flag (_flags & ~(RemoveAtDestroy|Removable|RemovableIfEmpty));

	return 0;
}

// FIXME: Merge this with audiofilesource somehow (make a generic filesource?)
bool
SMFSource::find (string pathstr, bool must_exist, bool& isnew)
{
	string::size_type pos;
	bool ret = false;

	isnew = false;

	/* clean up PATH:CHANNEL notation so that we are looking for the correct path */

	if ((pos = pathstr.find_last_of (':')) == string::npos) {
		pathstr = pathstr;
	} else {
		pathstr = pathstr.substr (0, pos);
	}

	if (pathstr[0] != '/') {

		/* non-absolute pathname: find pathstr in search path */

		vector<string> dirs;
		int cnt;
		string fullpath;
		string keeppath;

		if (_search_path.length() == 0) {
			PBD::error << _("FileSource: search path not set") << endmsg;
			goto out;
		}

		split (_search_path, dirs, ':');

		cnt = 0;
		
		for (vector<string>::iterator i = dirs.begin(); i != dirs.end(); ++i) {

			fullpath = *i;
			if (fullpath[fullpath.length()-1] != '/') {
				fullpath += '/';
			}
			fullpath += pathstr;
			
			if (access (fullpath.c_str(), R_OK) == 0) {
				keeppath = fullpath;
				++cnt;
			} 
		}

		if (cnt > 1) {

			PBD::error << string_compose (_("FileSource: \"%1\" is ambigous when searching %2\n\t"), pathstr, _search_path) << endmsg;
			goto out;

		} else if (cnt == 0) {

			if (must_exist) {
				PBD::error << string_compose(_("Filesource: cannot find required file (%1): while searching %2"), pathstr, _search_path) << endmsg;
				goto out;
			} else {
				isnew = true;
			}
		}
		
		_name = pathstr;
		_path = keeppath;
		ret = true;

	} else {
		
		/* external files and/or very very old style sessions include full paths */
		
		_path = pathstr;
		_name = pathstr.substr (pathstr.find_last_of ('/') + 1);
		
		if (access (_path.c_str(), R_OK) != 0) {

			/* file does not exist or we cannot read it */

			if (must_exist) {
				PBD::error << string_compose(_("Filesource: cannot find required file (%1): %2"), _path, strerror (errno)) << endmsg;
				goto out;
			}
			
			if (errno != ENOENT) {
				PBD::error << string_compose(_("Filesource: cannot check for existing file (%1): %2"), _path, strerror (errno)) << endmsg;
				goto out;
			}
			
			/* a new file */

			isnew = true;
			ret = true;

		} else {
			
			/* already exists */

			ret = true;
		}
	}
	
  out:
	return ret;
}

void
SMFSource::set_search_path (string p)
{
	_search_path = p;
}


void
SMFSource::set_allow_remove_if_empty (bool yn)
{
	if (writable()) {
		_allow_remove_if_empty = yn;
	}
}

int
SMFSource::set_source_name (string newname, bool destructive)
{
	//Glib::Mutex::Lock lm (_lock); FIXME
	string oldpath = _path;
	string newpath = Session::change_midi_path_by_name (oldpath, _name, newname, destructive);

	if (newpath.empty()) {
		PBD::error << string_compose (_("programming error: %1"), "cannot generate a changed midi path") << endmsg;
		return -1;
	}

	if (rename (oldpath.c_str(), newpath.c_str()) != 0) {
		PBD::error << string_compose (_("cannot rename midi file for %1 to %2"), _name, newpath) << endmsg;
		return -1;
	}

	_name = Glib::path_get_basename (newpath);
	_path = newpath;

	return 0;//rename_peakfile (peak_path (_path));
}

bool
SMFSource::is_empty (string path)
{
	/* XXX fix me */

	return false;
}


void
SMFSource::write_chunk_header(char id[4], uint32_t length)
{
	const uint32_t length_be = GUINT32_TO_BE(length);

	fwrite(id, 1, 4, _fd);
	fwrite(&length_be, 4, 1, _fd);
}

void
SMFSource::write_chunk(char id[4], uint32_t length, void* data)
{
	write_chunk_header(id, length);
	
	fwrite(data, 1, length, _fd);
}

/** Returns the size (in bytes) of the value written. */
size_t
SMFSource::write_var_len(uint32_t value)
{
	size_t ret = 0;

	uint32_t buffer = value & 0x7F;

	while ( (value >>= 7) ) {
		buffer <<= 8;
		buffer |= ((value & 0x7F) | 0x80);
	}

	while (true) {
		//printf("Writing var len byte %X\n", (unsigned char)buffer);
		++ret;
		fputc(buffer, _fd);
		if (buffer & 0x80)
			buffer >>= 8;
		else
			break;
	}

	return ret;
}

uint32_t
SMFSource::read_var_len() const
{
	assert(!feof(_fd));

	//int offset = ftell(_fd);
	//cerr << "SMF - reading var len at " << offset << endl;

	uint32_t value;
	unsigned char c;

	if ( (value = getc(_fd)) & 0x80 ) {
		value &= 0x7F;
		do {
			assert(!feof(_fd));
			value = (value << 7) + ((c = getc(_fd)) & 0x7F);
		} while (c & 0x80);
	}

	return value;
}

void
SMFSource::load_model(bool lock, bool force_reload)
{
	if (lock)
		Glib::Mutex::Lock lm (_lock);

	if (_model && _model_loaded && ! force_reload) {
		assert(_model);
		return;
	}

	if (! _model)
		_model = new MidiModel();

	_model->start_write();

	fseek(_fd, _header_size, 0);

	uint64_t time = 0; /* in SMF ticks */
	jack_midi_event_t ev;
	ev.time = 0;
	ev.size = 0;
	ev.buffer = NULL;
	
	// FIXME: assumes tempo never changes after start
	const double frames_per_beat = _session.tempo_map().tempo_at(_timeline_position).frames_per_beat(
			_session.engine().frame_rate());
	
	int ret;
	while ((ret = read_event(ev)) >= 0) {
		time += ev.time;
		
		const double ev_time = (double)(time * frames_per_beat / (double)_ppqn); // in frames

		if (ret > 0) { // didn't skip (meta) event
			//cerr << "ADDING EVENT TO MODEL: " << ev.time << endl;
			_model->append(ev_time, ev.size, ev.buffer);
		}
	}
	
	_model->end_write(false); /* FIXME: delete stuck notes iff percussion? */

	_model_loaded = true;
}


void
SMFSource::destroy_model()
{
	delete _model;
	_model = NULL;
}

