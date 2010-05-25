#include <cstdlib>
#include <gtkmm.h>
#include "gtkmm2ext/motionfeedback.h"

using namespace std;
using namespace Gtkmm2ext;

int
main (int argc, char* argv[])
{
        Gtk::Main app (&argc, &argv);
        int size = atoi (argv[1]);
        Glib::RefPtr<Gdk::Pixbuf> pb;
        MotionFeedback mf (pb, MotionFeedback::Rotary, 0, 0, false, size, size);
        mf.render_file (argv[2], size, size);
}
