/* Copyright (C) 2004 The glibmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm.h>
#include <iostream>

Glib::ustring bool_text (bool val)
{
	return val ? "true" : "false";
}

int main(int argc, char** argv)
{
  Glib::init();
  
  /* Reusing one regex pattern: */ 
  Glib::RefPtr<Glib::Regex> regex = Glib::Regex::create ("(a)?(b)");
  std::cout << "Pattern=" << regex->get_pattern() 
     << ", with string=abcd, result=" 
     << bool_text( regex->match("abcd") )
     << std::endl;
  std::cout << "Pattern=" << regex->get_pattern()
     << ", with string=1234, result=" 
     << bool_text( regex->match("1234") )
     << std::endl;
  std::cout << std::endl;

  /* Using the static function without a regex instance: */
  std::cout << "Pattern=b* with string=abcd, result=" 
    << bool_text( Glib::Regex::match_simple("b*", "abcd") )
    << std::endl;

  return 0;
}

