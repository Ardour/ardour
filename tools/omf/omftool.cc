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

#include <glibmm.h>

#include "pbd/xml++.h"
#include "omftool.h"

//#define DEBUG(fmt,...) fprintf (stderr, fmt, ## __VA_ARGS__)
#define DEBUG(fmt,...) 
#define INFO(fmt,...) fprintf (stdout, fmt, ## __VA_ARGS__)

#define MB_OK 0
void
MessageBox (FILE* /*ignored*/, const char* msg, const char* title, int status)
{
	fprintf (stderr, msg);
}

using namespace std;

OMF::OMF ()
{
	bigEndian = false;
	id_counter = 0;
	session_name = "omfsession";
	base_dir = ".";
	sample_rate = 44100;
	version = 3000;
	db = 0;
	file = 0;

	session = new XMLNode ("Session");
	sources = new XMLNode ("Sources");
	routes = new XMLNode ("Routes");
	regions = new XMLNode ("Regions");
	playlists = new XMLNode ("Playlists");
	diskstreams = new XMLNode ("Diskstreams");
	locations = new XMLNode ("Locations");
	options = new XMLNode ("Options");
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

	return 0;
}

XMLNode*
OMF::new_playlist_node ()
{
	char sbuf[256];
	XMLNode* playlist = new XMLNode ("Playlist");
	playlists->add_child_nocopy (*playlist);
	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	playlist->add_property ("id", sbuf);
	playlist->add_property ("type", "audio");
	playlist->add_property ("frozen", "no");
	
	return playlist;
}

XMLNode*
OMF::new_diskstream_node ()
{
	char sbuf[256];
	XMLNode* diskstream = new XMLNode ("AudioDiskstream");
	diskstreams->add_child_nocopy (*diskstream);
	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	diskstream->add_property ("id", sbuf);
	diskstream->add_property ("flags", "Recordable");
	diskstream->add_property ("speed", "1");
	diskstream->add_property ("channels", "1");
	
	return diskstream;
}

XMLNode*
OMF::new_route_node ()
{
	char sbuf[256];
	XMLNode* route = new XMLNode ("Route");

	routes->add_child_nocopy (*route);
	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	route->add_property ("id", sbuf);
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
	region->add_property ("source-0", "1");
	region->add_property ("master-source-0", "1");
	region->add_property ("channels", "1");
	gui_extra->add_property ("waveform-visible","yes");
	gui_extra->add_property ("envelope-visible", "no");
	gui_extra->add_property ("waveform-rectified", "no");
	gui_extra->add_property ("waveform-logscaled","no");

	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	region->add_property ("id", sbuf);
	return region;
}

XMLNode*
OMF::new_source_node ()
{
	XMLNode* source;
	char sbuf[256];

	id_counter++;
	source = new XMLNode ("Source");
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	source->add_property ("id", sbuf);
	source->add_property ("type", "audio");
	source->add_property ("flags", "CanRename");
	
	sources->add_child_nocopy (*source);

	return source;
}

XMLNode*
OMF::get_known_source (const char* name)
{
	string s (name);
	KnownSources::iterator i = known_sources.find (s);
	if (i != known_sources.end()) {
		return i->second;
	} 
	return 0;
}

