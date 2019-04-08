/*  Rewritten for Ardour by Paul Davis <paul@linuxaudiosystems.com>, Feb 2010
    but based on ...
 */

/*  REAPER OMF plug-in
    Copyright (C) 2009 Hannes Breul

    Provides OMF import.

    Based on the m3u example included in the Reaper SDK,
    Copyright (C) 2005-2008 Cockos Incorporated

    Original source available at:
    http://www.reaper.fm/sdk/plugin/plugin.php#ext_dl

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS /* PRI<foo>; C++ requires explicit requesting of these */
#endif

#include <iostream>

#include <getopt.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <sys/errno.h>
#include <sndfile.h>
#include <glibmm.h>

#include "pbd/xml++.h"
#include "pbd/basename.h"
#include "omftool.h"

//#define DEBUG(fmt,...) fprintf (stderr, fmt, ## __VA_ARGS__)
#define DEBUG(fmt,...)
#define INFO(fmt,...) fprintf (stdout, fmt, ## __VA_ARGS__)

using namespace std;
using namespace PBD;

OMF::OMF ()
{
	char sbuf[256];

	bigEndian = false;
	id_counter = 0;
	session_name = "omfsession";
	base_dir = ".";
	sample_rate = 0;
	frame_rate = 0;
	version = 3000;
	db = 0;
	file = 0;

	session = new XMLNode ("Session");
	sources = new XMLNode ("Sources");
	routes = new XMLNode ("Routes");
	regions = new XMLNode ("Regions");
	playlists = new XMLNode ("Playlists");
	diskstreams = new XMLNode ("DiskStreams");
	locations = new XMLNode ("Locations");
	options = new XMLNode ("Options");
	options = new XMLNode ("RouteGroups");

	/* add master, default 2in/2out */

	XMLNode* master = new_route_node ();
	master->add_property ("name", "master");
	set_route_node_channels (master, 2, 2, false);

	XMLNode* tempo_map = new XMLNode ("TempoMap");
	XMLNode* tempo = new XMLNode ("Tempo");
	tempo->add_property ("start", "1|1|0");
	tempo->add_property ("beats-per-minute", "120.0");
	tempo->add_property ("note-type", "4.0");
	tempo->add_property ("movable", "no");
	tempo_map->add_child_nocopy (*tempo);
	XMLNode* meter = new XMLNode ("Meter");
	meter->add_property ("start", "1|1|0");
	meter->add_property ("beats-per-bar", "4.0");
	meter->add_property ("note-type", "4.0");
	meter->add_property ("movable", "no");
	tempo_map->add_child_nocopy (*meter);

	XMLNode* click = new XMLNode ("Click");
	XMLNode* io = new XMLNode ("IO");
	click->add_child_nocopy (*io);
	io->add_property ("name", "click");
	add_id (io);
	io->add_property ("direction", "Output");
	io->add_property ("default-type", "audio");
	XMLNode* port = new XMLNode ("Port");
	io->add_child_nocopy (*port);
	port->add_property ("type", "audio");
	port->add_property ("name", "click/audio_out 1");
	XMLNode* connection = new XMLNode ("Connection");
	connection->add_property ("other", "system:playback_1");
	port->add_child_nocopy (*connection);

	port = new XMLNode ("Port");
	io->add_child_nocopy (*port);
	port->add_property ("type", "audio");
	port->add_property ("name", "click/audio_out 2");
	connection = new XMLNode ("Connection");
	connection->add_property ("other", "system:playback_2");
	port->add_child_nocopy (*connection);

	session->add_child_nocopy (*options);
	session->add_child_nocopy (*sources);
	session->add_child_nocopy (*regions);
	session->add_child_nocopy (*playlists);
	session->add_child_nocopy (*diskstreams);
	session->add_child_nocopy (*routes);
	session->add_child_nocopy (*locations);
	session->add_child_nocopy (*tempo_map);
	session->add_child_nocopy (*click);
}

OMF::~OMF ()
{
	/* clean up */
	sqlite3_close (db);
	fclose (file);
}

void
OMF::set_sample_rate (int sr)
{
	sample_rate = sr;
}

void
OMF::set_session_name (const std::string& str)
{
	base_dir = Glib::path_get_dirname (str); // returns "." if no dirs were given
	session_name = Glib::path_get_basename (str);
}

