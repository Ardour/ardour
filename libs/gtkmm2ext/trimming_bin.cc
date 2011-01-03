#include <iostream>
#include "gtkmm2ext/trimming_bin.h"

using namespace std;
using namespace Gtkmm2ext;

void
TrimmingBin::on_size_request (Gtk::Requisition* r)
{
	Gtk::ScrolledWindow::on_size_request (r);

	/* Munge the height request so that it is that of the child;
	   the Gtk::ScrolledWindow's request may include space for
	   a horizontal scrollbar, which we will never show.
	*/

	Gtk::Widget* c = get_child ();
	if (c && c->is_visible ()) {
		Gtk::Requisition cr;
		c->size_request (cr);
		r->height = cr.height;
	}
}

void
TrimmingBin::on_size_allocate (Gtk::Allocation& a)
{
	/* We replace Gtk::ScrolledWindow's on_size_allocate with this
	   which accepts what we are given and forces the child to use
	   the same allocation (which may result in it being shrunk).
	*/
	
	set_allocation (a);
	Widget* c = get_child ();
	if (c && c->is_visible ()) {
		c->size_allocate (a);
	}
}
