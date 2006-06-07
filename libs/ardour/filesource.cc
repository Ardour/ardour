/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <algorithm>

/* This is is very hacky way to get pread and pwrite declarations.
   First, include <features.h> so that we can avoid its #undef __USE_UNIX98.
   Then define __USE_UNIX98, include <unistd.h>, and then undef it
   again. If #define _XOPEN_SOURCE actually worked, I'd use that, but
   despite claims in the header that it does, it doesn't.

   features.h isn't available on osx and it compiles fine without it.
*/

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#if __GNUC__ >= 3
// #define _XOPEN_SOURCE 500
#include <unistd.h>
#else
#define __USE_UNIX98
#include <unistd.h>
#undef  __USE_UNIX98
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <climits>
#include <cerrno>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <sys/utsname.h>
#include <vector>
#include <cstdio> /* for rename(2) */

#include <glibmm.h>

#include <pbd/stl_delete.h>

#include <glibmm/thread.h>
#include <pbd/pathscanner.h>

#include <ardour/ardour.h>
#include <ardour/version.h>
#include <ardour/source.h>
#include <ardour/filesource.h>
#include <ardour/session.h>
#include <ardour/cycle_timer.h>
#include <ardour/pcm_utils.h>

#include "i18n.h"

using namespace ARDOUR;

string prepare_string(string& regex);

char   FileSource::bwf_country_code[3] = "us";
char   FileSource::bwf_organization_code[4] = "las";
char   FileSource::bwf_serial_number[13] = "000000000000";
string FileSource::search_path;

#undef WE_ARE_BIGENDIAN
#ifdef __BIG_ENDIAN__
#define WE_ARE_BIGENDIAN true
#else
#define WE_ARE_BIGENDIAN false
#endif

#define Swap_32(value)         \
	(((((uint32_t)value)<<24) & 0xFF000000) | \
	 ((((uint32_t)value)<< 8) & 0x00FF0000) | \
	 ((((uint32_t)value)>> 8) & 0x0000FF00) | \
	 ((((uint32_t)value)>>24) & 0x000000FF))

#define Swap_16(value)         \
	(((((uint16_t)value)>> 8) & 0x000000FF) | \
	 ((((uint16_t)value)<< 8) & 0x0000FF00))


void
FileSource::set_search_path (string p)
{
	search_path = p;
}

FileSource::FileSource (string pathstr, jack_nframes_t rate, bool repair_first, SampleFormat samp_format)
{
	/* constructor used when the file cannot already exist or might be damaged */
	_sample_format = samp_format;
	if (samp_format == FormatInt24) {
		_sample_size = 3;
	} else {
		_sample_size = sizeof(float);
	}
	
	if (repair_first && repair (pathstr, rate)) {
		throw failed_constructor ();
	}
	
	if (init (pathstr, false, rate)) {
		throw failed_constructor ();
	}

	SourceCreated (this); /* EMIT SIGNAL */
}

FileSource::FileSource (const XMLNode& node, jack_nframes_t rate) 
	: Source (node)
{
	_sample_format = FormatFloat;
	_sample_size = sizeof(float);

	if (set_state (node)) {
		throw failed_constructor();
	}

	/* constructor used when the file must already exist */

	if (init (_name, true, rate)) {
		throw failed_constructor ();
	}

	SourceCreated (this); /* EMIT SIGNAL */
}

