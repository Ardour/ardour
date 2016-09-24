/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <iostream>
#include <cstdlib>
#include <getopt.h>

#include <glibmm.h>

#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/stateful.h"

#include "ardour/region_factory.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/session_directory.h"
#include "ardour/source.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"

#include "evoral/Note.hpp"
#include "evoral/Sequence.hpp"

#include "common.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

bool
clone_bbt_source_to_source (boost::shared_ptr<MidiSource>  bbt_source, boost::shared_ptr<MidiSource> source,
				 const Glib::Threads::Mutex::Lock& source_lock, const double session_offset)
{
	const bool old_percussive = bbt_source->model()->percussive();

	bbt_source->model()->set_percussive (false);

	source->mark_streaming_midi_write_started (source_lock, bbt_source->model()->note_mode());

	TempoMap& map (source->session().tempo_map());

	for (Evoral::Sequence<MidiModel::TimeType>::const_iterator i = bbt_source->model()->begin(MidiModel::TimeType(), true); i != bbt_source->model()->end(); ++i) {
		const double new_time = map.quarter_note_at_beat ((*i).time().to_double() + map.beat_at_pulse (session_offset)) - (session_offset * 4.0);
		Evoral::Event<Evoral::Beats> new_ev (*i, true);
		new_ev.set_time (Evoral::Beats (new_time));
		source->append_event_beats (source_lock, new_ev);
	}

	bbt_source->model()->set_percussive (old_percussive);
	source->mark_streaming_write_completed (source_lock);

	return true;
}

boost::shared_ptr<MidiSource>
ensure_qn_source (Session* session, std::string path, boost::shared_ptr<MidiRegion> region, bool one_file_per_source)
{
	boost::shared_ptr<MidiSource> newsrc;
	string newsrc_filename;

	if (one_file_per_source) {
		newsrc_filename = region->source()->name() +  "-a54-compat.mid";
	} else {
		newsrc_filename = region->name() +  "-a54-compat.mid";
	}

	string newsrc_path = Glib::build_filename (path, newsrc_filename);

	/* create a new source if none exists and write corrected events to it.
	   if file exists, assume that it is correct.
	*/
	if (Glib::file_test (newsrc_path, Glib::FILE_TEST_EXISTS)) {
		Source::Flag flags =  Source::Flag (Source::Writable | Source::CanRename);
		newsrc = boost::dynamic_pointer_cast<MidiSource>(
			SourceFactory::createExternal(DataType::MIDI, *session,
						      newsrc_path, 1, flags));
		/* hack flags */
		XMLNode* node = new XMLNode (newsrc->get_state());

		if (node->property ("flags") != 0) {
			node->property ("flags")->set_value (enum_2_string (flags));
		}

		newsrc->set_state (*node, PBD::Stateful::loading_state_version);

		std::cout << UTILNAME << ": Using existing midi source file : " << newsrc_path << std::endl;
		std::cout << "for region : " << region->name() << std::endl;

	} else {
		newsrc = boost::dynamic_pointer_cast<MidiSource>(
			SourceFactory::createWritable(DataType::MIDI, *session,
						      newsrc_path, false, session->frame_rate()));
		Source::Lock newsrc_lock (newsrc->mutex());

		clone_bbt_source_to_source (region->midi_source(0), newsrc, newsrc_lock, region->pulse() - (region->start_beats().to_double() / 4.0));

		std::cout << UTILNAME << ": Created new midi source file " << newsrc_path << std::endl;
		std::cout << "for region : " <<  region->name() << std::endl;
	}

	return newsrc;
}

void
reset_start_and_length (Session* session, boost::shared_ptr<MidiRegion> region)
{
	/* set start_beats & length_beats to quarter note value */
	TempoMap& map (session->tempo_map());

	region->set_start_beats (Evoral::Beats ((map.pulse_at_beat (region->beat())
						 - map.pulse_at_beat (region->beat() - region->start_beats().to_double())) * 4.0));

	region->set_length_beats (Evoral::Beats ((map.pulse_at_beat (region->beat() + region->length_beats().to_double())
						  - map.pulse_at_beat (region->beat())) * 4.0));

	std::cout << UTILNAME << ": Reset start and length beats for region : " << region->name() << std::endl;
}