void
OMF::set_version (int v)
{
	version = v;
}

int
OMF::init ()
{
	/* create directory tree */

	string dir;

	audiofile_path_vector.push_back (base_dir);
	audiofile_path_vector.push_back (session_name);
	audiofile_path_vector.push_back ("interchange");
	audiofile_path_vector.push_back (session_name);
	audiofile_path_vector.push_back ("audiofiles");

	dir = Glib::build_filename (audiofile_path_vector);
	g_mkdir_with_parents (dir.c_str(), 0775);

	/* and the rest */


	vector<string> v;
	v.push_back (base_dir);
	v.push_back (session_name);

	vector<string> d;
	d.push_back ("analysis");
	d.push_back ("dead_sounds");
	d.push_back ("export");
	d.push_back ("peaks");

	for (vector<string>::iterator i = d.begin(); i != d.end(); ++i) {
		v.push_back (*i);
		dir = Glib::build_filename (v);
		g_mkdir_with_parents (dir.c_str(), 0775);
		v.pop_back ();
	}

	return 0;
}

bool
OMF::get_audio_info (const std::string& path)
{
	SNDFILE *sf;
	SF_INFO sf_info;

	sf_info.format = 0; // libsndfile says to clear this before sf_open().

	if ((sf = sf_open ((char*) path.c_str(), SFM_READ, &sf_info)) == 0) {
		char errbuf[256];
		cerr << "Cannot open source file " << path << sf_error_str (0, errbuf, sizeof (errbuf) - 1) << endl;
		return false;
	}

	if (known_sources.find (Glib::path_get_basename (path)) != known_sources.end()) {
		/* already exists */
		return true;
	}

	XMLNode* source = new_source_node();

	known_sources.insert (pair<string,SourceInfo*>
			      (Glib::path_get_basename (path),
			       new SourceInfo (sf_info.channels,
					       sf_info.samplerate,
					       sf_info.frames,
					       source)));

	source->add_property ("name", basename_nosuffix (path));
	cerr << "Source file " << basename_nosuffix (path) << " = " << sf_info.channels << '/' << sf_info.samplerate << '/' << sf_info.frames << endl;
	sf_close (sf);
	return true;
}

void
OMF::add_id (XMLNode* node)
{
	char sbuf[64];
	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	node->add_property ("id", sbuf);
}

XMLNode*
OMF::new_playlist_node ()
{
	XMLNode* playlist = new XMLNode ("Playlist");
	playlists->add_child_nocopy (*playlist);
	add_id (playlist);
	playlist->add_property ("type", "audio");
	playlist->add_property ("frozen", "no");

	return playlist;
}

XMLNode*
OMF::new_diskstream_node ()
{
	XMLNode* diskstream = new XMLNode ("AudioDiskstream");
	diskstreams->add_child_nocopy (*diskstream);
	add_id (diskstream);
	diskstream->add_property ("flags", "Recordable");
	diskstream->add_property ("speed", "1");
	diskstream->add_property ("channels", "1");

	return diskstream;
}
void
OMF::set_region_sources (XMLNode* region, SourceInfo* sinfo)
{
	char buf[256];

	region->add_property ("name", sinfo->node->property ("name")->value());

	for (int i = 0; i < sinfo->channels; ++i) {
		snprintf (buf, sizeof (buf), "source-%d", i);
		region->add_property (buf, sinfo->node->property ("id")->value());
	}
}

void
OMF::legalize_name (string& name)
{
	string::size_type pos;
	string illegal_chars = ":";
	pos = 0;

	while ((pos = name.find_first_of (illegal_chars, pos)) != string::npos) {
		name.replace (pos, 1, "_");
		pos += 1;
	}
}