int
FileSource::init (string pathstr, bool must_exist, jack_nframes_t rate)
{
	bool new_file = false;
	int ret = -1;
	PathScanner scanner;

	/* all native files end in .wav. this lets us discard
	   SndFileSource paths, which have ":N" at the end to
	   indicate which channel to read from, as well as any
	   other kind of non-native file. obviously, there
	   are more subtle checks later on.
	*/

	if (pathstr.length() < 4 || pathstr.rfind (".wav") != pathstr.length() - 4) {
		return ret;
	}

	is_bwf = false;
	_length = 0;
	fd = -1;
	remove_at_unref = false;
	next_peak_clear_should_notify = false;
	allow_remove_if_empty = true;

	if (pathstr[0] != '/') {

		/* find pathstr in search path */
		
		if (search_path.length() == 0) {
			error << _("FileSource: search path not set") << endmsg;
			goto out;
		}

		/* force exact match on the filename component by prefixing the regexp.
		   otherwise, "Drums-2.wav" matches "Comp_Drums-2.wav".
		*/

		string regexp = "^";
		regexp += prepare_string(pathstr);
		regexp += '$';

		vector<string*>* result = scanner (search_path, regexp, false, true, -1);
		
		if (result == 0 || result->size() == 0) {
			error << string_compose (_("FileSource: \"%1\" not found when searching %2 using %3"), 
					  pathstr, search_path, regexp) << endmsg;
			goto out;
		}
		
		if (result->size() > 1) {
			string msg = string_compose (_("FileSource: \"%1\" is ambigous when searching %2\n\t"), pathstr, search_path);
			vector<string*>::iterator x = result->begin();

			while (true) {
				msg += *(*x);
				++x;

				if (x == result->end()) {
					break;
				}

				msg += "\n\t";
			}
			
			error << msg << endmsg;
			goto out;
		}
		
		_name = pathstr;
		_path = *(result->front());

		vector_delete (result);
		delete result;

	} else {

		/* old style sessions include full paths */

		_path = pathstr;
		_name = pathstr.substr (pathstr.find_last_of ('/') + 1);

	}

	if (access (_path.c_str(), F_OK) != 0) {
		if (must_exist) {
			error << string_compose(_("Filesource: cannot find required file (%1): %2"), _path, strerror (errno)) << endmsg;
			goto out;
			
		}

		if (errno == ENOENT) {
			new_file = true;
		} else {
			error << string_compose(_("Filesource: cannot check for existing file (%1): %2"), _path, strerror (errno)) << endmsg;
			goto out;
		}
	}

	if ((fd = open64 (_path.c_str(), O_RDWR|O_CREAT, 0644)) < 0) {
		error << string_compose(_("FileSource: could not open \"%1\": (%2)"), _path, strerror (errno)) << endmsg;
		goto out;
	}
	
	/* if there was no timestamp available via XML,
	   then get it from the filesystem.
	*/

	if (_timestamp == 0) {
		struct stat statbuf;
		
		fstat (fd, &statbuf);
		_timestamp = statbuf.st_mtime;
	}

	if (lseek (fd, 0, SEEK_END) == 0) {
		new_file = true;
	}

	/* check that its a RIFF/WAVE format file */
	
	if (new_file) {

		switch (Config->get_native_file_header_format()) {
		case BWF:
			is_bwf = true;
			break;
		default:
			is_bwf = false;
			break;
		}

		if (fill_header (rate)) {
			error << string_compose (_("FileSource: cannot write header in %1"), _path) << endmsg;
			goto out;
		}

		struct tm* now;
		time_t     xnow;
		
		time (&xnow);
		now = localtime (&xnow);

		update_header (0, *now, xnow);
		
	} else {

		if (discover_chunks (must_exist)) {
			error << string_compose (_("FileSource: cannot locate chunks in %1"), _path) << endmsg;
			goto out;
		}
		
		if (read_header (must_exist)) {
			error << string_compose (_("FileSource: cannot read header in %1"), _path) << endmsg;
			goto out;
		}

		if (check_header (rate, must_exist)) {
			error << string_compose (_("FileSource: cannot check header in %1"), _path) << endmsg;
			goto out;
		}

		compute_header_size ();
	}
	
	if ((ret = initialize_peakfile (new_file, _path))) {
		error << string_compose (_("FileSource: cannot initialize peakfile for %1 as %2"), _path, peakpath) << endmsg;
	}

  out:
	if (ret) {

		if (fd >= 0) {
			close (fd);
		}

		if (new_file) {
			unlink (_path.c_str());
		}
	}

	return ret;

}

FileSource::~FileSource ()
{
	GoingAway (this); /* EMIT SIGNAL */
	
	if (fd >= 0) {

		if (remove_at_unref || (is_empty (_path) && allow_remove_if_empty)) {
			unlink (_path.c_str());
			unlink (peakpath.c_str());
		}

		close (fd);
	} 
}

void
FileSource::set_allow_remove_if_empty (bool yn)
{
	allow_remove_if_empty = yn;
}

int
FileSource::set_name (string newname, bool destructive)
{
	Glib::Mutex::Lock lm (_lock);
	string oldpath = _path;
	string newpath = Session::change_audio_path_by_name (oldpath, _name, newname, destructive);

	if (newpath.empty()) {
		error << string_compose (_("programming error: %1"), "cannot generate a changed audio path") << endmsg;
		return -1;
	}

	if (rename (oldpath.c_str(), newpath.c_str()) != 0) {
		error << string_compose (_("cannot rename audio file for %1 to %2"), _name, newpath) << endmsg;
		return -1;
	}

	_name = Glib::path_get_basename (newpath);
	_path = newpath;

	return rename_peakfile (peak_path (_path));
}

string
FileSource::peak_path (string audio_path)
{
	return Session::peak_path_from_audio_path (audio_path);
}

