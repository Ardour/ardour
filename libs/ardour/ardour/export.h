#ifndef __ardour_export_h__
#define __ardour_export_h__

#include <map>
#include <vector>
#include <string>

#include <sigc++/signal.h>

#include <sndfile.h>
#include <samplerate.h>

#include <ardour/ardour.h>
#include <ardour/gdither.h>

using std::map;
using std::vector;
using std::string;
using std::pair;

namespace ARDOUR 
{
	class Port;

	typedef pair<Port *, uint32_t> PortChannelPair;
	typedef map<uint32_t, vector<PortChannelPair> > AudioExportPortMap;

	struct AudioExportSpecification : public SF_INFO, public sigc::trackable {

	    AudioExportSpecification();
	    ~AudioExportSpecification ();

	    void init ();
	    void clear ();


	    int prepare (jack_nframes_t blocksize, jack_nframes_t frame_rate);

	    int process (jack_nframes_t nframes);

	    /* set by the user */

	    string              path;
	    jack_nframes_t      sample_rate;

	    int                 src_quality;
	    SNDFILE*            out;
	    uint32_t       channels;
	    AudioExportPortMap  port_map;
	    jack_nframes_t      start_frame;
	    jack_nframes_t      end_frame;
	    GDitherType         dither_type;
	    bool                do_freewheel;

	    /* used exclusively during export */

	    jack_nframes_t      frame_rate;
	    GDither             dither;
	    float*              dataF;
	    float*              dataF2;
	    float*              leftoverF;
	    jack_nframes_t      leftover_frames;
	    jack_nframes_t      max_leftover_frames;
	    void*               output_data;
	    jack_nframes_t      out_samples_max;
	    uint32_t        sample_bytes;
	    uint32_t        data_width;

	    jack_nframes_t      total_frames;
	    SF_INFO             sfinfo;
	    SRC_DATA            src_data;
	    SRC_STATE*          src_state;
	    jack_nframes_t      pos;

	    sigc::connection    freewheel_connection;

	    /* shared between UI thread and audio thread */

	    float progress;  /* audio thread sets this */
	    bool  stop;      /* UI sets this */
	    bool  running;   /* audio thread sets to false when export is done */

	    int   status;

	};
};

#endif /* __ardour_export_h__ */
