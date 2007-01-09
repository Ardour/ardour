#include <pbd/enumwriter.h>

#include "audio_clock.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

void
setup_gtk_ardour_enums ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	AudioClock::Mode clock_mode;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_CLASS_ENUM (AudioClock, SMPTE);
	REGISTER_CLASS_ENUM (AudioClock, BBT);
	REGISTER_CLASS_ENUM (AudioClock, MinSec);
	REGISTER_CLASS_ENUM (AudioClock, Frames);
	REGISTER_CLASS_ENUM (AudioClock, Off);
	REGISTER (clock_mode);
}
