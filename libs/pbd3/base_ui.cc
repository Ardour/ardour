#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <pbd/base_ui.h>
#include <pbd/error.h>
#include <pbd/compose.h>
#include <pbd/failed_constructor.h>

#include "i18n.h"

using namespace std;
	
uint32_t BaseUI::rt_bit = 1;
BaseUI::RequestType BaseUI::CallSlot = BaseUI::new_request_type();

BaseUI::BaseUI (string str, bool with_signal_pipe)
	: _name (str)
{
	/* odd pseudo-singleton semantics */

	base_ui_instance = this;

	signal_pipe[0] = -1;
	signal_pipe[1] = -1;

	if (with_signal_pipe) {
		if (setup_signal_pipe ()) {
			throw failed_constructor ();
		}
	}
}

BaseUI::~BaseUI()
{
	if (signal_pipe[0] >= 0) {
		close (signal_pipe[0]);
	} 

	if (signal_pipe[1] >= 0) {
		close (signal_pipe[1]);
	} 
}

BaseUI::RequestType
BaseUI::new_request_type ()
{
	RequestType rt;

	/* XXX catch out-of-range */

	rt = RequestType (rt_bit);
	rt_bit <<= 1;

	return rt;
}

int
BaseUI::setup_signal_pipe ()
{
	/* setup the pipe that other threads send us notifications/requests
	   through.
	*/

	if (pipe (signal_pipe)) {
		error << string_compose (_("%1-UI: cannot create error signal pipe (%2)"), _name, std::strerror (errno))
		      << endmsg;

		return -1;
	}

	if (fcntl (signal_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose (_("%1-UI: cannot set O_NONBLOCK on signal read pipe (%2)"), _name, std::strerror (errno))
		      << endmsg;
		return -1;
	}

	if (fcntl (signal_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose (_("%1-UI: cannot set O_NONBLOCK on signal write pipe (%2)"), _name, std::strerror (errno))
		      << endmsg;
		return -1;
	}

	return 0;
}

