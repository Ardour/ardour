#ifndef __i18n_h__
#define __i18n_h__

#include "pbd/compose.h"
#include "gettext.h"

#define _(Text) dgettext (PACKAGE, Text)
#define N_(Text) gettext_noop (Text)
#define X_(Text) (Text)
#define I18N(Array) PBD::internationalize (PACKAGE, Array)

#endif // __i18n_h__
