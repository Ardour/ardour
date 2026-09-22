/*
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include "common.h"

#include "pbd/basename.h"
#include "pbd/enumwriter.h"

#include "ardour/broadcast_info.h"
#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_filename.h"
#include "ardour/route.h"
#include "ardour/session_metadata.h"
#include "ardour/broadcast_info.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

struct ExportSettings
{
	ExportSettings ()
		: _samplerate (0)
		, _sample_format (ExportFormatBase::SF_16)
		, _normalize (false)
		, _bwf (false)
		, _warmup_seconds (1)
	{}

	std::string samplerate () const
	{
		stringstream ss;
		ss << _samplerate;
		return ss.str();
	}

	std::string sample_format () const
	{
		return enum_2_string (_sample_format);
	}

	std::string normalize () const
	{
		return _normalize ? "true" : "false";
	}

	std::string bwf () const
	{
		return _bwf ? "true" : "false";
	}

	int _samplerate;
	ExportFormatBase::SampleFormat _sample_format;
	bool _normalize;
	bool _bwf;
	uint32_t _warmup_seconds;
};

static int export_session (Session *session,
		std::string outfile,
		std::string route_name,
		ExportSettings const& settings)
{
	ExportTimespanPtr tsp = session->get_export_handler()->add_timespan();
	std::shared_ptr<ExportChannelConfiguration> ccp = session->get_export_handler()->add_channel_config();
	std::shared_ptr<ARDOUR::ExportFilename> fnp = session->get_export_handler()->add_filename();
	std::shared_ptr<ARDOUR::BroadcastInfo> b;

	XMLTree tree;

	tree.read_buffer(std::string (
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<ExportFormatSpecification name=\"UTIL-WAV-EXPORT\" id=\"b1280899-0459-4aef-9dc9-7e2277fa6d24\">"
"  <Encoding id=\"F_WAV\" type=\"T_Sndfile\" extension=\"wav\" name=\"WAV\" has-sample-format=\"true\" channel-limit=\"256\"/>"
"  <SampleRate rate=\""+ settings.samplerate () +"\"/>"
"  <SRCQuality quality=\"SRC_SincBest\"/>"
"  <EncodingOptions>"
"    <Option name=\"sample-format\" value=\"" + settings.sample_format () + "\"/>"
"    <Option name=\"dithering\" value=\"D_None\"/>"
"    <Option name=\"tag-metadata\" value=\"true\"/>"
"    <Option name=\"tag-support\" value=\"false\"/>"
"    <Option name=\"broadcast-info\" value=\"" + settings.bwf () +"\"/>"
"  </EncodingOptions>"
"  <Processing>"
"    <Normalize enabled=\""+ settings.normalize () +"\" target=\"0\"/>"
"    <Silence>"
"      <Start>"
"        <Trim enabled=\"false\"/>"
"        <Add enabled=\"false\">"
"          <Duration format=\"Timecode\" hours=\"0\" minutes=\"0\" seconds=\"0\" frames=\"0\"/>"
"        </Add>"
"      </Start>"
"      <End>"
"        <Trim enabled=\"false\"/>"
"        <Add enabled=\"false\">"
"          <Duration format=\"Timecode\" hours=\"0\" minutes=\"0\" seconds=\"0\" frames=\"0\"/>"
"        </Add>"
"      </End>"
"    </Silence>"
"  </Processing>"
"</ExportFormatSpecification>"
	).c_str());

	std::shared_ptr<ExportFormatSpecification> fmp = session->get_export_handler()->add_format(*tree.root());

	/* set up range */
	samplepos_t start, end;
	start = session->current_start_sample();
	end   = session->current_end_sample();
	tsp->set_range (start, end);
	tsp->set_range_id ("session");

	/* Use the requested route's outputs, or the master outputs by default. */
	std::shared_ptr<Route> source_route;
	if (route_name.empty ()) {
		source_route = session->master_out ();
		if (!source_route) {
			cerr << "Export Util: Session has no master bus; use --route to select an audio route\n";
			return -1;
		}
	} else {
		source_route = session->route_by_name (route_name);
		if (!source_route) {
			cerr << "Export Util: Route '" << route_name << "' was not found\n";
			return -1;
		}
	}

	IO* source_out = source_route->output().get();
	if (!source_out || source_out->n_ports().n_audio() == 0) {
		cerr << "Export Util: Route '" << source_route->name () << "' has no audio output ports\n";
		return -1;
	}

	if (route_name.empty ()) {
		for (uint32_t n = 0; n < source_out->n_ports().n_audio(); ++n) {
			PortExportChannel * channel = new PortExportChannel ();
			channel->add_port (source_out->audio (n));
			ExportChannelPtr chan_ptr (channel);
			ccp->register_channel (chan_ptr);
		}
	} else {
		std::list<ExportChannelPtr> channels;
		RouteExportChannel::create_from_route (channels, source_route);
		if (channels.empty ()) {
			cerr << "Export Util: Route '" << source_route->name () << "' has no exportable audio channels\n";
			return -1;
		}
		ccp->register_channels (channels);
	}

	/* output filename */
	if (outfile.empty ()) {
		tsp->set_name ("session");
	} else {
		std::string dirname = Glib::path_get_dirname (outfile);
		std::string basename = Glib::path_get_basename (outfile);

		if (basename.size() > 4 && !basename.compare (basename.size() - 4, 4, ".wav")) {
			basename = PBD::basename_nosuffix (basename);
		}

		fnp->set_folder(dirname);
		tsp->set_name (basename);
	}

	/* set broadcast info */
	if (settings._bwf) {
		b.reset (new BroadcastInfo);
		b->set_from_session (*session, tsp->get_start ());
	}

	cout << "* Writing " << Glib::build_filename (fnp->get_folder(), tsp->name() + ".wav") << endl;


	/* output */
	fnp->set_timespan(tsp);
	fnp->include_label = false;

	/* do audio export */
	fmp->set_soundcloud_upload(false);
	session->get_export_handler()->add_export_config (tsp, ccp, fmp, fnp, b);

	if (0 != session->get_export_handler()->do_export()) {
		return -1;
	}

	std::shared_ptr<ARDOUR::ExportStatus> status = session->get_export_status ();

	// TODO trap SIGINT -> status->abort();

	while (status->running ()) {
		double progress = 0.0;
		switch (status->active_job) {
		case ExportStatus::Normalizing:
			progress = ((float) status->current_postprocessing_cycle) / status->total_postprocessing_cycles;
			printf ("* Normalizing %.1f%%      \r", 100. * progress); fflush (stdout);
			break;
		case ExportStatus::Exporting:
			progress = ((float) status->processed_samples_current_timespan) / status->total_samples_current_timespan;
			printf ("* Exporting Audio %.1f%%  \r", 100. * progress); fflush (stdout);
			break;
		default:
			printf ("* Exporting...            \r");
			break;
		}
		Glib::usleep (1000000);
	}
	printf("\n");

	status->finish (TRS_UI);

	printf ("* Done.\n");
	return 0;
}

