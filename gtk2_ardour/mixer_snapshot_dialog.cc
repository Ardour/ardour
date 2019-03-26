#include <iostream>

#include "mixer_snapshot_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace PBD;
using namespace std;

MixerSnapshotDialog::MixerSnapshotDialog()
    : ArdourDialog(_("this is a dialog"), true, false)
{

    set_name("PluginSelectorWindow");

    if(_session)
        cout << " MixerSnapshotDialog is being constructed in session -" << _session->name() << endl;
}

MixerSnapshotDialog::~MixerSnapshotDialog()
{

}

int MixerSnapshotDialog::run() {
    show_all();
    Dialog::run();
    return 0;
}