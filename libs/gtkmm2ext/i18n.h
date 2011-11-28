#ifndef __i18n_h__
#define __i18n_h__

#include <pbd/compose.h>
#include <pbd/convert.h>
#include "gettext.h"

#define _(Text) dgettext (PACKAGE, Text)
#define N_(Text) gettext_noop (Text)
#define X_(Text) (Text)
/** Use this to translate strings that have different meanings in different places.
 *  Text should be of the form Context|Message.
 */
#define S_(Text) PBD::sgettext (PACKAGE, Text)

#endif // __i18n_h__