bool
write_one_source_per_region (Session* session)
{
	const RegionFactory::RegionMap& region_map (RegionFactory::all_regions());

	if (!region_map.size()) {
		return false;
	}

	/* for every midi region, ensure a new source and switch to it. */
	for (RegionFactory::RegionMap::const_iterator i = region_map.begin(); i != region_map.end(); ++i) {
		boost::shared_ptr<MidiRegion> mr = 0;

		if ((mr = boost::dynamic_pointer_cast<MidiRegion>((*i).second)) != 0) {
			reset_start_and_length (session, mr);
			boost::shared_ptr<MidiSource> newsrc = ensure_qn_source (session, session->session_directory().midi_path(), mr, false);

			mr->clobber_sources (newsrc);
		}
	}

	return true;
}

bool
write_one_source_per_source (Session* session)
{
	const RegionFactory::RegionMap& region_map (RegionFactory::all_regions());

	if (!region_map.size()) {
		return false;
	}

	map<PBD::ID, boost::shared_ptr<MidiSource> > old_id_to_new_source;
	/* for every midi source, ensure a new source and switch to it. */
	for (RegionFactory::RegionMap::const_iterator i = region_map.begin(); i != region_map.end(); ++i) {
		boost::shared_ptr<MidiRegion> mr = 0;
		map<PBD::ID, boost::shared_ptr<MidiSource> >::iterator src_it;

		if ((mr = boost::dynamic_pointer_cast<MidiRegion>((*i).second)) != 0) {
			reset_start_and_length (session, mr);

			if ((src_it = old_id_to_new_source.find (mr->source()->id())) != old_id_to_new_source.end()) {
				mr->clobber_sources ((*src_it).second);
			} else {
				boost::shared_ptr<MidiSource> newsrc = ensure_qn_source (session, session->session_directory().midi_path(), mr, true);
				old_id_to_new_source.insert (make_pair (mr->source()->id(), newsrc));
				mr->clobber_sources (newsrc);
			}
		}
	}

	return true;
}

static void usage (int status) {
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - convert an ardour session with 5.0 - 5.3 midi sources to be compatible with 5.4.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <session-dir> <session/snapshot-name>\n\n");
	printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -f, --force                override detection of affected sessions\n\
  -o, --output  <file>       output session snapshot name (without file suffix)\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
This Ardour-specific utility provides an upgrade path for sessions created or modified with Ardour versions 5.0 - 5.3.\n\
It creates a 5.4-compatible snapshot from affected Ardour session files.\n\
Affected versions (5.0 - 5.3 inclusive) contain a bug which caused some MIDI region properties and contents\n\
to be stored incorrectly (see more below).\n\n\
The utility will first determine whether or not a session requires any changes for 5.4 compatibility.\n\
If a session is determined to be affected by the bug, the program will take one of two approaches to correcting the problem.\n\n\
The first is to write a new MIDI source file for every existing MIDI source in the supplied snapshot.\n\
In the second approach, each MIDI region have its source converted and placed in the session midifiles directory\n\
as a new source (one source file per region).\n\
The second method is only used if the first approach cannot guarantee that the results would match the input snapshot.\n\n\
Both methods update MIDI region properties and save a new snapshot in the supplied session-dir, optionally using a supplied snapshot name (-o).\n\
The new snapshot may be used on Ardour-5.4.\n\n\
Running this utility will not alter any existing files, but it is recommended that you backup the session directory before use.\n\n\
EXAMPLE:\n\
ardour5-headless-chicken -o bantam ~/studio/leghorn leghorn\n\
will create a new snapshot file ~/studio/leghorn/bantam.ardour from ~/studio/leghorn/leghorn.ardour\n\
Converted midi sources will be created in ~/studio/leghorn/interchange/leghorn/midifiles/\n\
If the output option (-o) is omitted, the string \"-a54-compat\" will be appended to the supplied snapshot name.\n\n\
About the Bug\n\
If a session from affected versions used MIDI regions and a meter note divisor was set to anything but quarter notes,\n\
the source smf files would contain events at a PPQN value derived from BBT beats (using meter note divisor) rather than quarter-note beatss.\n\
The region start and length offsets would also be stored incorrectly.\n\
If a MIDI session only contains quarter note meter divisors, it will be unaffected.\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (status);
}

