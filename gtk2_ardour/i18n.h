#ifndef __i18n_h__
#define __i18n_h__

#include <pbd/compose.h>
#include "gettext.h"

#include <vector>
#include <string>

std::vector<std::string> internationalize (const char **);

#define _(Text)  dgettext (PACKAGE,Text)
#define N_(Text) gettext_noop (Text)
#define X_(Text) Text

#endif // __i18n_h__