void
OMF::set_route_node_channels (XMLNode* route, int in, int out, bool send_to_master)
{
	XMLNode* input_io;
	XMLNode* output_io;
	char sbuf[256];
	string name = route->property ("name")->value();

	legalize_name (name);

	output_io = new XMLNode ("IO");
	route->add_child_nocopy (*output_io);
	output_io->add_property ("name", name);
	add_id (output_io);
	output_io->add_property ("direction", "Output");
	output_io->add_property ("default-type", "audio");

	input_io = new XMLNode ("IO");
	route->add_child_nocopy (*input_io);
	input_io->add_property ("name", name);
	add_id (input_io);
	input_io->add_property ("direction", "Input");
	input_io->add_property ("default-type", "audio");

	for (int i = 0; i < out; ++i) {
		XMLNode* port = new XMLNode ("Port");
		output_io->add_child_nocopy (*port);
		port->add_property ("type", "audio");

		snprintf (sbuf, sizeof (sbuf), "%s/audio_out %d", name.c_str(), i+1);

		port->add_property ("name", sbuf);
		XMLNode* connection = new XMLNode ("Connection");

		if (send_to_master) {
			if (i % 2) {
				snprintf (sbuf, sizeof (sbuf), "master/audio_in 2");
			} else {
				snprintf (sbuf, sizeof (sbuf), "master/audio_in 1");
			}
		} else {
			if (i % 2) {
				snprintf (sbuf, sizeof (sbuf), "system:playback_2");
			} else {
				snprintf (sbuf, sizeof (sbuf), "system:playback_1");
			}
		}

		connection->add_property ("other", sbuf);
		port->add_child_nocopy (*connection);
	}

	for (int i = 0; i < in; ++i) {
		XMLNode* port = new XMLNode ("Port");
		input_io->add_child_nocopy (*port);
		port->add_property ("type", "audio");

		snprintf (sbuf, sizeof (sbuf), "%s/audio_out %d", name.c_str(), i+1);

		port->add_property ("name", sbuf);
		XMLNode* connection = new XMLNode ("Connection");

		if (i % 2) {
			snprintf (sbuf, sizeof (sbuf), "system:capture_2");
		} else {
			snprintf (sbuf, sizeof (sbuf), "system:capture_1");
		}

		connection->add_property ("other", sbuf);
		port->add_child_nocopy (*connection);
	}

	/* add main out processor */

	XMLNode* outs = new XMLNode ("Processor");
	route->add_child_nocopy (*outs);
	add_id (outs);
	outs->add_property ("name", name);
	outs->add_property ("active", "yes");
	outs->add_property ("own-input", "yes");
	outs->add_property ("own-output", send_to_master ? "no" : "yes");
	outs->add_property ("output", name);
	outs->add_property ("type", "main-outs");
	outs->add_property ("role", "Main");

	/* Panner setup */

	XMLNode* panner = new XMLNode ("Panner");
	outs->add_child_nocopy (*panner);

	panner->add_property ("linked", "no");
	panner->add_property ("link-direction", "SameDirection");
	panner->add_property ("bypassed", "no");

	for (int i = 0; i < out; ++i) {
		XMLNode* panout = new XMLNode ("Output");
		panner->add_child_nocopy (*panout);
		panout->add_property ("x", "0");
		panout->add_property ("y", "0");
	}

	for (int i = 0; i < in; ++i) {
		XMLNode* spanner = new XMLNode ("StreamPanner");
		panner->add_child_nocopy (*spanner);
		spanner->add_property ("x", "0");
		spanner->add_property ("type", "Equal Power Stereo");
		spanner->add_property ("muted", "no");
		spanner->add_property ("mono", "no");

		XMLNode* spc = new XMLNode ("Controllable");
		spanner->add_child_nocopy (*spc);
		add_id (spc);
		spc->add_property ("name", "panner");
		spc->add_property ("flags", "");
	}
}

