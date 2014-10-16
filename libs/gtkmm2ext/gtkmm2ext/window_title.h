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

#ifndef WINDOW_TITLE_INCLUDED
#define WINDOW_TITLE_INCLUDED

#include <string>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

/**
 * \class The WindowTitle class can be used to maintain the 
 * consistancy of window titles between windows and dialogs.
 *
 * Each string element that is added to the window title will
 * be separated by a hyphen.
 */
class LIBGTKMM2EXT_API WindowTitle
{
public:

	/**
	 * \param title The first string/element of the window title 
	 * which will may be the application name or the document 
	 * name in a document based application.
	 */
	WindowTitle(const std::string& title);

	/**
	 * Add an string element to the window title.
	 */
	void operator+= (const std::string&);

	/// @return The window title string.
	const std::string& get_string () { return m_title;}

private:

	std::string                         m_title;

};

} // Gtkmm2ext

#endif // WINDOW_TITLE_INCLUDED
