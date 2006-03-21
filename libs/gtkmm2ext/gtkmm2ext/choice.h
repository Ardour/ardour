#ifndef __pbd_gtkmm_choice_h__
#define __pbd_gtkmm_choice_h__

#include <gtkmm/dialog.h>
#include <string>
#include <vector>

namespace Gtkmm2ext {

class Choice : public Gtk::Dialog
{
  public:
	Choice (std::string prompt, std::vector<std::string> choices);
	virtual ~Choice ();

	int get_choice ();

  protected:
	void on_realize ();
	
  private:
	int  which_choice;
	bool choice_made (GdkEventButton* ev, int nbutton);
};

} /* namespace */

#endif  // __pbd_gtkmm_choice_h__
