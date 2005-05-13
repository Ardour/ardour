#ifndef __gtkmm2ext_idle_adjustment_h__
#define __gtkmm2ext_idle_adjustment_h__

#include <sys/time.h>
#include <gtkmm/adjustment.h>

namespace Gtkmm2ext {

class IdleAdjustment : public sigc::trackable
{
  public:
	IdleAdjustment (Gtk::Adjustment& adj);
	~IdleAdjustment ();

	sigc::signal<void> value_changed;

  private:
	void underlying_adjustment_value_changed();
	struct timeval last_vc;
	gint timeout_handler();
	bool timeout_queued;
};

}

#endif /* __gtkmm2ext_idle_adjustment_h__ */
