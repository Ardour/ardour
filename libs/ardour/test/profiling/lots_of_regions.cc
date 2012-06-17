#include "test_util.h"
#include "ardour/ardour.h"
#include "ardour/midi_track.h"
#include "ardour/midi_region.h"
#include "ardour/session.h"
#include "ardour/playlist.h"
#include "pbd/stateful_diff_command.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

int
main (int argc, char* argv[])
{
	ARDOUR::init (false, true);
	Session* session = load_session ("../libs/ardour/test/profiling/sessions/1region", "1region.ardour");

	/* Find the track */
	boost::shared_ptr<MidiTrack> track = boost::dynamic_pointer_cast<MidiTrack> (session->get_routes()->back());
	assert (track);

	/* And the playlist */
	boost::shared_ptr<Playlist> playlist = track->playlist ();
	assert (playlist);

	/* And the region */
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion> (playlist->region_list().rlist().front());
	assert (region);

	/* Duplicate it a lot */
	session->begin_reversible_command ("foo");
	playlist->clear_changes ();
	playlist->duplicate (region, region->last_frame(), 1000);
	session->add_command (new StatefulDiffCommand (playlist));
	session->commit_reversible_command ();

	/* Undo that */
	session->undo (1);

	/* And do it again */
	session->begin_reversible_command ("foo");
	playlist->clear_changes ();
	playlist->duplicate (region, region->last_frame(), 1000);
	session->add_command (new StatefulDiffCommand (playlist));
	session->commit_reversible_command ();
}