int
FileSource::discover_chunks (bool silent)
{
	WAVEChunk rw;
	off64_t end;
	off64_t offset;
	char null_terminated_id[5];
	bool doswap = false;
	
	if ((end = lseek (fd, 0, SEEK_END)) < 0) {
		error << _("FileSource: cannot seek to end of file") << endmsg;
		return -1;
	}

	if (::pread64 (fd, &rw, sizeof (rw), 0) != sizeof (rw)) {
		error << _("FileSource: cannot read RIFF/WAVE chunk from file") << endmsg;
		return -1;
	}

 	if (memcmp (rw.id, "RIFF", 4) == 0 && memcmp (rw.text, "WAVE", 4) == 0) {
 		header.bigendian = false;
 	}
 	else if (memcmp(rw.id, "RIFX", 4) == 0 && memcmp (rw.text, "WAVE", 4) == 0) {
 		header.bigendian = true;
 	}
 	else {
		if (!silent) {
			error << string_compose (_("FileSource %1: not a RIFF/WAVE file"), _path) << endmsg;
		}
		return -1;
	}

	null_terminated_id[4] = '\0';

	/* OK, its a RIFF/WAVE file. Find each chunk */

	doswap = header.bigendian != WE_ARE_BIGENDIAN;
	
	if (doswap) {
		swap_endian(rw);
	}

	
	
	memcpy (null_terminated_id, rw.id, 4);
	chunk_info.push_back (ChunkInfo (null_terminated_id, rw.size, 0));

	offset = sizeof (rw);

	while (offset < end) {

		GenericChunk this_chunk;
		
		if (::pread64 (fd, &this_chunk, sizeof (this_chunk), offset) != sizeof (this_chunk)) {
			error << _("FileSource: can't read a chunk") << endmsg;
			return -1;
		}

		if (doswap) {
			swap_endian(this_chunk);
		}

		memcpy (null_terminated_id, this_chunk.id, 4);

		/* do sanity check and possible correction to legacy ardour RIFF wavs
		   created on big endian platforms. after swapping, the size field will not be
		   in range for the fmt chunk
		*/
		if ((memcmp(null_terminated_id, "fmt ", 4) == 0 || memcmp(null_terminated_id, "bext", 4) == 0)
		     && !header.bigendian && (this_chunk.size > 700 || this_chunk.size < 0))
		{
			warning << _("filesource: correcting mis-written RIFF file to become a RIFX: ") << name() << endmsg;
			
			memcpy (&rw.id, "RIFX", 4);
			::pwrite64 (fd, &rw.id, 4, 0);
			header.bigendian = true;
			// fix wave chunk already read
			swap_endian(rw);
			
			doswap = header.bigendian != WE_ARE_BIGENDIAN;

			// now reset offset and continue the loop
			// to reread all the chunks
			chunk_info.clear();
			memcpy (null_terminated_id, rw.id, 4);
			chunk_info.push_back (ChunkInfo (null_terminated_id, rw.size, 0));
			offset = sizeof (rw);
			continue;
		}
				

		if (end != 44)
			if ((memcmp(null_terminated_id, "data", 4) == 0))
				if ((this_chunk.size == 0) || (this_chunk.size > (end - offset)))
					this_chunk.size = end - offset;
		
		chunk_info.push_back (ChunkInfo (null_terminated_id, this_chunk.size, offset));

		/* skip to the next chunk */

		offset += sizeof(GenericChunk) + this_chunk.size;
	}

	return 0;
}

void
FileSource::swap_endian (GenericChunk & chunk) const
{
	chunk.size = Swap_32(chunk.size);
}

void
FileSource::swap_endian (FMTChunk & chunk) const
{
	chunk.size = Swap_32(chunk.size);

	chunk.formatTag = Swap_16(chunk.formatTag);
	chunk.nChannels = Swap_16(chunk.nChannels);
	chunk.nSamplesPerSec = Swap_32(chunk.nSamplesPerSec);
	chunk.nAvgBytesPerSec = Swap_32(chunk.nAvgBytesPerSec);
	chunk.nBlockAlign = Swap_16(chunk.nBlockAlign);
	chunk.nBitsPerSample = Swap_16(chunk.nBitsPerSample);
}

void
FileSource::swap_endian (BroadcastChunk & chunk) const
{
	chunk.size = Swap_32(chunk.size);

 	chunk.time_reference_low = Swap_32(chunk.time_reference_low);
	chunk.time_reference_high = Swap_32(chunk.time_reference_high);
	chunk.version = Swap_16(chunk.version);
}

void FileSource::swap_endian (Sample *buf, jack_nframes_t cnt) const
{
	for (jack_nframes_t n=0; n < cnt; ++n) {
		uint32_t * tmp = (uint32_t *) &buf[n];
		*tmp = Swap_32(*tmp);
	}
}


FileSource::ChunkInfo*
FileSource::lookup_chunk (string what)
{
	for (vector<ChunkInfo>::iterator i = chunk_info.begin(); i != chunk_info.end(); ++i) {
		if ((*i).name == what) {
			return &*i;
		}
	}
	return 0;
}