static void usage () {
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - export an ardour session from the commandline.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <session-dir> <session/snapshot-name>\n\n");
	printf ("Options:\n\
  -b, --bitdepth <depth>     set export-format (16, 24, 32, float)\n\
  -B, --broadcast            include broadcast wave header\n\
  -h, --help                 display this help and exit\n\
  -n, --normalize            normalize signal level (to 0dBFS)\n\
  -o, --output  <file>       export output file name\n\
  -r, --route <name>         export named route/bus instead of the master\n\
  -s, --samplerate <rate>    samplerate to use\n\
  -w, --warmup <seconds>     wait before export for asynchronous plugins\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
This tool exports the session-range of a given ardour-session to a wave file,\n\
using the master-bus outputs, or a named route selected with --route.\n\
By default a 16bit signed .wav file at session-rate is exported.\n\
If the no output-file is given, the session's export dir is used.\n\
\n\
Note: the tool expects a session-name without .ardour file-name extension.\n\
\n");

	printf ("Report bugs to <https://tracker.ardour.org/>\n"
	        "Website: <https://ardour.org/>\n");
	::exit (EXIT_SUCCESS);
}

int main (int argc, char* argv[])
{
	ExportSettings settings;
	std::string outfile;
	std::string route_name;

	const char *optstring = "b:Bhno:r:s:w:V";

	const struct option longopts[] = {
		{ "bitdepth",   1, 0, 'b' },
		{ "broadcast",  0, 0, 'B' },
		{ "help",       0, 0, 'h' },
		{ "normalize",  0, 0, 'n' },
		{ "output",     1, 0, 'o' },
		{ "route",      1, 0, 'r' },
		{ "samplerate", 1, 0, 's' },
		{ "warmup",     1, 0, 'w' },
		{ "version",    0, 0, 'V' },
	};

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
					optstring, longopts, (int *) 0))) {
		switch (c) {

			case 'b':
				switch (atoi (optarg)) {
					case 16:
						settings._sample_format = ExportFormatBase::SF_16;
						break;
					case 24:
						settings._sample_format = ExportFormatBase::SF_24;
						break;
					case 32:
						settings._sample_format = ExportFormatBase::SF_32;
						break;
					case 0:
						if (0 == strcmp (optarg, "float")) {
							settings._sample_format = ExportFormatBase::SF_Float;
							break;
						}
						/* fallthrough */
					default:
						fprintf(stderr, "Invalid Bit Depth\n");
						break;
				}
				break;

			case 'B':
				settings._bwf = true;
				break;

			case 'n':
				settings._normalize = true;
				break;

			case 'o':
				outfile = optarg;
				break;

			case 'r':
				route_name = optarg;
				break;

			case 's':
				{
					const int sr = atoi (optarg);
					if (sr >= 8000 && sr <= 192000) {
						settings._samplerate = sr;
					} else {
						fprintf(stderr, "Invalid Samplerate\n");
					}
				}
				break;

			case 'w':
				{
					char* endptr = 0;
					unsigned long const seconds = strtoul (optarg, &endptr, 10);
					if (!*optarg || *endptr || seconds < 1 || seconds > 3600) {
						cerr << "Invalid warmup time (expected 1..3600 seconds)\n";
						::exit (EXIT_FAILURE);
					}
					settings._warmup_seconds = seconds;
				}
				break;

			case 'V':
				printf ("ardour-utils version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2015,2017 Robin Gareus <robin@gareus.org>\n");
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

	SessionUtils::init(false);
	Session* s = 0;

	s = SessionUtils::load_session (argv[optind], argv[optind+1]);

	cout << "* Waiting " << settings._warmup_seconds << " seconds for plugins to initialize" << endl;
	Glib::usleep (settings._warmup_seconds * G_USEC_PER_SEC);

	if (settings._samplerate == 0) {
		settings._samplerate = s->nominal_sample_rate ();
	}

	int const result = export_session (s, outfile, route_name, settings);

	SessionUtils::unload_session(s);
	SessionUtils::cleanup();

	return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
