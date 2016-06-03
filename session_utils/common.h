#ifndef _session_utils_common_h_
#define _session_utils_common_h_

#include "pbd/transmitter.h"
#include "pbd/receiver.h"

#include "ardour/ardour.h"
#include "ardour/session.h"

class TestReceiver : public Receiver
{
  protected:
    void receive (Transmitter::Channel chn, const char * str);
};

namespace SessionUtils {

	/** initialize libardour */
	void init (bool print_log = true);

	/** clean up, stop Processing Engine
	 * @param s Session to close (may me NULL)
	 */
	void cleanup ();

	/** @param dir Session directory.
	 *  @param state Session state file, without .ardour suffix.
	 */
	ARDOUR::Session * load_session (std::string dir, std::string state, bool exit_at_failure = true);

	/** close session and stop engine
	 * @param s Session to close (may me NULL)
	 */
	void unload_session (ARDOUR::Session *s);

};

#endif /* _session_utils_misc_h_ */
