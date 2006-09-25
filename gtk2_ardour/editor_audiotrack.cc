#include <ardour/location.h>
#include <ardour/audio_diskstream.h>

#include "editor.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "region_view.h"
#include "selection.h"

using namespace ARDOUR;
using namespace PBD;

void
Editor::set_route_loop_selection ()
{
	if (session == 0 || selection->time.empty()) {
		return;
	}

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;

	Location* loc = transport_loop_location();

	if (loc) {
		
		loc->set (start, end);

		// enable looping, reposition and start rolling
		session->request_play_loop (true);
		session->request_locate (loc->start(), true);
	}

}

void
Editor::set_show_waveforms (bool yn)
{
	AudioTimeAxisView* atv;

	if (_show_waveforms != yn) {
		_show_waveforms = yn;
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((atv = dynamic_cast<AudioTimeAxisView*>(*i)) != 0) {
				atv->set_show_waveforms (yn);
			}
		}
		DisplayControlChanged (Editing::ShowWaveforms);
	}
}

void
Editor::set_show_waveforms_recording (bool yn)
{
	AudioTimeAxisView* atv;

	if (_show_waveforms_recording != yn) {
		_show_waveforms_recording = yn;
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((atv = dynamic_cast<AudioTimeAxisView*>(*i)) != 0) {
				atv->set_show_waveforms_recording (yn);
			}
		}
		DisplayControlChanged (Editing::ShowWaveformsRecording);
	}
}
