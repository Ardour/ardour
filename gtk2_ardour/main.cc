/*
    Copyright (C) 2001-2007 Paul Davis
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cstdlib>

#include <sigc++/bind.h>
#include <gtkmm/settings.h>

#include <pbd/error.h>
#include <pbd/textreceiver.h>
#include <pbd/failed_constructor.h>
#include <pbd/pthread_utils.h>

#include <jack/jack.h>

#include <ardour/version.h>
#include <ardour/ardour.h>
#include <ardour/audioengine.h>

#include <gtkmm/main.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/utils.h>

#include "svn_revision.h"
#include "version.h"
#include "ardour_ui.h"
#include "opts.h"
#include "enums.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR_COMMAND_LINE;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

TextReceiver text_receiver ("ardour");

extern int curvetest (string);

static ARDOUR_UI  *ui = 0;

gint
show_ui_callback (void *arg)
{
 	ARDOUR_UI * ui = (ARDOUR_UI *) arg;

	ui->hide_splash();
	
	return FALSE;
}

#ifdef VST_SUPPORT
/* this is called from the entry point of a wine-compiled
   executable that is linked against gtk2_ardour built
   as a shared library.
*/
extern "C" {
int ardour_main (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif

{
	vector<Glib::ustring> null_file_list;

        Glib::thread_init();
	gtk_set_locale ();

	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	/* our i18n translations are all in UTF-8, so make sure
	   that even if the user locale doesn't specify UTF-8,
	   we use that when handling them.
	*/
	(void) bind_textdomain_codeset (PACKAGE,"UTF-8");
	(void) textdomain (PACKAGE);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// catch error message system signals ();

	text_receiver.listen_to (error);
	text_receiver.listen_to (info);
	text_receiver.listen_to (fatal);
	text_receiver.listen_to (warning);

	if (parse_opts (argc, argv)) {
		exit (1);
	}

	if (curvetest_file) {
		return curvetest (curvetest_file);
	}
	
	cout << _("Ardour/GTK ") 
	     << VERSIONSTRING
	     << _("\n   (built using ")
	     << ardour_svn_revision
#ifdef __GNUC__
	     << _(" and GCC version ") << __VERSION__ 
#endif
	     << ')'
	     << endl;
	
	if (just_version) {
		exit (0);
	}

	if (no_splash) {
		cerr << _("Copyright (C) 1999-2007 Paul Davis") << endl
		     << _("Some portions Copyright (C) Steve Harris, Ari Johnson, Brett Viren, Joel Baker") << endl
		     << endl
		     << _("Ardour comes with ABSOLUTELY NO WARRANTY") << endl
		     <<	_("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.") << endl
		     << _("This is free software, and you are welcome to redistribute it ") << endl
		     << _("under certain conditions; see the source for copying conditions.")
		     << endl;
	}

	/* some GUI objects need this */

	PBD::ID::init ();

        try { 
		ui = new ARDOUR_UI (&argc, &argv);
	} catch (failed_constructor& err) {
		error << _("could not create ARDOUR GUI") << endmsg;
		exit (1);
	}

	if (!no_splash) {
		ui->show_splash ();
		if (session_name.length()) {  
			g_timeout_add (4000, show_ui_callback, ui);
		}
	}

	if (!keybindings_path.empty()) {
		ui->set_keybindings_path (keybindings_path);
	}

	ui->run (text_receiver);
	ui = 0;

	ARDOUR::cleanup ();
	pthread_cancel_all ();
	return 0;
}
#ifdef VST_SUPPORT
} // end of extern C block
#endif