void
OMF::add_source (const char* name, XMLNode* node)
{
	pair<string,XMLNode*> newpair (name, node);
	known_sources.insert (newpair);
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
OMF::get_offset_and_length (const char* offstr, const char* lenstr, uint32_t& offset, uint32_t len)
{
	if (sscanf (offstr, "%d", &offset) == 0) {
		return false;
	}

	if (sscanf (lenstr, "%d", &len) == 0) {
		return false;
	}

	if (((int32_t) offset) <= 0) {
		return false;
	}

	if (((int32_t) len) <= 0) {
		return false;
	}

	return true;
}

void
OMF::create_xml ()
{
	XMLNode* region;
	XMLNode* playlist;
	XMLNode* diskstream;
	XMLNode* source;
	char sbuf[256];
	int64_t id_counter = 0;

	session->add_property ("version", "3.0.0");
	session->add_property ("name", "iNeedAName");

	session->add_child_nocopy (*options);
	session->add_child_nocopy (*sources);
	session->add_child_nocopy (*regions);
	session->add_child_nocopy (*playlists);
	session->add_child_nocopy (*diskstreams);
	session->add_child_nocopy (*routes);
	session->add_child_nocopy (*locations);

	/* write RPP */
	printf("Writing project...\n");

	char **tracks;
	int numtracks;
	//sqlite3_get_table(db, "SELECT d3.value FROM data d1, data d2, data d3 WHERE d1.property = 'OMFI:OOBJ:ObjClass' AND d1.value = 'CMOB' AND d2.object = d1.object AND d2.property = 'OMFI:MOBJ:Slots' AND d2.value = d3.object", &tracks, &numtracks, 0, 0);
	sqlite3_get_table(db, "SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE property = 'OMFI:OOBJ:ObjClass' AND value = 'CMOB' LIMIT 1) AND property = 'OMFI:MOBJ:Slots')", &tracks, &numtracks, 0, 0);
	int i;
	for (i = 1; i <= numtracks; i++) {
		printf("Processing track %d / %d...\n", i, numtracks);
		int descCount;
		char **desc;
		//sqlite3_get_table(db, sqlite3_mprintf("SELECT d2.value FROM data d1, data d2 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:MSLT:Segment' AND d1.value LIKE d2.object AND d2.value LIKE 'SEQU' LIMIT 1", tracks[i]), &desc, &descCount, 0, 0);
		sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND value = 'SEQU' LIMIT 1", tracks[i]), &desc, &descCount, 0, 0);
		sqlite3_free_table(desc);

		if (descCount > 0) {

			/* create a new route, which will mean that we need a new diskstream and playlist too
			 */

			XMLNode* route = new_route_node ();
			XMLNode* playlist = new_playlist_node ();
			XMLNode* diskstream = new_diskstream_node ();

			/* route and playlist both need diskstream ID */

			route->add_property ("diskstream-id", diskstream->property ("id")->value());
			playlist->add_property ("orig-diskstream-id", diskstream->property ("id")->value());

			DEBUG("<TRACK\n");

			char **name;
			int nameCount;
			//sqlite3_get_table(db, sqlite3_mprintf("SELECT d2.offset, d2.length FROM data d1, data d2 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:MSLT:TrackDesc' AND d2.object LIKE d1.value AND d2.property LIKE 'OMFI:TRKD:TrackName' LIMIT 1", tracks[i]), &name, &nameCount, 0, 0);
			sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MSLT:TrackDesc' LIMIT 1) AND property = 'OMFI:TRKD:TrackName' LIMIT 1", tracks[i]), &name, &nameCount, 0, 0);
			if (nameCount > 0) {
				uint32_t nOffs;
				uint32_t nLen;
				if (get_offset_and_length (name[2], name[3], nOffs, nLen)) {
					char* nBuf = read_name (nOffs, nLen);
					DEBUG("NAME \"%s\"\n", nBuf);
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
			sqlite3_get_table(db, sqlite3_mprintf("SELECT offset FROM data WHERE object = %s AND property = 'OMFI:MSLT:EditRate' LIMIT 1", tracks[i]), &rate, &rateCount, 0, 0);
			int num = 1, denom = 1;
			if (rateCount > 0) {
				uint32_t rOffs = atoi(rate[1]);
				//sscanf(rate[1], "%d", &rOffs);
				fseek(file, rOffs, SEEK_SET);
				fread(&denom, 4, 1, file);
				denom = e32(denom);
				fread(&num, 4, 1, file);
				num = e32(num);
			}
			double frameRate = (double) num / (double) denom;
			sqlite3_free_table(rate);

			char **items;
			int itemCount;
			//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.value FROM data d1, data d2, data d3 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:MSLT:Segment' AND d2.object LIKE d1.value AND d2.property LIKE 'OMFI:SEQU:Components' AND d3.object LIKE d2.value", tracks[i]), &items, &itemCount, 0, 0);
			sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SEQU:Components' LIMIT 1)", tracks[i]), &items, &itemCount, 0, 0);
			double position = 0.0;
			int j;
			double fadeTime = 0.0;

			for (j = 1; j <= itemCount; j++) {
				//MessageBox(NULL, items[j],"Info",MB_OK);
				printf("  item %d / %d\n", j, itemCount);
				char **len;
				int lenCount;
				double length = 0.0;
				int lenFrames = 0;
				sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND property = 'OMFI:CPNT:Length' LIMIT 1", items[j]), &len, &lenCount, 0, 0);
				if (lenCount > 0) {
					char **type;
					int typeCount;

					sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND property = 'OMFI:OOBJ:ObjClass' LIMIT 1", items[j]), &type, &typeCount, 0, 0);
					if (typeCount > 0) {
						lenFrames = atoi(len[1]);
						//sscanf(len[1],"%d", &lenFrames);
						length = lenFrames * frameRate;

						if (!strcmp(type[1], "TRAN")) {
							position -= length;
							char **effID;
							int effIDCount;
							//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.offset, d3.length FROM data d1, data d2, data d3 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:TRAN:Effect' AND d2.object LIKE d1.value AND d2.property LIKE 'OMFI:EFFE:EffectKind' AND d3.object LIKE d2.value AND d3.property LIKE 'OMFI:EDEF:EffectID' LIMIT 1", items[j]), &effID, &effIDCount, 0, 0);
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
							sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1)) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &itemName, &itemNameCount, 0, 0);

							XMLNode* region = new_region_node ();
							playlist->add_child_nocopy (*region);

							region->add_property ("name", "iNeedAName");
							snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (position));
							region->add_property ("position", sbuf);
							snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (length));
							region->add_property ("length", sbuf);
	
							DEBUG("<ITEM\nPOSITION %.8f\nLENGTH %.8f\nFADEIN 0 %.8f 0.0\nLOOP 0\n", position, length, fadeTime);

							char **startTime;
							int startTimeCount;
							//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.value FROM data d1, data d2, data d3 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:NEST:SLOTS' AND d2.object LIKE d1.value AND d3.object LIKE d2.value AND d3.property LIKE 'OMFI:SCLP:StartTime' LIMIT 1", items[j]), &startTime, &startTimeCount, 0, 0);
							sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1)) AND property = 'OMFI:SCLP:StartTime' LIMIT 1", items[j]), &startTime, &startTimeCount, 0, 0);
							if (startTimeCount > 0) {
								int soffs = atoi(startTime[1]);
								//sscanf(startTime[1],"%d", &soffs);
								snprintf (sbuf, sizeof (sbuf), "%" PRId64, (int64_t) soffs);
								region->add_property ("start", sbuf);
								DEBUG("SOFFS %.14f\n", (double) soffs * frameRate);
							} else {
								region->add_property ("start", "0");
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
								if (itemNameCount > 0) {
									uint32_t inOffs;
									uint32_t inLen;
									if (get_offset_and_length (itemName[2], itemName[3], inOffs, inLen)) {
										char *inBuf = read_name (inOffs, inLen);
										// ctx->AddLine("NAME \"%s\"", inBuf);
										DEBUG("NAME \"%s\"\n", inBuf);
										region->add_property ("name", inBuf);
										free (inBuf);
									}
								}
								uint32_t sfOffs;
								uint32_t sfLen;
								if (get_offset_and_length (sourceFile[2], sourceFile[3], sfOffs, sfLen)) {
									char *sfBuf = read_name (sfOffs, sfLen);
									
									if ((source = get_known_source (sfBuf)) == 0) {
										source = new_source_node ();
									}
									
									region->add_property ("source-0", source->property ("id")->value());
									source->add_property ("name", sfBuf);
									add_source (sfBuf, source);
									
									free (sfBuf);
								}
							} else {
								char **fallbackFile;
								int fallbackFileCount;
								//sqlite3_get_table(db, sqlite3_mprintf("SELECT d9.object FROM data d1, data d2, data d3, data d4, data d5, data d6, data d7, data d8, data d9 WHERE d1.object LIKE '%s' AND d1.property LIKE 'OMFI:NEST:Slots' AND d2.object LIKE d1.value AND d3.object LIKE d2.value AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
								sqlite3_get_table(db, sqlite3_mprintf("SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:NEST:Slots' LIMIT 1) LIMIT 3) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
								if (fallbackFileCount > 0) {

									if (itemNameCount > 0) {
										uint32_t inOffs;
										uint32_t inLen;
										if (get_offset_and_length (itemName[2], itemName[3], inOffs, inLen)) {
											char *inBuf = read_name (inOffs, inLen);
											DEBUG("NAME \"%s\"\n", inBuf);
											region->add_property ("name", inBuf);
											free (inBuf);
										}
									}

									DEBUG("<SOURCE WAVE\nFILE \"%s\"\n>\n", fallbackFile[1]);
									
									if ((source = get_known_source (fallbackFile[1])) == 0) {
										source = new_source_node ();
									}
									
									region->add_property ("source-0", source->property ("id")->value());
									source->add_property ("name", fallbackFile[1]);
									add_source (fallbackFile[1], source);

								}
								sqlite3_free_table(fallbackFile);
							}
              
							sqlite3_free_table(sourceFile);
							sqlite3_free_table(itemName);
							position += length;

						} else if (!strcmp(type[1], "SCLP")) {

							char **itemName;
							int itemNameCount;
							//sqlite3_get_table(db, sqlite3_mprintf("SELECT d5.offset, d5.length FROM data d3, data d4, data d5 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &itemName, &itemNameCount, 0, 0);
							sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &itemName, &itemNameCount, 0, 0);

							XMLNode* region = new_region_node ();
							playlist->add_child_nocopy (*region);

							region->add_property ("name", "iNeedAName");
							snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (position));
							region->add_property ("position", sbuf);
							snprintf (sbuf, sizeof (sbuf), "%" PRId64, llrintf (length));
							region->add_property ("length", sbuf);

							DEBUG("<ITEM\nPOSITION %.8f\nLENGTH %.8f\nFADEIN 0 %.8f 0.0\nLOOP 0\n", position, length, fadeTime);
							fadeTime = 0.0;

							char **startTime;
							int startTimeCount;
							//sqlite3_get_table(db, sqlite3_mprintf("SELECT d3.value FROM data d3 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:StartTime'", items[j]), &startTime, &startTimeCount, 0, 0);
							sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:StartTime' LIMIT 1", items[j]), &startTime, &startTimeCount, 0, 0);
							if (startTimeCount > 0) {
								int soffs = atoi(startTime[1]);
								//sscanf(startTime[1],"%d", &soffs);
								DEBUG("SOFFS %.14f\n", (double) soffs * frameRate);
								snprintf (sbuf, sizeof (sbuf), "%" PRId64, (int64_t) soffs);
								region->add_property ("start", sbuf);
							} else {
								region->add_property ("start", "0");
							}
							sqlite3_free_table(startTime);

							char **sourceFile;
							int sourceFileCount;
							//sqlite3_get_table(db, sqlite3_mprintf("SELECT d10.offset, d10.length FROM data d3, data d4, data d5, data d6, data d7, data d8, data d9, data d10 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MOBJ:MobID' AND d10.object LIKE d9.object AND d10.property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &sourceFile, &sourceFileCount, 0, 0);
							sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property LIKE 'OMFI:MOBJ:Name' LIMIT 1", items[j]), &sourceFile, &sourceFileCount, 0, 0);
              
							if (sourceFileCount > 0) {
								if (itemNameCount > 0) {
									uint32_t inOffs = atoi(itemName[2]);
									uint32_t inLen = atoi(itemName[3]);
									if (get_offset_and_length (itemName[2], itemName[3], inOffs, inLen)) {
										char *inBuf = read_name (inOffs, inLen);
										DEBUG("NAME \"%s\"\n", inBuf);
										region->add_property ("name", inBuf);
										free (inBuf);
									}
								}
								uint32_t sfOffs = atoi(sourceFile[2]);
								uint32_t sfLen = atoi(sourceFile[3]);
								if (get_offset_and_length (sourceFile[2], sourceFile[3], sfOffs, sfLen)) {
									char *sfBuf = read_name (sfOffs, sfLen);
									
									if ((source = get_known_source (sfBuf)) == 0) {
										source = new_source_node ();
									}
									
									region->add_property ("source-0", source->property ("id")->value());
									source->add_property ("name", sfBuf);
									add_source (sfBuf, source);
									
									DEBUG("<SOURCE WAVE\nFILE \"%s\"\n>\n", sfBuf);
									free (sfBuf);
								}
							} else {
								char **fallbackFile;
								int fallbackFileCount;
								//sqlite3_get_table(db, sqlite3_mprintf("SELECT d9.object FROM data d3, data d4, data d5, data d6, data d7, data d8, data d9 WHERE d3.object LIKE '%s' AND d3.property LIKE 'OMFI:SCLP:SourceID' AND d4.value LIKE d3.value AND d4.property LIKE 'OMFI:MOBJ:MobID' AND d5.object LIKE d4.object AND d5.property LIKE 'OMFI:MOBJ:Slots' AND d6.object LIKE d5.value AND d7.object LIKE d6.value AND d7.property LIKE 'OMFI:MSLT:Segment' AND d8.object LIKE d7.value AND d8.property LIKE 'OMFI:SCLP:SourceID' AND d9.value LIKE d8.value AND d9.property LIKE 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
								sqlite3_get_table(db, sqlite3_mprintf("SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Slots' LIMIT 1) LIMIT 1) AND property = 'OMFI:MSLT:Segment' LIMIT 1) AND property = 'OMFI:SCLP:SourceID' LIMIT 1)AND property = 'OMFI:SCLP:SourceID' LIMIT 1) AND property LIKE 'OMFI:MDAT:MobID' LIMIT 1", items[j]), &fallbackFile, &fallbackFileCount, 0, 0);
								if (fallbackFileCount > 0) {
									if (itemNameCount > 0) {
										uint32_t inOffs = atoi(itemName[2]);
										uint32_t inLen = atoi(itemName[3]);
										if (get_offset_and_length (itemName[2], itemName[3], inOffs, inLen)) {
											char *inBuf = read_name (inOffs, inLen+1);
											DEBUG("NAME \"%s\"\n", inBuf);
											free (inBuf);
										}
									}

									DEBUG("<SOURCE WAVE\nFILE \"%s\"\n>\n", fallbackFile[1]);

									if ((source = get_known_source (fallbackFile[1])) == 0) {
										source = new_source_node ();
									}

									region->add_property ("source-0", source->property ("id")->value());
									source->add_property ("name", fallbackFile[1]);
									add_source (fallbackFile[1], source);
								}
								sqlite3_free_table(fallbackFile);
							}
              
							sqlite3_free_table(sourceFile);
							sqlite3_free_table(itemName);
							position += length;
						}
            
					}
					sqlite3_free_table(type);
				}
				sqlite3_free_table(len);
			}
			sqlite3_free_table(items);
		}
	}

	sqlite3_free_table(tracks);

	id_counter++;
	snprintf (sbuf, sizeof (sbuf), "%" PRId64, id_counter);
	session->add_property ("id-counter", sbuf);

	XMLTree xml;

	xml.set_root (session);

	vector<string> v;

	v.push_back (base_dir);
	v.push_back (session_name);
	v.push_back (session_name + ".ardour");

	xml.write (Glib::build_filename(v).c_str());
}

