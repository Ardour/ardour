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

#include "gtkmm2ext/window_title.h"

#include "i18n.h"

using namespace std;

namespace {
	
// I don't know if this should be translated.
const char* const title_separator = X_(" - ");

} // anonymous namespace

namespace Gtkmm2ext {

WindowTitle::WindowTitle(const string& title)
	: m_title(title)
{

}

void
WindowTitle::operator+= (const string& element)
{
	m_title = m_title + title_separator + element;
}

}
