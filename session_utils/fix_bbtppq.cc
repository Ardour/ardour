/*
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <cstdlib>
#include <getopt.h>

#include <glibmm.h>

#include "pbd/file_utils.h"
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

#include "evoral/Note.h"
#include "evoral/Sequence.h"

#include "common.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

void
session_fail (Session* session)
{
	SessionUtils::unload_session(session);
	SessionUtils::cleanup();
	exit (EXIT_FAILURE);
}

bool
write_bbt_source_to_source (boost::shared_ptr<MidiSource>  bbt_source, boost::shared_ptr<MidiSource> source,
				 const Glib::Threads::Mutex::Lock& source_lock, const double session_offset)
{
	assert (source->empty());
	const bool old_percussive = bbt_source->model()->percussive();

	bbt_source->model()->set_percussive (false);

	source->mark_streaming_midi_write_started (source_lock, bbt_source->model()->note_mode());

	TempoMap& map (source->session().tempo_map());

	for (Evoral::Sequence<MidiModel::TimeType>::const_iterator i = bbt_source->model()->begin(MidiModel::TimeType(), true); i != bbt_source->model()->end(); ++i) {
		const double new_time = map.quarter_note_at_beat ((*i).time().to_double() + map.beat_at_quarter_note (session_offset * 4.0)) - (session_offset * 4.0);
		Evoral::Event<Temporal::Beats> new_ev (*i, true);
		new_ev.set_time (Temporal::Beats (new_time));
		source->append_event_beats (source_lock, new_ev);
	}

	bbt_source->model()->set_percussive (old_percussive);
	source->mark_streaming_write_completed (source_lock);
	source->set_natural_position (bbt_source->natural_position());

	return true;
}

boost::shared_ptr<MidiSource>
ensure_per_region_source (Session* session, boost::shared_ptr<MidiRegion> region, string newsrc_path)
{
	boost::shared_ptr<MidiSource> newsrc;

	/* create a new source if none exists and write corrected events to it.
	   if file exists, assume that it is correct.
	*/
	if (Glib::file_test (newsrc_path, Glib::FILE_TEST_EXISTS)) {
		Source::Flag flags =  Source::Flag (Source::Writable | Source::CanRename);
		try {
			newsrc = boost::dynamic_pointer_cast<MidiSource> (SourceFactory::createExternal (DataType::MIDI, *session, newsrc_path, 1, flags));
		} catch (failed_constructor& err) {
			cout << UTILNAME << ":" << endl
			     << " An error occurred creating external source from " << newsrc_path << " exiting." << endl;
			session_fail (session);
		}

		/* hack flags */
		XMLNode* node = new XMLNode (newsrc->get_state());

		if (node->property ("flags") != 0) {
			node->property ("flags")->set_value (enum_2_string (flags));
		}

		newsrc->set_state (*node, PBD::Stateful::loading_state_version);

		delete node;

		cout << UTILNAME << ":" << endl
		     << " Using existing midi source file" << endl
		     << " " << newsrc_path << endl
		     << " for region " << region->name() << endl;

	} else {
		try {
			newsrc = boost::dynamic_pointer_cast<MidiSource> (SourceFactory::createWritable (DataType::MIDI, *session, newsrc_path, session->sample_rate()));
		} catch (failed_constructor& err) {
			cout << UTILNAME << ":" << endl
			     << " An error occurred creating writeable source " << newsrc_path << " exiting." << endl;
			session_fail (session);
		}

		if (!newsrc->empty()) {
			cout << UTILNAME << ":" << endl
			     << " An error occurred/ " << newsrc->name() << " is not empty. exiting." << endl;
			session_fail (session);
		}

		Source::Lock newsrc_lock (newsrc->mutex());

		write_bbt_source_to_source (region->midi_source(0), newsrc, newsrc_lock, (region->quarter_note() - region->start_beats()) / 4.0);

		cout << UTILNAME << ":" << endl
		     << " Created new midi source file" << endl
		     << " " << newsrc_path << endl
		     << " for region " <<  region->name() << endl;
	}

	return newsrc;
}