int
FileSource::fill_header (jack_nframes_t rate)
{
	/* RIFF/WAVE */

	if (WE_ARE_BIGENDIAN) {
		memcpy (header.wave.id, "RIFX", 4);
		header.bigendian = true;
	}
	else {
		memcpy (header.wave.id, "RIFF", 4);
		header.bigendian = false;
	}
	header.wave.size = 0; /* file size */
	memcpy (header.wave.text, "WAVE", 4);

	/* BROADCAST WAVE EXTENSION */
	
	if (is_bwf) {

		/* fill the entire BWF header with nulls */

		memset (&header.bext, 0, sizeof (header.bext));

		memcpy (header.bext.id, "bext", 4);

		snprintf (header.bext.description, sizeof (header.bext.description), "%s", "ambiguity is clearer than precision.");

		struct passwd *pwinfo;
		struct utsname utsinfo;

		if ((pwinfo = getpwuid (getuid())) == 0) {
			error << string_compose(_("FileSource: cannot get user information for BWF header (%1)"), strerror(errno)) << endmsg;
			return -1;
		}
		if (uname (&utsinfo)) {
			error << string_compose(_("FileSource: cannot get host information for BWF header (%1)"), strerror(errno)) << endmsg;
			return -1;
		}

		snprintf (header.bext.originator, sizeof (header.bext.originator), "ardour:%s:%s:%s:%s:%s)", 
			  pwinfo->pw_gecos,
			  utsinfo.nodename,
			  utsinfo.sysname,
			  utsinfo.release,
			  utsinfo.version);

		header.bext.version = 1;  
		
		/* XXX do something about this field */

		snprintf (header.bext.umid, sizeof (header.bext.umid), "%s", "fnord");

		/* add some coding history */

		char buf[64];

		/* encode: PCM,rate,mono,24bit,ardour-version
		   
		   Note that because we use JACK, there is no way to tell
		   what the original bit depth of the signal was.
		 */
		
		snprintf (buf, sizeof(buf), "F=%u,A=PCM,M=mono,W=24,T=ardour-%d.%d.%d", 
			  rate,
			  libardour_major_version,
			  libardour_minor_version,
			  libardour_micro_version);

		header.coding_history.push_back (buf);

		/* initial size reflects coding history + "\r\n" */

		header.bext.size = sizeof (BroadcastChunk) - sizeof (GenericChunk) + strlen (buf) + 2;
	}
	
	memcpy (header.format.id, "fmt ", 4);
	header.format.size = sizeof (FMTChunk) - sizeof (GenericChunk);

	if (_sample_format == FormatInt24) {
		header.format.formatTag = 1; // PCM
		header.format.nBlockAlign = 3;
		header.format.nBitsPerSample = 24;
	}
	else {
		header.format.formatTag = 3; /* little-endian IEEE float format */
		header.format.nBlockAlign = 4;
		header.format.nBitsPerSample = 32;
	}
	header.format.nChannels = 1; /* mono */
	header.format.nSamplesPerSec = rate;
	header.format.nAvgBytesPerSec = rate * _sample_size;
	
	/* DATA */

	memcpy (header.data.id, "data", 4);
	header.data.size = 0;
	
	return 0;
}

void
FileSource::compute_header_size ()
{
	off64_t end_of_file;
	int32_t coding_history_size = 0;
		
	end_of_file = lseek (fd, 0, SEEK_END);

	if (is_bwf) {
		
		/* include the coding history */
		
		for (vector<string>::iterator i = header.coding_history.begin(); i != header.coding_history.end(); ++i) {
			coding_history_size += (*i).length() + 2; // include "\r\n";
		}
		
		header.bext.size = sizeof (BroadcastChunk) - sizeof (GenericChunk) + coding_history_size;
		data_offset = bwf_header_size + coding_history_size;
		
	} else {
		data_offset = wave_header_size;
	}

	if (end_of_file == 0) {

		/* newfile condition */
		
		if (is_bwf) {
			/* include "WAVE" then all the chunk sizes (bext, fmt, data) */	
			header.wave.size = 4 + sizeof (BroadcastChunk) + coding_history_size + sizeof (FMTChunk) + sizeof (GenericChunk);
		} else {
			/* include "WAVE" then all the chunk sizes (fmt, data) */
			header.wave.size = 4 + sizeof (FMTChunk) + sizeof (GenericChunk);
		}

		header.data.size = 0;

	} else {

		header.wave.size = end_of_file - 8; /* size of initial RIFF+size pseudo-chunk */
		header.data.size = end_of_file - data_offset;
	}
}

int
FileSource::update_header (jack_nframes_t when, struct tm& now, time_t tnow)
{
	Glib::Mutex::Lock lm (_lock);

	if (is_bwf) {
		/* random code is 9 digits */

		int random_code = random() % 999999999;

		snprintf (header.bext.originator_reference, sizeof (header.bext.originator_reference), "%2s%3s%12s%02d%02d%02d%9d",
			  bwf_country_code,
			  bwf_organization_code,
			  bwf_serial_number,
			  now.tm_hour,
			  now.tm_min,
			  now.tm_sec,
			  random_code);
			  
		snprintf (header.bext.origination_date, sizeof (header.bext.origination_date), "%4d-%02d-%02d",
			  1900 + now.tm_year,
			  now.tm_mon,
			  now.tm_mday);

		snprintf (header.bext.origination_time, sizeof (header.bext.origination_time), "%02d-%02d-%02d",
			  now.tm_hour,
			  now.tm_min,
			  now.tm_sec);

		header.bext.time_reference_high = 0;
		header.bext.time_reference_low = when;
	}

	compute_header_size ();

	if (write_header()) {
		error << string_compose(_("FileSource[%1]: cannot update data size: %2"), _path, strerror (errno)) << endmsg;
		return -1;
	}

	stamp (tnow);

	return 0;
}

