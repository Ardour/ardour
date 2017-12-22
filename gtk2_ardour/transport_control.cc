
#include "actions.h"
#include "transport_control.h"

#include "pbd/i18n.h"

using namespace Gtk;

TransportControlProvider::TransportControlProvider ()
	: roll_controllable (new TransportControllable ("transport roll", TransportControllable::Roll))
	, stop_controllable (new TransportControllable ("transport stop", TransportControllable::Stop))
	, goto_start_controllable (new TransportControllable ("transport goto start", TransportControllable::GotoStart))
	, goto_end_controllable (new TransportControllable ("transport goto end", TransportControllable::GotoEnd))
	, auto_loop_controllable (new TransportControllable ("transport auto loop", TransportControllable::AutoLoop))
	, play_selection_controllable (new TransportControllable ("transport play selection", TransportControllable::PlaySelection))
	, rec_controllable (new TransportControllable ("transport rec-enable", TransportControllable::RecordEnable))
{
}

TransportControlProvider::TransportControllable::TransportControllable (std::string name, ToggleType tp)
	: Controllable (name), type(tp)
{
}

void
TransportControlProvider::TransportControllable::set_value (double val, PBD::Controllable::GroupControlDisposition /*group_override*/)
{
	if (val < 0.5) {
		/* do nothing: these are radio-style actions */
		return;
	}

	const char *action = 0;

	switch (type) {
	case Roll:
		action = X_("Roll");
		break;
	case Stop:
		action = X_("Stop");
		break;
	case GotoStart:
		action = X_("GotoStart");
		break;
	case GotoEnd:
		action = X_("GotoEnd");
		break;
	case AutoLoop:
		action = X_("Loop");
		break;
	case PlaySelection:
		action = X_("PlaySelection");
		break;
	case RecordEnable:
		action = X_("Record");
		break;
	default:
		break;
	}

	if (action == 0) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", action);

	if (act) {
		act->activate ();
	}
}