boost::shared_ptr<MidiSource>
ensure_per_source_source (Session* session, boost::shared_ptr<MidiRegion> region, string newsrc_path)
{
	boost::shared_ptr<MidiSource> newsrc;

	/* create a new source if none exists and write corrected events to it. */
	if (Glib::file_test (newsrc_path, Glib::FILE_TEST_EXISTS)) {
		/* flags are ignored for external MIDI source */
		Source::Flag flags =  Source::Flag (Source::Writable | Source::CanRename);

		try {
			newsrc = boost::dynamic_pointer_cast<MidiSource> (SourceFactory::createExternal (DataType::MIDI, *session, newsrc_path, 1, flags));
		} catch (failed_constructor& err) {
			cout << UTILNAME << ":" << endl
			     << " An error occurred creating external source from " << newsrc_path << " exiting." << endl;
			session_fail (session);
		}

		cout << UTILNAME << ":" << endl
		     << " Using existing midi source file" << endl
		     << " " << newsrc_path << endl
		     << " for source " <<  region->midi_source(0)->name() << endl;
	} else {

		try {
			newsrc = boost::dynamic_pointer_cast<MidiSource> (SourceFactory::createWritable (DataType::MIDI, *session, newsrc_path, session->sample_rate()));
		} catch (failed_constructor& err) {
			cout << UTILNAME << ":" << endl
			     <<" An error occurred creating writeable source " << newsrc_path << " exiting." << endl;
			session_fail (session);
		}

		if (!newsrc->empty()) {
			cout << UTILNAME << ":" << endl
			     << " An error occurred/ " << newsrc->name() << " is not empty. exiting." << endl;
			session_fail (session);
		}

		Source::Lock newsrc_lock (newsrc->mutex());

		write_bbt_source_to_source (region->midi_source(0), newsrc, newsrc_lock, (region->quarter_note() - region->start_beats()) / 4.0);

		cout << UTILNAME << ":" << endl
		     << " Created new midi source file" << endl
		     << " " << newsrc_path << endl
		     << " for source " <<  region->midi_source(0)->name() << endl;

	}

	return newsrc;
}

void
reset_start (Session* session, boost::shared_ptr<MidiRegion> region)
{
	/* set start_beats to quarter note value from incorrect bbt*/
	TempoMap& tmap (session->tempo_map());
	double new_start_qn = tmap.quarter_note_at_beat (region->beat()) - tmap.quarter_note_at_beat (region->beat() - region->start_beats());

	/* force a change to start and start_beats */
	PositionLockStyle old_pls = region->position_lock_style();
	region->set_position_lock_style (AudioTime);
	region->set_start (tmap.sample_at_quarter_note (region->quarter_note()) - tmap.sample_at_quarter_note (region->quarter_note() - new_start_qn) + 1);
	region->set_start (tmap.sample_at_quarter_note (region->quarter_note()) - tmap.sample_at_quarter_note (region->quarter_note() - new_start_qn));
	region->set_position_lock_style (old_pls);

}

void
reset_length (Session* session, boost::shared_ptr<MidiRegion> region)
{
	/* set length_beats to quarter note value */
	TempoMap& tmap (session->tempo_map());
	double new_length_qn = tmap.quarter_note_at_beat (region->beat() + region->length_beats())
		- tmap.quarter_note_at_beat (region->beat());

	/* force a change to length and length_beats */
	PositionLockStyle old_pls = region->position_lock_style();
	region->set_position_lock_style (AudioTime);
	region->set_length (tmap.sample_at_quarter_note (region->quarter_note() + new_length_qn) + 1 - region->position(), 0);
	region->set_length (tmap.sample_at_quarter_note (region->quarter_note() + new_length_qn)- region->position(), 0);
	region->set_position_lock_style (old_pls);
}

