#ifndef __gtk_ardour_missing_plugin_dialog_h__
#define __gtk_ardour_missing_plugin_dialog_h__

#include <string>
#include "ardour_dialog.h"

namespace ARDOUR {
        class Session;
}

class MissingPluginDialog : public ArdourDialog
{
  public:
        MissingPluginDialog (ARDOUR::Session *, std::list<std::string> const &);
};

#endif /* __gtk_ardour_missing_plugin_dialog_h__ */