int main (int argc, char* argv[])
{
	std::string outfile;
	bool force = false;

	const char *optstring = "hfo:r:V";

	const struct option longopts[] = {
		{ "help",       0, 0, 'h' },
		{ "force",      0, 0, 'f' },
		{ "output",     1, 0, 'o' },
		{ "version",    0, 0, 'V' },
	};

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
					optstring, longopts, (int *) 0))) {
		switch (c) {

		case 'f':
			force = true;
			break;

		case 'o':
			outfile = optarg;
			break;

		case 'V':
			printf ("ardour-utils version %s\n\n", VERSIONSTRING);
			printf ("Copyright (C) GPL 2015 Robin Gareus <robin@gareus.org>\n");
			exit (0);
			break;

		case 'h':
			usage (0);
			break;

		default:
			usage (EXIT_FAILURE);
			break;
		}
	}

	if (optind + 2 > argc) {
		usage (EXIT_FAILURE);
	}
	std::cout << UTILNAME << ": hello" << std::endl;

	SessionDirectory* session_dir = new SessionDirectory (argv[optind]);
	std::string snapshot_name (argv[optind+1]);
	std::string statefile_suffix (X_(".ardour"));
	std::string pending_suffix (X_(".pending"));

	XMLTree* state_tree;

	std::string xmlpath(argv[optind]);

	if (!outfile.empty ()) {
		string file_test_path = Glib::build_filename (argv[optind], outfile + statefile_suffix);
		if (Glib::file_test (file_test_path, Glib::FILE_TEST_EXISTS)) {
			std::cout << UTILNAME << ": session file " << file_test_path << " already exists!" << std::endl;
                        return EXIT_FAILURE;
		}
	} else {
		string file_test_path = Glib::build_filename (argv[optind], snapshot_name + "-a54-compat" + statefile_suffix);
		if (Glib::file_test (file_test_path, Glib::FILE_TEST_EXISTS)) {
			std::cout << UTILNAME << ": session file " << file_test_path << "already exists!" << std::endl;
                        return EXIT_FAILURE;
		}
	}

	xmlpath = Glib::build_filename (xmlpath, legalize_for_path (snapshot_name) + pending_suffix);

	if (Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {

		/* there is pending state from a crashed capture attempt */
		std::cout << UTILNAME << ": There seems to be pending state for snapshot : " << snapshot_name << std::endl;

	}

	xmlpath = Glib::build_filename (argv[optind], argv[optind+1]);

	if (!Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {
		xmlpath = Glib::build_filename (argv[optind], legalize_for_path (argv[optind+1]) + ".ardour");
		if (!Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {
			std::cout << UTILNAME << ": session file " << xmlpath << " doesn't exist!" << std::endl;
                        return EXIT_FAILURE;
                }
        }

	state_tree = new XMLTree;

	bool writable = PBD::exists_and_writable (xmlpath) && PBD::exists_and_writable(Glib::path_get_dirname(xmlpath));

	if (!writable) {
		std::cout << UTILNAME << ": Error : The session directory must exist and be writable." << std::endl;
		return -1;
	}

	if (!state_tree->read (xmlpath)) {
		std::cout << UTILNAME << ": Could not understand session file " << xmlpath << std::endl;
		delete state_tree;
		state_tree = 0;
		return EXIT_FAILURE;
	}

	XMLNode const & root (*state_tree->root());

	if (root.name() != X_("Session")) {
		std::cout << UTILNAME << ": Session file " << xmlpath<< " is not a session" << std::endl;
		delete state_tree;
		state_tree = 0;
		return EXIT_FAILURE;
	}

	XMLProperty const * prop;

	if ((prop = root.property ("version")) == 0) {
		/* no version implies very old version of Ardour */
		std::cout << UTILNAME << ": The session " << argv[optind+1] << " has no version or is too old to be affected. exiting." << std::endl;
		return EXIT_FAILURE;
	} else {
		if (prop->value().find ('.') != string::npos) {
			/* old school version format */
			std::cout << UTILNAME << ": The session " << argv[optind+1] << " is too old to be affected. exiting." << std::endl;
			return EXIT_FAILURE;
		} else {
			PBD::Stateful::loading_state_version = atoi (prop->value().c_str());
		}
	}

	std::cout <<  UTILNAME << ": Checking snapshot : " << snapshot_name << " in directory : " << session_dir->root_path() << std::endl;

	bool midi_regions_use_bbt_beats = false;

	if (PBD::Stateful::loading_state_version == 3002 && writable) {
		XMLNode* child;
		if ((child = find_named_node (root, "ProgramVersion")) != 0) {
			if ((prop = child->property ("modified-with")) != 0) {
				std::string modified_with = prop->value ();

				const double modified_with_version = atof (modified_with.substr ( modified_with.find(" ", 0) + 1, string::npos).c_str());
				const int modified_with_revision = atoi (modified_with.substr (modified_with.find("-", 0) + 1, string::npos).c_str());

				if (modified_with_version <= 5.3 && !(modified_with_version == 5.3 && modified_with_revision >= 42)) {
					midi_regions_use_bbt_beats = true;
				}
			}
		}
	}

	XMLNode* tm_node;
	bool all_metrum_divisors_are_quarters = true;
	list<double> divisor_list;

	if ((tm_node = find_named_node (root, "TempoMap")) != 0) {
		XMLNodeList metrum;
		XMLNodeConstIterator niter;
		metrum = tm_node->children();
		for (niter = metrum.begin(); niter != metrum.end(); ++niter) {
			XMLNode* child = *niter;

			if (child->name() == MeterSection::xml_state_node_name && (prop = child->property ("divisions-per-bar")) != 0) {
				double divisions_per_bar;

				if (sscanf (prop->value().c_str(), "%lf", &divisions_per_bar) ==1) {

					if (divisions_per_bar != 4.0) {
						all_metrum_divisors_are_quarters = false;
						divisor_list.push_back (divisions_per_bar);
					}
				}
			}
		}
	} else {
		std::cout << UTILNAME << ": Session file " <<  xmlpath << " has no TempoMap node. exiting." << std::endl;
		return EXIT_FAILURE;
	}

	if (all_metrum_divisors_are_quarters && !force) {
		std::cout << UTILNAME << ": The session " << argv[optind+1] << " is clear for use in 5.4 (all divisors are quarters). Use -f to override." << std::endl;
		return EXIT_FAILURE;
	}

	/* check for multiple note divisors. if there is only one, we can create one file per source. */
	bool new_source_file_per_source = false;
	divisor_list.unique();

	if (divisor_list.size() == 1) {
		new_source_file_per_source = true;
	}

	if (midi_regions_use_bbt_beats || force) {
		if (force) {
			std::cout << UTILNAME << ": Forced update of snapshot : " << argv[optind+1] << std::endl;
		}

		SessionUtils::init();
		Session* s = 0;

		std::cout <<  UTILNAME << ": Loading snapshot." << std::endl;

		s = SessionUtils::load_session (argv[optind], argv[optind+1]);
		if (new_source_file_per_source) {
			std::cout << UTILNAME << ": Will create one MIDI file per source." << std::endl;

			if (!write_one_source_per_source (s)) {
				std::cout << UTILNAME << ": The snapshot " << argv[optind+1] << " is clear for use in 5.4 (no midi regions). exiting." << std::endl;
				SessionUtils::unload_session(s);
				SessionUtils::cleanup();
				return EXIT_FAILURE;
			}
		} else {
			std::cout << UTILNAME << ": Will create one MIDI file per midi region." << std::endl;

			if (!write_one_source_per_region (s)) {
				std::cout << UTILNAME << ": The snapshot " << argv[optind+1] << " is clear for use in 5.4 (no midi regions). exiting."  << std::endl;
				SessionUtils::unload_session(s);
				SessionUtils::cleanup();
				return EXIT_FAILURE;
			}
		}
		/* we've already checked that these don't exist */
		if (outfile.empty ()) {
			s->save_state (snapshot_name + "-a54-compat");
			std::cout << UTILNAME << ": Saved new snapshot: " << snapshot_name + "-a54-compat" << " in " << session_dir << std::endl;

		} else {
			s->save_state (outfile);
			std::cout << UTILNAME << ": Saved new snapshot: " << outfile.c_str() << " in " << session_dir << std::endl;
		}

		SessionUtils::unload_session(s);
		SessionUtils::cleanup();
		std::cout << UTILNAME << ": Finished." << std::endl;
	} else {
		std::cout << UTILNAME << ": The snapshot " << argv[optind+1] << " doesn't require any change for use in 5.4. Use -f to override." << std::endl;
	}

	return 0;
}