XMLNode*
OMF::new_route_node ()
{
	char sbuf[256];
	XMLNode* route = new XMLNode ("Route");

	routes->add_child_nocopy (*route);
	add_id (route);
	route->add_property ("default-type","audio");
	route->add_property ("active","yes");
	route->add_property ("phase-invert","no");
	route->add_property ("denormal-protection","no");
	route->add_property ("meter-point","MeterPostFader");
	snprintf (sbuf, sizeof (sbuf), "editor=%" PRId64 ":signal=%" PRId64, id_counter, id_counter);
	route->add_property ("order-keys", sbuf);
	route->add_property ("self-solo","no");
	route->add_property ("soloed-by-others","0");
	route->add_property ("mode","Normal");

	/* other boilerplate */

	XMLNode* controllable = new XMLNode ("Controllable");
	route->add_child_nocopy (*controllable);
	controllable->add_property ("name", "solo");
	add_id (controllable);
	controllable->add_property ("flags", "Toggle");

	XMLNode* mutemaster = new XMLNode ("MuteMaster");
	route->add_child_nocopy (*mutemaster);
	mutemaster->add_property ("mute-point", "");

	XMLNode* remotecontrol = new XMLNode ("RemoteControl");
	route->add_child_nocopy (*remotecontrol);
	remotecontrol->add_property ("id", route->property ("id")->value());

	XMLNode* amp = new XMLNode ("Processor");
	route->add_child_nocopy (*amp);
	add_id (amp);
	amp->add_property ("name", "Amp");
	amp->add_property ("active", "yes");
	amp->add_property ("type", "amp");
	amp->add_property ("gain", "1.0");

	XMLNode* meter = new XMLNode ("Processor");
	route->add_child_nocopy (*meter);
	add_id (meter);
	meter->add_property ("name", "Meter");
	meter->add_property ("active", "yes");
	meter->add_property ("type", "meter");

	XMLNode* extra = new XMLNode ("Extra");
	route->add_child_nocopy (*extra);
	XMLNode* gui = new XMLNode ("GUI");
	extra->add_child_nocopy (*gui);
	snprintf (sbuf, sizeof (sbuf), "%d:%d:%d",
		  random() % 65536,
		  random() % 65536,
		  random() % 65536);
	gui->add_property ("color", sbuf);
	gui->add_property ("shown-mixer", "yes");
	gui->add_property ("height", "62");
	gui->add_property ("shown-editor", "yes");

	return route;
}

XMLNode*
OMF::new_region_node ()
{
	XMLNode* region = new XMLNode ("Region");
	XMLNode* region_extra = new XMLNode ("Extra");
	XMLNode* gui_extra = new XMLNode ("GUI");
	char sbuf[256];

	region_extra->add_child_nocopy (*gui_extra);
	region->add_child_nocopy (*region_extra);

	/* boilerplate */

	region->add_property ("ancestral-start", "0");
	region->add_property ("ancestral-start", "0");
	region->add_property ("ancestral-length", "0");
	region->add_property ("stretch", "1");
	region->add_property ("shift", "1");
	region->add_property ("first-edit", "nothing");
	region->add_property ("layer", "0");
	region->add_property ("sync-position", "0");
	region->add_property ("flags", "Opaque,DefaultFadeIn,DefaultFadeOut,FadeIn,FadeOut,External");
	region->add_property ("scale-gain", "1");
	region->add_property ("channels", "1");
	gui_extra->add_property ("waveform-visible","yes");
	gui_extra->add_property ("envelope-visible", "no");
	gui_extra->add_property ("waveform-rectified", "no");
	gui_extra->add_property ("waveform-logscaled","no");

	add_id (region);
	return region;
}

XMLNode*
OMF::new_source_node ()
{
	XMLNode* source;

	source = new XMLNode ("Source");
	add_id (source);
	source->add_property ("type", "audio");
	source->add_property ("flags", "CanRename");

	sources->add_child_nocopy (*source);

	return source;
}

OMF::SourceInfo*
OMF::get_known_source (const char* name)
{
	string s (name);
	KnownSources::iterator i = known_sources.find (s);
	if (i != known_sources.end()) {
		return i->second;
	}
	return 0;
}

char *
OMF::read_name (size_t offset, size_t len)
{
	char* buf = (char*) malloc (len+1);
	fseek (file, offset, SEEK_SET);
	fread (buf, len, 1, file);
	buf[len] = '\0';
	return buf;
}

bool
OMF::get_offset_and_length (const char* offstr, const char* lenstr, uint32_t& offset, uint32_t& len)
{
	if (sscanf (offstr, "%d", &offset) == 0) {
		cerr << "bad offset\n";
		return false;
	}

	if (sscanf (lenstr, "%d", &len) == 0) {
		cerr << "bad length\n";
		return false;
	}

	if (((int32_t) offset) <= 0) {
		cerr << "illegal offset\n";
		return false;
	}

	if (((int32_t) len) <= 0) {
		cerr << "illegal length\n";
		return false;
	}

	return true;
}

