//
//  session_close_dialog.cpp
//  Tracks
//
//  Created by User on 6/12/14.
//
//

#include "session_close_dialog.h"


#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fstream>
#include <algorithm>

#include "waves_button.h"

#include <gtkmm/filechooser.h>
#include "dbg_msg.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;


SessionCloseDialog::SessionCloseDialog ()
: WavesDialog (_("session_close_dialog.xml"), true, false)
, _cancel_button (get_waves_button ("cancel_button"))
, _dont_save_button (get_waves_button ("dont_save_button"))
, _save_button (get_waves_button ("save_button"))
, _top_label (get_label("top_label"))
, _bottom_label (get_label("bottom_label"))
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionCloseDialog::on_cancel));
    _dont_save_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionCloseDialog::on_dont_save));
    _save_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionCloseDialog::on_save));
}

void
SessionCloseDialog::on_cancel (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_CANCEL);
}

void
SessionCloseDialog::on_dont_save (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_NO);
}

void
SessionCloseDialog::on_save (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_YES);
}