int
FileSource::read_header (bool silent)
{
	/* we already have the chunk info, so just load up whatever we have */

	ChunkInfo* info;
	
	if (header.bigendian == false && (info = lookup_chunk ("RIFF")) == 0) {
		error << _("FileSource: can't find RIFF chunk info") << endmsg;
		return -1;
	}
	else if (header.bigendian == true && (info = lookup_chunk ("RIFX")) == 0) {
		error << _("FileSource: can't find RIFX chunk info") << endmsg;
		return -1;
	}
	
	
	/* just fill this chunk/header ourselves, disk i/o is stupid */

	if (header.bigendian) {
		memcpy (header.wave.id, "RIFX", 4);
	}
	else {
		memcpy (header.wave.id, "RIFF", 4);
	}
	header.wave.size = 0;
	memcpy (header.wave.text, "WAVE", 4);

	if ((info = lookup_chunk ("bext")) != 0) {

		is_bwf = true;
		
		if (::pread64 (fd, &header.bext, sizeof (header.bext), info->offset) != sizeof (header.bext)) {
			error << _("FileSource: can't read RIFF chunk") << endmsg;
			return -1;
		}

		if (read_broadcast_data (*info)) {
			return -1;
		}
	}

	if ((info = lookup_chunk ("fmt ")) == 0) {
		error << _("FileSource: can't find format chunk info") << endmsg;
		return -1;
	}

	if (::pread64 (fd, &header.format, sizeof (header.format), info->offset) != sizeof (header.format)) {
		error << _("FileSource: can't read format chunk") << endmsg;
		return -1;
	}

	if (header.bigendian != WE_ARE_BIGENDIAN) {
		swap_endian (header.format);
	}
	
	if ((info = lookup_chunk ("data")) == 0) {
		error << _("FileSource: can't find data chunk info") << endmsg;
		return -1;
	}

	if (::pread64 (fd, &header.data, sizeof (header.data), info->offset) != sizeof (header.data)) {
		error << _("FileSource: can't read data chunk") << endmsg;
		return -1;
	}

	if (header.bigendian != WE_ARE_BIGENDIAN) {
		swap_endian (header.data);
	}

	return 0;
}

int
FileSource::read_broadcast_data (ChunkInfo& info)
{
	int32_t coding_history_size;

	if (::pread64 (fd, (char *) &header.bext, sizeof (header.bext), info.offset + sizeof (GenericChunk)) != sizeof (header.bext)) {
		error << string_compose(_("FileSource: cannot read Broadcast Wave data from existing audio file \"%1\" (%2)"),
				 _path, strerror (errno)) << endmsg;
		return -1;
	}

	if (header.bigendian != WE_ARE_BIGENDIAN) {
		swap_endian (header.bext);
	}
	
	if (info.size > sizeof (header.bext)) {

		coding_history_size = info.size - (sizeof (header.bext) - sizeof (GenericChunk));
		
		char data[coding_history_size];
		
		if (::pread64 (fd, data, coding_history_size, info.offset + sizeof (BroadcastChunk)) != coding_history_size) {
			error << string_compose(_("FileSource: cannot read Broadcast Wave coding history from audio file \"%1\" (%2)"),
					 _path, strerror (errno)) << endmsg;
			return -1;
		}
		
		/* elements of the coding history are divided by \r\n */
		
		char *p = data;
		char *end = data + coding_history_size;
		string tmp;

		while (p < end) {
			if (*p == '\r' && (p+1) != end && *(p+1) == '\n') {
				if (tmp.length()) {
					header.coding_history.push_back (tmp);
					tmp = "";
				}
				p += 2;
			} else {
				tmp += *p;
				p++;
			}
		}
	}

	return 0;
}

int
FileSource::check_header (jack_nframes_t rate, bool silent)
{
	if (header.format.formatTag == 1 && header.format.nBitsPerSample == 24) {
		// 24 bit PCM
		_sample_format = FormatInt24;
		_sample_size = 3;
	} else if (header.format.formatTag == 3) {
		/* IEEE float */		
		_sample_format = FormatFloat;
		_sample_size = 4;
	}
	else {
		if (!silent) {
			error << string_compose(_("FileSource \"%1\" does not use valid sample format.\n"   
					   "This is probably a programming error."), _path) << endmsg;
		}
		return -1;
	}
	
	/* compute the apparent length of the data */

	data_offset = 0;

	for (vector<ChunkInfo>::iterator i = chunk_info.begin(); i != chunk_info.end();) {
		vector<ChunkInfo>::iterator n;

		n = i;
		++n;

		if ((*i).name == "data") {

			data_offset = (*i).offset + sizeof (GenericChunk);

			if (n == chunk_info.end()) {
				off64_t end_of_file;
				end_of_file = lseek (fd, 0, SEEK_END);

				_length = end_of_file - data_offset;

			} else {
				_length = (*n).offset - data_offset;
			}

			_length /= sizeof (Sample);

			break;
		}
		
		i = n;
	}

	if (data_offset == 0) {
		error << string_compose(_("FileSource \"%1\" has no \"data\" chunk"), _path) << endmsg;
		return -1;
	}

	if (_length * sizeof (Sample) != (jack_nframes_t) header.data.size) {
		warning << string_compose(_("%1: data length in header (%2) differs from implicit size in file (%3)"),
				   _path, header.data.size, _length * sizeof (Sample)) << endmsg;
	}

//	if ((jack_nframes_t) header.format.nSamplesPerSec != rate) {
//		warning << string_compose(_("\"%1\" has a sample rate of %2 instead of %3 as used by this session"),
//				   _path, header.format.nSamplesPerSec, rate) << endmsg;
//	}

	return 0;
}