int
OMF::create_xml ()
{
	XMLNode* region;
	XMLNode* playlist;
	XMLNode* diskstream;
	SourceInfo* sinfo;
	char sbuf[256];
	int route_max_channels;
	int major;
	int minor;
	int micro;

	major = version / 1000;
	minor = version - (major * 1000);
	micro = version - (major * 1000) - (minor * 100);

	snprintf (sbuf, sizeof (sbuf), "%d.%d.%d", major, minor, micro);

	session->add_property ("version", sbuf);
	session->add_property ("name", session_name);

	char **tracks;
	int numtracks;
	sqlite3_get_table(db, "SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE property = 'OMFI:OOBJ:ObjClass' AND value = 'CMOB' LIMIT 1) AND property = 'OMFI:MOBJ:Slots')", &tracks, &numtracks, 0, 0);

	for (int i = 1; i <= numtracks; i++) {

		int descCount;
		char **desc;

		sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND value = 'SEQU' LIMIT 1", tracks[i]), &desc, &descCount, 0, 0);
		sqlite3_free_table(desc);

		sinfo = 0;
		route_max_channels = 0;

		INFO ("Processing track %d / %d...\n", i, numtracks);

		if (descCount <= 0) {
			continue;
		}

		/* create a new route, which will mean that we need a new diskstream and playlist too
		 */

		XMLNode* route = new_route_node ();
		XMLNode* playlist = new_playlist_node ();
		XMLNode* diskstream = new_diskstream_node ();

		/* route and playlist both need diskstream ID */

		route->add_property ("diskstream-id", diskstream->property ("id")->value());
		playlist->add_property ("orig-diskstream-id", diskstream->property ("id")->value());

		char **name;
		int nameCount;
		//sqlite3_get_table(db, sqlite3_mprintf("SELECT d2.offset, d2.length FROM data d1, data d2 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:MSLT:TrackDesc' AND d2.object LIKE d1.value AND d2.property LIKE 'OMFI:TRKD:TrackName' LIMIT 1", tracks[i]), &name, &nameCount, 0, 0);
		sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MSLT:TrackDesc' LIMIT 1) AND property = 'OMFI:TRKD:TrackName' LIMIT 1", tracks[i]), &name, &nameCount, 0, 0);
		if (nameCount > 0) {
			uint32_t nOffs;
			uint32_t nLen;
			if (get_offset_and_length (name[2], name[3], nOffs, nLen)) {
				char* nBuf = read_name (nOffs, nLen);
				route->add_property ("name", nBuf);
				playlist->add_property ("name", nBuf);
				diskstream->add_property ("name", nBuf);
				diskstream->add_property ("playlist", nBuf);
				free (nBuf);
			} else {
				INFO ("Track %d has unreadable name\n", i);
				snprintf (sbuf, sizeof (sbuf), "Track %d", i);
				route->add_property ("name", sbuf);
				playlist->add_property ("name", sbuf);
				diskstream->add_property ("name", sbuf);
				diskstream->add_property ("playlist", sbuf);
			}
		} else {
			INFO ("Track %d has no name\n", i);
			snprintf (sbuf, sizeof (sbuf), "Track %d", i);
			route->add_property ("name", sbuf);
			playlist->add_property ("name", sbuf);
			diskstream->add_property ("name", sbuf);
			diskstream->add_property ("playlist", sbuf);
		}
		sqlite3_free_table(name);

		char **rate;
		int rateCount;
		int num = 1, denom = 1;

		sqlite3_get_table(db, sqlite3_mprintf("SELECT offset FROM data WHERE object = %s AND property = 'OMFI:MSLT:EditRate' LIMIT 1", tracks[i]), &rate, &rateCount, 0, 0);

		if (rateCount > 0) {
			uint32_t rOffs = atoi(rate[1]);
			//sscanf(rate[1], "%d", &rOffs);
			fseek(file, rOffs, SEEK_SET);
			fread(&denom, 4, 1, file);
			denom = e32(denom);
			fread(&num, 4, 1, file);
			num = e32(num);
			INFO ("Rate = %d / %d\n", num, denom);
			if (frame_rate == 0) {
				frame_rate = (double) num / (double) denom;
			}
			if (sample_rate == 0) {
				sample_rate = denom;
			}
		} else {
			INFO ("OMF file is missing frame rate information for track %d\n", i);
			frame_rate = 0.04; // 25FPS
			if (sample_rate == 0) {
				sample_rate = 44100;
			}
		}

		sqlite3_free_table(rate);

		char **items;
		int itemCount;
		//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.value FROM data d1, data d2, data d3 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:MSLT:Segment' AND d2.object LIKE d1.value AND d2.property LIKE 'OMFI:SEQU:Components' AND d3.object LIKE d2.value", tracks[i]), &items, &itemCount, 0, 0);
		sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SEQU:Components' LIMIT 1)", tracks[i]), &items, &itemCount, 0, 0);
		double position = 0.0;
		int j;
		double fadeTime = 0.0;

		for (j = 1; j <= itemCount; j++) {

			printf("  item %d / %d\n", j, itemCount);

			char **len;
			int lenCount;
			double length = 0.0;
			int lenFrames = 0;

			region = 0;

			sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND property = 'OMFI:CPNT:Length' LIMIT 1", items[j]), &len, &lenCount, 0, 0);

			if (lenCount <= 0) {
				sqlite3_free_table(len);
				continue;
			}

			char **type;
			int typeCount;

			sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND property = 'OMFI:OOBJ:ObjClass' LIMIT 1", items[j]), &type, &typeCount, 0, 0);

			if (typeCount <= 0) {
				sqlite3_free_table(type);
				sqlite3_free_table(len);
				continue;
			}

			lenFrames = atoi(len[1]);
			length = lenFrames * frame_rate;

			if (!strcmp(type[1], "TRAN")) {

				position -= length;
				char **effID;
				int effIDCount;
				sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:TRAN:Effect' LIMIT 1) AND property = 'OMFI:EFFE:EffectKind' LIMIT 1) AND property = 'OMFI:EDEF:EffectID' LIMIT 1", items[j]), &effID, &effIDCount, 0, 0);
				if (effIDCount > 0) {
					uint32_t eOffs;
					uint32_t eLen;
					if (get_offset_and_length (effID[2], effID[3], eOffs, eLen)) {
						char* eBuf = read_name (eOffs, eLen);
						if (!strcmp(eBuf, "omfi:effect:StereoAudioDissolve") | !strcmp(eBuf, "omfi:effect:SimpleMonoAudioDissolve")) {
							fadeTime = length;
						}
					}
				}
				sqlite3_free_table(effID);

			} else if (!strcmp(type[1], "FILL")) {

				position += length;

			} else if (!strcmp(type[1], "NEST")) {

				char **itemName;
				int itemNameCount;
				int64_t start = 0;

				sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1)) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &itemName, &itemNameCount, 0, 0);

				char **startTime;
				int startTimeCount;
				//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.value FROM data d1, data d2, data d3 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:NEST:SLOTS' AND d2.object LIKE d1.value AND d3.object LIKE d2.value AND d3.property LIKE 'OMFI:SCLP:StartTime' LIMIT 1", items[j]), &startTime, &startTimeCount, 0, 0);
				sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1)) AND property = 'OMFI:SCLP:StartTime' LIMIT 1", items[j]), &startTime, &startTimeCount, 0, 0);
				if (startTimeCount > 0) {
					start = atoi(startTime[1]);
				}

				sqlite3_free_table(startTime);

				char **itemEffect;
				int itemEffectCount;
				//sqlite3_get_table(db, sqlite3_mprintf("select d7.offset from data d1, data d2, data d3, data d4, data d5, data d6, data d7 where d1.object like '%s' and d1.property like 'OMFI:NEST:Slots' and d2.object like d1.value and d3.object like d2.value and d3.property like 'OMFI:EFFE:EffectSlots' and d4.object like d3.value and d5.object like d4.value and d5.property like 'OMFI:ESLT:ArgValue' and d6.object like d4.value and d6.property like 'OMFI:ESLT:ArgID' and d6.value like '1' and d7.object like d5.value and d7.property like 'OMFI:CVAL:Value' LIMIT 2", items[j]), &itemEffect, &itemEffectCount, 0, 0);
				sqlite3_get_table(db, sqlite3_mprintf("SELECT offset FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1)) AND property = 'OMFI:EFFE:EffectSlots' LIMIT 1) LIMIT 2) AND property = 'OMFI:ESLT:ArgValue' LIMIT 2) AND property like 'OMFI:CVAL:Value' LIMIT 1", items[j]), &itemEffect, &itemEffectCount, 0, 0);
				if (itemEffectCount > 0) {
					int effNum = 1;
					int effDenom = 1;
					uint32_t effOffs = atoi(itemEffect[1]);
					//sscanf(itemEffect[1], "%d", &effOffs);
					fseek(file, effOffs, SEEK_SET);
					fread(&effDenom, 4, 1, file);
					fread(&effNum, 4, 1, file);
					double vol = (double) effNum / (double) effDenom;
					//ctx->AddLine("VOLPAN %.8f 0.000000 1.000000 -1.000000", vol);
					DEBUG("VOLPAN %.8f 0.000000 1.000000 -1.000000\n", vol);
				}
				sqlite3_free_table(itemEffect);

				char **sourceFile;
				int sourceFileCount;
				//sqlite3_get_table(db, sqlite3_mprintf("SELECT d10.offset, d10.length FROM data d1, data d2, data d3, data d4, data d5, data d6, data d7, data d8, data d9, data d10 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:NEST:Slots' AND d2.object LIKE d1.value AND d3.object LIKE d2.value AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MOBJ:MobID' AND d10.object LIKE d9.object AND d10.property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &sourceFile, &sourceFileCount, 0, 0);
				sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1) LIMIT 3) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &sourceFile, &sourceFileCount, 0, 0);
				if (sourceFileCount > 0) {
					uint32_t sfOffs;
					uint32_t sfLen;

					if (get_offset_and_length (sourceFile[2], sourceFile[3], sfOffs, sfLen)) {
						char *sfBuf = read_name (sfOffs, sfLen);

						if ((sinfo = get_known_source (sfBuf)) == 0) {
							cerr << "Reference to unknown source [" << sfBuf << "]1" << endl;
							return -1;
						}

						free (sfBuf);
					} else {
						cerr << "offs/len illegal\n";
					}
				} else {
					char **fallbackFile;
					int fallbackFileCount;
					//sqlite3_get_table(db, sqlite3_mprintf("SELECT d9.object FROM data d1, data d2, data d3, data d4, data d5, data d6, data d7, data d8, data d9 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:NEST:Slots' AND d2.object LIKE d1.value AND d3.object LIKE d2.value AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
					sqlite3_get_table(db, sqlite3_mprintf("SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1) LIMIT 3) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
					if (fallbackFileCount > 0) {
						if ((sinfo = get_known_source (fallbackFile[1])) == 0) {
							cerr << "Reference to unknown source [" << fallbackFile[1] << "]2" << endl;
							return -1;
						}

					} else {
						cerr << "no fallback file\n";
					}

					sqlite3_free_table(fallbackFile);
				}


				if (sinfo) {

					region = new_region_node ();
					playlist->add_child_nocopy (*region);

					snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (position * sample_rate));
					region->add_property ("position", sbuf);
					snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (length * sample_rate));
					region->add_property ("length", sbuf);
					snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (start * frame_rate * sample_rate));
					region->add_property ("start", sbuf);
					set_region_sources (region, sinfo);

					route_max_channels = max (route_max_channels, sinfo->channels);
				}

				sqlite3_free_table(sourceFile);
				sqlite3_free_table(itemName);
				position += length;

			} else if (!strcmp(type[1], "SCLP")) {

				char **itemName;
				int itemNameCount;
				int64_t start = 0;
				//sqlite3_get_table(db, sqlite3_mprintf("SELECT d5.offset, d5.length FROM data d3, data d4, data d5 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &itemName, &itemNameCount, 0, 0);
				sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &itemName, &itemNameCount, 0, 0);

				fadeTime = 0.0;

				char **startTime;
				int startTimeCount;
				//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.value FROM data d3 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:StartTime'", items[j]), &startTime, &startTimeCount, 0, 0);
				sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:StartTime' LIMIT 1", items[j]), &startTime, &startTimeCount, 0, 0);
				if (startTimeCount > 0) {
					start = atoi(startTime[1]);
				}
				sqlite3_free_table(startTime);

				char **sourceFile;
				int sourceFileCount;
				//sqlite3_get_table(db, sqlite3_mprintf("SELECT d10.offset, d10.length FROM data d3, data d4, data d5, data d6, data d7, data d8, data d9, data d10 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MOBJ:MobID' AND d10.object LIKE d9.object AND d10.property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &sourceFile, &sourceFileCount, 0, 0);
				sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &sourceFile, &sourceFileCount, 0, 0);

				if (sourceFileCount > 0) {
					uint32_t sfOffs;
					uint32_t sfLen;

					if (get_offset_and_length (sourceFile[2], sourceFile[3], sfOffs, sfLen)) {
						cerr << "get source file from " << sfOffs << " + " << sfLen << endl;
						char *sfBuf = read_name (sfOffs, sfLen);

						if ((sinfo = get_known_source (sfBuf)) == 0) {
							cerr << "Reference to unknown source [" << sfBuf << ']' << endl;
							return -1;
						}

						free (sfBuf);
					} else {
						cerr << "can't get off+len\n";
					}
				} else {
					char **fallbackFile;
					int fallbackFileCount;
					//sqlite3_get_table(db, sqlite3_mprintf("SELECT d9.object FROM data d3, data d4, data d5, data d6, data d7, data d8, data d9 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
					sqlite3_get_table(db, sqlite3_mprintf("SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1)AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property LIKE 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
					if (fallbackFileCount > 0) {
						if ((sinfo = get_known_source (fallbackFile[1])) == 0) {
							cerr << "Reference to unknown source [" << fallbackFile[1] << ']' << endl;
							return -1;
						}

					}
					sqlite3_free_table(fallbackFile);
				}

				if (sinfo) {

					region = new_region_node ();
					playlist->add_child_nocopy (*region);

					snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (position * sample_rate));
					region->add_property ("position", sbuf);
					snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (length * sample_rate));
					region->add_property ("length", sbuf);
					snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (start * frame_rate * sample_rate));
					region->add_property ("start", sbuf);
					set_region_sources (region, sinfo);

					route_max_channels = max (route_max_channels, sinfo->channels);
				}

				sqlite3_free_table(sourceFile);
				sqlite3_free_table(itemName);
				position += length;
			}

			sqlite3_free_table(type);
			sqlite3_free_table(len);
		}

		/* finalize route information */

		cerr << "Set up track with " << route_max_channels << " channels" << endl;
		set_route_node_channels (route, route_max_channels, route_max_channels, true);
		sqlite3_free_table(items);
	}

	sqlite3_free_table(tracks);

	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	session->add_property ("id-counter", sbuf);
	snprintf (sbuf, sizeof (sbuf), "%" PRId32, sample_rate);
	session->add_property ("sample-rate", sbuf);

	XMLTree xml;

	xml.set_root (session);

	vector<string> v;

	v.push_back (base_dir);
	v.push_back (session_name);
	v.push_back (session_name + ".ardour");

	xml.write (Glib::build_filename(v).c_str());
	return 0;
}


