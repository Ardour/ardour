#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <glibmm.h>

#include "common.h"

#include "pbd/basename.h"

#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_filename.h"
#include "ardour/route.h"
#include "ardour/session_metadata.h"
#include "ardour/broadcast_info.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

static int export_session (Session *session,
		std::string outfile,
		std::string samplerate,
		bool normalize)
{
	ExportTimespanPtr tsp = session->get_export_handler()->add_timespan();
	boost::shared_ptr<ExportChannelConfiguration> ccp = session->get_export_handler()->add_channel_config();
	boost::shared_ptr<ARDOUR::ExportFilename> fnp = session->get_export_handler()->add_filename();
	boost::shared_ptr<AudioGrapher::BroadcastInfo> b;

	XMLTree tree;

	tree.read_buffer(std::string(
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<ExportFormatSpecification name=\"UTIL-WAV-16\" id=\"14792644-44ab-4209-a4f9-7ce6c2910cac\">"
"  <Encoding id=\"F_WAV\" type=\"T_Sndfile\" extension=\"wav\" name=\"WAV\" has-sample-format=\"true\" channel-limit=\"256\"/>"
"  <SampleRate rate=\""+ samplerate +"\"/>"
"  <SRCQuality quality=\"SRC_SincBest\"/>"
"  <EncodingOptions>"
"    <Option name=\"sample-format\" value=\"SF_16\"/>"
"    <Option name=\"dithering\" value=\"D_None\"/>"
"    <Option name=\"tag-metadata\" value=\"true\"/>"
"    <Option name=\"tag-support\" value=\"false\"/>"
"    <Option name=\"broadcast-info\" value=\"false\"/>"
"  </EncodingOptions>"
"  <Processing>"
"    <Normalize enabled=\""+ (normalize ? "true" : "false") +"\" target=\"0\"/>"
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
));

	boost::shared_ptr<ExportFormatSpecification> fmp = session->get_export_handler()->add_format(*tree.root());

	/* set up range */
	framepos_t start, end;
	start = session->current_start_frame();
	end   = session->current_end_frame();
	tsp->set_range (start, end);
	tsp->set_range_id ("session");

	/* add master outs as default */
	IO* master_out = session->master_out()->output().get();
	if (!master_out) {
		PBD::warning << _("Export Util: No Master Out Ports to Connect for Audio Export") << endmsg;
		return -1;
	}

	for (uint32_t n = 0; n < master_out->n_ports().n_audio(); ++n) {
		PortExportChannel * channel = new PortExportChannel ();
		channel->add_port (master_out->audio (n));
		ExportChannelPtr chan_ptr (channel);
		ccp->register_channel (chan_ptr);
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

	cout << "* Writing " << Glib::build_filename (fnp->get_folder(), tsp->name() + ".wav") << endl;


	/* output */
	fnp->set_timespan(tsp);
	fnp->include_label = false;

	/* do audio export */
	fmp->set_soundcloud_upload(false);
	session->get_export_handler()->add_export_config (tsp, ccp, fmp, fnp, b);
	session->get_export_handler()->do_export();

	boost::shared_ptr<ARDOUR::ExportStatus> status = session->get_export_status ();

	// TODO trap SIGINT -> status->abort();

	while (status->running ()) {
		double progress = 0.0;
		switch (status->active_job) {
		case ExportStatus::Normalizing:
			progress = ((float) status->current_postprocessing_cycle) / status->total_postprocessing_cycles;
			printf ("* Normalizing %.1f%%      \r", 100. * progress); fflush (stdout);
			break;
		case ExportStatus::Exporting:
			progress = ((float) status->processed_frames_current_timespan) / status->total_frames_current_timespan;
			printf ("* Exporting Audio %.1f%%  \r", 100. * progress); fflush (stdout);
			break;
		default:
			printf ("* Exporting...            \r");
			break;
		}
		Glib::usleep (1000000);
	}
	printf("\n");

	status->finish ();

	printf ("* Done.\n");
	return 0;
}

static void usage (int status) {
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - export an ardour session from the commandline.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <session-dir> <session/snapshot-name>\n\n");
	printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -n, --normalize            normalize signal level (to 0dBFS)\n\
  -o, --output  <file>       export output file name\n\
  -s, --samplerate <rate>    samplerate to use (default: 48000)\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
The session is exported as 16bit wav.\n\
If the no output file is given, the session's export dir is used.\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (status);
}

int main (int argc, char* argv[])
{
	std::string rate = "48000";
	std::string outfile;
	bool normalize = false;

	const char *optstring = "hno:r:V";

	const struct option longopts[] = {
		{ "help",       0, 0, 'h' },
		{ "normalize",  0, 0, 'n' },
		{ "output",     1, 0, 'o' },
		{ "samplerate", 1, 0, 'r' },
		{ "version",    0, 0, 'V' },
	};

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
					optstring, longopts, (int *) 0))) {
		switch (c) {

			case 'n':
				normalize = true;
				break;

			case 'o':
				outfile = optarg;
				break;

			case 's':
				{
					const int sr = atoi (optarg);
					if (sr >= 8000 && sr <= 192000) {
						stringstream ss;
						ss << sr;
						rate = ss.str();
					} else {
						fprintf(stderr, "Invalid Samplerate\n");
					}
				}
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

	SessionUtils::init();
	Session* s = 0;

	s = SessionUtils::load_session (argv[optind], argv[optind+1]);

	export_session (s, outfile, rate, normalize);

	SessionUtils::unload_session(s);
	SessionUtils::cleanup();

	return 0;
}
