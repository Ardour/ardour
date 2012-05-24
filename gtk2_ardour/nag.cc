#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fstream>
#include <gtkmm/stock.h>

#include "pbd/openuri.h"

#include "ardour/filesystem_paths.h"

#include "nag.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace Glib;
using namespace Gtk;

NagScreen::NagScreen (std::string /*context*/, bool maybe_sub)
	: ArdourDialog (_("Support Ardour Development"), true)
	, donate_button (button_group, _("I'd like to make a one-time donation"))
	, subscribe_button (button_group, _("Tell me more about becoming a subscriber"))
	, existing_button (button_group, _("I'm already a subscriber!"))
	, next_time_button (button_group, _("Ask about this the next time I export"))
	, never_again_button (button_group, _("Never ever ask me about this again"))
{
	if (maybe_sub) {
		message.set_text (_("Congratulations on your session export.\n\n\
It looks as if you may already be a subscriber. If so, thanks, and sorry\n\
to bother you again about this - I'm working on improving our subscriber system\n\
so that I don't have to keep annoying you with this message.\n\n\
If you're not a subscriber, perhaps you might consider supporting my work\n\
on Ardour with either a one-time donation or subscription. Nothing will \n\
happen if you choose not to do so. However Ardour's continuing development\n\
relies on a stable, sustainable income stream. Thanks for using Ardour!"));
	} else {
		message.set_text (_("Congratulations on your session export.\n\n\
I hope you find Ardour a useful tool. I'd like to ask you to consider supporting\n\
its development with either a one-time donation or subscription. Nothing\n\
will happen if you choose not to do so. However Ardour's continuing development\n\
relies on a stable, sustainable income stream. Thanks for using Ardour!"));
	}

	button_box.pack_start (donate_button);
	button_box.pack_start (subscribe_button);
	button_box.pack_start (existing_button);
	button_box.pack_start (next_time_button);
	button_box.pack_start (never_again_button);

	get_vbox()->set_spacing (12);
	get_vbox()->pack_start (message);
	get_vbox()->pack_start (button_box);

	set_border_width (12);
	add_button (Stock::OK, RESPONSE_ACCEPT);
}

NagScreen::~NagScreen ()
{
}

void
NagScreen::nag ()
{
	show_all ();

	int response = run ();

	hide ();

	switch (response) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	if (donate_button.get_active()) {
		offer_to_donate ();
	} else if (subscribe_button.get_active()) {
		offer_to_subscribe ();
	} else if (never_again_button.get_active ()) {
		mark_never_again ();
	} else if (existing_button.get_active ()) {
		mark_affirmed_subscriber ();
	}
}

NagScreen*
NagScreen::maybe_nag (std::string why)
{
	std::string path;
	bool really_subscribed;
	bool maybe_subscribed;

	path = Glib::build_filename (user_config_directory().to_string(), ".nevernag");

	if (Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return 0;
	}

	maybe_subscribed = is_subscribed (really_subscribed);

	if (really_subscribed) {
		return 0;
	}

	return new NagScreen (why, maybe_subscribed);
}

void
NagScreen::mark_never_again ()
{
	std::string path;

	path = Glib::build_filename (user_config_directory().to_string(), ".nevernag");

	ofstream nagfile (path.c_str());
}

void
NagScreen::mark_subscriber ()
{
	std::string path;

	path = Glib::build_filename (user_config_directory().to_string(), ".askedaboutsub");

	ofstream subsfile (path.c_str());
}

void
NagScreen::mark_affirmed_subscriber ()
{
	std::string path;

	path = Glib::build_filename (user_config_directory().to_string(), ".isubscribe");

	ofstream subsfile (path.c_str());
}

bool
NagScreen::is_subscribed (bool& really)
{
	std::string path;

	really = false;

	/* what we'd really like here is a way to query paypal
	   for someone's subscription status. thats a bit complicated
	   so for now, just see if they ever told us they were
	   subscribed. we try to trust our users :)
	*/

	path = Glib::build_filename (user_config_directory().to_string(), ".isubscribe");
	if (file_test (path, FILE_TEST_EXISTS)) {
		really = true;
		return true;
	}

	path = Glib::build_filename (user_config_directory().to_string(), ".askedaboutsub");
	if (file_test (path, FILE_TEST_EXISTS)) {
		/* they never said they were subscribed but they
		   did once express an interest in it.
		*/
		really = false;
		return true;
	}

	return false;
}

void
NagScreen::offer_to_donate ()
{
	const char* uri = "http://ardour.org/donate";

	/* we don't care if it fails */

        PBD::open_uri (uri);
}

void
NagScreen::offer_to_subscribe ()
{
	const char* uri = "http://ardour.org/subscribe";

	if (PBD::open_uri (uri)) {
		mark_subscriber ();
	}
}