static void
print_help (const char* execname)
{
	cout << execname
	     << " [ -r sample-rate ]"
	     << " [ -n session-name ]"
	     << " [ -v ardour-session-version ]"
	     << " OMF2_session_file"
	     << endl;
	exit (1);
}

int
main (int argc, char* argv[])
{
	const char *execname = strrchr (argv[0], '/');
	const char* optstring = "r:n:v:h";
	const char* session_name = 0;
	int         sample_rate = 0;
	int         version = 0;

	const struct option longopts[] = {
		{ "rate", 1, 0, 'r' },
		{ "name", 1, 0, 'n' },
		{ "version", 1, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ 0, 0, 0, 0 }
	};


	int option_index = 0;
	int c = 0;

	while (1) {
		c = getopt_long (argc, argv, optstring, longopts, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'r':
			sample_rate = atoi (optarg);
			break;

		case 'n':
			session_name = optarg;
			break;

		case 'v':
			version = atoi (optarg);
			break;

		case 'h':
		default:
			print_help (execname);
			break;
		}
	}

	if (optind > argc) {
		print_help (execname);
		/*NOTREACHED*/
	}

	OMF omf;

	if (version) {
		omf.set_version (version);
	}

	if (sample_rate) {
		omf.set_sample_rate (sample_rate);
	}

	if (session_name) {
		omf.set_session_name (session_name);
	} else {
		omf.set_session_name (basename_nosuffix (argv[optind]));
	}

	if (omf.init () == 0) {

		if (omf.load (argv[optind++]) == 0) {
			omf.create_xml ();
		}
	}

	return 0;
}
