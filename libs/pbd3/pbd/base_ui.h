#ifndef __pbd_base_ui_h__
#define __pbd_base_ui_h__

#include <string>
#include <stdint.h>

#include <sigc++/slot.h>
#include <sigc++/trackable.h>

class BaseUI : virtual public sigc::trackable {
  public:
	BaseUI (std::string name, bool with_signal_pipes);
	virtual ~BaseUI();

	BaseUI* base_instance() { return base_ui_instance; }

	std::string name() const { return _name; }

	bool ok() const { return _ok; }

	enum RequestType {
		range_guarantee = ~0
	};

	struct BaseRequestObject {
	    RequestType type;
	    sigc::slot<void> the_slot;
	};

	static RequestType new_request_type();
	static RequestType CallSlot;

  protected:
	int signal_pipe[2];
	bool _ok; 

  private:
	std::string _name; 
	BaseUI* base_ui_instance;

	static uint32_t rt_bit;

	int setup_signal_pipe ();
};

#endif /* __pbd_base_ui_h__ */
