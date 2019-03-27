#include <iostream>

#include <gtkmm/table.h>

#include "mixer_snapshot_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace PBD;
using namespace std;

MixerSnapshotDialog::MixerSnapshotDialog()
    : ArdourDialog(_("this is a dialog"), true, false)
{

    set_name("msdialog");
    Table* table = new Table(3, 3);
    table->set_size_request(-1, 600);
    table->attach (scroller,               0, 3, 0, 5);
    get_vbox()->pack_start (*table);
    
}

MixerSnapshotDialog::~MixerSnapshotDialog() 
{

}

int MixerSnapshotDialog::run() {
    show_all();
    return 0;
}