void 
OMF::name_types () 
{
	/* Add built-in types */
	sqlite3_exec(db, "INSERT INTO lookup VALUES (1, 'TOC property 1')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (2, 'TOC property 2')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (3, 'TOC property 3')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (4, 'TOC property 4')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (5, 'TOC property 5')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (6, 'TOC property 6')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (7, '(Type 7)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (8, '(Type 8)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (9, '(Type 9)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (10, '(Type 10)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (11, '(Type 11)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (12, '(Type 12)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (13, '(Type 13)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (14, '(Type 14)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (15, '(Type 15)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (16, '(Type 16)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (17, '(Type 17)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (18, '(Type 18)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (19, 'TOC Value')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (20, '(Type 20)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (21, 'String')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (22, '(Type 22)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (23, 'Type Name')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (24, 'Property Name')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (25, '(Type 25)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (26, '(Type 26)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (27, '(Type 27)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (28, '(Type 28)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (29, '(Type 29)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (30, '(Type 30)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (31, 'Referenced Object')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (32, 'Object')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (33, '(Type 33)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (34, '(Type 34)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (35, '(Type 35)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (36, '(Type 36)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (37, '(Type 37)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (38, '(Type 38)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (39, '(Type 39)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (40, '(Type 40)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (41, '(Type 41)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (42, '(Type 42)')", 0, 0, 0);
	
	/* Assign type and property values to names */
	sqlite3_exec(db, "UPDATE data SET property = (SELECT name FROM lookup WHERE property = key), type = (SELECT name FROM lookup WHERE type = key)", 0, 0, 0);
	sqlite3_exec(db, "DROP TABLE lookup", 0, 0, 0);
}