bool
apply_one_source_per_region_fix (Session* session)
{
	const RegionFactory::RegionMap& region_map (RegionFactory::all_regions());

	if (!region_map.size()) {
		return false;
	}

	list<boost::shared_ptr<MidiSource> > old_source_list;

	/* set start and length for every midi region. ensure a new converted source exists and switch to it. */
	for (RegionFactory::RegionMap::const_iterator i = region_map.begin(); i != region_map.end(); ++i) {
		boost::shared_ptr<MidiRegion> mr;

		if ((mr = boost::dynamic_pointer_cast<MidiRegion>((*i).second)) != 0) {

			if (!mr->midi_source()->writable()) {
				/* we know the midi dir is writable, so this region is external. leave it alone*/
				cout << mr->source()->name() << "is not writable. skipping." << endl;
				continue;
			}

			old_source_list.push_back (mr->midi_source());

			reset_start (session, mr);
			reset_length (session, mr);

			string newsrc_filename = mr->name() + "-a54-compat.mid";
			string newsrc_path = Glib::build_filename (session->session_directory().midi_path(), newsrc_filename);

			boost::shared_ptr<MidiSource> newsrc = ensure_per_region_source (session, mr, newsrc_path);

			mr->clobber_sources (newsrc);
		}
	}

	old_source_list.unique();

	/* remove old sources from the session. current snapshot is saved.*/
	cout << UTILNAME << ":" << endl
	     << " clearing old sources." << endl;

	for (list<boost::shared_ptr<MidiSource> >::iterator i = old_source_list.begin(); i != old_source_list.end(); ++i) {
		session->remove_source (boost::weak_ptr<MidiSource> (*i));
	}

	return true;
}

bool
apply_one_source_per_source_fix (Session* session)
{
	const RegionFactory::RegionMap& region_map (RegionFactory::all_regions());

	if (!region_map.size()) {
		return false;
	}

	map<PBD::ID, boost::shared_ptr<MidiSource> > old_source_to_new;
	/* reset every midi region's start and length. ensure its corrected source exists. */
	for (RegionFactory::RegionMap::const_iterator i = region_map.begin(); i != region_map.end(); ++i) {
		boost::shared_ptr<MidiRegion> mr;
		map<PBD::ID, boost::shared_ptr<MidiSource> >::iterator src_it;

		if ((mr = boost::dynamic_pointer_cast<MidiRegion>((*i).second)) != 0) {

			if (!mr->midi_source()->writable()) {
				cout << mr->source()->name() << "is not writable. skipping." << endl;
				continue;
			}

			reset_start (session, mr);
			reset_length (session, mr);

			if ((src_it = old_source_to_new.find (mr->midi_source()->id())) == old_source_to_new.end()) {
				string newsrc_filename = mr->source()->name() +  "-a54-compat.mid";
				string newsrc_path = Glib::build_filename (session->session_directory().midi_path(), newsrc_filename);

				boost::shared_ptr<MidiSource> newsrc = ensure_per_source_source (session, mr, newsrc_path);

				old_source_to_new.insert (make_pair (mr->midi_source()->id(), newsrc));

				mr->midi_source(0)->set_name (newsrc->name());
			}
		}
	}

	/* remove new sources from the session. current snapshot is saved.*/
	cout << UTILNAME << ":" << endl
	     << " clearing new sources." << endl;

	for (map<PBD::ID, boost::shared_ptr<MidiSource> >::iterator i = old_source_to_new.begin(); i != old_source_to_new.end(); ++i) {
		session->remove_source (boost::weak_ptr<MidiSource> ((*i).second));
	}

	return true;
}

