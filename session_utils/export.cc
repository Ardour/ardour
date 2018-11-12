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
};

static int export_session (Session *session,
		std::string outfile,
		ExportSettings const& settings)
{
	ExportTimespanPtr tsp = session->get_export_handler()->add_timespan();
	boost::shared_ptr<ExportChannelConfiguration> ccp = session->get_export_handler()->add_channel_config();
	boost::shared_ptr<ARDOUR::ExportFilename> fnp = session->get_export_handler()->add_filename();
	boost::shared_ptr<ARDOUR::BroadcastInfo> b;

	XMLTree tree;

	tree.read_buffer(std::string(
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
"          <Duration format=\"Timecode\" hours=\"0\" minutes=\"0\" seconds=\"0\" samples=\"0\"/>"
"        </Add>"
"      </Start>"
"      <End>"
"        <Trim enabled=\"false\"/>"
"        <Add enabled=\"false\">"
"          <Duration format=\"Timecode\" hours=\"0\" minutes=\"0\" seconds=\"0\" samples=\"0\"/>"
"        </Add>"
"      </End>"
"    </Silence>"
"  </Processing>"
"</ExportFormatSpecification>"
));

	boost::shared_ptr<ExportFormatSpecification> fmp = session->get_export_handler()->add_format(*tree.root());

	/* set up range */
	samplepos_t start, end;
	start = session->current_start_sample();
	end   = session->current_end_sample();
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

	status->finish ();

	printf ("* Done.\n");
	return 0;
}

static void usage (int status) {
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - export an ardour session from the commandline.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <session-dir> <session/snapshot-name>\n\n");
	printf ("Options:\n\
  -b, --bitdepth <depth>     set export-format (16, 24, 32, float)\n\
  -B, --broadcast            include broadcast wave header\n\
  -h, --help                 display this help and exit\n\
  -n, --normalize            normalize signal level (to 0dBFS)\n\
  -o, --output  <file>       export output file name\n\
  -s, --samplerate <rate>    samplerate to use\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
This tool exports the session-range of a given ardour-session to a wave file,\n\
using the master-bus outputs.\n\
By default a 16bit signed .wav file at session-rate is exported.\n\
If the no output-file is given, the session's export dir is used.\n\
\n\
Note: the tool expects a session-name without .ardour file-name extension.\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (status);
}

int main (int argc, char* argv[])
{
	ExportSettings settings;
	std::string outfile;

	const char *optstring = "b:Bhno:s:V";

	const struct option longopts[] = {
		{ "bitdepth",   1, 0, 'b' },
		{ "broadcast",  0, 0, 'B' },
		{ "help",       0, 0, 'h' },
		{ "normalize",  0, 0, 'n' },
		{ "output",     1, 0, 'o' },
		{ "samplerate", 1, 0, 's' },
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

						/* fall through */
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

			case 'V':
				printf ("ardour-utils version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2015,2017 Robin Gareus <robin@gareus.org>\n");
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

	SessionUtils::init(false);
	Session* s = 0;

	s = SessionUtils::load_session (argv[optind], argv[optind+1]);

	if (settings._samplerate == 0) {
		settings._samplerate = s->nominal_sample_rate ();
	}

	export_session (s, outfile, settings);

	SessionUtils::unload_session(s);
	SessionUtils::cleanup();

	return 0;
}
