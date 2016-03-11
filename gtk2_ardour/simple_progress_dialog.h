#ifndef _ardour_gtk_simpple_progress_dialog_h_
#define _ardour_gtk_simpple_progress_dialog_h_

#include <gtkmm/messagedialog.h>
#include <gtkmm/button.h>
#include <gtkmm/progressbar.h>

#include "ardour/types.h"

class SimpleProgressDialog : public Gtk::MessageDialog
{
public:
	SimpleProgressDialog (std::string title, const Glib::SignalProxy0< void >::SlotType & cancel)
		: MessageDialog (title, false, MESSAGE_OTHER, BUTTONS_NONE, true)
	{
		get_vbox()->set_size_request(400,-1);
		set_title (title);
		pbar = manage (new Gtk::ProgressBar());
		pbar->show();
		get_vbox()->pack_start (*pbar, PACK_SHRINK, 4);

		Gtk::Button *cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		cancel_button->signal_clicked().connect (cancel);
		cancel_button->show();
		get_vbox()->pack_start (*cancel_button, PACK_SHRINK);
	}

	void update_progress (framecnt_t c, framecnt_t t) {
		pbar->set_fraction ((float) c / (float) t);
		// see also ARDOUR_UI::gui_idle_handler();
		int timeout = 30;
		while (gtk_events_pending() && --timeout) {
			gtk_main_iteration ();
		}
	}
private:
	Gtk::ProgressBar *pbar;
};
#endif