static void usage () {
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - convert an ardour session with 5.0 - 5.3 midi sources to be compatible with 5.4.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <session-dir> <snapshot-name>\n\n");
	printf ("Options:\n\
  -h, --help                    display this help and exit\n\
  -f, --force                   override detection of affected sessions\n\
  -o, --output <snapshot-name>  output session snapshot name (without file suffix)\n\
  -V, --version                 print version information and exit\n\
\n");
	printf ("\n\
This Ardour-specific utility provides an upgrade path for sessions created or\n\
modified with Ardour versions 5.0 - 5.3.\n\
It creates a 5.4-compatible snapshot from affected Ardour session files.\n\
Affected versions (5.0 - 5.3 inclusive) contain a bug which caused some\n\
MIDI region properties and contents to be stored incorrectly\n\
(see more below).\n\n\
The utility will first determine whether or not a session requires any\n\
changes for 5.4 compatibility.\n\
If a session is determined to be affected by the bug, the program will take\n\
one of two approaches to correcting the problem.\n\n\
The first is to write a new MIDI source file for every existing MIDI source\n\
in the supplied snapshot.\n\
In the second approach, each MIDI region have its source converted and placed\n\
in the session midifiles directory as a new source\n\
(one source file per region).\n\
The second method is only offered if the first approach cannot logically ensure\n\
that the results would match the input snapshot.\n\
Using the first method even if the second method is offered\n\
will usually  match the input exactly\n\
(partly due to a characteristic of the bug).\n\n\
Both methods update MIDI region properties and save a new snapshot in the\n\
supplied session-dir, optionally using a supplied snapshot name (-o).\n\
The new snapshot may be used on Ardour-5.4.\n\n\
Running this utility should not alter any existing files,\n\
but it is recommended that you run it on a backup of the session directory.\n\n\
EXAMPLE:\n\
ardour5-fix_bbtppq -o bantam ~/studio/leghorn leghorn\n\
will create a new snapshot file ~/studio/leghorn/bantam.ardour from\n\
~/studio/leghorn/leghorn.ardour\n\
Converted midi sources will be created in\n\
~/studio/leghorn/interchange/leghorn/midifiles/\n\
If the output option (-o) is omitted, the string \"-a54-compat\"\n\
will be appended to the supplied snapshot name.\n\n\
About the Bug\n\
If a session from affected versions used MIDI regions and a meter note divisor\n\
was set to anything but quarter notes, the source smf files would contain events\n\
at a PPQN value derived from BBT beats (using meter note divisor)\n\
rather than quarter-note beats.\n\
The region start and length offsets would also be stored incorrectly.\n\
If a MIDI session only contains quarter note meter divisors, it will be unaffected.\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (EXIT_SUCCESS);
}

int main (int argc, char* argv[])
{
	string outfile;
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
			exit (EXIT_SUCCESS);
			break;

		case 'h':
			usage ();
			break;

		default:
			cerr << "Error: unrecognized option. See --help for usage information.\n";
			::exit (EXIT_FAILURE);
			break;
		}
	}

	if (optind + 2 > argc) {
		cerr << "Error: Missing parameter. See --help for usage information.\n";
		::exit (EXIT_FAILURE);
	}

	SessionDirectory* session_dir = new SessionDirectory (argv[optind]);
	string snapshot_name (argv[optind+1]);
	string statefile_suffix (X_(".ardour"));
	string pending_suffix (X_(".pending"));

	XMLTree* state_tree;

	string xmlpath(argv[optind]);
	string out_snapshot_name;

	if (!outfile.empty()) {
		string file_test_path = Glib::build_filename (argv[optind], outfile + statefile_suffix);
		if (Glib::file_test (file_test_path, Glib::FILE_TEST_EXISTS)) {
			cout << UTILNAME << ":" << endl
			     << " session file " << file_test_path << " already exists!" << endl;
                        ::exit (EXIT_FAILURE);
		}
		out_snapshot_name = outfile;
	} else {
		string file_test_path = Glib::build_filename (argv[optind], snapshot_name + "-a54-compat" + statefile_suffix);
		if (Glib::file_test (file_test_path, Glib::FILE_TEST_EXISTS)) {
			cout << UTILNAME << ":" << endl
			     << " session file " << file_test_path << " already exists!" << endl;
                        ::exit (EXIT_FAILURE);
		}
		out_snapshot_name = snapshot_name + "-a54-compat";
	}

	xmlpath = Glib::build_filename (xmlpath, legalize_for_path (snapshot_name) + pending_suffix);

	if (Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {

		/* there is pending state from a crashed capture attempt */
		cout << UTILNAME << ":" << endl
		     << " There seems to be pending state for snapshot : " << snapshot_name << endl;

	}

	xmlpath = Glib::build_filename (argv[optind], argv[optind+1]);

	if (!Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {
		xmlpath = Glib::build_filename (argv[optind], legalize_for_path (argv[optind+1]) + ".ardour");
		if (!Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {
			cout << UTILNAME << ":" << endl
			     << " session file " << xmlpath << " doesn't exist!" << endl;
                        ::exit (EXIT_FAILURE);
                }
        }

	state_tree = new XMLTree;

	bool writable = PBD::exists_and_writable (xmlpath) && PBD::exists_and_writable(Glib::path_get_dirname(xmlpath));

	if (!writable) {
		cout << UTILNAME << ":" << endl
		     << " Error : The session directory must exist and be writable." << endl;
		return -1;
	}

	if (!PBD::exists_and_writable (Glib::path_get_dirname (session_dir->midi_path()))) {
		cout << UTILNAME << ":" << endl
		     << " Error : The session midi directory " << session_dir->midi_path() << " must be writable. exiting." << endl;
		::exit (EXIT_FAILURE);
	}

	if (!state_tree->read (xmlpath)) {
		cout << UTILNAME << ":" << endl
		     << " Could not understand session file " << xmlpath << endl;
		delete state_tree;
		state_tree = 0;
		::exit (EXIT_FAILURE);
	}

	XMLNode const & root (*state_tree->root());

	if (root.name() != X_("Session")) {
		cout << UTILNAME << ":" << endl
		     << " Session file " << xmlpath<< " is not a session" << endl;
		delete state_tree;
		state_tree = 0;
		::exit (EXIT_FAILURE);
	}

	XMLProperty const * prop;

	if ((prop = root.property ("version")) == 0) {
		/* no version implies very old version of Ardour */
		cout << UTILNAME << ":" << endl
		     << " The session " << snapshot_name << " has no version or is too old to be affected. exiting." << endl;
		::exit (EXIT_FAILURE);
	} else {
		if (prop->value().find ('.') != string::npos) {
			/* old school version format */
			cout << UTILNAME << ":" << endl
			     << " The session " << snapshot_name << " is too old to be affected. exiting." << endl;
			::exit (EXIT_FAILURE);
		} else {
			PBD::Stateful::loading_state_version = atoi (prop->value().c_str());
		}
	}

	cout <<  UTILNAME << ":" << endl
	     << " Checking snapshot : " << snapshot_name << " in directory : " << session_dir->root_path() << endl;

	bool midi_regions_use_bbt_beats = false;

	if (PBD::Stateful::loading_state_version == 3002 && writable) {
		XMLNode* child;
		if ((child = find_named_node (root, "ProgramVersion")) != 0) {
			if ((prop = child->property ("modified-with")) != 0) {
				string modified_with = prop->value ();

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

			if (child->name() == MeterSection::xml_state_node_name && (prop = child->property ("note-type")) != 0) {
				double note_type;

				if (sscanf (prop->value().c_str(), "%lf", &note_type) ==1) {

					if (note_type != 4.0) {
						all_metrum_divisors_are_quarters = false;
					}

					divisor_list.push_back (note_type);
				}
			}
		}
	} else {
		cout << UTILNAME << ":" << endl
		     << " Session file " <<  xmlpath << " has no TempoMap node. exiting." << endl;
		::exit (EXIT_FAILURE);
	}

	if (all_metrum_divisors_are_quarters && !force) {
		cout << UTILNAME << ":" << endl
		     << " The session " << snapshot_name << " is clear for use in 5.4 (all divisors are quarters). Use -f to override." << endl;
		::exit (EXIT_FAILURE);
	}

	/* check for multiple note divisors. if there is only one, we can create one file per source. */
	bool one_source_file_per_source = false;
	divisor_list.unique();

	if (divisor_list.size() == 1) {
		cout << endl << UTILNAME << ":" << endl
		     << " Snapshot " << snapshot_name << " will be converted using one new file per source." << endl
		     << " To continue with per-source conversion enter s. q to quit." << endl;

		while (1) {
			cout  << " [s/q]" << endl;

			string input;
			getline (cin, input);

			if (input == "s") {
				break;
			}

			if (input == "q") {
				exit (EXIT_SUCCESS);
				break;
			}
		}

		one_source_file_per_source = true;
	} else {

		cout << endl << UTILNAME  << ":" << endl
		     << " Snapshot " << snapshot_name << " contains multiple meter note divisors." << endl
		     << " Per-region source conversion ensures that the output snapshot will be identical to the original," << endl
		     << " however regions in the new snapshot will no longer share sources." << endl << endl
		     << " In many (but not all) cases per-source conversion will work equally well." << endl
		     << " It is recommended that you test a snapshot created with the per-source method before using per-region conversion." << endl << endl
		     << " To continue with per-region conversion enter r. For per-source conversion, enter s. q to quit." << endl;

		while (1) {
			cout  << " [r/s/q]" << endl;

			string input;
			getline (cin, input);

			if (input == "s") {
				one_source_file_per_source = true;
				break;
			}

			if (input == "r") {
				break;
			}

			if (input == "q") {
				exit (EXIT_SUCCESS);
				break;
			}
		}
	}

	if (midi_regions_use_bbt_beats || force) {

		if (force) {
			cout << UTILNAME << ":" << endl
			     << " Forced update of snapshot : " << snapshot_name << endl;
		}

		SessionUtils::init();
		Session* s = 0;

		cout << UTILNAME << ":" << endl
		     << " Loading snapshot " << snapshot_name << endl;

		s = SessionUtils::load_session (argv[optind], argv[optind+1]);

		/* save new snapshot and prevent alteration of the original by switching to it.
		   we know these files don't yet exist.
		*/
		if (s->save_state (out_snapshot_name, false, true)) {
			cout << UTILNAME << ":" << endl
			     << " Could not save new snapshot: " << out_snapshot_name << " in " << session_dir->root_path() << endl;

			session_fail (s);
		}

		cout << UTILNAME << ":" << endl
		     << " Saved new snapshot: " << out_snapshot_name << " in " << session_dir->root_path() << endl;

		if (one_source_file_per_source) {
			cout << UTILNAME << ":" << endl
			     << " Will create one MIDI file per source." << endl;

			if (!apply_one_source_per_source_fix (s)) {
				cout << UTILNAME << ":" << endl
				     << " The snapshot " << snapshot_name << " is clear for use in 5.4 (no midi regions). exiting." << endl;
				session_fail (s);
			}
		} else {
			cout << UTILNAME << ":" << endl
			     << " Will create one MIDI file per midi region." << endl;

			if (!apply_one_source_per_region_fix (s)) {
				cout << UTILNAME << ":" << endl
				     << " The snapshot " << snapshot_name << " is clear for use in 5.4 (no midi regions). exiting."  << endl;
				session_fail (s);
			}

			if (s->save_state (out_snapshot_name, false, true)) {
				cout << UTILNAME << ":" << endl
				     << " Could not save snapshot: " << out_snapshot_name << " in " << session_dir->root_path() << endl;
				session_fail (s);
			}
			cout << UTILNAME << ":" << endl
			     << " Saved new snapshot: " << out_snapshot_name << " in " << session_dir->root_path() << endl;
		}

		SessionUtils::unload_session(s);
		SessionUtils::cleanup();
		cout << UTILNAME << ":" << endl
		     << " Snapshot " << out_snapshot_name << " is ready for use in 5.4" << endl;
	} else {
		cout << UTILNAME << ":" << endl
		     << " The snapshot " << snapshot_name << " doesn't require any change for use in 5.4. Use -f to override." << endl;
		::exit (EXIT_FAILURE);
	}

	return 0;
}