int 
OMF::load (const string& path)
{ 
	if ((file = fopen(path.c_str(), "rb")) == 0) {
		MessageBox(NULL, "Cannot open file","OMF Error", MB_OK);
		return -1;
	}

	/* --------------- */
	char *fname = (char*) malloc (path.size()+5);

	strcpy(fname, path.c_str());
	strcat(fname, ".db3");
	//remove(fname);
	if(sqlite3_open(":memory:", &db)) {
		char error[512];
		sprintf(error, "Can't open database: %s", sqlite3_errmsg(db));
		MessageBox(NULL, error,"OMF Error", MB_OK);
		sqlite3_close(db);
		return -3;
	}
	sqlite3_exec(db, "BEGIN", 0, 0, 0);
	sqlite3_exec(db, "CREATE TABLE data (object, property, type, value, offset, length)", 0, 0, 0);
	sqlite3_exec(db, "CREATE TABLE lookup (key, name)", 0, 0, 0);
  
	uint8_t magic[8];
	fseek(file, -24, SEEK_END);
	fread(magic, 8, 1, file);
	if ((magic[0] != 0xa4) | (magic[1] != 0x43) | (magic[2] != 0x4d) | (magic[3] != 0xa5) | (magic[4] != 0x48) | (magic[5] != 0x64) | (magic[6] != 0x72) | (magic[7] != 0xd7)) {
		MessageBox(NULL, "No valid OMF file.","OMF Error", MB_OK);
		return -4;
	}

	uint16_t bSize, version;
	fseek(file, -12, SEEK_END);
	fread(&version, 2, 1, file);
	bigEndian = false;
	if ((version == 1) | (version == 256)) {
		MessageBox(NULL, "You tried to open an OMF1 file.\nOMF1 is not supported.","OMF Error", MB_OK);
		return -2;
	} else if (version == 512) {
		bigEndian = true;
	} else if (version != 2) {
		MessageBox(NULL, "You tried to open a corrupted file.","OMF Error", MB_OK);
		return -2;
	}

	uint32_t tocStart, tocSize;
	fseek(file, -14, SEEK_END);
	fread(&bSize, 2, 1, file);
	bSize = e16(bSize);
  
	fseek(file, -8, SEEK_END);
	fread(&tocStart, 4, 1, file);
	tocStart = e32(tocStart);

	fseek(file, -4, SEEK_END);
	fread(&tocSize, 4, 1, file);
	tocSize = e32(tocSize);
	DEBUG ("block size: %d\n toc start: %d\n  toc size: %d\n", bSize, tocStart, tocSize);
  
    
	/* Calculate number of TOC blocks */
	uint32_t tocBlocks = tocSize / (bSize * 1024) + 1;
	DEBUG ("toc blocks: %d\n", tocBlocks);
	/* ------------------------------ */

	time_t globalstart, starttime, endtime;
	time(&globalstart);
	starttime = globalstart;
	INFO ("Parsing TOC... ");

	/* Go through TOC blocks */
	uint32_t j;
	uint32_t currentObj = 0;
	uint32_t currentProp = 0;
	uint32_t currentType = 0;
	char skip = 0;
	//uint64_t len = 0;
	for (j = 0; j < tocBlocks; j++) {
		uint32_t currentBlock = tocStart + j * 1024 * bSize; // Start at beginning of current block
		uint32_t currentPos;
		for (currentPos = currentBlock; currentPos < currentBlock + 1024 * bSize; currentPos++) {
			if (currentPos > tocStart + tocSize) break; // Exit at end of TOC
			char cByte; // TOC control byte
			fseek(file, currentPos, SEEK_SET);
			fread(&cByte, 1, 1, file);
      
			/* New object */
			if (cByte == 1) {
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&currentObj, 4, 1, file);
				currentObj = e32(currentObj);
				fseek(file, currentPos + 5, SEEK_SET);
				fread(&currentProp, 4, 1, file);
				currentProp = e32(currentProp);
				fseek(file, currentPos + 9, SEEK_SET);
				fread(&currentType, 4, 1, file);
				currentType = e32(currentType);
				DEBUG("---------------------\n");
				DEBUG("   object: 0x%x\n", currentObj);
				DEBUG(" property: 0x%x\n", currentProp);
				DEBUG("     type: 0x%x\n", currentType);
				skip = 0;
				currentPos += 12; // Skip the bytes that were just read
			}
			/* ---------- */
      
      
			/* New property */
			else if (cByte == 2) {
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&currentProp, 4, 1, file);
				currentProp = e32(currentProp);
				fseek(file, currentPos + 5, SEEK_SET);
				fread(&currentType, 4, 1, file);
				currentType = e32(currentType);
				DEBUG(" property: 0x%x\n", currentProp);
				DEBUG("     type: 0x%x\n", currentType);
				skip = 0;
				currentPos += 8;
			}
			/* ------------ */
      
      
			/* New type */
			else if (cByte == 3) {
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&currentType, 4, 1, file);
				currentType = e32(currentType);
				DEBUG("     type: 0x%x\n", currentType);
				skip = 0;
				currentPos += 4;
			}
			/* -------- */
      
      
			/* (unused) */
			else if (cByte == 4) {
				currentPos += 4;
			}
			/* -------- */
      
      
			/* Reference to a value - 4/8 byte offset, 4/8 byte size */
			else if ((cByte == 5) | (cByte == 6) | (cByte == 7) | (cByte == 8)) {
				if (!skip) {
					uint32_t offset32 = 0;
					uint32_t length32 = 0;
					uint64_t dataOffset = 0;
					uint64_t dataLength = 0;
					if ((cByte == 5) | (cByte == 6)) {
						fseek(file, currentPos + 1, SEEK_SET);
						fread(&offset32, 4, 1, file);
						fseek(file, currentPos + 5, SEEK_SET);
						fread(&length32, 4, 1, file);
						dataOffset = e32(offset32);
						dataLength = e32(length32);
					} else {
						fseek(file, currentPos + 1, SEEK_SET);
						fread(&dataOffset, 8, 1, file);
						dataOffset = e64(dataOffset);
						fseek(file, currentPos + 9, SEEK_SET);
						fread(&dataLength, 8, 1, file);
						dataLength = e64(dataLength);
					}
					DEBUG("   offset: %d\n", dataOffset);
					DEBUG("   length: %d\n", dataLength);
    		  
					if (currentType == 21) {
						char* string = (char*) malloc((uint32_t) dataLength);
						fseek(file, dataOffset, SEEK_SET);
						fread(string, dataLength, 1, file);
						char* query = sqlite3_mprintf("INSERT INTO lookup VALUES(%d, '%s')",currentObj, string);
						sqlite3_exec(db, query, 0, 0, 0);
						sqlite3_free(query);
					} else if (currentType == 32){
						uint32_t object = 0;
						fseek(file, dataOffset, SEEK_SET);
						fread(&object, 4, 1, file);
						object = e32(object);
						char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, object);
						sqlite3_exec(db, query, 0, 0, 0);
						sqlite3_free(query);
						if (dataLength == 16) {
							DEBUG("   offset: %lld\n", dataOffset + 8);
							DEBUG("   length: %lld\n", dataLength);
							fseek(file, dataOffset + 8, SEEK_SET);
							fread(&object, 4, 1, file);
							object = e32(object);
							char* query2 = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, object);
							sqlite3_exec(db, query2, 0, 0, 0);
							sqlite3_free(query2);
						}
					} else {
						char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, '', %lld, %lld)",currentObj, currentProp, currentType, dataOffset, dataLength);
						sqlite3_exec(db, query, 0, 0, 0);
						sqlite3_free(query);
					}
				}
				if ((cByte == 5) | (cByte == 6)) {
					currentPos += 8;
				} else {
					currentPos += 16;
				}
			}
			/* ----------------------------------------------------- */
      
      
			/* Zero byte value */
			else if (cByte == 9) {
				if (!skip) {
					char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, 'true', -1, -1)",currentObj, currentProp, currentType);
					sqlite3_exec(db, query, 0, 0, 0);
					sqlite3_free(query);
					DEBUG("    value: true\n");
				}
			}
			/* --------------- */


			/* Immediate value */
			else if ((cByte == 10) | (cByte == 11) | (cByte == 12) | (cByte == 13) | (cByte == 14)) {
				if (!skip) {
					uint32_t data = 0;
					fseek(file, currentPos + 1, SEEK_SET);
					fread(&data, 4, 1, file);
					data = e32(data);
					char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, data);
					sqlite3_exec(db, query, 0, 0, 0);
					sqlite3_free(query);
					DEBUG("    value: %d\n", data);
          
				}
				currentPos += 4;
			}
			/* --------------- */
      
      
			/* Reference list */
			else if (cByte == 15) {
				uint32_t data = 0;
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&data, 4, 1, file);
				data = e32(data);
				char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, data);
				sqlite3_exec(db, query, 0, 0, 0);
				sqlite3_free(query);
				DEBUG("reference: 0x%x\n", data);
				skip = 1;
				currentPos += 4;
			}
			/* -------------- */
			else {
				break;
			}
      
		}
	}
	/* --------------------- */
	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;
  
	INFO("Assigning type and property names... ");
	name_types ();
	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	bool isAvid = false;

	/* resolve ObjRefArrays */
	char **arrays;
	int arrayCount;
	int l;
	INFO("Resolving ObjRefArrays ");
	sqlite3_get_table(db, "SELECT * FROM data WHERE type LIKE 'omfi:ObjRefArray' AND value = ''", &arrays, &arrayCount, 0, 0);
	INFO("(%d to be processed)... ", arrayCount);
	sqlite3_exec(db,"DELETE FROM data WHERE type LIKE 'omfi:ObjRefArray' AND value = ''",0,0,0); 
	for (l = 6; l <= arrayCount * 6; l+=6) {
		uint16_t counter;
		uint32_t arrOffs = atoi(arrays[l+4]);
		uint32_t arrLen = atoi(arrays[l+5]);
		fseek(file, arrOffs, SEEK_SET);
		fread(&counter, 2, 1, file);
		counter = e16(counter);
		if (arrLen = 4 * counter + 2) {
			isAvid = true;
			currentObj++;
			DEBUG("currentObj: %d - references:", currentObj);
			for (counter = 2; counter < arrLen; counter += 4) {
				uint32_t temp;
				fseek(file, arrOffs + counter, SEEK_SET);
				fread(&temp, 4, 1, file);
				temp = e32(temp);
				DEBUG(" %d", temp);
				sqlite3_exec(db, sqlite3_mprintf("INSERT INTO data VALUES (%d, 'Referenced Object', 'Object', %d, -1, -1)", currentObj, temp), 0, 0, 0);
			}
			DEBUG("\nData: %s | %s | %s | %d | -1 | -1\n", arrays[l], arrays[l+1], arrays[l+2], currentObj);
			sqlite3_exec(db, sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', '%s', %d, -1, -1)", arrays[l], arrays[l+1], arrays[l+2], currentObj), 0, 0, 0);
		}
	}
	sqlite3_free_table(arrays);
	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;
	/* -------------------- */


	//return -1;
	/*char **refs;
	  int refCount;
	  int currentRef;
	  printf("Resolving ObjRefs...\n");
	  sqlite3_get_table(db,"SELECT object, property, value FROM data WHERE type = 'omfi:ObjRef'", &refs, &refCount, 0, 0);
	  printf("temporary table created\n");
	  for (currentRef = 3; currentRef <= refCount * 3; currentRef += 3) {
	  DEBUG("%d / %d\n", currentRef/3, refCount);
	  char **target;
	  int targetCount;
	  sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND type = 'Object' LIMIT 1", refs[currentRef+2]), &target, &targetCount, 0, 0);
	  DEBUG("temporary table filled\n");
	  if (targetCount > 0) {
	  //sqlite3_exec(db,sqlite3_mprintf("DELETE FROM data WHERE object = %s", refs[currentRef+2]),0,0,0);
	  DEBUG("unused reference deleted\n");
	  sqlite3_exec(db,sqlite3_mprintf("UPDATE data SET value = %s WHERE object LIKE '%s' AND property LIKE '%s' LIMIT 1", target[1], refs[currentRef], refs[currentRef+1]),0,0,0);
	  printf("temporary data inserted\n");
	  }
	  sqlite3_free_table(target);
	  }
	  sqlite3_free_table(refs);
	  printf("temporary table deleted\n"); */
  
	if (!isAvid) {
		INFO("Resolving ObjRefs ");
		sqlite3_exec(db,"CREATE TABLE reference (object1, property1, value1)",0,0,0);
		sqlite3_exec(db,"INSERT INTO reference SELECT object, property, value FROM data WHERE type LIKE 'omfi:ObjRef'",0,0,0);
		sqlite3_exec(db,"CREATE TABLE objects (object2, value2)",0,0,0);
		sqlite3_exec(db,"INSERT INTO objects SELECT object, value FROM data WHERE type LIKE 'Object'",0,0,0);
		sqlite3_exec(db,"UPDATE reference SET value1 = (SELECT value2 FROM objects WHERE object2 = value1)",0,0,0);
		//sqlite3_exec(db,"UPDATE data SET value = (SELECT value1 FROM references WHERE object1 = object) WHERE ",0,0,0);
		char **refs;
		int refCount;
		int currentRef;
  
		sqlite3_get_table(db,"SELECT * FROM reference", &refs, &refCount, 0, 0);
		INFO ("(%d to be processed)... ", refCount);
		for (currentRef = 3; currentRef <= refCount * 3; currentRef += 3) {
			DEBUG("%d / %d: object %s, property %s, value %s\n", currentRef/3, refCount, refs[currentRef], refs[currentRef+1], refs[currentRef+2]);
			sqlite3_exec(db,sqlite3_mprintf("DELETE FROM data WHERE object = %s AND property = '%s'", refs[currentRef], refs[currentRef+1]),0,0,0);
			sqlite3_exec(db,sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', 'omfi:ObjRef', %s, -1, -1)", refs[currentRef], refs[currentRef+1], refs[currentRef+2]),0,0,0);
		}
		sqlite3_free_table(refs);
	}
	DEBUG("temporary table deleted\n");
  
	/*sqlite3_get_table(db,"SELECT object, property, value FROM data WHERE type LIKE 'omfi:ObjRef'", &refs, &refCount, 0, 0);
	  printf("%d\n", refCount);
	  for (currentRef = 3; currentRef <= refCount * 3; currentRef += 3) {
	  printf("%d / %d: object %s, property %s, value %s\n", currentRef/3, refCount, refs[currentRef], refs[currentRef+1], refs[currentRef+2]);
	  }
	  sqlite3_free_table(refs);
	  }*/


	/* resolve ObjRefs *
	   printf("Resolving ObjRefs...\n");
	   sqlite3_exec(db,"CREATE TABLE temp (object, property, type, value, offset, length)",0,0,0);
	   printf("temporary table created\n");
	   sqlite3_exec(db,"INSERT INTO temp SELECT d1.object, d1.property, d1.type, d2.value, d1.offset, d1.length FROM data d1, data d2 WHERE d1.type = 'omfi:ObjRef' AND d1.value = d2.object AND d2.type = 'Object'",0,0,0);
	   printf("temporary table filled\n");
	   //sqlite3_exec(db,"DELETE FROM data WHERE object IN (SELECT value FROM data WHERE type LIKE 'omfi:ObjRef')",0,0,0);
	   sqlite3_exec(db,"DELETE FROM data WHERE object IN (SELECT object FROM temp) AND type = 'omfi:ObjRef'",0,0,0);
	   printf("unused referenced deleted\n");
	   sqlite3_exec(db,"INSERT INTO data SELECT * FROM temp",0,0,0);
	   printf("temporary data inserted\n");
	   sqlite3_exec(db,"DROP TABLE temp",0,0,0);
	   printf("temporary table deleted\n");
	   * --------------- */

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	//return -1;
	/* resolve UIDs */
	INFO("Resolving UIDs... ");
	char **mobID;
	int mobIDCount;
	int currentID;
	sqlite3_get_table(db,"SELECT object, property, offset FROM data WHERE type LIKE 'omfi:UID'", &mobID, &mobIDCount, 0, 0);
	sqlite3_exec(db,"DELETE FROM data WHERE type LIKE 'omfi:UID'",0,0,0);
	for (currentID = 3; currentID <= mobIDCount * 3; currentID += 3) {
		uint32_t mobIDoffs = atoi(mobID[currentID+2]);
		//sscanf(mobID[currentID+2], "%d", &mobIDoffs);
		int mobBuffer[3];
		fseek(file, mobIDoffs, SEEK_SET);
		fread(mobBuffer, 12, 1, file);
		sqlite3_exec(db,sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', 'omfi:UID', '%d %d %d', -1, -1)", mobID[currentID], mobID[currentID + 1], mobBuffer[0], mobBuffer[1], mobBuffer[2]),0,0,0);
	}
	sqlite3_free_table(mobID);
	/* ------------ */

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;
  
	//return -1;
  
	/* extract media data */
	printf("Extracting media data...\n");
	char **objects;
	int objectsCount, k;
	//sqlite3_exec(db,"CREATE TABLE names (UID, value)",0,0,0);
	sqlite3_get_table(db, "SELECT object, offset, length FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE property = 'OMFI:HEAD:MediaData' LIMIT 1)) AND type = 'omfi:DataValue'", &objects, &objectsCount, 0, 0);
	for (k = 3; k <= objectsCount * 3; k += 3) {
		char **fileName;
		int fileNameCount;
		FILE *fd;
		char *fnTemp;
		sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MDAT:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Name' LIMIT 1", objects[k]), &fileName, &fileNameCount, 0, 0);
		if (fileNameCount > 0) {
			uint32_t fnOffs = atoi(fileName[2]);
			uint32_t fnLen = atoi(fileName[3]);
			if (get_offset_and_length (fileName[2], fileName[3], fnOffs, fnLen)) {
				char *fnBuf = (char*) malloc(fnLen+1);
				fseek(file, fnOffs, SEEK_SET);
				fread(fnBuf, fnLen, 1, file);
				fnBuf[fnLen] = '\0';

				audiofile_path_vector.push_back (fnBuf);
				std::string full_path = Glib::build_filename (audiofile_path_vector);
				audiofile_path_vector.pop_back ();

				fd = fopen(full_path.c_str(), "wb");

				/* this is just the "name" */
				fnTemp = fnBuf;
			} else {
				INFO ("Skip unnamed media file\n");
				continue;
			}
		} else {
			fnTemp = objects[k];
			fd = fopen(objects[k], "wb");
		}
		if(fd == NULL){
			char error[255];
			sprintf(error, "Can't create file [%s] (object %s)", fnTemp, objects[k]);
			MessageBox(NULL,error,"OMF Error", MB_OK);
			sqlite3_exec(db, "COMMIT", 0, 0, 0);
			sqlite3_close(db);
			return 1;
		} else {
			INFO("Writing file %s (object %s): ", fnTemp, objects[k]);
			XMLNode* source = new_source_node ();
			source->add_property ("name", fnTemp);
			add_source (fnTemp, source);
		}
		uint32_t foffset = atoi(objects[k+1]);
		uint32_t flength = atoi(objects[k+2]);
		uint32_t written = 0;
		//sscanf(, "%d", &foffset);
		//sscanf(objects[k+2], "%d", &flength);
		//foffset = 
		int blockSize = 1024;
		uint32_t currentBlock;
		char* buffer = (char*) malloc(blockSize);
		fseek(file, foffset, SEEK_SET);
		for (currentBlock = 0; currentBlock < flength / blockSize; currentBlock++) {
			fread(buffer, blockSize, 1, file);
			written += fwrite(buffer, 1, blockSize, fd);
		}
		fread(buffer, flength % blockSize, 1, file);
		written += fwrite(buffer, 1, flength % blockSize, fd);
		INFO("%d of %d bytes\n", written, flength);
		fclose(fd);
		sqlite3_free_table(fileName);
	}
	sqlite3_free_table(objects);
	/* ------------------ */

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	/* resolve ClassIDs */
	printf("Resolving ClassIDs ");
	char **classID;
	int classIDCount;
	int currentClsID;
	sqlite3_get_table(db,"SELECT object, property, value FROM data WHERE type = 'omfi:ClassID'", &classID, &classIDCount, 0, 0);
	sqlite3_exec(db,"DELETE FROM data WHERE type = 'omfi:ClassID'",0,0,0);
	INFO("(%d to be processed)... ", classIDCount);
	for (currentClsID = 3; currentClsID <= classIDCount * 3; currentClsID += 3) {
		//int clsID = (int) malloc(5);
		int clsID = atoi(classID[currentClsID+2]);
		clsID = e32(clsID);
		//sscanf(classID[currentClsID+2], "%d", &clsID);
		char clsString[5];
		strncpy(clsString, (char *) &clsID, 4);
		clsString[4] = 0;
		DEBUG("%d -> %s\n", clsID, clsString);
    
		sqlite3_exec(db,sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', 'omfi:ClassID', '%s', -1, -1)", classID[currentClsID], classID[currentClsID + 1], clsString),0,0,0);
	}
	sqlite3_free_table(classID);
	/* ---------------- */

	sqlite3_exec(db, "COMMIT", 0, 0, 0);

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	/*time(&endtime);
	  printf("Took %ld seconds\n", endtime - starttime);
	  starttime = endtime;*/
  

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	INFO("Overall time: %ld seconds\n", endtime - globalstart);

	return 0;
	/* -------- */
}

static void 
print_help (const char* execname)
{
	cout << execname 
	     << " [ -r sample-rate ]"
	     << " [ -n session-name ]"
	     << " [ -v ardour-session-version ]"
	     << endl;
	exit (1);
}

int
main (int argc, char* argv[])
{
	const char *execname = strrchr (argv[0], '/');
	const char* optstring = "r:n:v:h";
	const char* session_name = 0;
	int         sample_rate = 44100;
	int         version = 3000;

	const struct option longopts[] = {
		{ "rate", 1, 0, 'r' },
		{ "name", 1, 0, 'n' },
		{ "version", 1, 0, 'v' },
		{ "help", 1, 0, 'h' },
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
	
	OMF omf;

	omf.set_version (version);
	omf.set_sample_rate (sample_rate);
	if (session_name) {
		omf.set_session_name (session_name);
	}

	cerr << "args done, file = " << argv[optind] << endl;
	
	if (omf.init () == 0) {
		cerr << "Attempting to load " << argv[optind] << endl;

		if (omf.load (argv[optind++]) == 0) {
			omf.create_xml ();
		}
	}

	return 0;
}
