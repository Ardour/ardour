#include "option_editor.h"

namespace ARDOUR {
	class Session;
	class SessionConfiguration;
}

class SessionOptionEditor : public OptionEditor
{
public:
	SessionOptionEditor (ARDOUR::Session* s);

private:
	ARDOUR::SessionConfiguration* _session_config;
	void populate_sync_options (ARDOUR::Session*, Option*);
	void follow_sync_state (std::string, ARDOUR::Session*, Option*);
};