float
FileSource::sample_rate () const
{
	return header.format.nSamplesPerSec;
}

int
FileSource::write_header()
{
	off64_t pos;

	/* write RIFF/WAVE boilerplate */

	pos = 0;

 	WAVEChunk wchunk = header.wave;
 
 	if (header.bigendian != WE_ARE_BIGENDIAN) {
 		swap_endian(wchunk);
 	}
	
	if (::pwrite64 (fd, (char *) &wchunk, sizeof (wchunk), pos) != sizeof (wchunk)) {
		error << string_compose(_("FileSource: cannot write WAVE chunk: %1"), strerror (errno)) << endmsg;
		return -1;
	}
	
	pos += sizeof (header.wave);
	
	if (is_bwf) {

		/* write broadcast chunk data without copy history */

		BroadcastChunk bchunk = header.bext;
		if (header.bigendian != WE_ARE_BIGENDIAN) {
			swap_endian (bchunk);
		}
		
		if (::pwrite64 (fd, (char *) &bchunk, sizeof (bchunk), pos) != sizeof (bchunk)) {
			return -1;
		}

		pos += sizeof (header.bext);

		/* write copy history */

		for (vector<string>::iterator i = header.coding_history.begin(); i != header.coding_history.end(); ++i) {
			string x;
			
			x = *i;
			x += "\r\n";
			
			if (::pwrite64 (fd, x.c_str(), x.length(), pos) != (int32_t) x.length()) {
				return -1;
			}
			
			pos += x.length();
		}
	}

        /* write fmt and data chunks */
 	FMTChunk fchunk = header.format;
 	if (header.bigendian != WE_ARE_BIGENDIAN) {
 		swap_endian (fchunk);
 	}
 	
 	if (::pwrite64 (fd, (char *) &fchunk, sizeof (fchunk), pos) != sizeof (fchunk)) {
		error << string_compose(_("FileSource: cannot write format chunk: %1"), strerror (errno)) << endmsg;
		return -1;
	}

	pos += sizeof (header.format);

 	GenericChunk dchunk = header.data;
 	if (header.bigendian != WE_ARE_BIGENDIAN) {
 		swap_endian (dchunk);
 	}
 
 	if (::pwrite64 (fd, (char *) &dchunk, sizeof (dchunk), pos) != sizeof (dchunk)) {
		error << string_compose(_("FileSource: cannot data chunk: %1"), strerror (errno)) << endmsg;
		return -1;
	}

	return 0;
}

void
FileSource::mark_for_remove ()
{
	remove_at_unref = true;
}

jack_nframes_t
FileSource::read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	Glib::Mutex::Lock lm (_lock);
	return read_unlocked (dst, start, cnt, workbuf);
}

jack_nframes_t
FileSource::read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	jack_nframes_t file_cnt;

	if (start > _length) {

		/* read starts beyond end of data, just memset to zero */
		
		file_cnt = 0;

	} else if (start + cnt > _length) {
		
		/* read ends beyond end of data, read some, memset the rest */
		
		file_cnt = _length - start;

	} else {
		
		/* read is entirely within data */

		file_cnt = cnt;
	}

	if (file_cnt) {
		if (file_read(dst, start, file_cnt, workbuf) != (ssize_t) file_cnt) {
			return 0;
		}
	}

	if (file_cnt != cnt) {
		jack_nframes_t delta = cnt - file_cnt;
		memset (dst+file_cnt, 0, sizeof (Sample) * delta);
	}
	
	return cnt;
}

jack_nframes_t
FileSource::write (Sample *data, jack_nframes_t cnt, char * workbuf)
{
	{
		Glib::Mutex::Lock lm (_lock);
		
		jack_nframes_t oldlen;
		int32_t frame_pos = _length;
		
		if (file_write(data, frame_pos, cnt, workbuf) != (ssize_t) cnt) {
			return 0;
		}

		oldlen = _length;
		_length += cnt;

		if (_build_peakfiles) {
			PeakBuildRecord *pbr = 0;

			if (pending_peak_builds.size()) {
				pbr = pending_peak_builds.back();
			}
			
			if (pbr && pbr->frame + pbr->cnt == oldlen) {
				
				/* the last PBR extended to the start of the current write,
				   so just extend it again.
				*/

				pbr->cnt += cnt;
			} else {
				pending_peak_builds.push_back (new PeakBuildRecord (oldlen, cnt));
			}
			
			_peaks_built = false;
		}

	}


	if (_build_peakfiles) {
		queue_for_peaks (*this);
	}

	return cnt;
}

