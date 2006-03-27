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

#ifndef __playlist_file_buffer_h__ 
#define __playlist_file_buffer_h__

// darwin supports 64 by default and doesn't provide wrapper functions.
#if defined (__APPLE__)
typedef off_t off64_t;
#define open64 open
#define close64 close
#define lseek64 lseek
#define pread64 pread
#define pwrite64 pwrite
#endif

#include <vector>
#include <string>

#include <ardour/source.h>

struct tm;

using std::string;

namespace ARDOUR {

class FileSource : public Source {
  public:
	FileSource (string path, jack_nframes_t rate, bool repair_first = false, SampleFormat samp_format=FormatFloat);
	FileSource (const XMLNode&, jack_nframes_t rate);
	~FileSource ();

	int set_name (std::string str, bool destructive);

	void set_allow_remove_if_empty (bool yn);

	jack_nframes_t length() const { return _length; }
	jack_nframes_t read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
	jack_nframes_t write (Sample *src, jack_nframes_t cnt, char * workbuf);
	void           mark_for_remove();
	string         peak_path(string audio_path);
	string         path() const { return _path; }

	virtual int            seek (jack_nframes_t frame) {return 0; }
	virtual jack_nframes_t last_capture_start_frame() const { return 0; }
	virtual void           mark_capture_start (jack_nframes_t) {}
	virtual void           mark_capture_end () {}
	virtual void           clear_capture_marks() {}

	int update_header (jack_nframes_t when, struct tm&, time_t);

	int move_to_trash (const string trash_dir_name);

	static bool is_empty (string path);
	void mark_streaming_write_completed ();

	void   mark_take (string);
	string take_id() const { return _take_id; }

	static void set_bwf_country_code (string x);
	static void set_bwf_organization_code (string x);
	static void set_bwf_serial_number (int);
	
	static void set_search_path (string);

  protected:
	int            fd;
	string        _path;
	bool           remove_at_unref;
	bool           is_bwf;
	off64_t        data_offset;
	string        _take_id;
	SampleFormat  _sample_format;
	int           _sample_size;
	bool           allow_remove_if_empty;

	static char bwf_country_code[3];
	static char bwf_organization_code[4];
	static char bwf_serial_number[13];

	struct GenericChunk {
	    char    id[4];
	    int32_t  size; 
	};

	struct WAVEChunk : public GenericChunk {
	    char    text[4];      /* wave pseudo-chunk id "WAVE" */
	};

	struct FMTChunk : public GenericChunk {
	    int16_t   formatTag;           /* format tag; currently pcm   */
	    int16_t   nChannels;           /* number of channels         */
	    uint32_t  nSamplesPerSec;      /* sample rate in hz          */
	    int32_t   nAvgBytesPerSec;     /* average bytes per second   */
	    int16_t   nBlockAlign;         /* number of bytes per sample */
	    int16_t   nBitsPerSample;      /* number of bits in a sample */
	};

	struct BroadcastChunk : public GenericChunk {
	    char   description[256];
	    char   originator[32];
	    char   originator_reference[32];
	    char   origination_date[10];
	    char   origination_time[8];
	    int32_t time_reference_low;
	    int32_t time_reference_high;
	    int16_t version;               /* 1.0 because we have umid and 190 bytes of "reserved" */
	    char   umid[64];
	    char   reserved[190];
	    /* we don't treat coding history as part of the struct */
	};

	struct ChunkInfo {
	    string        name;
	    uint32_t size;
	    off64_t         offset;
	    
	    ChunkInfo (string s, uint32_t sz, off64_t o) 
		    : name (s), size (sz), offset (o) {}
	};

	vector<ChunkInfo> chunk_info;
	
	struct {
	    WAVEChunk               wave;
	    FMTChunk                format;
	    GenericChunk            data;
	    BroadcastChunk          bext;
	    vector<string>          coding_history;
	    bool                    bigendian;
	} header;

	int init (string, bool must_exist, jack_nframes_t);
	jack_nframes_t read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;

	ssize_t file_write (Sample *src, jack_nframes_t framepos, jack_nframes_t cnt, char * workbuf) {
		switch (_sample_format) {
		case FormatInt24: return write_pcm_24(src, framepos, cnt, workbuf);
		default: return write_float(src, framepos, cnt, workbuf);
		};
	}
	
	ssize_t file_read  (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const  {
		switch (_sample_format) {
		case FormatInt24: return read_pcm_24(dst, start, cnt, workbuf);
		default: return read_float(dst, start, cnt, workbuf);
		};
	}
	
	ssize_t write_float(Sample *src, jack_nframes_t framepos, jack_nframes_t cnt, char * workbuf);
	ssize_t read_float (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
        ssize_t write_pcm_24(Sample *src, jack_nframes_t framepos, jack_nframes_t cnt, char * workbuf);
	ssize_t read_pcm_24 (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
	
	int discover_chunks (bool silent);
	ChunkInfo* lookup_chunk (string name);

	int write_header ();
	int read_header (bool silent);

	int check_header (jack_nframes_t rate, bool silent);
	int fill_header (jack_nframes_t rate);
	
	int read_broadcast_data (ChunkInfo&);
	void compute_header_size ();
	
	static const int32_t wave_header_size = sizeof (WAVEChunk) + sizeof (FMTChunk) + sizeof (GenericChunk);
	static const int32_t bwf_header_size = wave_header_size + sizeof (BroadcastChunk);

	static string search_path;

	int repair (string, jack_nframes_t);

	void swap_endian (GenericChunk & chunk) const;
	void swap_endian (FMTChunk & chunk) const;
	void swap_endian (BroadcastChunk & chunk) const;
	void swap_endian (Sample *buf, jack_nframes_t cnt) const;
};

}

#endif /* __playlist_file_buffer_h__ */
