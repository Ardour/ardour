#include <gtkmm/scrolledwindow.h>

namespace Gtkmm2ext {

/** A somewhat specialised adaption of Gtk::ScrolledWindow which is the same,
 *  except that the scrollbars are never visible.  It is useful for long toolbars
 *  which may not fit horizontally on smaller screens; it lets them extend off the
 *  right-hand side of the screen without causing the parent window to jump around.
 *
 *  It is not the same as a Gtk::ScrolledWindow with policies to never display
 *  scrollbars, as these do not behave as we require in this case.
 *
 *  It is hard-wired to perform as if it were a Gtk::ScrolledWindow with a
 *  vertical scrollbar policy of POLICY_NEVER and a horizontal policy of
 *  POLICY_AUTOMATIC.  This could be generalised.
 */
class TrimmingBin : public Gtk::ScrolledWindow
{
public:
	void on_size_request (Gtk::Requisition *);
	void on_size_allocate (Gtk::Allocation &);
};
	
}