ssize_t
FileSource::write_float(Sample *data, jack_nframes_t framepos, jack_nframes_t cnt, char * workbuf)
{
	int32_t byte_cnt = cnt * _sample_size;
	int32_t byte_pos = data_offset + (framepos * _sample_size);
	ssize_t retval;
	
	if ((retval = ::pwrite64 (fd, (char *) data, byte_cnt, byte_pos)) != (ssize_t) byte_cnt) {
		error << string_compose(_("FileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
		if (retval > 0) {
			return retval / _sample_size;
		}
		else {
			return retval;
		}
	}

	_write_data_count = byte_cnt;
	
	return cnt;
}

ssize_t
FileSource::read_float (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	ssize_t nread;
	ssize_t byte_cnt = (ssize_t) cnt * sizeof (Sample);
	int readfd;

	/* open, read, close */

	if ((readfd = open64 (_path.c_str(), O_RDONLY)) < 0) {
		error << string_compose(_("FileSource: could not open \"%1\": (%2)"), _path, strerror (errno)) << endmsg;
		return 0;
	}
	
	nread = ::pread64 (readfd, (char *) dst, byte_cnt, data_offset + (start * _sample_size));
	close (readfd);

	if (nread != byte_cnt) {

		cerr << "FileSource: \""
		     << _path
		     << "\" bad read at frame "
		     << start
		     << ", of "
		     << cnt
		     << " (bytes="
		     << byte_cnt
		     << ") frames [length = " << _length 
		     << " eor = " << start + cnt << "] ("
		     << strerror (errno)
		     << ") (read "
		     << nread / sizeof (Sample)
		     << " (bytes=" <<nread
		     << ")) pos was"
		     << data_offset
		     << '+'
		     << start << '*' << sizeof(Sample)
		     << " = " << data_offset + (start * sizeof(Sample))
		     << endl;
		
		if (nread > 0) {
			return nread / _sample_size;
		} else {
			return nread;
		}
	}

	if (header.bigendian != WE_ARE_BIGENDIAN) {
		swap_endian(dst, cnt);
	}
	
	_read_data_count = byte_cnt;

	return cnt;
}

ssize_t
FileSource::write_pcm_24(Sample *data, jack_nframes_t framepos, jack_nframes_t cnt, char * workbuf)
{
	int32_t byte_cnt = cnt * _sample_size;
	int32_t byte_pos = data_offset + (framepos * _sample_size);
	ssize_t retval;
	
	// convert to int24
	if (header.bigendian) {
		pcm_f2bet_clip_array (data, workbuf, cnt);
	} else {
		pcm_f2let_clip_array (data, workbuf, cnt);
	}
	
	if ((retval = ::pwrite64 (fd, (char *) workbuf, byte_cnt, byte_pos)) != (ssize_t) byte_cnt) {
		error << string_compose(_("FileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
		if (retval > 0) {
			return retval / _sample_size;
		}
		else {
			return retval;
		}
	}

	return (ssize_t) cnt;
}

ssize_t
FileSource::read_pcm_24 (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	ssize_t nread;
	ssize_t byte_cnt = (ssize_t) cnt * _sample_size;
	int readfd;

	/* open, read, close */

	if ((readfd = open64 (_path.c_str(), O_RDONLY)) < 0) {
		error << string_compose(_("FileSource: could not open \"%1\": (%2)"), _path, strerror (errno)) << endmsg;
		return 0;
	}

	nread = ::pread64 (readfd, (char *) workbuf, byte_cnt, data_offset + (start * _sample_size));
	close (readfd);

	if (nread != byte_cnt) {

		cerr << "May be OK - FileSource: \""
		     << _path
		     << "\" bad 24bit read at frame "
		     << start
		     << ", of "
		     << cnt
		     << " (bytes="
		     << byte_cnt
		     << ") frames [length = " << _length 
		     << " eor = " << start + cnt << "] ("
		     << strerror (errno)
		     << ") (read "
		     << nread / sizeof (Sample)
		     << " (bytes=" <<nread
		     << ")) pos was"
		     << data_offset
		     << '+'
		     << start << '*' << sizeof(Sample)
		     << " = " << data_offset + (start * sizeof(Sample))
		     << endl;

		if (nread > 0) {
			return nread / _sample_size;
		}
		else {
			return nread;
		}
	}

	// convert from 24bit->float
	
	if (header.bigendian) {
		pcm_bet2f_array (workbuf, cnt, dst);
	} else {
		pcm_let2f_array (workbuf, cnt, dst);
	}
	
	_read_data_count = byte_cnt;

	return (ssize_t) cnt;
}


bool
FileSource::is_empty (string path)
{
	struct stat statbuf;

	stat (path.c_str(), &statbuf);
	
	/* its a bit of a problem if an audio file happens
	   to be a regular WAVE file with just enough data
	   to match the size of an empty BWF. hmmm. not very
	   likely however - that represents a duration of
	   less than 1msec at typical sample rates.  
	*/

	/* NOTE: 698 bytes is the size of a BWF header structure *plus* our minimal coding history */

	return (statbuf.st_size == 0 || statbuf.st_size == wave_header_size || statbuf.st_size == 698);
}

void
FileSource::mark_streaming_write_completed ()
{
	Glib::Mutex::Lock lm (_lock);

	next_peak_clear_should_notify = true;

	if (_peaks_built || pending_peak_builds.empty()) {
		_peaks_built = true;
		 PeaksReady (); /* EMIT SIGNAL */
	}
}

void
FileSource::mark_take (string id)
{
	_take_id = id;
}

int
FileSource::move_to_trash (const string trash_dir_name)
{
	string newpath;

	/* don't move the file across filesystems, just
	   stick it in the `trash_dir_name' directory
	   on whichever filesystem it was already on.
	*/

        // XXX Portability

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
			error << string_compose (_("there are already 1000 files with names like %1; versioning discontinued"),
					  newpath)
			      << endmsg;
		} else {
			newpath = newpath_v;
		}

	} else {

		/* it doesn't exist, or we can't read it or something */

	}

	if (::rename (_path.c_str(), newpath.c_str()) != 0) {
		error << string_compose (_("cannot rename audio file source from %1 to %2 (%3)"),
				  _path, newpath, strerror (errno))
		      << endmsg;
		return -1;
	}

	if (::unlink (peakpath.c_str()) != 0) {
		error << string_compose (_("cannot remove peakfile %1 for %2 (%3)"),
				  peakpath, _path, strerror (errno))
		      << endmsg;
		/* try to back out */
		rename (newpath.c_str(), _path.c_str());
		return -1;
	}
	    
	_path = newpath;
	peakpath = "";
	remove_at_unref = false;
	
	return 0;
}

string
prepare_string(string& str)
{
	string prepared;
	
	for (uint32_t i = 0; i < str.size(); ++i){
		char c = str[i];
		if (isdigit(c) || isalpha(c)){
			prepared += c;
		} else {
			prepared += '\\';
			prepared += c;
		}
	}

	return prepared;
}

int
FileSource::repair (string path, jack_nframes_t rate)
{
	FILE* in;
	char buf[700];
	char* ptr;
	struct stat statbuf;
	size_t i;
	int ret = -1;
	bool bigend = false;
	bool doswap = false;
	
	if (stat (path.c_str(), &statbuf)) {
		return -1;
	}

	if (statbuf.st_size <= (off_t) sizeof (buf)) {
		/* nothing was ever written to the file, so there is nothing
		   really to do.
		*/
		return 0;
	}

	if ((in = fopen (path.c_str(), "r+")) == NULL) {
		return -1;
	}

	if (fread (buf, sizeof (buf), 1, in) != 1) {
		goto out;
	}
	
	if ((memcmp (&buf[0], "RIFF", 4) && memcmp (&buf[0], "RIFX", 4)) || memcmp (&buf[8], "WAVE", 4)) {
		/* no header. too dangerous to proceed */
		goto out;
	} 

	if (memcmp (&buf[0], "RIFX", 4)==0) {
		bigend = true;
	}

	doswap = bigend != WE_ARE_BIGENDIAN;
	
	/* reset the size of the RIFF chunk header */

	if (doswap) {
		*((int32_t *)&buf[4]) = Swap_32((int32_t)(statbuf.st_size - 8));
	}
	else {
		*((int32_t *)&buf[4]) = statbuf.st_size - 8;
	}

	/* find data chunk and reset the size */

	ptr = buf;

	for (i = 0; i < sizeof (buf); ) {

		if (memcmp (ptr, "fmt ", 4) == 0) {
			
			FMTChunk fmt;

			memcpy (&fmt, ptr, sizeof (fmt));
			if (doswap) {
				swap_endian(fmt);
			}
			
			fmt.nSamplesPerSec = rate;
			fmt.nAvgBytesPerSec = rate * 4;
			
			/* put it back */
			if (doswap) {
				swap_endian(fmt);
			}

			memcpy (ptr, &fmt, sizeof (fmt));
			ptr += sizeof (fmt);
			i += sizeof (fmt);

		} else if (memcmp (ptr, "data", 4) == 0) {
			GenericChunk dchunk;
			memcpy(&dchunk, ptr, sizeof(dchunk));

			if(doswap) {
				swap_endian(dchunk);
			}

			dchunk.size = statbuf.st_size - i - 8;

			if (doswap) {
				swap_endian(dchunk);
			}
			memcpy (ptr, &dchunk, sizeof (dchunk));
			break;

		} else {
			++ptr;
			++i;
		}
	}

	/* now flush it back to disk */
	
	rewind (in);

	if (fwrite (buf, sizeof (buf), 1, in) != 1) {
		goto out;
	}

	ret = 0;
	fflush (in);

  out:
	fclose (in);
	return ret;
}

