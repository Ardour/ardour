#ifndef __pbd_gtkmm_choice_h__
#define __pbd_gtkmm_choice_h__

#include <gtkmm/dialog.h>
#include <gtkmm/image.h>
#include <gtkmm/stock.h>
#include <gtkmm/box.h>
#include <string>
#include <vector>

namespace Gtkmm2ext {

class Choice : public Gtk::Dialog
{
  public:
	Choice (std::string prompt, std::vector<std::string> choices, bool center = true);
	virtual ~Choice ();

  protected:
	void on_realize ();
};

} /* namespace */

#endif  // __pbd_gtkmm_choice_h__
