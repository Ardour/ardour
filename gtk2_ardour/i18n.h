/*
    Copyright (C) 2000-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __i18n_h__
#define __i18n_h__

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "gettext.h"

#include <vector>
#include <string>

#define _(Text)  dgettext (PACKAGE,Text)
#define N_(Text) gettext_noop (Text)
#define X_(Text) Text
#define I18N(Array) PBD::internationalize (PACKAGE, Array)
/** Use this to translate strings that have different meanings in different places.
 *  Text should be of the form Context|Message.
 */
#define S_(Text) PBD::sgettext (PACKAGE, Text)

/** Use this to translate strings with plural forms
 */
#define P_(Singular,Plural,HowMany) dngettext (PACKAGE, (Singular), (Plural), (HowMany))

#endif // __i18n_h__
